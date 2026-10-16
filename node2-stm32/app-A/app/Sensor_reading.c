#include "Sensor_reading.h"
#include "can_protocol.h"
#include "bsp_photo.h"
#include "bsp_vibration.h"

#include <stdio.h>

/* ===========================================================================
 * 节点2 传感器任务
 *
 *   光电 E3F-DS30C4 -> PB0（开关量）
 *   振动 SW-420     -> PB1（开关量）
 *
 * 采样 100Hz（10ms 一次）。数据写进 g_node2（见 can_protocol.h），由 CAN 任务
 * 周期上报；产生的"事件"通过 can_proto_post_event() 投给 CAN 任务。
 *
 * 两个传感器都只有开关量，所以没有真正的物理量：
 *   光电 -> 触发次数 + 当前状态位
 *   振动 -> 用"1 秒窗口内有效电平占比 x1000"当作振动强度 vib_level(0~1000)，
 *           这是开关量振动传感器能给出的最常见的强度近似。
 * =========================================================================== */

void AI_edge_computing(float buffer[], int in_len);

/* AI 模型输入 f32(1x3)、输出 f32(1x3)（见 X-CUBE-AI/App/network1_generate_report.txt），
 * 所以输入输出 buffer 各要 3 个 float —— 原来 out_buffer 只开了 1 个，
 * 模型往里面写 3 个 float 会越界 8 字节。 */
float in_buffer[3];
float out_buffer[3];

osThreadId_t cSensor_reading_task_ID;
osThreadAttr_t Sensor_reading_task_attributes = {"Sensor_reading_task", 0, 0, 0, 0, 1024, (osPriority_t)osPriorityNormal7};

/* ---- 采样与判定参数 ---- */
#define SR_PERIOD_MS        10              /* 采样周期 -> 100Hz */
#define SR_TICKS_PER_SEC    (1000 / SR_PERIOD_MS)
#define SR_PHOTO_DEBOUNCE   2               /* 连续 2 次读到新电平才认（20ms 消抖） */
#define SR_VIB_ENTER_N      3               /* 连续 3 次读到振动 -> 进入振动状态（30ms） */
#define SR_VIB_EXIT_N       50              /* 连续 50 次读到静止 -> 结束（500ms） */
#define SR_STUCK_TICKS      1000            /* 连续 10s 一直是有效电平 -> 判定卡死 */
#define SR_AI_DIV           100             /* 每 100 次采样（1s）跑一次 AI */

void Sensor_reading_task(void* argument){

	uint8_t  photo_stable = 0;		/* 消抖后的状态：1 = 检测到物体 */
	uint8_t  photo_same   = 0;		/* 连续读到"和当前状态不同"的次数 */
	uint32_t photo_active = 0;		/* 连续处于"有物体"的采样数 */

	uint16_t vib_run      = 0;		/* 连续振动的采样数 */
	uint16_t vib_quiet    = 0;		/* 连续静止的采样数 */
	uint8_t  vib_alarm    = 0;		/* 是否处于一次振动过程中 */
	uint16_t vib_peak     = 0;		/* 本次振动的强度峰值 */
	uint16_t vib_win_hit  = 0;		/* 本秒窗口内读"在振"的采样数 */
	uint16_t vib_win_all  = 0;		/* 本秒窗口内的采样总数 */
	uint32_t vib_active   = 0;		/* 连续处于"在振"的采样数 */
	uint32_t ai_div       = 0;		/* AI 分频计数 */

	(void)argument;

	photo_init();
	vib_sw_init();

	while(1){
		/* ---------------- 1. 光电：消抖 + 上升沿计次 ---------------- */
		uint8_t photo_now = photo_is_active() ? 1u : 0u;

		if (photo_now == photo_stable)
		{
			photo_same = 0;			/* 电平没变，偏差计数清零 */
		}
		else if (++photo_same >= SR_PHOTO_DEBOUNCE)
		{
			/* 新电平稳定保持够久了，才认这次变化 */
			photo_same   = 0;
			photo_stable = photo_now;

			if (photo_stable)
			{
				g_node2.photo_count++;	/* 计一次触发 */
				can_proto_post_event(N2_EVT_PHOTO, 0);
			}
		}

		g_node2.photo_bits = photo_stable ? N2_ST_PHOTO1 : 0;

		/* 卡死判断：一直"有物体"超过 10s，基本是传感器坏 / 被长时间挡住 */
		if (photo_stable)
		{
			if (++photo_active >= SR_STUCK_TICKS)
				g_node2.fault_bits |= N2_FAULT_PHOTO;
		}
		else
		{
			photo_active = 0;
			g_node2.fault_bits &= (uint8_t)~N2_FAULT_PHOTO;
		}

		/* ---------------- 2. 振动：开关量 -> 强度指标 + 事件 ---------------- */
		uint8_t vib_now = vib_sw_active() ? 1u : 0u;

		vib_win_all++;
		if (vib_now)
			vib_win_hit++;

		if (vib_win_all >= SR_TICKS_PER_SEC)
		{
			/* 1 秒窗口结束：有效电平占比 x1000 就是本秒的振动强度 */
			g_node2.vib_level = (uint16_t)((uint32_t)vib_win_hit * 1000u / vib_win_all);
			if (vib_alarm && g_node2.vib_level > vib_peak)
				vib_peak = g_node2.vib_level;

			vib_win_all = 0;
			vib_win_hit = 0;
		}

		if (vib_now)
		{
			vib_quiet = 0;
			if (vib_run < 1000)
				vib_run++;

			if (!vib_alarm && vib_run >= SR_VIB_ENTER_N)
			{
				vib_alarm = 1;		/* 开始一次振动过程 */
				vib_peak  = g_node2.vib_level;
			}

			if (++vib_active >= SR_STUCK_TICKS)
				g_node2.fault_bits |= N2_FAULT_VIB;
		}
		else
		{
			vib_run    = 0;
			vib_active = 0;
			g_node2.fault_bits &= (uint8_t)~N2_FAULT_VIB;

			if (vib_alarm && ++vib_quiet >= SR_VIB_EXIT_N)
			{
				/* 安静够久了：本次振动结束，上报一次事件 */
				vib_alarm = 0;
				vib_quiet = 0;
				g_node2.vib_count++;
				can_proto_post_event(N2_EVT_VIB, vib_peak);
			}
		}
		g_node2.vib_alarm = vib_alarm;

		/* ---------------- 3. AI（每 1s 跑一次） ---------------- */
		if (++ai_div >= SR_AI_DIV)
		{
			float feat[3];

			ai_div = 0;

			/* 节点2 只有两路开关量，凑成模型要的 3 个输入 */
			feat[0] = g_node2.photo_bits ? 1.0f : 0.0f;
			feat[1] = (float)g_node2.vib_level / 1000.0f;
			feat[2] = (float)g_node2.vib_count;

			AI_edge_computing(feat, sizeof(feat) / sizeof(feat[0]));
		}

		osDelay(SR_PERIOD_MS);
	}
}

/* AI 边缘计算：把特征塞进模型输入 buffer，跑一次推理，再打印结果 */
void AI_edge_computing(float buffer[], int in_len){
	uint8_t cls = 0;

	for (int i = 0; i < in_len && i < 3; i++) {
		in_buffer[i] = buffer[i];
	}

	MX_X_CUBE_AI_Process();

	/* 模型是 3 分类的 softmax 输出，取最大值下标当作类别 */
	if (out_buffer[1] > out_buffer[cls]) cls = 1;
	if (out_buffer[2] > out_buffer[cls]) cls = 2;

	printf("[NODE2] photo=%u/%u vib=%u/%u level=%u ai=%u\r\n",
	       (unsigned)g_node2.photo_bits, (unsigned)g_node2.photo_count,
	       (unsigned)g_node2.vib_alarm, (unsigned)g_node2.vib_count,
	       (unsigned)g_node2.vib_level, (unsigned)cls);
}

void Sensor_reading_task_Init(void){
	 cSensor_reading_task_ID = osThreadNew(Sensor_reading_task, NULL, &Sensor_reading_task_attributes);
}

