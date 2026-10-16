#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* ============================================================================
 * 节点1 的 WiFi + 巴法云 OTA 模块
 *
 * 完整流程：连 WiFi → 查云端版本 → 有新版就下载并写入另一个分区（A/B 轮流）
 *           → 设置启动分区 → 重启 → 新固件启动 5 秒后确认，没确认就自动回滚。
 *
 * 版本比较用 NVS 里记录的"已安装版本"，不用编译期宏 —— 详见 wifi_ota.c 的说明。
 *
 * 用法（在 app_main 里起一个独立任务）：
 *     xTaskCreate(wifi_ota_task, "wifi_ota", 8192, NULL, 4, NULL);
 * 优先级 4 低于采样任务(6) 和 CAN 接收(5)：网络慢不会影响 100Hz 采样。
 * ========================================================================== */

/* 从巴法云查到的固件信息 */
typedef struct
{
    char url[256];      /* 固件下载地址 */
    int  version;       /* 云端固件版本号 */
    int  size;          /* 固件字节数 */
} ota_cloud_info_t;

/* 连接 WiFi（阻塞直到拿到 IP 或超时） */
esp_err_t wifi_ota_wifi_connect(void);

/* 查询云端最新固件并和本地版本比较；拿到数据返回 ESP_OK，
 * 有新版本时把 *has_new 置 true */
esp_err_t wifi_ota_check_version(ota_cloud_info_t *info, bool *has_new);

/* 独立任务：连 WiFi → 每 60 秒查一次版本 */
void wifi_ota_task(void *arg);

/* 开机自检通过后调用：确认本次启动的固件是好的，解除回滚保护。
 * 只有"刚升级完、待验证"的镜像才真正生效；USB 烧录的镜像会直接跳过。 */
void wifi_ota_confirm_boot(void);

