/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-10-14     RT-Thread    从 HTTP 下载固件并写入"另一个区"（OTA 下载器）
 *
 * 流程：解析 URL → 解析目标区 → 先整区擦除 → TCP 连接（SAL socket + esp8266）
 *       → 发 HTTP GET → 解析响应头拿 Content-Length → 边收边写 →
 *       校验镜像向量 → 置 pending（重启后由 bootloader 切换）
 *
 * 包格式：镜像 + 末尾 4 字节 CRC32（小端，标准 zlib/IEEE 多项式）。
 *       下载写入完成后，按 flash 里的镜像内容重算 CRC 再和包尾比对
 *       （只算镜像部分，不含包尾自身）；不一致直接拒绝、不置 pending。
 *
 * 为什么必须"连接之前就把区擦完"：F4 擦一个 128KB 扇区要 1~2 秒，四个扇区
 * 4~5 秒，这期间 CPU 取指被 FLASH 挂住、串口中断都跑不了，UART FIFO 里的
 * 数据会静默丢掉（实测：先连接后擦除，194500 字节的包固定丢 ~48KB，
 * 正好等于擦除期间服务器推过来的量）。擦完再连接，下载期间每次编程只停顿
 * 几十 µs，串口完全跟得上。
 *
 * 注意：本文件的字符串字面量保持纯 ASCII —— ARMCC 默认按 GBK 解读源码，
 *       UTF-8 中文出现在字符串里会偶发"多字节吞引号"的编译错误；
 *       中文只放注释里（日志用英文，串口显示也更干净）。
 */

#include "ota_download.h"
#include "ota_flag.h"
#include "ota_flash.h"
#include "mqtt_client.h"

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <netdb.h>
#include <sys/socket.h>

#define DBG_TAG "ota_dl"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define DL_BUF_SIZE         4096U
#define DL_RECV_TIMEOUT_S   15
#define DL_HDR_MAX          2048U

/* 默认 OTA 地址（巴法云 mainota 条目）；ota_fetch 不带参数时用它。
 * 注：msh 命令行缓冲有限（FINSH_CMD_SIZE），这么长的 URL 直接贴在命令行里
 * 很容易超长 —— 走"无参数默认地址"最省事，要换地址就改这里 */
#define OTA_DEFAULT_URL "http://bin.bemfa.com/b/399739/1BcZTM3MWRmYTM5ODMxNGFmZGJiMzllZTFkYmZkODViMDY=mainota.bin"

/* 接收 + 字对齐中转区（用 uint32 数组保证 4 字节对齐） */
static rt_uint32_t dl_stage[(DL_BUF_SIZE + 8) / 4];

/* 在缓冲区里找 "\r\n\r\n"，返回其"之后"的偏移（没有则 -1） */
static int dl_find_hdr_end(const char *buf, rt_uint32_t len)
{
    rt_uint32_t i;

    for (i = 0; i + 3U < len; i++)
    {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n')
        {
            return (int)(i + 4U);
        }
    }
    return -1;
}

/* CRC32（标准 zlib/IEEE：多项式 0xEDB88320，初值全 1，末尾取反） */
static rt_uint32_t dl_crc32_update(rt_uint32_t crc, const rt_uint8_t *data, rt_uint32_t len)
{
    rt_uint32_t i, j;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
        }
    }
    return crc;
}

/* 解析 http://host[:port]/path */
static int dl_parse_url(const char *url, char *host, rt_size_t host_sz,
                        int *port, char *path, rt_size_t path_sz)
{
    const char *p;
    const char *slash;
    const char *colon;
    rt_size_t host_len;

    if (strncmp(url, "http://", 7) != 0)
    {
        LOG_E("only http:// urls are supported");
        return -1;
    }

    p = url + 7;
    slash = strchr(p, '/');
    if (slash == RT_NULL)
    {
        LOG_E("no path part in url");
        return -1;
    }

    colon = (const char *)memchr(p, ':', slash - p);
    if (colon != RT_NULL)
    {
        host_len = (rt_size_t)(colon - p);
        *port = atoi(colon + 1);
    }
    else
    {
        host_len = (rt_size_t)(slash - p);
        *port = 80;
    }

    if (host_len == 0 || host_len >= host_sz)
    {
        LOG_E("bad host length");
        return -1;
    }

    memcpy(host, p, host_len);
    host[host_len] = '\0';

    if (strlen(slash) >= path_sz)
    {
        LOG_E("path too long");
        return -1;
    }
    strcpy(path, slash);
    return 0;
}

int ota_download_and_stage(const char *url)
{
    char host[64];
    char path[256];
    char port_str[8];
    char req[384];
    int port = 80;
    int fd = -1;
    int ret = -1;
    int hdr_end;
    struct addrinfo hint;
    struct addrinfo *res = RT_NULL;
    struct timeval tv;
    char *hdr = RT_NULL;
    rt_uint32_t me, target, base, limit;
    rt_uint32_t hdr_len, content_len = 0, image_len;
    rt_uint32_t written = 0, staged = 0, last_log = 0;
    rt_uint32_t crc;

    LOG_I("start download: %s", url);

    /* 下载要独占网络通道：MQTT 和下载器都会抢 esp8266 的 AT 串口，
     * 尤其在 MQTT 疯狂重连时双方互相卡死（实测：CIPDOMAIN 互相超时）。
     * 注意：停掉后不自动恢复 —— 下载失败时重启程序即可恢复 MQTT */
    if (mqtt_client_stop() == 0)
    {
        LOG_I("MQTT stopped for OTA (network busy)");
    }

    if (dl_parse_url(url, host, sizeof(host), &port, path, sizeof(path)) != 0)
    {
        return -1;
    }

    LOG_I("target: %s:%d%s", host, port, path);

    /* ---- 目标区：另一个运行区（谁跑着就写另一个） ---- */
    me = ota_flag_my_slot();
    target = (me == OTA_SLOT_B) ? OTA_SLOT_A : OTA_SLOT_B;
    base = ota_flash_slot_base(target);
    limit = ota_flash_slot_size(target);

    rt_snprintf(port_str, sizeof(port_str), "%d", port);

    rt_memset(&hint, 0, sizeof(hint));
    if (getaddrinfo(host, port_str, &hint, &res) != 0)
    {
        LOG_E("DNS resolve failed: %s", host);
        return -1;
    }

    /* ---- 先整区擦除，再去连接（顺序是关键，见文件头说明） ---- */
    LOG_I("running in slot %s -> writing slot %s (0x%08X)",
          me == OTA_SLOT_B ? "B" : "A",
          target == OTA_SLOT_B ? "B" : "A", (unsigned)base);

    LOG_I("erasing target slot (a few seconds, system stalls) ...");
    if (ota_flash_erase_slot(target) != 0)
    {
        LOG_E("erase failed");
        freeaddrinfo(res);
        return -1;
    }
    LOG_I("erase done, connecting ...");

    fd = socket(res->ai_family, SOCK_STREAM, 0);
    if (fd < 0)
    {
        LOG_E("socket create failed");
        freeaddrinfo(res);
        return -1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0)
    {
        LOG_E("connect failed (check network: ifconfig)");
        closesocket(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    res = RT_NULL;

    tv.tv_sec = DL_RECV_TIMEOUT_S;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void *)&tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (void *)&tv, sizeof(tv));

    rt_snprintf(req, sizeof(req),
                "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: stm32-ota\r\nConnection: close\r\n\r\n",
                path, host);
    if (send(fd, req, rt_strlen(req), 0) != (int)rt_strlen(req))
    {
        LOG_E("send request failed");
        goto _out;
    }

    /* ---- 读响应头 ---- */
    hdr = rt_malloc(DL_HDR_MAX + 1);
    if (hdr == RT_NULL)
    {
        LOG_E("out of memory");
        goto _out;
    }

    hdr_len = 0;
    hdr_end = -1;
    while (hdr_end < 0)
    {
        int n = recv(fd, hdr + hdr_len, (int)(DL_HDR_MAX - hdr_len), 0);

        if (n <= 0)
        {
            LOG_E("read response failed (%d)", n);
            goto _out;
        }
        hdr_len += (rt_uint32_t)n;
        hdr_end = dl_find_hdr_end(hdr, hdr_len);

        if (hdr_end < 0 && hdr_len >= DL_HDR_MAX)
        {
            LOG_E("response header too large");
            goto _out;
        }
    }
    hdr[hdr_len] = '\0';

    /* 状态码必须是 200 */
    {
        char *sp = strchr(hdr, ' ');
        int code = (sp != RT_NULL) ? atoi(sp + 1) : 0;

        if (code != 200)
        {
            LOG_E("HTTP status %d (only 200 supported)", code);
            goto _out;
        }
    }

    /* Content-Length（没有就按"读到连接关闭"处理） */
    {
        char *p = strstr(hdr, "Content-Length:");

        if (p == RT_NULL)
        {
            p = strstr(hdr, "content-length:");
        }

        content_len = (p != RT_NULL) ? (rt_uint32_t)atoi(p + 15) : 0;
    }

    LOG_I("HTTP 200, firmware length %u bytes", (unsigned)content_len);

    if (content_len != 0 && (content_len < 1028U || content_len > limit))
    {
        LOG_E("firmware size %u exceeds target slot (max %u)",
              (unsigned)content_len, (unsigned)limit);
        goto _out;
    }

    /* body 开头可能和响应头在同一个 TCP 段里，先接进中转区 */
    staged = hdr_len - (rt_uint32_t)hdr_end;
    if (staged > 0)
    {
        rt_memcpy(dl_stage, hdr + hdr_end, staged);
    }
    rt_free(hdr);
    hdr = RT_NULL;

    LOG_I("downloading ...");

    /* ---- 边收边写 ---- */
    while (content_len == 0 || (written + staged) < content_len)
    {
        rt_uint32_t want = DL_BUF_SIZE;
        int n;

        if (content_len != 0)
        {
            rt_uint32_t left = content_len - (written + staged);

            if (left < want)
            {
                want = left;
            }
        }

        n = recv(fd, (rt_uint8_t *)dl_stage + staged, (int)want, 0);
        if (n < 0)
        {
            LOG_E("recv failed (%d)", n);
            goto _out;
        }
        if (n == 0)
        {
            break;                          /* 服务器关闭连接 */
        }

        staged += (rt_uint32_t)n;

        {
            rt_uint32_t words = (staged / 4U) * 4U;

            if (words > 0)
            {
                if (ota_flash_program(base + written, dl_stage, words) != 0)
                {
                    LOG_E("flash write failed (offset %u)", (unsigned)written);
                    goto _out;
                }

                written += words;

                if (staged > words)
                {
                    rt_memmove((rt_uint8_t *)dl_stage,
                               (rt_uint8_t *)dl_stage + words, staged - words);
                }
                staged -= words;
            }
        }

        if (written - last_log >= 32768U)
        {
            last_log = written;

            if (content_len != 0)
            {
                LOG_I("written %u / %u bytes (%u%%)", (unsigned)written,
                      (unsigned)content_len, (unsigned)((written * 100U) / content_len));
            }
            else
            {
                LOG_I("written %u bytes", (unsigned)written);
            }
        }
    }

    if (content_len != 0 && (written + staged) != content_len)
    {
        LOG_E("download incomplete: %u / %u bytes",
              (unsigned)(written + staged), (unsigned)content_len);
        goto _out;
    }

    if (content_len == 0)
    {
        content_len = written + staged;     /* 未知长度的模式：收到多少算多少 */
    }

    /* 尾部不足 4 字节：补 0xFF 凑一个字写进去 */
    if (staged != 0)
    {
        rt_uint8_t *tail = (rt_uint8_t *)dl_stage;

        while (staged < 4U)
        {
            tail[staged++] = 0xFF;
        }

        if (ota_flash_program(base + written, dl_stage, 4) != 0)
        {
            LOG_E("write tail failed");
            goto _out;
        }
        written += 4;
        staged = 0;
    }

    LOG_I("download done, verifying image ...");

    /* ---- 校验 1：CRC32 ----
     * 按 flash 里写好的前 image_len 字节重算 CRC（顺便验证写入结果），
     * 再和包末尾 4 字节（小端）比对；不一致 = 坏包，直接拒绝。
     * 注意：只算镜像部分，包尾 4 字节自身不参与计算。 */
    if (content_len < 4U)
    {
        LOG_E("package too short");
        goto _out;
    }

    image_len = content_len - 4U;

    crc = 0xFFFFFFFFUL;
    {
        rt_uint32_t off;

        for (off = 0; off < image_len; off += 4096U)
        {
            rt_uint32_t chunk = image_len - off;

            if (chunk > 4096U)
            {
                chunk = 4096U;
            }
            crc = dl_crc32_update(crc, (const rt_uint8_t *)(base + off), chunk);
        }
    }
    crc ^= 0xFFFFFFFFUL;

    {
        volatile rt_uint8_t *rb = (volatile rt_uint8_t *)(base + image_len);
        rt_uint32_t expected = (rt_uint32_t)rb[0] | ((rt_uint32_t)rb[1] << 8) |
                               ((rt_uint32_t)rb[2] << 16) | ((rt_uint32_t)rb[3] << 24);

        if (crc != expected)
        {
            LOG_E("CRC mismatch: calc 0x%08X, pkg 0x%08X (upload the *_ota.bin package?)",
                  (unsigned)crc, (unsigned)expected);
            goto _out;
        }
        LOG_I("CRC check OK: 0x%08X (image %u bytes)", (unsigned)crc, (unsigned)image_len);
    }

    /* ---- 校验镜像：向量表必须落在目标区里（顺带挡住"版本混用"） ---- */
    {
        rt_uint32_t sp = *(volatile rt_uint32_t *)base;
        rt_uint32_t pc = *(volatile rt_uint32_t *)(base + 4);

        if (sp < 0x20000000UL || sp > 0x20020000UL)
        {
            LOG_E("image check failed: bad SP=0x%08X", (unsigned)sp);
            goto _out;
        }

        if ((pc & 1UL) == 0 || pc < base || pc >= (base + limit))
        {
            LOG_E("image check failed: reset vector 0x%08X not in slot [0x%08X, 0x%08X)",
                  (unsigned)pc, (unsigned)base, (unsigned)(base + limit));
            goto _out;
        }

        LOG_I("image check OK: SP=0x%08X PC=0x%08X", (unsigned)sp, (unsigned)pc);
    }

    /* ---- 置 pending：重启后由 bootloader 切换 ---- */
    if (ota_flag_set_pending(target) != 0)
    {
        LOG_E("set pending failed");
        goto _out;
    }

    LOG_I("OTA staged: %u bytes -> slot %s, pending set; reset to switch",
          (unsigned)content_len, target == OTA_SLOT_B ? "B" : "A");
    ret = 0;

_out:
    if (hdr != RT_NULL)
    {
        rt_free(hdr);
    }
    if (fd >= 0)
    {
        closesocket(fd);
    }
    return ret;
}

#ifdef FINSH_USING_MSH
static char dl_url[300];
static volatile rt_bool_t dl_busy = RT_FALSE;

static void dl_thread_entry(void *param)
{
    (void)param;

    if (ota_download_and_stage(dl_url) != 0)
    {
        LOG_E("download failed (MQTT paused; reboot to restore)");
    }
    dl_busy = RT_FALSE;
}

/**
 * ota_fetch —— 从 HTTP 地址下载固件并写入"另一个区"
 * 用法：ota_fetch [url]   不带参数 = 用 OTA_DEFAULT_URL（巴法云 mainota）
 */
static int ota_fetch(int argc, char **argv)
{
    rt_thread_t tid;
    const char *url;

    if (argc == 1)
    {
        url = OTA_DEFAULT_URL;              /* 无参数：用默认地址 */
    }
    else if (argc == 2)
    {
        url = argv[1];
    }
    else
    {
        rt_kprintf("usage: ota_fetch [url]   (no arg = default bemfa url)\n");
        return -1;
    }

    if (dl_busy)
    {
        rt_kprintf("ota_fetch: download already running\n");
        return -1;
    }

    if (rt_strlen(url) >= sizeof(dl_url))
    {
        rt_kprintf("ota_fetch: URL too long (max %u)\n", (unsigned)(sizeof(dl_url) - 1));
        return -1;
    }

    rt_strncpy(dl_url, url, sizeof(dl_url) - 1);
    dl_url[sizeof(dl_url) - 1] = '\0';
    dl_busy = RT_TRUE;

    tid = rt_thread_create("ota_dl", dl_thread_entry, RT_NULL, 4096, 20, 10);
    if (tid == RT_NULL)
    {
        dl_busy = RT_FALSE;
        rt_kprintf("ota_fetch: create thread failed\n");
        return -1;
    }

    rt_thread_startup(tid);
    rt_kprintf("ota_fetch: started, watch the log...\n");
    return 0;
}
MSH_CMD_EXPORT(ota_fetch, download firmware from URL to the other slot);
#endif /* FINSH_USING_MSH */
