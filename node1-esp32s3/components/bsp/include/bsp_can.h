#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* CAN 总线封装（ESP32-S3 TWAI 控制器，需外接收发器如 TJA1050）
 *
 * 接线：
 *   TX -> GPIO11（接 TJA1050 TXD）
 *   RX -> GPIO12（接 TJA1050 RXD）
 *   总线参数：500kbps，NORMAL 模式，接收所有 ID
 *
 * 说明：本文件只封装总线层（初始化 / 收发 / bus-off 恢复 / 接收回调），
 *       不关心字节含义；帧 ID 与数据格式见 can_protocol.h。
 */

/* 接收回调：收到任意帧时被调用（在接收任务上下文），可为 NULL */
typedef void (*can_rx_cb_t)(uint32_t id, const uint8_t *data, uint8_t len);

/* 安装并启动 CAN 驱动（500kbps, NORMAL）。失败返回错误码 */
esp_err_t can_init(void);

/* 注册接收回调（可选）。传 NULL 取消注册 */
void can_set_rx_callback(can_rx_cb_t cb);

/* 发送一帧标准帧（数据 0~8 字节）。返回 true = 成功。
 * 发送前自动检查 bus-off 状态，必要时重装驱动恢复。 */
bool can_send(uint32_t id, const uint8_t *data, uint8_t len);

/* FreeRTOS 任务入口：接收总线帧，先回调应用层，再打印（调试用）
 * 用法：xTaskCreate(can_receive_task, "can_rx", 4096, NULL, 5, NULL); */
void can_receive_task(void *arg);
