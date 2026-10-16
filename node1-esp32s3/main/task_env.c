#include "app_tasks.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dht.h"                   /* esp-idf-lib/dht 现成库 */
#include "can_protocol.h"          /* 服务：CAN 帧协议 */
#include "environment_monitor.h"   /* 服务：仓储环境标准（五种货物可切换） */
#include "node_health.h"           /* 服务：传感器健康统计 */

/* ============================================================================
 * 环境任务（应用层）
 *
 * 1Hz：读 DHT11 → 按当前环境标准判合规 → CAN 上报环境帧；
 *      同一节拍发故障帧（无论有无故障都发，兼作节点心跳）。
 *
 * 固定跑在核心0：DHT11 读取会关中断约 25ms，别去打扰核心1 上的 100Hz 采样。
 * ========================================================================== */

#define DHT_PIN                  GPIO_NUM_4     /* DHT11 DATA 引脚 */
#define ENV_PERIOD_MS           1000            /* 1Hz */

void task_env(void *arg)
{
    float humidity = 0, temperature = 0;

    (void)arg;

    while (1)
    {
        bool ok = (dht_read_float_data(DHT_TYPE_DHT11, DHT_PIN, &humidity, &temperature) == ESP_OK);

        /* 只在"刚判故障 / 刚恢复"时打印一次 */
        if (node_health_dht_report(ok))
        {
            if (node_health_dht_fault())
                printf("[故障] DHT11 连续读取失败，判定传感器故障\n");
            else
                printf("[故障] DHT11 已恢复\n");
        }

        if (ok)
        {
            uint32_t evt = env_monitor_update(temperature, humidity);   /* 合规判定 */
            (void)evt;
            /* 报警打印暂时静音（只上报）。恢复打印时把上面那行换成下面这段：
            const env_limits_t *cur = env_monitor_get_limits();
            if (evt & ENV_EVT_TEMP_HIGH)
                printf("[环境] 温度过高！%.1f℃（上限 %.1f）\n", temperature, cur->temp_max);
            if (evt & ENV_EVT_TEMP_LOW)
                printf("[环境] 温度过低！%.1f℃（下限 %.1f）\n", temperature, cur->temp_min);
            if (evt & ENV_EVT_HUMI_HIGH)
                printf("[环境] 湿度过高！%.1f%%RH（上限 %.1f）\n", humidity, cur->humi_max);
            if (evt & ENV_EVT_HUMI_LOW)
                printf("[环境] 湿度过低！%.1f%%RH（下限 %.1f）\n", humidity, cur->humi_min);
            */
            can_proto_send_env(temperature, humidity);
        }

        /* 故障帧（1Hz，与 DHT 读取同节拍）：无论有无故障都上报，
         * 兼作节点心跳 —— 即使两个传感器都失效，主节点也能确认节点在线 */
        {
            uint8_t fault_bits = 0;
            if (node_health_imu_fault()) fault_bits |= CAN_FAULT_BIT_IMU;
            if (node_health_dht_fault()) fault_bits |= CAN_FAULT_BIT_DHT;
            can_proto_send_fault(fault_bits, node_health_imu_fail_total(), node_health_dht_fail_total());
        }

        vTaskDelay(pdMS_TO_TICKS(ENV_PERIOD_MS));
    }
}

