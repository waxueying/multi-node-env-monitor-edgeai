#include "bsp_can.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/twai.h"

/* CAN 引脚：TX=GPIO11，RX=GPIO12 */
#define CAN_TX_GPIO GPIO_NUM_11
#define CAN_RX_GPIO GPIO_NUM_12

static can_rx_cb_t s_rx_cb = NULL;

/* bus-off 时完全重装驱动恢复 */
static void can_recover_if_busoff(void)
{
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK && status.state == TWAI_STATE_BUS_OFF)
    {
        /* printf("CAN bus-off，正在完全重装驱动恢复...\n"); */
        twai_stop();
        twai_driver_uninstall();
        twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
        twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
        twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        twai_driver_install(&g, &t, &f);
        twai_start();
    }
}

esp_err_t can_init(void)
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK)
        return err;

    err = twai_start();
    if (err == ESP_OK)
    {
        /* printf("CAN 已启动 (500kbps, NORMAL mode)\n"); */
    }

    return err;
}

void can_set_rx_callback(can_rx_cb_t cb)
{
    s_rx_cb = cb;
}

bool can_send(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (len > 8)
        return false;

    can_recover_if_busoff();

    twai_message_t tx_msg = {
        .identifier = id,
        .data_length_code = len,
    };
    memcpy(tx_msg.data, data, len);

    esp_err_t err = twai_transmit(&tx_msg, pdMS_TO_TICKS(100));
    if (err != ESP_OK)
    {
        /* printf("CAN 发送失败! id=0x%03X err=0x%X 请检查总线/终端电阻\n", (unsigned)id, err); */
        return false;
    }
    return true;
}

void can_receive_task(void *arg)
{
    twai_message_t rx_msg;
    while (1)
    {
        if (twai_receive(&rx_msg, pdMS_TO_TICKS(1000)) == ESP_OK)
        {
            /* 先交给应用层处理（命令帧等） */
            if (s_rx_cb != NULL)
                s_rx_cb(rx_msg.identifier, rx_msg.data, rx_msg.data_length_code);

            /* 暂时静音（只留 AI 结果）：每收到一帧都打印，是最主要的刷屏来源
            printf("收到 CAN: ID=0x%03lX DLC=%d data:",
                   (unsigned long)rx_msg.identifier, rx_msg.data_length_code);
            for (int i = 0; i < rx_msg.data_length_code; i++)
                printf(" %02X", rx_msg.data[i]);
            printf("\n");
            */
        }
    }
}
