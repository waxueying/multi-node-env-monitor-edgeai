/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-27     RT-Thread    OLED 状态显示（CAN 接收监视）
 * 2026-09-29     RT-Thread    AI 页与诊断页轮播
 * 2026-10-08     RT-Thread    精简：只保留 AI 结果页，诊断页与SD寄存器监视页移除
 */

#include <rtthread.h>
#include <rtdevice.h>
#include "oled_display.h"
#include "can_receiver.h"
#include "OLED.h"

#define DBG_TAG "oled"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* 显示线程优先级比数据转发线程(11)低，不抢占业务 */
#define OLED_THREAD_PRIORITY   20
#define OLED_THREAD_STACK_SIZE 1536
#define OLED_REFRESH_MS        500

/* 收到新的 AI 结果后，AI 页停留这么多次刷新（10×500ms = 5 秒）再回到待机页 */
#define OLED_AI_HOLD_TICKS     10

static rt_thread_t oled_thread = RT_NULL;

/* 节点1 上报帧里的多字节字段是小端 */
#define RD_U16(p) ((rt_uint32_t)(p)[0] | ((rt_uint32_t)(p)[1] << 8))

/* 0x103 帧的类别字段：与节点1 ai_infer.h 的 AI_CLASS_* 一致 */
static char *ai_class_text(rt_uint8_t cls)
{
    switch (cls)
    {
    case 0:  return "IDLE";
    case 1:  return "IMPACT";
    case 2:  return "PUSH";
    default: return "UNKNOWN";
    }
}

/* AI 结果页：数据取自最近一帧 0x103（节点1 在识别到撞击/推压时才上报）。
 * 还没收到过 0x103 时显示 "--"，避免屏幕出现看起来像真实结果的 0 值。 */
static void oled_show_ai_page(const can_receiver_stats_t *st)
{
    const rt_uint8_t *d = st->last_ai.data;

    /* 行1 (Y=0)：AI 类别 */
    OLED_ShowString(0, 0, "AI:", OLED_8X16);
    OLED_ShowString(24, 0, st->has_ai ? ai_class_text(d[1]) : "--", OLED_8X16);

    /* 行2 (Y=16)：置信度，帧里是 ×1000 的整数 */
    OLED_ShowString(0, 16, "CONF:", OLED_8X16);
    if (st->has_ai)
    {
        OLED_ShowFloatNum(40, 16, (double)RD_U16(d + 2) / 10.0, 2, 1, OLED_8X16);
        OLED_ShowString(72, 16, "%", OLED_8X16);
    }
    else
    {
        OLED_ShowString(40, 16, "--", OLED_8X16);
    }

    /* 行3 (Y=32)：触发峰值，帧里是 ×100 的整数 */
    OLED_ShowString(0, 32, "PEAK:", OLED_8X16);
    if (st->has_ai)
    {
        OLED_ShowFloatNum(40, 32, (double)RD_U16(d + 4) / 100.0, 2, 2, OLED_8X16);
        OLED_ShowString(72, 32, "G", OLED_8X16);
    }
    else
    {
        OLED_ShowString(40, 32, "--", OLED_8X16);
    }

    /* 行4 (Y=48)：事件序号 */
    OLED_ShowString(0, 48, "SEQ:", OLED_8X16);
    OLED_ShowNum(32, 48, st->has_ai ? RD_U16(d + 6) : 0, 5, OLED_8X16);
}

/* 待机页（平时显示）：环境温湿度 + 环境状态 + 结构状态。
 * 数据取"最近一帧" 0x456（环境）和 0x100（结构）；还没收到过就显示 --。
 * 注意 0x456 里的温度、湿度都是 ×10 的 int16（小端），0x100 里倾角 ×10、振动是 mg。 */
static void oled_show_idle_page(const can_receiver_stats_t *st)
{
    char line[28];

    OLED_ShowString(0, 0, "SHELF MONITOR", OLED_6X8);

    /* 环境温湿度 */
    if (st->has_env)
    {
        int t = (int)(int16_t)RD_U16(st->last_env.data);          /* 温度 ×10 */
        int h = (int)(int16_t)RD_U16(st->last_env.data + 2);      /* 湿度 ×10 */

        rt_snprintf(line, sizeof(line), "TEMP %d.%dC HUM %d.%d%%",
                    t / 10, (t < 0 ? -t : t) % 10,
                    h / 10, (h < 0 ? -h : h) % 10);
    }
    else
    {
        rt_snprintf(line, sizeof(line), "TEMP --.-C HUM --.-%%");
    }
    OLED_ShowString(0, 8, line, OLED_6X8);

    /* 环境状态：B4 报警位 bit0 温高 / bit1 温低 / bit2 湿高 / bit3 湿低 */
    if (st->has_env)
    {
        rt_uint8_t alarm = st->last_env.data[4];
        const char *txt = "NORMAL";

        if (alarm & 0x01)      txt = "TEMP HIGH";
        else if (alarm & 0x02) txt = "TEMP LOW";
        else if (alarm & 0x04) txt = "HUMI HIGH";
        else if (alarm & 0x08) txt = "HUMI LOW";

        rt_snprintf(line, sizeof(line), "ENV  %s", txt);
    }
    else
    {
        rt_snprintf(line, sizeof(line), "ENV  --");
    }
    OLED_ShowString(0, 16, line, OLED_6X8);

    /* 结构状态：倾角 ×10、振动 RMS(mg)、累计撞击次数 */
    if (st->has_struct)
    {
        const rt_uint8_t *d = st->last_struct.data;
        int tilt = (int)(int16_t)RD_U16(d + 2);

        rt_snprintf(line, sizeof(line), "TILT %d.%dD VIB %uMG",
                    tilt / 10, (tilt < 0 ? -tilt : tilt) % 10,
                    (unsigned)RD_U16(d + 4));
        OLED_ShowString(0, 24, line, OLED_6X8);

        rt_snprintf(line, sizeof(line), "IMPACTS %u", (unsigned)RD_U16(d + 6));
        OLED_ShowString(0, 32, line, OLED_6X8);
    }
    else
    {
        OLED_ShowString(0, 24, "TILT --.-D VIB --MG", OLED_6X8);
        OLED_ShowString(0, 32, "IMPACTS --", OLED_6X8);
    }
}

static void oled_display_thread_entry(void *param)
{
    can_receiver_stats_t st;

    /* AI 事件优先：收到新的 0x103（序号变化）就切到 AI 页停留 5 秒，平时显示待机页 */
    rt_uint32_t ai_hold_ticks = 0;
    rt_uint32_t last_seq = 0xFFFFFFFFu;

    RT_UNUSED(param);

    OLED_Init();

    while (1)
    {
        can_receiver_get_stats(&st);

        OLED_Clear();

        if (!st.dev_ready)
        {
            /* CAN 设备没打开：保留唯一一个异常提示，其余页面已移除 */
            OLED_ShowString(8, 24, "CAN OPEN FAIL", OLED_8X16);
        }
        else
        {
            /* 新的 AI 结果到了（0x103 的事件序号变了）→ 切 AI 页显示几秒 */
            if (st.has_ai)
            {
                rt_uint32_t seq = RD_U16(st.last_ai.data + 6);

                if (seq != last_seq)
                {
                    last_seq = seq;
                    ai_hold_ticks = OLED_AI_HOLD_TICKS;
                }
            }

            if (ai_hold_ticks > 0)
            {
                ai_hold_ticks--;
                oled_show_ai_page(&st);
            }
            else
            {
                oled_show_idle_page(&st);
            }
        }

        OLED_Update();
        rt_thread_mdelay(OLED_REFRESH_MS);
    }
}

int oled_display_start(void)
{
    if (oled_thread != RT_NULL)
    {
        LOG_W("OLED display thread already started");
        return 0;
    }

    oled_thread = rt_thread_create("oled", oled_display_thread_entry, RT_NULL,
                                   OLED_THREAD_STACK_SIZE, OLED_THREAD_PRIORITY, 10);
    if (oled_thread == RT_NULL)
    {
        LOG_E("Failed to create OLED display thread");
        return -1;
    }

    if (rt_thread_startup(oled_thread) != RT_EOK)
    {
        LOG_E("Failed to start OLED display thread");
        return -1;
    }

    LOG_I("OLED display thread started");
    return 0;
}
