#ifndef __CAN_PROTOCOL_H__
#define __CAN_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * 节点2 CAN 协议定义（与主节点 can_receiver.h 的白名单一一对应）
 *
 *   ID 规划：节点1 用了 0x100~0x103 / 0x200 / 0x456，节点2 往后排
 *     0x110  状态帧   1Hz       节点2 -> 主节点
 *     0x111  事件帧   触发即发   节点2 -> 主节点
 *     0x112  故障帧   1Hz       节点2 -> 主节点
 *     0x210  命令帧   下行       主节点 -> 节点2（预留）
 *
 *   帧格式与节点1 完全同构：多字节字段小端，B0 = 节点ID，
 *   这样主节点以后要解析时，两个节点能共用一套解码代码。
 *
 *   0x110 状态：B0=02 | B1=状态位 | B2-3=振动强度(0~1000) | B4-5=光电次数 | B6-7=振动次数
 *   0x111 事件：B0=02 | B1=事件类型 | B2-3=附带值 | B4-7=时间戳ms
 *   0x112 故障：B0=02 | B1=故障位 | B2-3=光电失败 | B4-5=振动失败 | B6-7=运行秒
 *
 *   硬件：光电 E3F-DS30C4 -> PB0，振动 SW-420 -> PB1，都是开关量，
 *         所以帧里没有真正的物理量，只有状态和计数。
 * ========================================================================== */

#define NODE2_ID                0x02

#define CAN_ID_NODE2_STATUS     0x110
#define CAN_ID_NODE2_EVENT      0x111
#define CAN_ID_NODE2_FAULT      0x112
#define CAN_ID_CMD_NODE2        0x210

/* 0x110 状态帧 B1：状态位 */
#define N2_ST_PHOTO1            0x01    /* 光电检测到物体 */
#define N2_ST_PHOTO2            0x02    /* 第二路光电（预留，本工程只装了 1 路） */
#define N2_ST_VIB_ALARM         0x04    /* 振动报警中 */
#define N2_ST_CALIBRATED        0x08    /* 已标定（开关量用不上，预留） */

/* 0x111 事件帧 B1：事件类型 */
#define N2_EVT_PHOTO            0x01    /* 光电触发（取/放货、越界） */
#define N2_EVT_VIB              0x02    /* 一次振动过程结束 */

/* 0x112 故障帧 B1：故障位 */
#define N2_FAULT_PHOTO          0x01    /* 光电长时间卡在有效电平（疑似损坏/被挡死） */
#define N2_FAULT_VIB            0x02    /* 振动长时间卡在有效电平（灵敏度电位器调太高） */

/* 0x210 命令帧 B0：命令字（预留） */
#define N2_CMD_SET_CONFIG       0x01    /* 设置参数（灵敏度等） */

/* 上行数据：由传感器任务填写，CAN 任务只读。
 * 各字段单写者、uint16/uint8 在 Cortex-M4 上是对齐的单次访问，不会读到撕裂值。
 *
 * 注意 E3F-DS30C4 和 SW-420 都是开关量传感器，给不出物理量：
 *   vib_level 是"1 秒窗口内振动有效电平的占比 x1000"（0~1000），
 *   不是加速度 RMS。想要真正的 g 值得换成 MPU6050 那类 I2C 加速度计。 */
typedef struct
{
    volatile uint8_t  photo_bits;       /* N2_ST_PHOTO1 / N2_ST_PHOTO2 的组合 */
    volatile uint8_t  vib_alarm;        /* 0/1 */
    volatile uint8_t  calibrated;       /* 0/1（预留） */
    volatile uint8_t  fault_bits;       /* N2_FAULT_* 的组合 */
    volatile uint16_t vib_level;        /* 振动强度指标 0~1000 */
    volatile uint16_t photo_count;      /* 累计光电触发次数 */
    volatile uint16_t vib_count;        /* 累计振动事件次数 */
    volatile uint16_t photo_fails;      /* 预留：开关量没有读失败，恒为 0 */
    volatile uint16_t vib_fails;        /* 预留：同上 */
} node2_data_t;

extern node2_data_t g_node2;

/* 上报帧发送（can_task.c 实现） */
bool can_proto_send_status(void);
bool can_proto_send_event(uint8_t evt_type, uint16_t value);
bool can_proto_send_fault(void);

/* 事件投递：传感器任务检测到事件时调用（非阻塞，队列满则丢弃）
 *   N2_EVT_PHOTO -> value 忽略（固定传 0）
 *   N2_EVT_VIB   -> value = 本次振动的强度峰值（0~1000） */
void can_proto_post_event(uint8_t evt_type, uint16_t value);

#endif /* __CAN_PROTOCOL_H__ */

