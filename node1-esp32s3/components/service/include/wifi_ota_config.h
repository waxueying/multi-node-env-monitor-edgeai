#pragma once

#include "secrets.h"

/* ============================================================================
 * 节点1 OTA 配置：WiFi + 巴法云
 *
 * 换 WiFi、换服务器、换主题，只改这一个文件。
 *
 * 安全提醒：BEMFA_OPEN_ID 是账号私钥，谁拿到都能往你的 OTA 里传固件。
 *           若本工程要推到公开仓库，把本文件加进 .gitignore。
 * ========================================================================== */

/* ---- WiFi（ESP32-S3 只支持 2.4GHz，不要填 5G 的 SSID） ---- */
#define WIFI_SSID               SECRET_WIFI_SSID
#define WIFI_PASSWORD           SECRET_WIFI_PASSWORD

/* ---- 巴法云 OTA ---- */
#define BEMFA_OPEN_ID           SECRET_BEMFA_OPEN_ID
#define BEMFA_TOPIC             "shelf1ota"
#define BEMFA_DEVICE_TYPE       1       /* 1=MQTT设备 3=TCP设备，要和主题类型一致 */

/* ---- 本地固件版本 ---- */
/* 每发一版固件把它 +1，设备拿它和云端 version 比大小，决定要不要升级 */
#define NODE1_FW_VERSION        1

/* ---- 时间参数 ---- */
#define WIFI_CONNECT_TIMEOUT_MS 15000   /* 等 WiFi 拿到 IP 的超时时间 */
#define OTA_CHECK_INTERVAL_MS   60000   /* 版本查询周期：60 秒 */
#define OTA_VERIFY_DELAY_MS     5000    /* 新固件启动后等 5 秒再确认"我好了"；这期间崩了就会回滚 */

