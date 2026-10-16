/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    MQTT client module implementation
 * 2026-09-29     RT-Thread    Rewrite on SAL + paho MQTT, drop raw AT driver
 */

#include "mqtt_client.h"

#include <rtdevice.h>
#include <netdev_ipaddr.h>
#include <netdev.h>
#include <string.h>
#include <stdio.h>

#include "paho_mqtt.h"

#define DBG_TAG "mqtt_client"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* MQTT客户端全局结构体 */
static mqtt_client_t mqtt_client = {0};

/* paho 客户端上下文，worker 线程由 paho 自己创建 */
static MQTTClient paho_client;

/* 下行命令：回调与计数（回调在 paho 线程上下文执行） */
static rt_uint32_t s_rx_ok = 0;
static mqtt_cmd_handler_t s_cmd_handler = RT_NULL;

/* 发布统计：诊断页用它区分「没收到帧」和「收到了但 MQTT 没连上」 */
static rt_uint32_t s_pub_ok   = 0;
static rt_uint32_t s_pub_drop = 0;

/* paho 要求 uri / clientID 在客户端生命周期内一直有效，不能用栈上的临时字符串 */
static char mqtt_uri[64] = {0};
static char mqtt_client_id[32] = {0};

/* 发布缓冲区：paho_mqtt_publish() 内部按字符串取长度，payload 必须以 '\0' 结尾 */
#define MQTT_PUB_BUF_SIZE 256
#define CAN_PUB_BUF_SIZE  128
#define MQTT_CMD_BUF_SIZE  64
static char mqtt_pub_buf[MQTT_PUB_BUF_SIZE] = {0};
static char can_pub_buf[CAN_PUB_BUF_SIZE] = {0};

/* 发布串行锁。两个原因需要它：
 *   1) mqtt_pub_buf 是共享缓冲区，实时转发和补传同时填会互相踩；
 *   2) paho 的 QoS1 用同一个信号量等 PUBACK，两个线程同发会偶发
 *      "等 5 秒超时"的假失败（实测约每 5~6 条错一条）。
 * 把"填缓冲 + 发布"整体串起来，两个问题一起消掉。 */
static rt_mutex_t mqtt_pub_lock = RT_NULL;

/* paho 收发缓冲区大小 */
#define MQTT_CLIENT_BUF_SIZE 1024

/* MQTT keepalive 周期（秒） */
#define MQTT_KEEPALIVE_INTERVAL 60

/* 断线后重连间隔（毫秒） */
#define MQTT_RECONNECT_INTERVAL 5000

/* ESP8266 在 SAL/netdev 中的设备名，at_device 固定注册为 "esp0" */
#define MQTT_NETDEV_NAME "esp0"

/**
 * 订阅主题收到消息（在 paho 线程上下文被调用）
 * @note payload 指向 paho 的 readbuf，不带 '\0' 结尾，必须按 payloadlen 拷贝；
 *       这里只做拷贝和转发，解析交给上层回调。
 */
static void mqtt_message_callback(MQTTClient *client, MessageData *data)
{
    char cmd[MQTT_CMD_BUF_SIZE];

    RT_UNUSED(client);

    if (data == RT_NULL || data->message == RT_NULL || s_cmd_handler == RT_NULL)
    {
        return;
    }

    if (data->message->payloadlen == 0 || data->message->payloadlen >= sizeof(cmd))
    {
        LOG_W("ignore command with bad length (%d)", (int)data->message->payloadlen);
        return;
    }

    rt_memcpy(cmd, data->message->payload, data->message->payloadlen);
    cmd[data->message->payloadlen] = '\0';

    s_rx_ok++;
    LOG_I("MQTT cmd: %s", cmd);

    s_cmd_handler(cmd);
}

/**
 * paho 每次尝试连接（TCP 之前）都会回调这里
 */
static void mqtt_connect_callback(MQTTClient *client)
{
    mqtt_client.status = MQTT_STATUS_CONNECTING;
    LOG_I("Connecting to %s ...", client->uri);
}

/**
 * MQTT CONNACK 成功、worker 进入收发循环后回调这里
 */
static void mqtt_online_callback(MQTTClient *client)
{
    mqtt_client.status = MQTT_STATUS_CONNECTED;
    LOG_I("MQTT online (%s, client_id=%s)", client->uri, mqtt_client_id);
}

/**
 * 连接失败或掉线，paho 会延迟 reconnect_interval 后自己重连
 */
static void mqtt_offline_callback(MQTTClient *client)
{
    mqtt_client.reconnect_count++;
    mqtt_client.status = MQTT_STATUS_RECONNECTING;
    LOG_W("MQTT offline, reconnect #%d in %d ms",
          mqtt_client.reconnect_count, MQTT_RECONNECT_INTERVAL);
}

/**
 * 初始化MQTT客户端模块
 */
int mqtt_client_init(mqtt_config_t *config)
{
    MQTTPacket_connectData condata = MQTTPacket_connectData_initializer;

    if (config == RT_NULL)
    {
        LOG_E("Invalid config");
        return -1;
    }

    rt_memset(&mqtt_client, 0, sizeof(mqtt_client));
    rt_memset(&paho_client, 0, sizeof(paho_client));
    mqtt_client.config = *config;

    if (mqtt_client.config.mqtt_broker == RT_NULL || mqtt_client.config.mqtt_client_id == RT_NULL)
    {
        LOG_E("mqtt_broker / mqtt_client_id not set");
        return -1;
    }

    /* 发布串行锁（只创建一次，说明见 mqtt_client_publish 上方注释） */
    if (mqtt_pub_lock == RT_NULL)
    {
        mqtt_pub_lock = rt_mutex_create("mqtt_pub", RT_IPC_FLAG_FIFO);
        if (mqtt_pub_lock == RT_NULL)
        {
            LOG_E("Failed to create publish lock");
            return -1;
        }
    }

    /* 组装 paho 需要的 URI，如 "tcp://test.mosquitto.org:1883"（域名由 SAL 解析为 IP） */
    rt_snprintf(mqtt_uri, sizeof(mqtt_uri), "tcp://%s:%d",
                mqtt_client.config.mqtt_broker, mqtt_client.config.mqtt_port);
    rt_snprintf(mqtt_client_id, sizeof(mqtt_client_id), "%s",
                mqtt_client.config.mqtt_client_id);

    paho_client.uri = mqtt_uri;
    rt_memcpy(&paho_client.condata, &condata, sizeof(condata));
    paho_client.condata.clientID.cstring = mqtt_client_id;
    paho_client.condata.keepAliveInterval = MQTT_KEEPALIVE_INTERVAL;
    paho_client.condata.cleansession = 1;

    if (mqtt_client.config.mqtt_user != RT_NULL && mqtt_client.config.mqtt_user[0] != '\0')
    {
        paho_client.condata.username.cstring = (char *)mqtt_client.config.mqtt_user;
        paho_client.condata.password.cstring = (char *)mqtt_client.config.mqtt_password;
    }

    paho_client.buf_size = paho_client.readbuf_size = MQTT_CLIENT_BUF_SIZE;
    paho_client.buf = (unsigned char *)rt_calloc(1, paho_client.buf_size);
    paho_client.readbuf = (unsigned char *)rt_calloc(1, paho_client.readbuf_size);
    if (paho_client.buf == RT_NULL || paho_client.readbuf == RT_NULL)
    {
        LOG_E("No memory for MQTT buffers");
        return -1;
    }

    paho_client.reconnect_interval = MQTT_RECONNECT_INTERVAL;
    paho_client.connect_callback = mqtt_connect_callback;
    paho_client.online_callback = mqtt_online_callback;
    paho_client.offline_callback = mqtt_offline_callback;

    /* 订阅槽位留给 mqtt_client_set_cmd_handler()：paho 只在 topicFilter 非空时才订阅 */
    paho_client.messageHandlers[0].topicFilter = RT_NULL;
    paho_client.defaultMessageHandler = RT_NULL;
    s_cmd_handler = RT_NULL;
    s_rx_ok = 0;

    /* 不碰 UART：ESP8266 的 AT 通道和 WiFi 连接由 at_device 的 esp8266 sample 独占 */
    mqtt_client.uart_dev = RT_NULL;
    mqtt_client.status = MQTT_STATUS_DISCONNECTED;

    LOG_I("MQTT client initialized: uri=%s id=%s", mqtt_uri, mqtt_client_id);
    return 0;
}

/**
 * 启动MQTT服务
 */
/**
 * 注册下行命令处理
 */
int mqtt_client_set_cmd_handler(const char *topic, mqtt_cmd_handler_t handler)
{
    char *topic_copy;

    if (topic == RT_NULL || handler == RT_NULL)
    {
        return -1;
    }

    if (paho_client.uri == RT_NULL)
    {
        LOG_E("MQTT client not initialized");
        return -1;
    }

    /* paho 在 net_disconnect_exit() 里会对 topicFilter 调 rt_free()，
     * 所以必须用 rt_strdup 传堆内存，不能直接挂字符串常量（否则误 free 静态区） */
    topic_copy = rt_strdup(topic);
    if (topic_copy == RT_NULL)
    {
        LOG_E("No memory for subscribe topic");
        return -1;
    }

    s_cmd_handler = handler;
    paho_client.messageHandlers[0].topicFilter = topic_copy;
    paho_client.messageHandlers[0].qos         = QOS1;  /* 命令不能丢；重复由幂等命令容忍 */
    paho_client.messageHandlers[0].callback    = mqtt_message_callback;

    LOG_I("Subscribe registered: %s", topic_copy);
    return 0;
}

int mqtt_client_start(void)
{
    if (paho_client.uri == RT_NULL)
    {
        LOG_E("MQTT client not initialized");
        return -1;
    }

    if (paho_mqtt_start(&paho_client) != PAHO_SUCCESS)
    {
        LOG_E("Failed to start paho MQTT worker thread");
        return -1;
    }

    /* 连不上时 paho 会每 5 秒自己重试，所以这里不必等 WiFi 就绪 */
    mqtt_client.status = MQTT_STATUS_CONNECTING;
    LOG_I("MQTT worker started");
    return 0;
}

/**
 * 发布MQTT消息
 */
int mqtt_client_publish(const char *topic, const void *payload, rt_size_t payload_len)
{
    if (topic == RT_NULL || payload == RT_NULL)
    {
        return -1;
    }

    if (mqtt_client.status != MQTT_STATUS_CONNECTED)
    {
        s_pub_drop++;
        /* 未连接时每帧都会走到这里，用 D 级避免刷屏；丢弃条数看 stats.dropped */
        LOG_D("MQTT not connected, drop publish to %s", topic);
        return -1;
    }

    if (payload_len >= MQTT_PUB_BUF_SIZE)
    {
        LOG_E("Payload too long (%d bytes)", (int)payload_len);
        return -1;
    }

    if (mqtt_pub_lock != RT_NULL)
    {
        rt_mutex_take(mqtt_pub_lock, RT_WAITING_FOREVER);
    }

    rt_memcpy(mqtt_pub_buf, payload, payload_len);
    mqtt_pub_buf[payload_len] = '\0';

    /* paho 只支持 QOS1 */
    if (paho_mqtt_publish(&paho_client, QOS1, topic, mqtt_pub_buf) != PAHO_SUCCESS)
    {
        if (mqtt_pub_lock != RT_NULL)
        {
            rt_mutex_release(mqtt_pub_lock);
        }
        /* 和"未连接"一样计入 dropped：诊断页能直接看出"没上云"的条数。
         * 用 D 级避免断网时每帧刷屏。 */
        s_pub_drop++;
        LOG_D("Publish to %s failed", topic);
        return -1;
    }

    LOG_D("Published to %s: %s", topic, mqtt_pub_buf);
    if (mqtt_pub_lock != RT_NULL)
    {
        rt_mutex_release(mqtt_pub_lock);
    }

    s_pub_ok++;
    return 0;
}

/**
 * 发送CAN数据到MQTT
 */
int mqtt_client_publish_can_data(rt_uint32_t can_id, const rt_uint8_t *data, rt_uint8_t len)
{
    int offset = 0;
    int i;

    if (data == RT_NULL || len == 0 || mqtt_client.config.mqtt_publish_topic == RT_NULL)
    {
        return -1;
    }

    /* 经典CAN一帧最多 8 字节，钳一下防止 snprintf 的剩余长度算成负数 */
    if (len > 8)
    {
        len = 8;
    }

    offset += rt_snprintf(can_pub_buf + offset, sizeof(can_pub_buf) - offset,
                          "{\"can_id\":\"0x%03X\",\"data\":[", can_id);

    for (i = 0; i < len; i++)
    {
        offset += rt_snprintf(can_pub_buf + offset, sizeof(can_pub_buf) - offset,
                              (i == 0) ? "0x%02X" : ",0x%02X", data[i]);
    }

    offset += rt_snprintf(can_pub_buf + offset, sizeof(can_pub_buf) - offset, "]}");

    return mqtt_client_publish(mqtt_client.config.mqtt_publish_topic,
                               can_pub_buf, (rt_size_t)offset);
}

/**
 * 获取MQTT连接状态
 */
mqtt_status_t mqtt_client_get_status(void)
{
    return mqtt_client.status;
}

/**
 * 获取MQTT运行统计
 */
void mqtt_client_get_stats(mqtt_client_stats_t *stats)
{
    if (stats == RT_NULL)
    {
        return;
    }

    stats->status = mqtt_client.status;
    stats->published = s_pub_ok;
    stats->received = s_rx_ok;
    stats->dropped = s_pub_drop;
    stats->reconnect_count = mqtt_client.reconnect_count;
}

/**
 * 检查是否连接到WiFi
 */
rt_bool_t mqtt_client_is_wifi_connected(void)
{
    struct netdev *dev = netdev_get_by_name(MQTT_NETDEV_NAME);

    if (dev == RT_NULL)
    {
        return RT_FALSE;
    }

    return netdev_is_link_up(dev) ? RT_TRUE : RT_FALSE;
}

/**
 * 停止MQTT服务
 */
int mqtt_client_stop(void)
{
    if (paho_client.uri == RT_NULL)
    {
        return -1;
    }

    /* worker 收到 DISCONNECT 后会自己退出，并释放 buf / readbuf / pub_sem */
    paho_mqtt_stop(&paho_client);
    paho_client.uri = RT_NULL;
    mqtt_client.status = MQTT_STATUS_DISCONNECTED;

    LOG_I("MQTT client stopped");
    return 0;
}

#ifdef FINSH_USING_MSH
/**
 * msh 手工发一条消息，用来单独验证 MQTT 链路（不依赖 CAN 数据）
 * 用法：mqtt_pub hello
 */
static int mqtt_pub(int argc, char **argv)
{
    if (argc != 2 || mqtt_client.config.mqtt_publish_topic == RT_NULL)
    {
        rt_kprintf("mqtt_pub <message>  --publish to %s\n",
                   mqtt_client.config.mqtt_publish_topic ? mqtt_client.config.mqtt_publish_topic : "(not init)");
        return -1;
    }

    if (mqtt_client_publish(mqtt_client.config.mqtt_publish_topic, argv[1], rt_strlen(argv[1])) != 0)
    {
        rt_kprintf("mqtt_pub failed (status=%d)\n", mqtt_client_get_status());
        return -1;
    }

    rt_kprintf("mqtt_pub ok -> %s\n", mqtt_client.config.mqtt_publish_topic);
    return 0;
}
MSH_CMD_EXPORT(mqtt_pub, publish a message over MQTT);
#endif /* FINSH_USING_MSH */
