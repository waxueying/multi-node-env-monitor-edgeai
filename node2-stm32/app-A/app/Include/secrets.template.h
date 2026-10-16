/* node2 本地配置模板 —— 复制成 secrets.h 并填自己的值。
 * secrets.h 已被 .gitignore 忽略，不会进仓库。
 */
#ifndef __SECRETS_H__
#define __SECRETS_H__

/* WiFi */
#define OTA_WIFI_SSID      "your-wifi-ssid"
#define OTA_WIFI_PASSWORD  "your-wifi-password"

/* 巴法云：私钥 + OTA 主题 */
#define OTA_BEMFA_KEY      "your-bemfa-private-key"
#define OTA_BEMFA_TOPIC    "your-ota-topic"

#endif
