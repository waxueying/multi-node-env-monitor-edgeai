/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-12     RT-Thread    OTA 启动标志区读写（与 bootloader 共用同一布局）
 */

#ifndef __OTA_FLAG_H__
#define __OTA_FLAG_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 标志区记录（放在 Sector 3 起始 0x0800C000）。
 * 字段布局必须和 bootloader/src/main.c 里的 boot_flag_t 保持一致 */
typedef struct
{
    rt_uint32_t magic;      /* OTA_FLAG_MAGIC 才认为记录有效 */
    rt_uint32_t active;     /* 当前正式版本所在的区：OTA_SLOT_A / OTA_SLOT_B */
    rt_uint32_t pending;    /* 待验证的目标区：0 = 没有 */
    rt_uint32_t check;      /* 前三个字段异或校验 */
} ota_flag_t;

#define OTA_FLAG_ADDR   0x0800C000UL
#define OTA_FLAG_MAGIC  0x5A5AA5A5UL

#define OTA_SLOT_A      0x41UL      /* 'A' */
#define OTA_SLOT_B      0x42UL      /* 'B' */

/* 当前运行在哪个区（读 SCB->VTOR，bootloader 跳转前会设好）：OTA_SLOT_A / OTA_SLOT_B */
rt_uint32_t ota_flag_my_slot(void);

/* 读标志：1 = 记录有效；0 = 无记录 / 校验不过 */
int ota_flag_read(ota_flag_t *f);

/* 擦除并写入标志（内部擦 Sector 3，约 0.25s，期间 CPU 取指会被 Flash 忙挂住） */
int ota_flag_write(rt_uint32_t active, rt_uint32_t pending);

/* 启动自确认：active = 本区、pending = 0。
 * 已经是目标状态时直接返回，不碰 Flash（正常启动零磨损） */
int ota_flag_confirm(void);

/* 设置"下次从 slot 启动"（升级流程写完 B 区后调用，随后复位交给 bootloader） */
int ota_flag_set_pending(rt_uint32_t slot);

#ifdef __cplusplus
}
#endif

#endif /* __OTA_FLAG_H__ */
