/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    first version
 * 2025-12-02     RT-Thread    Add CAN, WiFi/MQTT, and SD storage modules
 */

#include <rtthread.h>
#include "secrets.h"
#include "can_receiver.h"
#include "oled_display.h"

#include "mqtt_client.h"
#include <stdio.h>

/* 暂时停用：SD 卡存储 */
#include "sd_storage.h"
#include "ota_flag.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* 应用层主线程优先级 */
#define APP_MAIN_THREAD_PRIORITY 11

/* 数据转发线程堆栈大小 */
#define DATA_FORWARD_THREAD_STACK_SIZE 2048

/* SD 调试开关：1 = 最小系统（只留 CAN 接收 + 串口命令行 + SD 诊断）。
 * 目的：隔离"读 SD 卡把系统搞死"——把 MQTT、数据转发、OLED 全部停掉，
 * 看问题是否还复现。正常功能验证时改回 0。 */
#define SD_DEBUG_MINIMAL 0

/* MQTT配置 */
static mqtt_config_t mqtt_config = {
    /* WiFi 实际由 at_device 的 esp8266 sample 在启动时用 rtconfig.h 里的
     * ESP8266_SAMPLE_WIFI_SSID / PASSWORD 连接，这里只作记录 */
    .wifi_ssid = WIFI_SSID,               /* WiFi SSID */
    .wifi_password = WIFI_PASSWORD,           /* WiFi 密码 */
    .mqtt_broker = MQTT_BROKER,       /* MQTT broker 域名（原项目用的公共测试服务器） */
    .mqtt_port = 1883,                         /* MQTT 端口 */
    .mqtt_client_id = MQTT_CLIENT_ID,   /* MQTT client ID：公网 broker 上必须唯一，否则会被同名客户端踢下线 */
    .mqtt_user = "",                           /* MQTT 用户名 */
    .mqtt_password = "",                       /* MQTT 密码 */
    .mqtt_publish_topic = "maintest/yao-shelf/can"  /* MQTT 发布主题（公网 broker 要加唯一前缀） */
};

/* 暂时停用：原数据转发逻辑（MQTT 转发 + SD 卡备份） */
#if 0
/* 数据转发线程 - 负责从CAN接收数据并处理 */
static void data_forward_thread_entry(void *param)
{
    can_data_t can_data;
    sd_storage_record_t sd_record;
    rt_err_t err;

    LOG_I("Data forward thread started");

    while (1)
    {
        /* 从CAN接收缓冲中获取数据，超时时间100ms */
        err = can_receiver_get_data(&can_data, rt_tick_from_millisecond(100));
        if (err != 0)
        {
            /* 没有CAN数据，继续等待 */
            continue;
        }

        LOG_D("Received CAN data - ID: 0x%03X, DLC: %d", can_data.can_id, can_data.dlc);

        /* 检查WiFi连接状态
         * 判断顺序：先看 MQTT 状态（只是读一个变量），没连上时被 && 短路，
         * 省掉后面 mqtt_client_is_wifi_connected() 那次 netdev 查询。 */
        if (mqtt_client_get_status() == MQTT_STATUS_CONNECTED && mqtt_client_is_wifi_connected())
        {
            /* WiFi已连接，发送数据到MQTT */
            if (mqtt_client_publish_can_data(can_data.can_id, can_data.data, can_data.dlc) == 0)
            {
                LOG_D("CAN data published to MQTT successfully");
            }
            else
            {
                LOG_W("Failed to publish CAN data to MQTT");
                
                /* 发送失败，备份到SD卡 */
                sd_record.can_id = can_data.can_id;
                sd_record.dlc = can_data.dlc;
                sd_record.timestamp = can_data.timestamp;
                rt_memcpy(sd_record.data, can_data.data, can_data.dlc);
                
                sd_storage_save_record(&sd_record);
            }
        }
        else
        {
            /* WiFi未连接，保存数据到SD卡 */
            LOG_W("WiFi disconnected, saving CAN data to SD card");

            sd_record.can_id = can_data.can_id;
            sd_record.dlc = can_data.dlc;
            sd_record.timestamp = can_data.timestamp;
            rt_memcpy(sd_record.data, can_data.data, can_data.dlc);

            if (sd_storage_save_record(&sd_record) == 0)
            {
                LOG_D("CAN data saved to SD card buffer");
            }
            else
            {
                LOG_E("Failed to save CAN data to SD card");
            }
        }

        rt_thread_mdelay(10);
    }
}
#endif

/* ---- MQTT 下行命令 -> CAN 0x200 ---- */

/* 订阅主题：PC 上用 mosquitto_pub -h test.mosquitto.org -t maintest/yao-shelf/cmd -m "env 2" 下发 */
#define MQTT_CMD_TOPIC "maintest/yao-shelf/cmd"

/* 与节点1 can_protocol.h 保持一致：data[0]=命令字，data[1]=参数 */
#define CMD_SET_ENV_PROFILE 0x01
#define ENV_PROFILE_MAX     4   /* 0=食品/药品 1=电子元件 2=纺织品/纸箱 3=化学品 4=普通货物 */

/**
 * 下行命令处理：运行在 MQTT 线程上下文，只解析并发帧，不做等待
 * @note 节点1 执行后会在 0x200 上回一帧回执（B0=当前方案，B1=0 成功/1 失败），
 *       回执随上行链路出现在 maintest/yao-shelf/can，闭环由此可验证
 */
static void mqtt_cmd_handler(const char *cmd)
{
    unsigned int profile;
    rt_uint8_t frame[2];

    if (rt_strncmp(cmd, "env ", 4) != 0)
    {
        LOG_W("unknown cmd: %s", cmd);
        return;
    }

    if (sscanf(cmd + 4, "%u", &profile) != 1 || profile > ENV_PROFILE_MAX)
    {
        LOG_W("bad env profile: %s (expect 0~%d)", cmd, ENV_PROFILE_MAX);
        return;
    }

    frame[0] = CMD_SET_ENV_PROFILE;
    frame[1] = (rt_uint8_t)profile;

    if (can_receiver_send(CAN_ID_CMD_ENV_PROFILE, frame, sizeof(frame)) == RT_EOK)
    {
        LOG_I("env profile -> %u (CAN 0x200 sent)", profile);
    }
}

/* 数据转发线程 - 从 CAN 队列取帧并转发到 MQTT
 * SD 卡备份逻辑先不启用：MQTT 没连上时由 publish 内部丢帧并计入 dropped */
static void data_forward_thread_entry(void *param)
{
    can_data_t can_data;

    LOG_I("Data forward thread started (CAN -> MQTT)");

    while (1)
    {
        if (can_receiver_get_data(&can_data, rt_tick_from_millisecond(100)) == 0)
        {
            LOG_D("CAN RX id=0x%03X dlc=%d", can_data.can_id, can_data.dlc);

            /* 不在这里判断连接状态：统一交给 publish 内部处理，
             * 未连接时它自己记账（dropped++），诊断页据此区分「没收到帧」和「没发出去」 */
            if (mqtt_client_publish_can_data(can_data.can_id, can_data.data, can_data.dlc) != 0)
            {
                /* 没上云就落到 SD 卡：断网期间数据不丢，恢复后再导出（补传逻辑后面做） */
                sd_storage_record_t rec;

                rec.can_id = can_data.can_id;
                rec.dlc = can_data.dlc;
                rec.timestamp = can_data.timestamp;
                rt_memcpy(rec.data, can_data.data, can_data.dlc);
                sd_storage_save_record(&rec);
            }
        }
    }
}

int main(void)
{
    rt_thread_t data_forward_thread;
    rt_err_t err;

    LOG_I("==============================================");
    LOG_I("STM32F407ZGT6 Application Started");
    /* 自己跑在哪个区？bootloader 跳转前把 VTOR 设成了本区基址：
     *   0x08010000 = APP-A（默认），0x08080000 = APP-B
     * （直接读寄存器，不引 CMSIS 头文件） */
    LOG_I("Running in %s (VTOR=0x%08X)",
          (*(volatile rt_uint32_t *)0xE000ED08u == 0x08080000u) ? "APP-B" : "APP-A",
          (unsigned)(*(volatile rt_uint32_t *)0xE000ED08u));
    LOG_I("==============================================");
    LOG_I("Features:");
    LOG_I("  1. CAN Receiver with dual ID support");
    LOG_I("  2. OLED status display (CAN RX count / last frame)");
    LOG_I("==============================================");

    /* ==================== 初始化阶段 ==================== */
    
    /* 1. 初始化CAN接收模块 */
#if !SD_DEBUG_MINIMAL
    LOG_I("Initializing CAN receiver module...");
    if (can_receiver_init() != 0)
    {
        LOG_E("Failed to initialize CAN receiver");
        return -1;
    }
    LOG_I("CAN receiver module initialized");
#endif

    /* 2. 初始化MQTT客户端模块 */
#if !SD_DEBUG_MINIMAL
    LOG_I("Initializing MQTT client module...");
    if (mqtt_client_init(&mqtt_config) != 0)
    {
        LOG_E("Failed to initialize MQTT client");
        return -1;
    }
    LOG_I("MQTT client module initialized");
#endif

    /* 3. 注册下行命令处理（必须在 mqtt_client_start() 之前） */
#if !SD_DEBUG_MINIMAL
    LOG_I("Registering MQTT command handler...");
    if (mqtt_client_set_cmd_handler(MQTT_CMD_TOPIC, mqtt_cmd_handler) != 0)
    {
        LOG_E("Failed to register MQTT command handler");
        return -1;
    }

    /* 暂时停用：SD 卡存储模块初始化 */
#endif

    /* 3. 初始化SD卡存储模块 */
    LOG_I("Initializing SD storage module...");
    if (sd_storage_init() != 0)
    {
        LOG_E("Failed to initialize SD storage");
        return -1;
    }
    LOG_I("SD storage module initialized");

    /* ==================== 启动阶段 ==================== */

    /* 1. 启动CAN接收线程 */
#if !SD_DEBUG_MINIMAL
    LOG_I("Starting CAN receiver thread...");
    if (can_receiver_start() != 0)
    {
        LOG_E("Failed to start CAN receiver");
        return -1;
    }
    LOG_I("CAN receiver thread started");
#endif

    /* 2. 启动MQTT客户端线程 */
#if !SD_DEBUG_MINIMAL
    LOG_I("Starting MQTT client thread...");
    if (mqtt_client_start() != 0)
    {
        LOG_E("Failed to start MQTT client");
        return -1;
    }
    LOG_I("MQTT client thread started");
#endif

    /* 暂时停用：SD 卡存储线程 */
    /* 3. 启动SD卡存储线程 */
    LOG_I("Starting SD storage thread...");
    if (sd_storage_start() != 0)
    {
        LOG_E("Failed to start SD storage");
        return -1;
    }
    LOG_I("SD storage thread started");

    /* 4. 启动OLED显示线程 */
#if !SD_DEBUG_MINIMAL
    LOG_I("Starting OLED display thread...");
    if (oled_display_start() != 0)
    {
        LOG_E("Failed to start OLED display");
        return -1;
    }
    LOG_I("OLED display thread started");
#endif

    /* 5. 创建数据转发线程 */
#if !SD_DEBUG_MINIMAL
    LOG_I("Starting data forward thread...");
    data_forward_thread = rt_thread_create(
        "data_forward",
        data_forward_thread_entry,
        RT_NULL,
        DATA_FORWARD_THREAD_STACK_SIZE,
        APP_MAIN_THREAD_PRIORITY,
        10
    );

    if (data_forward_thread == RT_NULL)
    {
        LOG_E("Failed to create data forward thread");
        return -1;
    }

    err = rt_thread_startup(data_forward_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start data forward thread");
        return -1;
    }
    LOG_I("Data forward thread started");
#endif

    LOG_I("==============================================");
    LOG_I("All modules started successfully!");

    /* OTA 启动自确认：能走到这里说明新固件各模块都起来了，把"我在哪个区、
     * 哪个区就是正式版本"写回标志区。不需要变更时内部直接返回，不碰 Flash。*/
    ota_flag_confirm();

    LOG_I("Application running, waiting for CAN data...");
    LOG_I("==============================================");

    /* ==================== 运行阶段 ==================== */

    /* 主线程监控 */
    while (1)
    {
        rt_thread_mdelay(5000);

        /* 定期输出状态信息 */
        LOG_D("System Status:");
        LOG_D("  - CAN RX Queue: %d", can_receiver_get_count());
    }

    return RT_EOK;
}
