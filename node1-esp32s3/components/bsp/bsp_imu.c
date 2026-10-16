#include "bsp_imu.h"

#include <stdio.h>

#include "i2cdev.h"
#include "mpu6050.h"

/* MPU6050 接线：SDA=GPIO5，SCL=GPIO6，AD0 接地（0x68） */
#define IMU_SDA  GPIO_NUM_5
#define IMU_SCL  GPIO_NUM_6
#define IMU_ADDR MPU6050_I2C_ADDRESS_LOW

static mpu6050_dev_t s_dev;

esp_err_t imu_init(void)
{
    esp_err_t err = i2cdev_init();
    if (err != ESP_OK)
        return err;

    err = mpu6050_init_desc(&s_dev, IMU_ADDR, 0, IMU_SDA, IMU_SCL);
    if (err != ESP_OK)
        return err;

    err = mpu6050_init(&s_dev);
    if (err != ESP_OK)
        return err;

    /* 撞击检测需要更大量程：默认 ±2g 会被撞峰削顶，改用 ±8g */
    err = mpu6050_set_full_scale_accel_range(&s_dev, MPU6050_ACCEL_RANGE_8);
    if (err != ESP_OK)
        return err;

    /* 94Hz 带宽低通：滤除高频噪声，同时保留撞击尖峰 */
    err = mpu6050_set_dlpf_mode(&s_dev, MPU6050_DLPF_2);
    if (err != ESP_OK)
        return err;

    /* printf("MPU6050 初始化成功（量程 ±8g，DLPF 94Hz）\n"); */
    return ESP_OK;
}

esp_err_t imu_read_accel(float *x, float *y, float *z)
{
    mpu6050_acceleration_t accel;

    esp_err_t err = mpu6050_get_acceleration(&s_dev, &accel);
    if (err != ESP_OK)
        return err;

    *x = accel.x;
    *y = accel.y;
    *z = accel.z;
    return ESP_OK;
}
