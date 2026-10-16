#include "can_protocol.h"

#include <stdio.h>

#include "pal_time.h"                    /* PAL：平台时间基（时间戳统一从这里取） */

#include "bsp_can.h"                /* 总线层（BSP）：can_send / can_set_rx_callback */
#include "environment_monitor.h"    /* 环境状态与模式号来源 */
#include "structure_monitor.h"      /* 结构状态来源 */

/* 本节点 ID，由 can_proto_init() 设置 */
static uint8_t s_node_id = 0x01;

/* ---- 0x456 环境数据帧：温度 + 湿度 + 报警位 + 当前模式 ---- */
bool can_proto_send_env(float temperature, float humidity)
{
    int16_t temp_x10 = (int16_t)(temperature * 10);
    int16_t hum_x10  = (int16_t)(humidity * 10);

    const env_status_t *es = env_monitor_get_status();
    uint8_t alarm = 0;
    if (es->temp_high) alarm |= 0x01;
    if (es->temp_low)  alarm |= 0x02;
    if (es->humi_high) alarm |= 0x04;
    if (es->humi_low)  alarm |= 0x08;

    uint8_t data[8] = {
        (uint8_t)(temp_x10 & 0xFF), (uint8_t)((temp_x10 >> 8) & 0xFF),
        (uint8_t)(hum_x10 & 0xFF),  (uint8_t)((hum_x10 >> 8) & 0xFF),
        alarm,
        (uint8_t)env_monitor_get_profile(),
        0x00, 0x00,
    };
    return can_send(CAN_ID_ENV_DATA, data, sizeof(data));
}

/* ---- 0x100 结构状态帧 ---- */
bool can_proto_send_struct_status(void)
{
    const struct_status_t *st = structure_monitor_get_status();

    int16_t  tilt_x10 = (int16_t)(st->tilt_deg * 10.0f);
    uint16_t vib_mg   = (uint16_t)(st->vib_rms_g * 1000.0f);
    uint8_t  flags    = (st->calibrated ? 0x01 : 0x00) | (st->tilt_alarm ? 0x02 : 0x00);

    uint8_t data[8] = {
        s_node_id,
        flags,
        (uint8_t)(tilt_x10 & 0xFF),         (uint8_t)((tilt_x10 >> 8) & 0xFF),
        (uint8_t)(vib_mg & 0xFF),           (uint8_t)((vib_mg >> 8) & 0xFF),
        (uint8_t)(st->impact_count & 0xFF), (uint8_t)((st->impact_count >> 8) & 0xFF),
    };
    return can_send(CAN_ID_STRUCT_STATUS, data, sizeof(data));
}

/* ---- 0x101 撞击事件帧 ---- */
bool can_proto_send_impact(void)
{
    const struct_status_t *st = structure_monitor_get_status();
    uint16_t peak_x100 = (uint16_t)(st->last_impact_peak_g * 100.0f);
    uint32_t ts_ms     = pal_time_ms();

    uint8_t data[8] = {
        s_node_id,
        0x01,   /* 事件类型：撞击 */
        (uint8_t)(peak_x100 & 0xFF), (uint8_t)((peak_x100 >> 8) & 0xFF),
        (uint8_t)(ts_ms & 0xFF), (uint8_t)((ts_ms >> 8) & 0xFF),
        (uint8_t)((ts_ms >> 16) & 0xFF), (uint8_t)((ts_ms >> 24) & 0xFF),
    };
    return can_send(CAN_ID_STRUCT_IMPACT, data, sizeof(data));
}

/* ---- 0x103 AI 推理结果帧：类别 + 置信度 + 峰值 + 序号 ---- */
bool can_proto_send_ai(int cls, float conf, float peak_g, uint16_t seq)
{
    /* 置信度 0~1 放大 1000 倍；峰值放大 100 倍（0.01g 分辨率） */
    uint16_t conf_x1000 = (uint16_t)(conf * 1000.0f);
    uint16_t peak_x100  = (uint16_t)(peak_g * 100.0f);

    uint8_t data[8] = {
        s_node_id,
        (uint8_t)cls,
        (uint8_t)(conf_x1000 & 0xFF), (uint8_t)((conf_x1000 >> 8) & 0xFF),
        (uint8_t)(peak_x100 & 0xFF),  (uint8_t)((peak_x100 >> 8) & 0xFF),
        (uint8_t)(seq & 0xFF),        (uint8_t)((seq >> 8) & 0xFF),
    };
    return can_send(CAN_ID_AI_RESULT, data, sizeof(data));
}

/* ---- 0x102 故障上报帧：故障标志 + 累计失败次数 + 运行时长 ---- */
bool can_proto_send_fault(uint8_t fault_bits, uint16_t imu_fails, uint16_t dht_fails)
{
    /* 运行时长：esp_timer 自启动计时，换算为秒（uint16 约 18.2 小时后回绕） */
    uint16_t uptime_s = (uint16_t)(pal_time_ms() / 1000);

    uint8_t data[8] = {
        s_node_id,
        fault_bits,
        (uint8_t)(imu_fails & 0xFF), (uint8_t)((imu_fails >> 8) & 0xFF),
        (uint8_t)(dht_fails & 0xFF), (uint8_t)((dht_fails >> 8) & 0xFF),
        (uint8_t)(uptime_s & 0xFF),  (uint8_t)((uptime_s >> 8) & 0xFF),
    };
    return can_send(CAN_ID_FAULT_REPORT, data, sizeof(data));
}

/* ---- 下行命令解析（由 CAN.c 的接收任务回调进入） ---- */
static void can_proto_rx_handler(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (id != CAN_ID_CMD_ENV_PROFILE || len < 2 || data[0] != CMD_SET_ENV_PROFILE)
        return;

    uint8_t target = data[1];
    bool ok = (target < ENV_PROFILE_COUNT);

    if (ok)
    {
        /* 切换标准后旧报警状态由 env_monitor 内部清零，无需在此处理 */
        env_monitor_set_profile((env_profile_t)target);
        /* printf("[环境] 模式已切换为：%s\n", env_monitor_profile_name((env_profile_t)target)); */
    }
    else
    {
        /* printf("[环境] 无效模式号 %u，忽略\n", (unsigned)target); */
    }

    /* 回执：B0=当前模式号，B1=结果（0=成功，1=失败） */
    uint8_t ack[2] = { (uint8_t)env_monitor_get_profile(), ok ? 0x00 : 0x01 };
    can_send(CAN_ID_CMD_ENV_PROFILE, ack, sizeof(ack));
}

void can_proto_init(uint8_t node_id)
{
    s_node_id = node_id;
    can_set_rx_callback(can_proto_rx_handler);
}
