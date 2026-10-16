#include "structure_monitor.h"

#include <math.h>
#include <string.h>

/* 弧度 → 角度 */
#define RAD_TO_DEG  57.29578f

/* ---- 内部状态 ---- */
static struct_status_t s_status;

static float s_calib_sum[3];      /* 标定累加 */
static int   s_calib_count;

static float s_baseline[3];       /* 安装基准重力向量 */
static float s_grav[3];           /* 实时重力估计（低通） */

static float s_vib_win[SM_VIB_WINDOW_SAMPLES];  /* 振动滑动窗口：存放最近 N 个 dyn 值（环形缓冲） */
static int   s_vib_idx;                         /* 环形缓冲写入位置：下一个新样本写到哪一格（0..N-1 循环） */
static int   s_vib_count;                       /* 窗口内有效样本数：启动初期不足 N，用它算均值避免除以 0 */

static bool  s_impact_active;                   /* 撞击状态标志：true=正在一次撞击中，false=空闲等待 */
static float s_impact_peak;                     /* 本次撞击过程中 dyn 的最大值（峰值保持） */
static int   s_impact_samples;                  /* 本次撞击已持续的采样次数（用于超时强制结束） */
static int   s_below_count;                     /* 连续低于"释放阈值"的次数（连续够 N 次才判定撞击结束） */

static float vec_norm(const float *v)
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

/* 两个向量的夹角（度） */
static float angle_between(const float *v1, const float *v2)
{
    const float n1 = vec_norm(v1);
    const float n2 = vec_norm(v2);
    if (n1 < 0.1f || n2 < 0.1f)
        return 0.0f;

    float c = (v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2]) / (n1 * n2);
    if (c > 1.0f)  c = 1.0f;
    if (c < -1.0f) c = -1.0f;

    return acosf(c) * RAD_TO_DEG;
}

void structure_monitor_init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    memset(s_calib_sum, 0, sizeof(s_calib_sum));
    memset(s_baseline, 0, sizeof(s_baseline));
    memset(s_grav, 0, sizeof(s_grav));
    memset(s_vib_win, 0, sizeof(s_vib_win));

    s_calib_count    = 0;
    s_vib_idx        = 0;
    s_vib_count      = 0;
    s_impact_active  = false;
    s_impact_peak    = 0.0f;
    s_impact_samples = 0;
    s_below_count    = 0;
}

uint32_t structure_monitor_update(float ax, float ay, float az)
{
    uint32_t events = 0;

    /* ---------- 1. 上电标定：平均重力方向作为安装基准 ---------- */
    if (!s_status.calibrated)
    {
        s_calib_sum[0] += ax;
        s_calib_sum[1] += ay;
        s_calib_sum[2] += az;
        s_calib_count++;

        if (s_calib_count >= SM_CALIB_SAMPLES)
        {
            for (int i = 0; i < 3; i++)
            {
                s_baseline[i] = s_calib_sum[i] / (float)s_calib_count;
                s_grav[i]     = s_baseline[i];
            }
            s_status.calibrated = true;
        }
        return 0;
    }

    /* ---------- 2. 动态分量（当前加速度 - 重力估计） ---------- */
    const float dx = ax - s_grav[0];
    const float dy = ay - s_grav[1];
    const float dz = az - s_grav[2];
    const float dyn = sqrtf(dx * dx + dy * dy + dz * dz);       //动态加速度的模长

    /* 仅在准静态时更新重力估计，避免撞击/振动污染倾角 */
    if (dyn < SM_GRAV_UPDATE_LIMIT_G)
    {
        s_grav[0] += SM_GRAV_LPF_ALPHA * (ax - s_grav[0]);
        s_grav[1] += SM_GRAV_LPF_ALPHA * (ay - s_grav[1]);
        s_grav[2] += SM_GRAV_LPF_ALPHA * (az - s_grav[2]);
    }

    /* ---------- 3. 倾角与倾斜报警（滞回） ---------- */
    s_status.tilt_deg = angle_between(s_grav, s_baseline);

    if (!s_status.tilt_alarm && s_status.tilt_deg > SM_TILT_ALARM_DEG)
    {
        s_status.tilt_alarm = true;
        events |= STRUCT_EVT_TILT_ALARM;
    }
    else if (s_status.tilt_alarm && s_status.tilt_deg < SM_TILT_CLEAR_DEG)
    {
        s_status.tilt_alarm = false;
        events |= STRUCT_EVT_TILT_CLEAR;
    }

    /* ---------- 4. 振动 RMS（滑动窗口） ---------- */
    s_vib_win[s_vib_idx] = dyn;
    s_vib_idx = (s_vib_idx + 1) % SM_VIB_WINDOW_SAMPLES;
    if (s_vib_count < SM_VIB_WINDOW_SAMPLES)
        s_vib_count++;

    float sum_sq = 0.0f;
    for (int i = 0; i < s_vib_count; i++)
        sum_sq += s_vib_win[i] * s_vib_win[i];
    s_status.vib_rms_g = sqrtf(sum_sq / (float)s_vib_count);

    /* ---------- 5. 撞击检测（阈值触发 → 峰值保持 → 回落结束） ---------- */
    if (!s_impact_active)
    {
        if (dyn > SM_IMPACT_THRESH_G)
        {
            s_impact_active  = true;
            s_impact_peak    = dyn;
            s_impact_samples = 1;
            s_below_count    = 0;
        }
    }
    else
    {
        s_impact_samples++;
        if (dyn > s_impact_peak)
            s_impact_peak = dyn;

        if (dyn < SM_IMPACT_RELEASE_G)
            s_below_count++;
        else
            s_below_count = 0;

        if (s_below_count >= SM_IMPACT_RELEASE_N || s_impact_samples >= SM_IMPACT_MAX_SAMPLES)
        {
            s_impact_active = false;
            s_status.impact_count++;
            s_status.last_impact_peak_g = s_impact_peak;
            events |= STRUCT_EVT_IMPACT;
        }
    }

    return events;
}

const struct_status_t *structure_monitor_get_status(void)
{
    return &s_status;
}
