#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ===========================================================================
 * 结构安全监测模块（纯规则算法）
 *
 * 面向货架立柱的三类结构安全事件：
 *
 *   1) 撞击检测
 *      动态分量 = 当前加速度 - 重力估计。
 *      动态分量 > SM_IMPACT_THRESH_G            → 进入撞击状态，记录峰值；
 *      动态分量 < SM_IMPACT_RELEASE_G 连续 N 次 → 撞击结束，产生一次撞击事件。
 *      事件携带整个撞击过程中的加速度峰值（单位 g）。
 *
 *   2) 倾斜检测
 *      上电后前 SM_CALIB_SAMPLES 个样本的平均重力方向作为"安装基准"；
 *      实时重力方向（低通滤波）与基准的夹角即倾角。
 *      倾角 > SM_TILT_ALARM_DEG → 倾斜报警；< SM_TILT_CLEAR_DEG → 解除（滞回）。
 *      原理：立柱被撞变形后，传感器姿态相对安装态出现固定偏移。
 *
 *   3) 振动监测
 *      动态分量的 1 秒滑动窗口 RMS，反映结构松动 / 持续异常振动强度。
 *
 * 采样约定：每 10ms 调用一次 structure_monitor_update()（100Hz）。
 * 加速度单位：g（mpu6050_get_acceleration() 的返回值，无需换算）。
 * =========================================================================== */

/* ---- 可调参数 ---- */
#define SM_CALIB_SAMPLES        100     /* 上电标定样本数（100Hz → 1s） */

#define SM_IMPACT_THRESH_G      0.50f   /* 撞击判定阈值（动态分量，g） */
#define SM_IMPACT_RELEASE_G     0.20f   /* 撞击结束阈值（g） */
#define SM_IMPACT_RELEASE_N     5       /* 连续 N 个样本低于释放阈值 → 结束 */
#define SM_IMPACT_MAX_SAMPLES   200     /* 撞击最长样本数（2s），超时强制结束 */

#define SM_TILT_ALARM_DEG       2.0f    /* 倾斜报警阈值（度） */
#define SM_TILT_CLEAR_DEG       1.0f    /* 倾斜解除阈值（度，滞回） */

#define SM_VIB_WINDOW_SAMPLES   100     /* 振动 RMS 窗口（100Hz → 1s） */

#define SM_GRAV_LPF_ALPHA       0.01f   /* 重力估计低通系数（100Hz → 约 1s 时间常数） */
#define SM_GRAV_UPDATE_LIMIT_G  0.15f   /* 准静态门限：动态分量低于此值才更新重力估计 */

/* ---- 事件位（structure_monitor_update() 的返回值） ---- */
#define STRUCT_EVT_IMPACT       (1u << 0)   /* 一次撞击结束（峰值有效） */
#define STRUCT_EVT_TILT_ALARM   (1u << 1)   /* 倾角进入报警 */
#define STRUCT_EVT_TILT_CLEAR   (1u << 2)   /* 倾角报警解除 */

/* ---- 监测状态 ---- */
typedef struct
{
    bool     calibrated;          /* 安装基准是否已标定完成 */
    bool     tilt_alarm;          /* 当前是否处于倾斜报警状态 */
    float    tilt_deg;            /* 相对安装基准的倾角（度） */
    float    vib_rms_g;           /* 振动 RMS（g） */
    float    last_impact_peak_g;  /* 最近一次撞击峰值（g） */
    uint16_t impact_count;        /* 累计撞击次数 */
} struct_status_t;

/* 复位模块状态，重新开始上电标定 */
void structure_monitor_init(void);

/* 送入一帧加速度采样（单位 g，100Hz），返回本次产生的事件位 */
uint32_t structure_monitor_update(float ax, float ay, float az);

/* 读取当前监测状态（只读） */
const struct_status_t *structure_monitor_get_status(void);
