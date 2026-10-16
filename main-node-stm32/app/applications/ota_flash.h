/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-14     RT-Thread    OTA 片上 FLASH 底层读写（标志区 / A、B 两个运行区共用）
 */

#ifndef __OTA_FLASH_H__
#define __OTA_FLASH_H__

#include <rtthread.h>

/* 两个运行区（和 bootloader 的分区布局保持一致） */
#define OTA_SLOT_BASE_A     0x08010000UL
#define OTA_SLOT_BASE_B     0x08080000UL
#define OTA_SLOT_SIZE_A     0x70000UL       /* 448KB（扇区 4~7） */
#define OTA_SLOT_SIZE_B     0x80000UL       /* 512KB（扇区 8~11） */

/* slot 取 ota_flag.h 里的 OTA_SLOT_A / OTA_SLOT_B */
rt_uint32_t ota_flash_slot_base(rt_uint32_t slot);
rt_uint32_t ota_flash_slot_size(rt_uint32_t slot);

/* 擦除一个扇区（0~11）。注意：擦除期间 CPU 取指会被 FLASH 挂住 */
int ota_flash_erase_sector(rt_uint32_t sector);

/* 擦除整个运行区（A: 扇区 4~7，B: 扇区 8~11） */
int ota_flash_erase_slot(rt_uint32_t slot);

/* 按字编程（addr / len 必须 4 字节对齐）。返回 0 = 成功 */
int ota_flash_program(rt_uint32_t addr, const void *data, rt_uint32_t len);

#endif /* __OTA_FLASH_H__ */
