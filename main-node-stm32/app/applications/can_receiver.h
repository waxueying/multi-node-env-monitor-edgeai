/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RT-Thread    CAN receiver module header
 */

#ifndef __CAN_RECEIVER_H__
#define __CAN_RECEIVER_H__

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CAN接收数据结构体 */
typedef struct {
    rt_uint32_t can_id;              /* CAN ID */
    rt_uint8_t data[8];              /* 数据 */
    rt_uint8_t dlc;                  /* 数据长度 */
    rt_uint32_t timestamp;           /* 时间戳 */
} can_data_t;

/* CAN接收队列大小 */
#define CAN_RX_QUEUE_SIZE 32

/* "最近收到的帧"环形缓冲大小：OLED 用它显示 CAN 通讯内容 */
#define CAN_RECENT_MAX 8

/* 节点1 上报帧 ID（与节点1 can_protocol.h 保持一致） */
#define CAN_ID_STRUCT_STATUS    0x100  /* 结构状态帧（1Hz） */
#define CAN_ID_STRUCT_IMPACT    0x101  /* 撞击事件帧 */
#define CAN_ID_FAULT_REPORT     0x102  /* 故障上报帧（1Hz） */
#define CAN_ID_AI_RESULT        0x103  /* AI 推理结果帧 */
#define CAN_ID_CMD_ENV_PROFILE  0x200  /* 环境模式切换命令 / 回执 */
#define CAN_ID_ENV_DATA         0x456  /* 环境数据帧（1Hz） */

/* 节点2 上报帧 ID（与节点2 can_protocol.h 保持一致） */
#define CAN_ID_NODE2_STATUS     0x110  /* 结构状态帧（1Hz） */
#define CAN_ID_NODE2_EVENT      0x111  /* 事件帧（光电/振动触发即发） */
#define CAN_ID_NODE2_FAULT      0x112  /* 故障帧（1Hz） */

/* 节点1/节点2 上报帧 ID 表，下标即统计数组下标 */
#define CAN_RX_ID_COUNT         9
extern const rt_uint32_t can_rx_id_table[CAN_RX_ID_COUNT];

/* CAN 接收统计快照，供 OLED 显示 / 调试查看 */
typedef struct {
    rt_bool_t   dev_ready;              /* CAN 设备是否打开成功 */
    rt_bool_t   has_last;               /* 是否已收到过帧 */
    rt_bool_t   has_ai;                 /* 是否已收到过 0x103 AI 结果帧 */
    rt_uint32_t total;                  /* 累计收到且 ID 命中 can_rx_id_table 的帧数 */
    rt_uint32_t raw_total;              /* 累计收上来的帧数（含 ID 不在表内的） */
    rt_uint32_t unknown;                /* 其中 ID 不在 can_rx_id_table 里的帧数 */
    rt_uint32_t isr_frames;             /* 驱动 RX 中断回调次数：>0 说明帧真进了 MCU 接收路径 */
    rt_uint32_t cnt[CAN_RX_ID_COUNT];   /* 各 ID 累计帧数，顺序同 can_rx_id_table */
    can_data_t  last;                   /* 最近一帧的原始数据 */
    can_data_t  last_ai;                /* 最近一帧 0x103（不被别的 ID 覆盖，供显示用） */
    can_data_t  last_env;               /* 最近一帧 0x456（环境数据），待机页显示温湿度 */
    can_data_t  last_struct;            /* 最近一帧 0x100（结构状态），待机页显示倾角/振动/撞击 */
    rt_bool_t   has_env;                /* 是否收到过 0x456 */
    rt_bool_t   has_struct;             /* 是否收到过 0x100 */
} can_receiver_stats_t;

/* CAN 控制器硬件诊断快照：一帧都收不到时，用它区分三种情况
 *   1) 总线静默（REC/TEC/LEC 全 0 且 RF0R 无待读帧）-> 物理层不通，或对端没在发
 *   2) 帧到了但没收进软件 FIFO（RF0R 有待读帧 / 溢出）-> 中断或读取路径的问题
 *   3) 总线有错误（REC/TEC 上涨、LEC 非 0）-> 波特率、采样点或接线问题
 */
typedef struct {
    rt_uint32_t esr;            /* CAN1->ESR：bit23:16 TEC，bit31:24 REC，bit6:4 LEC */
    rt_uint32_t rf0r;           /* CAN1->RF0R：bit1:0 FMP0 待读帧数，bit4 FOVR0 溢出标志 */
    rt_uint32_t msr;            /* CAN1->MSR：bit0 INAK 卡在初始化模式，bit1 SLK 睡眠，bit9 RXM，bit11 RX */
    rt_uint32_t btr;            /* CAN1->BTR：bit9:0 预分频-1，bit19:16 BS1-1，bit22:20 BS2-1，bit31:30 SILM/LBKM */
    rt_uint32_t sysclk_hz;      /* 实际 SYSCLK */
    rt_uint32_t pclk1_hz;       /* 实际 APB1 时钟（drv_can.c 波特率表以 36MHz 为基准） */
} can_bus_diag_t;

/**
 * 初始化CAN接收模块
 * @return 0 成功，其他值失败
 */
int can_receiver_init(void);

/**
 * 发送一帧标准帧 CAN 数据（下行命令用）
 * @param id   标准帧 ID（11 位）
 * @param data 数据指针
 * @param len  数据长度（0~8）
 * @return RT_EOK 成功，-RT_ERROR 失败
 * @note 必须在接收线程把设备打开成功之后才能用；
 *       发送走 RT-Thread CAN 框架的发送 FIFO 并等发送中断返回，
 *       总线上没有其它节点应答时会以失败返回（不会一直卡住）。
 */
rt_err_t can_receiver_send(rt_uint32_t id, const rt_uint8_t *data, rt_uint8_t len);

/**
 * 从接收缓冲中获取CAN数据
 * @param data 接收数据缓冲区指针
 * @param timeout 超时时间（毫秒）
 * @return 0 成功，其他值失败
 */
int can_receiver_get_data(can_data_t *data, rt_int32_t timeout);

/**
 * 启动CAN接收线程
 * @return 0 成功，其他值失败
 */
int can_receiver_start(void);

/**
 * 获取接收队列中的数据个数
 * @return 队列中的数据个数
 */
rt_uint32_t can_receiver_get_count(void);

/**
 * 获取最近收到的若干帧（最新的一帧在 out[0]，按时间倒序）
 * @param out 输出缓冲（至少能放 max 条）
 * @param max 最多取几条
 * @return 实际取到的条数（开机后不足 max 条时按实际数量返回）
 */
rt_uint32_t can_receiver_get_recent(can_data_t *out, rt_uint32_t max);

/**
 * 获取CAN接收统计快照（线程安全）
 * @param stats 输出缓冲区指针
 */
void can_receiver_get_stats(can_receiver_stats_t *stats);

/**
 * 获取 CAN 控制器硬件诊断快照（直接读寄存器，随时可调）
 * @param diag 输出缓冲区指针
 */
void can_receiver_get_diag(can_bus_diag_t *diag);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_RECEIVER_H__ */
