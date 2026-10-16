#pragma once

/* 本地私有配置模板 —— 复制成 secrets.h 并填自己的值。
 * secrets.h 已被 .gitignore 忽略，不会进仓库。
 */

/* WiFi（ESP32-S3 只支持 2.4GHz，不要填 5G 的 SSID） */
#define SECRET_WIFI_SSID      "your-wifi-ssid"
#define SECRET_WIFI_PASSWORD  "your-wifi-password"

/* 巴法云私钥：谁拿到都能往你的 OTA 主题传固件，不要外传 */
#define SECRET_BEMFA_OPEN_ID  "your-bemfa-private-key"
