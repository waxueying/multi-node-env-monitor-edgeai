/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-14     RT-Thread    从 HTTP 下载固件并写入"另一个区"（OTA 下载器）
 */

#ifndef __OTA_DOWNLOAD_H__
#define __OTA_DOWNLOAD_H__

#include <rtthread.h>

/* 下载 url 指向的固件，写入"另一个区"（当前跑 A 就写 B），
 * 校验通过后置 pending 标志。返回 0 = 成功（重启后生效） */
int ota_download_and_stage(const char *url);

#endif /* __OTA_DOWNLOAD_H__ */
