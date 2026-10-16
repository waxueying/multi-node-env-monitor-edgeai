/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-09-27     RT-Thread    OLED 状态显示（CAN 接收监视）
 */

#ifndef __OLED_DISPLAY_H__
#define __OLED_DISPLAY_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 启动 OLED 显示线程（软件 I2C：SCL=PB8, SDA=PB9）
 * @return 0 成功，其他值失败
 */
int oled_display_start(void);

#ifdef __cplusplus
}
#endif

#endif /* __OLED_DISPLAY_H__ */
