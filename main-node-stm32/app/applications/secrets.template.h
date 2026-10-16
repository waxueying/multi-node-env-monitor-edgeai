/* 本地私有配置模板 —— 复制成 secrets.h 并填自己的值。
 * secrets.h 已被 .gitignore 忽略，不会进仓库。
 */
#ifndef __SECRETS_H__
#define __SECRETS_H__

/* WiFi（主节点用 ESP8266 AT 模块联网，由 at_device 的 esp8266 sample 使用） */
#define WIFI_SSID      "your-wifi-ssid"
#define WIFI_PASSWORD  "your-wifi-password"

/* MQTT broker（公网测试服务器或自己的 broker） */
#define MQTT_BROKER     "test.mosquitto.org"
#define MQTT_CLIENT_ID  "stm32_shelf_main_001"
#define MQTT_PUB_TOPIC  "maintest/yao-shelf/can"
#define MQTT_SUB_TOPIC  "maintest/yao-shelf/cmd"

#endif
