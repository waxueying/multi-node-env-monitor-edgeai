/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    MQTT client module header
 */

#ifndef __MQTT_CLIENT_H__
#define __MQTT_CLIENT_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT连接状态枚举 */
typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,    /* 未连接 */
    MQTT_STATUS_CONNECTING,          /* 连接中 */
    MQTT_STATUS_CONNECTED,           /* 已连接 */
    MQTT_STATUS_RECONNECTING         /* 重新连接中 */
} mqtt_status_t;

/* MQTT配置结构体 */
typedef struct {
    const char *wifi_ssid;           /* WiFi SSID */
    const char *wifi_password;       /* WiFi 密码 */
    const char *mqtt_broker;         /* MQTT broker地址 */
    rt_uint16_t mqtt_port;           /* MQTT port */
    const char *mqtt_client_id;      /* MQTT client ID */
    const char *mqtt_user;           /* MQTT 用户名 */
    const char *mqtt_password;       /* MQTT 密码 */
    const char *mqtt_publish_topic;  /* MQTT 发布主题 */
} mqtt_config_t;

/* MQTT客户端结构体 */
typedef struct {
    mqtt_config_t config;            /* 配置 */
    mqtt_status_t status;            /* 连接状态 */
    rt_device_t uart_dev;            /* UART设备指针 */
    rt_thread_t mqtt_thread;         /* MQTT线程 */
    rt_thread_t wifi_monitor_thread; /* WiFi监控线程 */
    rt_uint32_t reconnect_count;     /* 重连次数 */
} mqtt_client_t;

/* MQTT 运行统计快照，供 OLED / 调试查看 */
typedef struct {
    mqtt_status_t status;            /* 当前连接状态 */
    rt_uint32_t   published;         /* 成功交给 paho 的发布次数 */
    rt_uint32_t   dropped;           /* 未连接时被丢弃的发布次数 */
    rt_uint32_t   received;          /* 收到的下行命令条数 */
    rt_uint32_t   reconnect_count;   /* 掉线重连次数 */
} mqtt_client_stats_t;

/* 下行命令回调：cmd 已按 payload 长度截断并以 '\0' 结尾。
 * 回调运行在 paho 的任务线程里，里面不能阻塞（不要 delay、不要等信号量），
 * 否则会拖住 keepalive 与 QOS1 的 PUBACK 处理。 */
typedef void (*mqtt_cmd_handler_t)(const char *cmd);

/**
 * 注册下行命令处理：必须在 mqtt_client_start() 之前调用
 * @param topic   订阅主题（内部会复制一份，调用方不必保证生命周期）
 * @param handler 收到该主题消息时的回调
 * @return 0 成功，其他值失败
 * @note paho 在每次连接/重连成功后会按这里登记的主题自动订阅
 */
int mqtt_client_set_cmd_handler(const char *topic, mqtt_cmd_handler_t handler);

/**
 * 初始化MQTT客户端模块
 * @param config MQTT配置
 * @return 0 成功，其他值失败
 */
int mqtt_client_init(mqtt_config_t *config);

/**
 * 启动MQTT服务
 * @return 0 成功，其他值失败
 */
int mqtt_client_start(void);

/**
 * 发布MQTT消息
 * @param topic 主题
 * @param payload 消息内容
 * @param payload_len 消息长度
 * @return 0 成功，其他值失败
 */
int mqtt_client_publish(const char *topic, const void *payload, rt_size_t payload_len);

/**
 * 发送CAN数据到MQTT
 * @param can_id CAN ID
 * @param data 数据指针
 * @param len 数据长度
 * @return 0 成功，其他值失败
 */
int mqtt_client_publish_can_data(rt_uint32_t can_id, const rt_uint8_t *data, rt_uint8_t len);

/**
 * 获取MQTT连接状态
 * @return 当前连接状态
 */
mqtt_status_t mqtt_client_get_status(void);

/**
 * 获取MQTT运行统计快照
 * @param stats 输出缓冲区指针
 */
void mqtt_client_get_stats(mqtt_client_stats_t *stats);

/**
 * 检查是否连接到WiFi
 * @return RT_TRUE 已连接，RT_FALSE 未连接
 */
rt_bool_t mqtt_client_is_wifi_connected(void);

/**
 * 停止MQTT服务
 * @return 0 成功，其他值失败
 */
int mqtt_client_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* __MQTT_CLIENT_H__ */
