#pragma once

#include "esp_err.h"

/* MPU6050 六轴传感器封装（I2C）
 *
 * 接线：
 *   VCC -> 3.3V
 *   GND -> GND
 *   SDA -> GPIO5
 *   SCL -> GPIO6
 *   AD0 -> GND（器件地址 0x68）
 *
 * 初始化配置：加速度量程 ±8g，DLPF 94Hz（适合撞击 / 振动检测）
 *
 * 注：文件名用 IMU 而非 MPU6050，避免与 esp-idf-lib 的 mpu6050.h
 *     在 Windows（大小写不敏感）下同名冲突。
 */

/* 初始化 I2C 总线与 MPU6050（量程 ±8g，DLPF 94Hz）。失败返回错误码 */
esp_err_t imu_init(void);

/* 读取三轴加速度，单位 g（静止平放时 z ≈ 1.0） */
esp_err_t imu_read_accel(float *x, float *y, float *z);
