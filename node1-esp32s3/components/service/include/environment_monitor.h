#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ===========================================================================
 * 仓储环境监测模块（纯规则，无 AI）
 *
 * 针对五种典型仓储货物，提供各自的温湿度合规判据，可在运行期切换：
 *
 *   模式                        温度(℃)      湿度(%RH)     依据
 *   ----------------------------------------------------------------
 *   ENV_PROFILE_FOOD_DRUG       0 ~ 20       35 ~ 75       阴凉库储存条件
 *   ENV_PROFILE_ELECTRONICS     5 ~ 35       30 ~ 60       防潮（潮湿敏感器件）
 *   ENV_PROFILE_TEXTILE_PAPER   5 ~ 35       40 ~ 70       防潮防脆裂
 *   ENV_PROFILE_CHEMICAL        5 ~ 30       20 ~ 75       化学品常规储存
 *   ENV_PROFILE_GENERAL         5 ~ 35       30 ~ 75       普通货物常温库
 *
 * 注意：DHT11 精度为 ±2℃ / ±5%RH，上表阈值用于"合规性提示"，
 *       不适用于高精度计量场合。实际项目请按所在行业的国标/行标复核。
 *
 * 检测带滞回：超限触发报警，回落到阈值内侧 ENV_HYST_* 才解除，
 *             避免在阈值附近反复抖动产生事件风暴。
 * =========================================================================== */

/* ---- 环境模式 ---- */
typedef enum
{
    ENV_PROFILE_FOOD_DRUG = 0,   /* 食品药品：阴凉库 0~20℃，35~75%RH */
    ENV_PROFILE_ELECTRONICS,     /* 电子元件：5~35℃，30~60%RH */
    ENV_PROFILE_TEXTILE_PAPER,   /* 纺织品/纸箱：5~35℃，40~70%RH */
    ENV_PROFILE_CHEMICAL,        /* 化学品：5~30℃，20~75%RH */
    ENV_PROFILE_GENERAL,         /* 普通货物：常温库 5~35℃，30~75%RH */

    ENV_PROFILE_COUNT            /* 模式总数（内部使用） */
} env_profile_t;

/* ---- 事件位（env_monitor_update() 返回值） ---- */
#define ENV_EVT_TEMP_HIGH   (1u << 0)   /* 温度高于上限 */
#define ENV_EVT_TEMP_LOW    (1u << 1)   /* 温度低于下限 */
#define ENV_EVT_HUMI_HIGH   (1u << 2)   /* 湿度高于上限 */
#define ENV_EVT_HUMI_LOW    (1u << 3)   /* 湿度低于下限 */

/* ---- 某模式的温湿度限值 ---- */
typedef struct
{
    float temp_min;   /* 温度下限（℃） */
    float temp_max;   /* 温度上限（℃） */
    float humi_min;   /* 湿度下限（%RH） */
    float humi_max;   /* 湿度上限（%RH） */
} env_limits_t;

/* ---- 当前报警状态 ---- */
typedef struct
{
    bool  temp_high;
    bool  temp_low;
    bool  humi_high;
    bool  humi_low;
} env_status_t;

/* 初始化并指定环境模式 */
void env_monitor_init(env_profile_t profile);

/* 切换环境模式（运行期可调用）。切换会清空当前报警状态，按新模式重新判定 */
void env_monitor_set_profile(env_profile_t profile);

/* 读取当前环境模式 */
env_profile_t env_monitor_get_profile(void);

/* 读取当前模式的限值 */
const env_limits_t *env_monitor_get_limits(void);

/* 模式名称（用于日志 / CAN 上报） */
const char *env_monitor_profile_name(env_profile_t profile);

/* 送入一次温湿度测量，返回本次产生的事件位 */
uint32_t env_monitor_update(float temperature, float humidity);

/* 读取当前报警状态（只读） */
const env_status_t *env_monitor_get_status(void);
