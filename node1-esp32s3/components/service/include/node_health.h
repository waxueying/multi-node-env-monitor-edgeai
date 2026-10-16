#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * 节点健康状态（服务层）
 *
 * 把"传感器是不是坏了"从任务里抽出来集中管理：
 *   - 任务只负责"报结果"（这次读成功了吗）
 *   - 本模块负责"下判断"（连续失败几次才算故障、怎么恢复）
 *   - CAN 故障帧从这里取故障位和累计失败次数
 *
 * 判据：连续失败达到阈值才判故障（避免偶发干扰误报）；一旦读成功立即恢复。
 * 单写者：IMU 一侧只由结构任务写、DHT 一侧只由环境任务写，
 *         uint16/uint8 在 ESP32 上是对齐的单次访问，不会读到撕裂值，无需加锁。
 * ========================================================================== */

/* 上报一次读取结果。
 * 返回 true = 故障状态刚刚变化（刚判故障 或 刚恢复）—— 调用方据此只打印一次，避免刷屏 */
bool node_health_imu_report(bool ok);
bool node_health_dht_report(bool ok);

bool     node_health_imu_fault(void);
bool     node_health_dht_fault(void);
uint16_t node_health_imu_fail_total(void);
uint16_t node_health_dht_fail_total(void);

