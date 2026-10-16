/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-12     RT-Thread    OTA 启动标志区读写（与 bootloader 共用同一布局）
 * 2026-10-14     RT-Thread    FLASH 底层抽到 ota_flash.c，本文件只负责标志逻辑
 *
 * 实现说明：
 *   标志区 = Sector 3（0x0800C000，16KB）。app 本身运行在 Sector 4~7，
 *   擦 S3 不会伤到自己。FLASH 读写走 ota_flash 的公共实现。
 */

#include "ota_flag.h"
#include "ota_flash.h"
#include <rthw.h>

#define DBG_TAG "ota_flag"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define FLAG_SECTOR     3                   /* 标志区 = Sector 3（0x0800C000） */

rt_uint32_t ota_flag_my_slot(void)
{
    return (*(volatile rt_uint32_t *)0xE000ED08UL == 0x08080000UL) ? OTA_SLOT_B : OTA_SLOT_A;
}

int ota_flag_read(ota_flag_t *f)
{
    const ota_flag_t *p = (const ota_flag_t *)OTA_FLAG_ADDR;

    *f = *p;

    if (f->magic != OTA_FLAG_MAGIC)
    {
        return 0;
    }
    return (f->check == (f->magic ^ f->active ^ f->pending)) ? 1 : 0;
}

int ota_flag_write(rt_uint32_t active, rt_uint32_t pending)
{
    ota_flag_t f;

    f.magic   = OTA_FLAG_MAGIC;
    f.active  = active;
    f.pending = pending;
    f.check   = f.magic ^ f.active ^ f.pending;

    if (ota_flash_erase_sector(FLAG_SECTOR) != 0)
    {
        return -1;                          /* 具体错误 ota_flash 里已经打过日志 */
    }
    if (ota_flash_program(OTA_FLAG_ADDR, &f, sizeof(f)) != 0)
    {
        return -1;
    }

    LOG_I("flag written: active=%c pending=%c",
          active == OTA_SLOT_B ? 'B' : 'A',
          pending == 0 ? '-' : (pending == OTA_SLOT_B ? 'B' : 'A'));
    return 0;
}

int ota_flag_confirm(void)
{
    ota_flag_t f;
    rt_uint32_t me = ota_flag_my_slot();

    if (ota_flag_read(&f) == 1 && f.active == me && f.pending == 0)
    {
        return 0;                           /* 无需变更，别碰 Flash */
    }

    return ota_flag_write(me, 0);
}

int ota_flag_set_pending(rt_uint32_t slot)
{
    ota_flag_t f;
    rt_uint32_t active = (ota_flag_read(&f) == 1) ? f.active : ota_flag_my_slot();

    return ota_flag_write(active, slot);
}

#ifdef FINSH_USING_MSH
/**
 * bootflag —— 查看 / 模拟切区
 *   bootflag            查看标志 + 当前运行区
 *   bootflag set a|b    写 pending 并复位（模拟"升级完成，请切区"）
 *   bootflag clear      放弃切换：active=本区、pending 清零
 */
static int bootflag(int argc, char **argv)
{
    ota_flag_t f;
    rt_uint32_t me = ota_flag_my_slot();

    if (argc >= 3 && rt_strcmp(argv[1], "set") == 0)
    {
        rt_uint32_t slot;

        if (argv[2][0] == 'a' || argv[2][0] == 'A')
        {
            slot = OTA_SLOT_A;
        }
        else if (argv[2][0] == 'b' || argv[2][0] == 'B')
        {
            slot = OTA_SLOT_B;
        }
        else
        {
            rt_kprintf("bootflag set <a|b>\n");
            return -1;
        }

        if (ota_flag_set_pending(slot) != 0)
        {
            rt_kprintf("bootflag: write failed\n");
            return -1;
        }

        rt_kprintf("bootflag: pending -> %c, rebooting...\n", slot == OTA_SLOT_B ? 'B' : 'A');
        rt_thread_mdelay(100);              /* 让串口把上一行发完再复位 */
        rt_hw_cpu_reset();
        return 0;
    }

    if (argc >= 2 && rt_strcmp(argv[1], "clear") == 0)
    {
        if (ota_flag_write(me, 0) != 0)
        {
            rt_kprintf("bootflag: write failed\n");
            return -1;
        }
        rt_kprintf("bootflag: pending cleared (active=%c)\n", me == OTA_SLOT_B ? 'B' : 'A');
        return 0;
    }

    rt_kprintf("running in APP-%c (VTOR=0x%08X)\n",
               me == OTA_SLOT_B ? 'B' : 'A',
               (unsigned)(*(volatile rt_uint32_t *)0xE000ED08UL));

    if (ota_flag_read(&f) == 1)
    {
        rt_kprintf("flag: active=%c  pending=%c\n",
                   f.active == OTA_SLOT_B ? 'B' : 'A',
                   f.pending == 0 ? '-' : (f.pending == OTA_SLOT_B ? 'B' : 'A'));
    }
    else
    {
        rt_kprintf("flag: blank/invalid (bootloader defaults to APP-A)\n");
    }

    rt_kprintf("usage: bootflag | bootflag set a|b | bootflag clear\n");
    return 0;
}
MSH_CMD_EXPORT(bootflag, show or set the boot slot flag);
#endif /* FINSH_USING_MSH */
