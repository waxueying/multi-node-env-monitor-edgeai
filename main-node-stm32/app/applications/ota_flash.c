/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-14     RT-Thread    OTA 片上 FLASH 底层读写（标志区 / A、B 两个运行区共用）
 *
 * 实现说明（都是踩过的坑，写在这里备忘）：
 *   1) STM32F4 的 FLASH 控制器在 0x40023C00；0xA0000000 是 FSMC 的寄存器。
 *      地址写错的后果不是报错，而是"读不到 BSY/错误位"、擦写全部静默假成功。
 *   2) 系统复位不清 FLASH_SR：Keil 烧录、上次失败都可能留下错误标志，
 *      不清掉就会被误判成本次操作失败 —— 所以每次操作前先清一遍。
 *   3) PSIZE 复位默认 x8，和 32 位字写入不匹配会报错 —— 统一设成 x32（3.3V）。
 */

#include "ota_flash.h"
#include "ota_flag.h"

#define DBG_TAG "ota_flash"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* ---- FLASH 寄存器（STM32F4：FLASH 控制器基址 0x40023C00） ---- */
#define FLASH_KEYR      (*(volatile rt_uint32_t *)0x40023C04UL)
#define FLASH_SR        (*(volatile rt_uint32_t *)0x40023C0CUL)
#define FLASH_CR        (*(volatile rt_uint32_t *)0x40023C10UL)

#define FLASH_KEY1      0x45670123UL
#define FLASH_KEY2      0xCDEF89ABUL

#define FLASH_SR_EOP    0x00000001UL        /* 操作完成标志 */
#define FLASH_SR_ERR    0x000000F2UL        /* OPERR|WRPERR|PGAERR|PGPERR|PGSERR */
#define FLASH_SR_BSY    0x00010000UL        /* 忙（bit16） */

#define FLASH_CR_PG     0x00000001UL        /* 编程 */
#define FLASH_CR_SER    0x00000002UL        /* 扇区擦除 */
#define FLASH_CR_MER    0x00000004UL        /* 全片擦除（清理 CR 时一并清掉） */
#define FLASH_CR_SNB_POS 3                  /* SNB[6:3] */
#define FLASH_CR_STRT   0x00010000UL        /* 开始擦除 */
#define FLASH_CR_LOCK   0x80000000UL        /* 上锁 */
#define FLASH_CR_PSIZE      0x00000300UL    /* 编程并行度位（bit9:8） */
#define FLASH_CR_PSIZE_X32  0x00000200UL    /* x32：对应 VDD 2.7~3.6V */

#define FLASH_WAIT_LOOP 20000000UL          /* 等待上限（防死等；正常擦除几百 ms） */

static rt_uint32_t flash_sr_entry;          /* 进门时（清理前）的 SR 快照 */
static rt_uint32_t flash_last_sr;           /* 一次操作做完后的 SR 快照 */

/* 等 Flash 操作完成；返回 0 = 成功，-1 = 出错/超时（错误标志已清） */
static int flash_wait(void)
{
    rt_uint32_t i;

    for (i = 0; (FLASH_SR & FLASH_SR_BSY) && (i < FLASH_WAIT_LOOP); i++)
    {
    }

    flash_last_sr = FLASH_SR;

    if (flash_last_sr & (FLASH_SR_ERR | FLASH_SR_BSY))
    {
        FLASH_SR = FLASH_SR_ERR;            /* 写 1 清错误标志 */
        return -1;
    }
    return 0;
}

/* 解锁 + 把控制器归到"干净可用"状态（每次操作前都做一遍，见文件头说明） */
static void flash_begin(void)
{
    rt_uint32_t i;

    if (FLASH_CR & FLASH_CR_LOCK)
    {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }

    flash_sr_entry = FLASH_SR;

    for (i = 0; (FLASH_SR & FLASH_SR_BSY) && (i < FLASH_WAIT_LOOP); i++)
    {
    }

    FLASH_SR = FLASH_SR_ERR | FLASH_SR_EOP;     /* 写 1 清残留标志 */

    FLASH_CR &= ~(FLASH_CR_PG | FLASH_CR_SER | FLASH_CR_MER | FLASH_CR_PSIZE |
                  (0xFUL << FLASH_CR_SNB_POS));
    FLASH_CR |= FLASH_CR_PSIZE_X32;
}

static void flash_end(void)
{
    FLASH_CR |= FLASH_CR_LOCK;
}

int ota_flash_erase_sector(rt_uint32_t sector)
{
    int ret;

    if (sector > 11)
    {
        return -1;
    }

    flash_begin();

    FLASH_CR &= ~(0xFUL << FLASH_CR_SNB_POS);
    FLASH_CR |= FLASH_CR_SER | (sector << FLASH_CR_SNB_POS);
    FLASH_CR |= FLASH_CR_STRT;

    ret = flash_wait();

    FLASH_CR &= ~FLASH_CR_SER;
    flash_end();

    if (ret != 0)
    {
        LOG_E("erase sector %u failed (SR in=0x%08X out=0x%08X CR=0x%08X)",
              (unsigned)sector, (unsigned)flash_sr_entry,
              (unsigned)flash_last_sr, (unsigned)FLASH_CR);
    }
    return ret;
}

int ota_flash_program(rt_uint32_t addr, const void *data, rt_uint32_t len)
{
    const rt_uint32_t *src = (const rt_uint32_t *)data;
    rt_uint32_t i, words;
    int ret = 0;

    if ((addr & 3UL) || (len & 3UL) || (len == 0))
    {
        LOG_E("program align error: addr=0x%08X len=%u", (unsigned)addr, (unsigned)len);
        return -1;
    }

    words = len / 4;

    flash_begin();

    for (i = 0; i < words; i++)
    {
        if (flash_wait() != 0)
        {
            ret = -1;
            break;
        }

        FLASH_CR |= FLASH_CR_PG;
        *(volatile rt_uint32_t *)(addr + i * 4UL) = src[i];

        if (flash_wait() != 0)
        {
            ret = -1;
        }
        FLASH_CR &= ~FLASH_CR_PG;

        if (ret != 0)
        {
            break;
        }
    }

    flash_end();

    if (ret != 0)
    {
        LOG_E("program failed at 0x%08X (SR in=0x%08X out=0x%08X CR=0x%08X)",
              (unsigned)(addr + i * 4UL), (unsigned)flash_sr_entry,
              (unsigned)flash_last_sr, (unsigned)FLASH_CR);
    }
    return ret;
}

rt_uint32_t ota_flash_slot_base(rt_uint32_t slot)
{
    return (slot == OTA_SLOT_B) ? OTA_SLOT_BASE_B : OTA_SLOT_BASE_A;
}

rt_uint32_t ota_flash_slot_size(rt_uint32_t slot)
{
    return (slot == OTA_SLOT_B) ? OTA_SLOT_SIZE_B : OTA_SLOT_SIZE_A;
}

int ota_flash_erase_slot(rt_uint32_t slot)
{
    rt_uint32_t first, i;

    if (slot == OTA_SLOT_B)
    {
        first = 8;      /* B 区 = 扇区 8~11（每扇区 128KB） */
    }
    else
    {
        first = 4;      /* A 区 = 扇区 4~7（64K + 128K×3） */
    }

    for (i = 0; i < 4; i++)
    {
        if (ota_flash_erase_sector(first + i) != 0)
        {
            return -1;
        }
        LOG_I("sector %u erased", (unsigned)(first + i));
    }
    return 0;
}
