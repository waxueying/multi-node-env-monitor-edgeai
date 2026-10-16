#include "environment_monitor.h"

#include <string.h>

/* 滞回量：报警后需回落这么多才算恢复 */
#define ENV_HYST_TEMP   1.0f    /* ℃ */
#define ENV_HYST_HUMI   3.0f    /* %RH */

/* ---- 五种仓储环境的温湿度限值表 ---- */
static const env_limits_t s_profiles[ENV_PROFILE_COUNT] =
{
    /* 食品药品：阴凉库（≤20℃），药品储存湿度 35~75%RH */
    [ENV_PROFILE_FOOD_DRUG]     = {  0.0f, 20.0f, 35.0f, 75.0f },

    /* 电子元件：常温仓储，重点是防潮（潮湿敏感器件建议 <60%RH） */
    [ENV_PROFILE_ELECTRONICS]   = {  5.0f, 35.0f, 30.0f, 60.0f },

    /* 纺织品/纸箱：湿度过低纸箱脆裂，过高发霉 */
    [ENV_PROFILE_TEXTILE_PAPER] = {  5.0f, 35.0f, 40.0f, 70.0f },

    /* 化学品：常规储存，避免高温与过度潮湿 */
    [ENV_PROFILE_CHEMICAL]      = {  5.0f, 30.0f, 20.0f, 75.0f },

    /* 普通货物：常温库通用，日用品/五金/杂货等无特殊要求货物 */
    [ENV_PROFILE_GENERAL]       = {  5.0f, 35.0f, 30.0f, 75.0f },
};

static const char *s_profile_names[ENV_PROFILE_COUNT] =
{
    [ENV_PROFILE_FOOD_DRUG]     = "食品/药品(阴凉库)",
    [ENV_PROFILE_ELECTRONICS]   = "电子元件(防潮)",
    [ENV_PROFILE_TEXTILE_PAPER] = "纺织品/纸箱",
    [ENV_PROFILE_CHEMICAL]      = "化学品",
    [ENV_PROFILE_GENERAL]       = "普通货物(常温库)",
};

static env_profile_t s_profile = ENV_PROFILE_FOOD_DRUG;
static env_status_t  s_status;

void env_monitor_init(env_profile_t profile)
{
    memset(&s_status, 0, sizeof(s_status));
    s_profile = (profile < ENV_PROFILE_COUNT) ? profile : ENV_PROFILE_FOOD_DRUG;
}

void env_monitor_set_profile(env_profile_t profile)
{
    if (profile >= ENV_PROFILE_COUNT)
        return;

    /* 切换标准后旧报警状态不再适用，清零重新判定 */
    memset(&s_status, 0, sizeof(s_status));
    s_profile = profile;
}

env_profile_t env_monitor_get_profile(void)
{
    return s_profile;
}

const env_limits_t *env_monitor_get_limits(void)
{
    return &s_profiles[s_profile];
}

const char *env_monitor_profile_name(env_profile_t profile)
{
    if (profile >= ENV_PROFILE_COUNT)
        return "未知";
    return s_profile_names[profile];
}

uint32_t env_monitor_update(float temperature, float humidity)
{
    uint32_t events = 0;
    const env_limits_t *lim = &s_profiles[s_profile];

    /* ---- 温度上限（滞回） ---- */
    if (!s_status.temp_high && temperature > lim->temp_max)
    {
        s_status.temp_high = true;
        events |= ENV_EVT_TEMP_HIGH;
    }
    else if (s_status.temp_high && temperature < lim->temp_max - ENV_HYST_TEMP)
    {
        s_status.temp_high = false;
    }

    /* ---- 温度下限（滞回） ---- */
    if (!s_status.temp_low && temperature < lim->temp_min)
    {
        s_status.temp_low = true;
        events |= ENV_EVT_TEMP_LOW;
    }
    else if (s_status.temp_low && temperature > lim->temp_min + ENV_HYST_TEMP)
    {
        s_status.temp_low = false;
    }

    /* ---- 湿度上限（滞回） ---- */
    if (!s_status.humi_high && humidity > lim->humi_max)
    {
        s_status.humi_high = true;
        events |= ENV_EVT_HUMI_HIGH;
    }
    else if (s_status.humi_high && humidity < lim->humi_max - ENV_HYST_HUMI)
    {
        s_status.humi_high = false;
    }

    /* ---- 湿度下限（滞回） ---- */
    if (!s_status.humi_low && humidity < lim->humi_min)
    {
        s_status.humi_low = true;
        events |= ENV_EVT_HUMI_LOW;
    }
    else if (s_status.humi_low && humidity > lim->humi_min + ENV_HYST_HUMI)
    {
        s_status.humi_low = false;
    }

    return events;
}

const env_status_t *env_monitor_get_status(void)
{
    return &s_status;
}
