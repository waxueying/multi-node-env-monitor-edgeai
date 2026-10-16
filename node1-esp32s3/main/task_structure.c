#include "app_tasks.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"               /* esp_log_level_set：故障时静音 i2cdev 库的刷屏日志 */

#include "bsp_imu.h"               /* BSP ：MPU6050 */
#include "structure_monitor.h"     /* 服务：结构安全算法（撞击/倾斜/振动） */
#include "can_protocol.h"          /* 服务：CAN 帧协议 */
#include "ai_infer.h"              /* 服务：AI 推理 */
#include "node_health.h"           /* 服务：传感器健康统计 */

/* ============================================================================
 * 结构安全任务（应用层）
 *
 * 100Hz：读 MPU6050 → 结构安全判断 → 撞击上报 / AI 推理 → CAN 上报。
 * 传感器故障时自动降频到 1Hz 重试（避免 100Hz 猛敲 I2C 刷屏），读到一帧就恢复。
 * ========================================================================== */

#define SAMPLE_PERIOD_MS        10      /* 100Hz 采样周期 */
#define STATUS_REPORT_SAMPLES   100     /* 每 100 个样本（1 秒）上报一次结构状态 */
#define FAULT_PROBE_DIV         100     /* 故障期间每 100 个周期（1 秒）才探一次 */

void task_structure(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    int  status_counter  = 0;
    bool calib_reported  = false;
    int  probe_div       = 0;           /* 故障期间的降频计数 */

    (void)arg;

    structure_monitor_init();

    while (1)
    {
        float ax, ay, az;
        bool  ok;

        /* 故障期间不再 100Hz 猛敲 I2C：降到每 1 秒探一次 */
        if (node_health_imu_fault() && ++probe_div < FAULT_PROBE_DIV)
        {
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
            continue;
        }
        probe_div = 0;

        ok = (imu_read_accel(&ax, &ay, &az) == ESP_OK);

        /* 只在"刚判故障 / 刚恢复"时打印一次，避免刷屏 */
        if (node_health_imu_report(ok))
        {
            if (node_health_imu_fault())
            {
                printf("[故障] MPU6050 连续读取失败，判定传感器故障（改为 1 秒重试，恢复会自动上报）\n");
                esp_log_level_set("i2cdev", ESP_LOG_NONE);      /* 静音库的刷屏日志 */
            }
            else
            {
                esp_log_level_set("i2cdev", ESP_LOG_WARN);      /* 恢复库日志 */
                printf("[故障] MPU6050 已恢复\n");
            }
        }

        if (!ok)
        {
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
            continue;
        }

        /* ---- 读取正常：跑结构安全算法 + AI ---- */
        {
            uint32_t events = structure_monitor_update(ax, ay, az);
            const struct_status_t *st = structure_monitor_get_status();
            ai_result_t ai;

            ai_infer_feed(ax, ay, az);      /* 100Hz 喂一帧，单位 g */

            if (st->calibrated && !calib_reported)
            {
                calib_reported = true;
                /* printf("[结构] 安装基准标定完成，开始监测\n"); */
            }

            if (events & STRUCT_EVT_IMPACT)
            {
                /* printf("[结构] 撞击事件！峰值 %.2f g（累计 %u 次）\n",
                       st->last_impact_peak_g, (unsigned)st->impact_count); */
                can_proto_send_impact();
            }
            if (events & STRUCT_EVT_TILT_ALARM)
            {
                /* printf("[结构] 倾斜报警！倾角 %.1f°\n", st->tilt_deg); */
            }
            if (events & STRUCT_EVT_TILT_CLEAR)
            {
                /* printf("[结构] 倾斜解除（倾角 %.1f°）\n", st->tilt_deg); */
            }

            if (ai_infer_poll(&ai))
            {
                printf("[AI] %s (conf:%.1f%% peak:%.2fg seq:%u)\n",
                       ai_infer_class_name(ai.cls),
                       ai.conf * 100.0f, ai.peak_g, (unsigned)ai.seq);
                can_proto_send_ai(ai.cls, ai.conf, ai.peak_g, (uint16_t)ai.seq);
            }

            if (++status_counter >= STATUS_REPORT_SAMPLES)
            {
                status_counter = 0;
                can_proto_send_struct_status();
                /* printf("[结构] 倾角 %.1f° | 振动RMS %.3f g | 累计撞击 %u 次%s\n",
                       st->tilt_deg, st->vib_rms_g, (unsigned)st->impact_count,
                       st->tilt_alarm ? " | 倾斜报警中" : ""); */
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

