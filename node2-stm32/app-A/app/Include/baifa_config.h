#ifndef __BAIFA_CONFIG_H
#define __BAIFA_CONFIG_H

#include <stdint.h>
#include "cmsis_os.h"
/* 巴法云平台配置 */

// 巴法云服务器地址
#define BAIFA_HOST "api.bemfa.com"

// HTTP协议端口
#define BAIFA_PORT "80"

// 你的巴法云设备ID（需要在巴法云平台注册获取）
#define BAIFA_DEVICE_ID "your_device_id_here"

// 你的巴法云主密钥（需要在巴法云平台获取）
#define BAIFA_API_KEY "your_api_key_here"

// 设备标识符
#define BAIFA_UID "device_uid"

// 版本检查接口 (返回JSON格式: {"version":"1.0.1", "url":"http://...", "info":"..."})
#define BAIFA_VERSION_API "/api/device/version"

// 固件下载接口前缀
#define BAIFA_FW_DOWNLOAD_API "/api/device/firmware"

// 当前固件版本（与OTA.h的MCU_VER对应）
#define CURRENT_FW_VERSION "1.0.0"

/* 下载参数 */
#define BAIFA_DOWNLOAD_CHUNK_SIZE 512  // 每次下载512字节

/* HTTP请求超时配置 (ms) */
#define BAIFA_HTTP_TIMEOUT 5000
#define BAIFA_DOWNLOAD_TIMEOUT 10000

/* 签名方式：巴法云支持的认证方式 */
// 1 - 简单认证 (deviceId+apikey)
// 2 - 加密认证 (MD5签名)
#define BAIFA_AUTH_MODE 1

#endif /* __BAIFA_CONFIG_H */
