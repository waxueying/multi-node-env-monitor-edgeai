/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    CAN receiver module implementation
 */

#include "can_receiver.h"
#include <rtdevice.h>
#include <board.h>          /* 诊断用：读 CAN1 寄存器 + HAL_RCC_GetPCLK1Freq() */
#include <string.h>

#define DBG_TAG "can_receiver"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* CAN设备指针 */
static rt_device_t can_dev = RT_NULL;

/* CAN接收消息队列 */
static struct rt_messagequeue *can_rx_queue = RT_NULL;

/* CAN接收线程 */
static rt_thread_t can_rx_thread = RT_NULL;

/* CAN接收线程优先级 */
#define CAN_RX_THREAD_PRIORITY 15

/* CAN接收线程堆栈大小 */
#define CAN_RX_THREAD_STACK_SIZE 1024

/* 节点1/节点2 上报帧 ID 表，下标即 can_receiver_stats_t 中统计数组的下标 */
const rt_uint32_t can_rx_id_table[CAN_RX_ID_COUNT] = {
    CAN_ID_STRUCT_STATUS,     /* 0x100 */
    CAN_ID_STRUCT_IMPACT,     /* 0x101 */
    CAN_ID_FAULT_REPORT,      /* 0x102 */
    CAN_ID_AI_RESULT,         /* 0x103 */
    CAN_ID_CMD_ENV_PROFILE,   /* 0x200 */
    CAN_ID_ENV_DATA,          /* 0x456 */
    CAN_ID_NODE2_STATUS,      /* 0x110 */
    CAN_ID_NODE2_EVENT,       /* 0x111 */
    CAN_ID_NODE2_FAULT        /* 0x112 */
};

/* 接收统计快照：接收线程写、显示线程读，读写都在临界区内 */
static can_receiver_stats_t rx_stats;

/* 最近收到的帧（环形缓冲）：接收线程写、显示线程读，都在临界区里访问。
 * 只记 ID 在表内的帧，和进入 can_rx_queue 的是同一批数据。 */
static can_data_t rx_recent[CAN_RECENT_MAX];
static rt_uint32_t rx_recent_head = 0;    /* 下一个要写入的位置 */
static rt_uint32_t rx_recent_total = 0;   /* 累计写入条数 */

/**
 * 查找帧 ID 在统计表里的下标
 * @return 下标（>=0）；不在表内的帧返回 -1
 */
static rt_int32_t report_id_index(rt_uint32_t id)
{
    rt_uint8_t i;

    for (i = 0; i < CAN_RX_ID_COUNT; i++)
    {
        if (can_rx_id_table[i] == id)
        {
            return (rt_int32_t)i;
        }
    }

    return -1;
}

/**
 * CAN接收回调函数
 * @note 由驱动在 CAN1_RX0/RX1 中断里调用（见 can.c 的 rt_hw_can_isr），
 *       所以只能用关中断保护计数，不能打印日志、不能用 rt_enter_critical。
 *       该计数是"帧到底有没有进 MCU"最直接的证据：>0 才说明接收路径真的动了。
 */
static rt_err_t can_rx_callback(rt_device_t dev, rt_size_t size)
{
    rt_base_t level;

    RT_UNUSED(dev);
    RT_UNUSED(size);

    level = rt_hw_interrupt_disable();
    rx_stats.isr_frames++;
    rt_hw_interrupt_enable(level);

    return RT_EOK;
}

/**
 * CAN接收线程函数
 */
static void can_receiver_thread_entry(void *param)
{
    struct rt_can_msg rx_msg;
    can_data_t can_data;
    rt_size_t size;
    rt_err_t err;
    struct can_configure can_cfg = CANDEFAULTCONFIG;

    LOG_I("CAN receiver thread started");

    /* 诊断：确认实际时钟与 drv_can.c 波特率表的基准（APB1 = 36MHz）一致 */
    LOG_I("clk: SYSCLK=%d PCLK1=%d",
          HAL_RCC_GetSysClockFreq(), HAL_RCC_GetPCLK1Freq());

    /* 打开CAN设备 */
    if (can_dev == RT_NULL)
    {
        can_dev = rt_device_find("can1");
        if (can_dev == RT_NULL)
        {
            LOG_E("CAN device not found");
            return;
        }

        /* 顺序要紧：必须先 open。
         * rt_device_open 会触发 rt_can_init()，那里用 can->config 配硬件，
         * 而 can->config 是注册时的默认值 CAN1MBaud。若先 configure 再 open，
         * 刚设的 500k 会被这次默认初始化覆盖回 1M。 */
        /* INT_TX 不能省：框架的 rt_can_write() 只在 open_flag 带 INT_TX 时才真正
         * 走发送流程，否则写操作静默返回 0，下行命令根本发不出去。 */
        err = rt_device_open(can_dev, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX);
        if (err != RT_EOK)
        {
            LOG_E("Failed to open CAN device: %d", err);
            return;
        }

        /* 波特率必须与节点1 一致：500 kbps */
        can_cfg.baud_rate = CAN500kBaud;
        can_cfg.mode = RT_CAN_MODE_NORMAL;
        err = rt_device_control(can_dev, RT_DEVICE_CTRL_CONFIG, &can_cfg);
        if (err != RT_EOK)
        {
            LOG_E("Failed to configure CAN baud rate: %d", err);
            return;
        }

        /* BTR 低10位 = 预分频-1。36MHz/(预分频*12)：500k 应为 6，被覆盖成 1M 则为 3 */
        LOG_I("CAN configured: BTR=0x%08X prescaler=%u",
              (unsigned)CAN1->BTR, (unsigned)((CAN1->BTR & 0x3FFu) + 1u));

        /* 设置接收回调函数 */
        rt_device_set_rx_indicate(can_dev, can_rx_callback);

        /* 设备已就绪：显示层据此区分"CAN 没起来"和"单纯没数据" */
        rx_stats.dev_ready = RT_TRUE;
    }

    /* 接收循环 */
    while (1)
    {
        /* 读取CAN数据 */
        size = rt_device_read(can_dev, 0, &rx_msg, sizeof(struct rt_can_msg));

        if (size == sizeof(struct rt_can_msg))
        {
            rt_int32_t idx = report_id_index(rx_msg.id);

            /* 先无条件记账：用来区分"总线上一帧都没有"和"帧收到了但 ID 不在表内" */
            rt_enter_critical();
            rx_stats.raw_total++;
            if (idx < 0)
            {
                rx_stats.unknown++;
            }
            rt_exit_critical();

            if (rx_stats.raw_total == 1)
            {
                LOG_I("first CAN frame id=0x%03X dlc=%d", rx_msg.id, rx_msg.len);
            }
            else if (idx < 0 && rx_stats.unknown == 1)
            {
                LOG_W("CAN id 0x%03X not in id table (first unknown frame)", rx_msg.id);
            }

            /* 检查是否是目标CAN ID */
            if (idx >= 0)
            {
                /* 构造CAN数据 */
                can_data.can_id = rx_msg.id;
                can_data.dlc = rx_msg.len;
                can_data.timestamp = rt_tick_get();

                /* 复制数据 */
                rt_memcpy(can_data.data, rx_msg.data, rx_msg.len);

                /* 更新统计快照（临界区：避免显示线程读到半更新的结构） */
                rt_enter_critical();
                rx_stats.total++;
                rx_stats.cnt[idx]++;
                rx_stats.last = can_data;
                rx_stats.has_last = RT_TRUE;
                /* AI 结果帧单独留一份：显示层只看 0x103，不被别的 ID 覆盖 */
                if (rx_msg.id == CAN_ID_AI_RESULT)
                {
                    rx_stats.last_ai = can_data;
                    rx_stats.has_ai = RT_TRUE;
                }
                else if (rx_msg.id == CAN_ID_ENV_DATA)
                {
                    /* 待机页要显示温湿度，单独留一份不被别的 ID 覆盖 */
                    rx_stats.last_env = can_data;
                    rx_stats.has_env = RT_TRUE;
                }
                else if (rx_msg.id == CAN_ID_STRUCT_STATUS)
                {
                    /* 待机页要显示倾角/振动/撞击，同样单独留一份 */
                    rx_stats.last_struct = can_data;
                    rx_stats.has_struct = RT_TRUE;
                }
                rt_exit_critical();

                /* 发送到消息队列 */
                /* 记入"最近帧"环形缓冲：OLED 的 CAN 监视页用它显示通讯内容 */
                rt_enter_critical();
                rx_recent[rx_recent_head] = can_data;
                rx_recent_head++;
                if (rx_recent_head >= CAN_RECENT_MAX)
                {
                    rx_recent_head = 0;
                }
                rx_recent_total++;
                rt_exit_critical();

                err = rt_mq_send(can_rx_queue, &can_data, sizeof(can_data_t));
                if (err != RT_EOK)
                {
                    LOG_W("CAN message queue full, data lost");
                }

                LOG_D("CAN ID: 0x%03X, DLC: %d", rx_msg.id, rx_msg.len);
            }
        }
        else
        {
            /* 延迟等待 */
            rt_thread_mdelay(10);
        }
    }
}

/**
 * 初始化CAN接收模块
 */
int can_receiver_init(void)
{
    rt_err_t err;

    LOG_I("Initializing CAN receiver module");

    /* 创建消息队列 */
    can_rx_queue = rt_mq_create("can_queue", sizeof(can_data_t), CAN_RX_QUEUE_SIZE, RT_IPC_FLAG_FIFO);
    if (can_rx_queue == RT_NULL)
    {
        LOG_E("Failed to create CAN message queue");
        return -1;
    }

    LOG_I("CAN receiver module initialized successfully");
    return 0;
}

/**
 * 启动CAN接收线程
 */
int can_receiver_start(void)
{
    rt_err_t err;

    if (can_rx_thread != RT_NULL)
    {
        LOG_W("CAN receiver thread already started");
        return 0;
    }

    /* 创建接收线程 */
    can_rx_thread = rt_thread_create(
        "can_rx",
        can_receiver_thread_entry,
        RT_NULL,
        CAN_RX_THREAD_STACK_SIZE,
        CAN_RX_THREAD_PRIORITY,
        10
    );

    if (can_rx_thread == RT_NULL)
    {
        LOG_E("Failed to create CAN receiver thread");
        return -1;
    }

    /* 启动线程 */
    err = rt_thread_startup(can_rx_thread);
    if (err != RT_EOK)
    {
        LOG_E("Failed to start CAN receiver thread: %d", err);
        return -1;
    }

    LOG_I("CAN receiver thread started successfully");
    return 0;
}

/**
 * 从接收缓冲中获取CAN数据
 */
int can_receiver_get_data(can_data_t *data, rt_int32_t timeout)
{
    rt_ssize_t received;

    if (data == RT_NULL)
    {
        return -1;
    }

    if (can_rx_queue == RT_NULL)
    {
        return -1;
    }

    /* 从消息队列接收数据 */
    /* RT-Thread 5.1 起 rt_mq_recv 成功时返回收到的字节数（不再是 RT_EOK），
     * 失败返回负值。这里必须按“长度”判断：若按 == RT_EOK 判断，
     * 每帧都会被误判成失败，数据被丢掉（队列看起来是空的）。 */
    received = rt_mq_recv(can_rx_queue, data, sizeof(can_data_t), timeout);
    if (received != (rt_ssize_t)sizeof(can_data_t))
    {
        return -1;
    }

    return 0;
}

/**
 * 获取接收队列中的数据个数
 */
rt_uint32_t can_receiver_get_count(void)
{
    if (can_rx_queue == RT_NULL)
    {
        return 0;
    }

    return can_rx_queue->entry;
}

/**
 * 获取最近收到的若干帧（最新的一帧在 out[0]）
 */
rt_uint32_t can_receiver_get_recent(can_data_t *out, rt_uint32_t max)
{
    rt_uint32_t i;
    rt_uint32_t n;
    rt_uint32_t head;

    if (out == RT_NULL || max == 0)
    {
        return 0;
    }

    rt_enter_critical();

    /* 累计写入条数决定有效条数（超出缓冲大小就只保留最近 CAN_RECENT_MAX 条） */
    n = rx_recent_total;
    if (n > CAN_RECENT_MAX)
    {
        n = CAN_RECENT_MAX;
    }
    if (n > max)
    {
        n = max;
    }

    head = rx_recent_head;   /* 下一个写入位置，它的前一个就是最新一条 */

    for (i = 0; i < n; i++)
    {
        rt_uint32_t idx = (head + CAN_RECENT_MAX - 1u - i) % CAN_RECENT_MAX;
        out[i] = rx_recent[idx];
    }

    rt_exit_critical();

    return n;
}

/**
 * 发送一帧标准帧 CAN 数据
 */
rt_err_t can_receiver_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len)
{
    struct rt_can_msg tx_msg;
    rt_size_t written;

    if (can_dev == RT_NULL || data == RT_NULL || len > 8)
    {
        return -RT_ERROR;
    }

    rt_memset(&tx_msg, 0, sizeof(tx_msg));
    tx_msg.id  = id;
    tx_msg.ide = RT_CAN_STDID;   /* 与节点1 一致：标准帧 */
    tx_msg.rtr = RT_CAN_DTR;     /* 数据帧 */
    tx_msg.len = len;
    rt_memcpy(tx_msg.data, data, len);

    /* 成功时返回整条消息的字节数；邮箱满或没有应答时返回 0 */
    written = rt_device_write(can_dev, 0, &tx_msg, sizeof(tx_msg));
    if (written != sizeof(tx_msg))
    {
        LOG_W("CAN send failed (id=0x%03X len=%u)", (unsigned)id, (unsigned)len);
        return -RT_ERROR;
    }

    return RT_EOK;
}

/**
 * 获取CAN接收统计快照（线程安全）
 */
void can_receiver_get_stats(can_receiver_stats_t *stats)
{
    if (stats == RT_NULL)
    {
        return;
    }

    rt_enter_critical();
    *stats = rx_stats;
    rt_exit_critical();
}

/**
 * 获取 CAN 控制器硬件诊断快照
 */
void can_receiver_get_diag(can_bus_diag_t *diag)
{
    if (diag == RT_NULL)
    {
        return;
    }

    diag->esr = CAN1->ESR;
    diag->rf0r = CAN1->RF0R;
    diag->msr = CAN1->MSR;
    diag->btr = CAN1->BTR;
    diag->sysclk_hz = HAL_RCC_GetSysClockFreq();
    diag->pclk1_hz = HAL_RCC_GetPCLK1Freq();
}
