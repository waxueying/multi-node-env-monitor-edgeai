#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ===========================================================================
 * 本节点 CAN 帧协议（应用层）：帧 ID 定义 + 打包 / 解析
 *
 * 分层约定：
 *   CAN.c          总线层——初始化 / 收发 / bus-off 恢复，不关心字节含义
 *   can_protocol.c 协议层——本文件，只决定"字节长什么样"
 *   main.c         应用层——决定"什么时候发"（周期上报 / 事件触发）与日志文案
 *
 * 注意：本模块不负责调 can_init()，也不建立接收任务。
 *
 * ---- 帧规划 ----
 * 0x100 结构状态帧（1Hz 周期上报）
 *   B0      节点ID
 *   B1      状态位：bit0=基准已标定，bit1=倾斜报警
 *   B2~B3   倾角 ×10（int16 小端，0.1°）
 *   B4~B5   振动RMS ×1000（uint16 小端，mg）
 *   B6~B7   累计撞击次数（uint16 小端）
 *
 * 0x101 撞击事件帧（事件触发上报）
 *   B0      节点ID
 *   B1      事件类型：1 = 撞击
 *   B2~B3   加速度峰值 ×100（uint16 小端，0.01g）
 *   B4~B7   时间戳（uint32 小端，ms，自启动）
 *
 * 0x102 故障上报帧（1Hz 周期上报）
 *   B0      节点ID
 *   B1      故障标志：bit0=MPU6050 故障，bit1=DHT11 故障
 *   B2~B3   MPU6050 累计读取失败次数（uint16 小端）
 *   B4~B5   DHT11 累计读取失败次数（uint16 小端）
 *   B6~B7   节点运行时长（秒，uint16 小端，18.2 小时后回绕）
 *
 * 0x103 AI 推理结果帧（AI 判定为事件时触发上报）
 *   B0      节点ID
 *   B1      类别：0 = idle，1 = impact，2 = push
 *   B2~B3   置信度 ×1000（uint16 小端，0~1000，受 int8 量化上限约束最大 996）
 *   B4~B5   触发峰值 ×100（uint16 小端，0.01g）
 *   B6~B7   事件序号（uint16 小端，节点上电后自增）
 *
 * 0x456 环境数据帧（1Hz 周期上报）
 *   B0~B1   温度 ×10（int16 小端，℃）
 *   B2~B3   湿度 ×10（int16 小端，%RH）
 *   B4      环境报警位：bit0=温度高, bit1=温度低, bit2=湿度高, bit3=湿度低
 *   B5      当前环境模式（0~4）
 *   B6~B7   保留（0x00）
 *
 * 0x200 环境模式切换命令（主节点 → 本节点）/ 回执
 *   下行 B0  命令码：0x01 = 设置环境模式
 *        B1  目标模式号（0~4）
 *   回执 B0  当前模式号
 *        B1  结果：0=成功，1=失败
 * ========================================================================== */

/* ---- 帧 ID ---- */
#define CAN_ID_STRUCT_STATUS    0x100
#define CAN_ID_STRUCT_IMPACT    0x101
#define CAN_ID_FAULT_REPORT     0x102
#define CAN_ID_AI_RESULT        0x103
#define CAN_ID_CMD_ENV_PROFILE  0x200
#define CAN_ID_ENV_DATA         0x456

/* 0x102 帧 B1 故障标志位 */
#define CAN_FAULT_BIT_IMU       0x01    /* MPU6050 故障 */
#define CAN_FAULT_BIT_DHT       0x02    /* DHT11 故障 */

/* 0x200 帧 B0 命令码 */
#define CMD_SET_ENV_PROFILE     0x01

/* 初始化协议层：记录本节点 ID，并把接收回调挂到总线层。
 * 必须在 can_init() 之后、创建 can_receive_task 之前调用 */
void can_proto_init(uint8_t node_id);

/* 发送环境数据帧 0x456。内部自动读取 environment_monitor 的报警位与模式号。
 * 返回 true = 发送成功 */
bool can_proto_send_env(float temperature, float humidity);

/* 发送结构状态帧 0x100。内部自动读取 structure_monitor 当前状态。
 * 返回 true = 发送成功 */
bool can_proto_send_struct_status(void);

/* 发送撞击事件帧 0x101。内部自动读取撞击峰值，时间戳取自 esp_timer。
 * 返回 true = 发送成功 */
bool can_proto_send_impact(void);

/* 发送 AI 推理结果帧 0x103。类别 / 置信度 / 峰值 / 序号由应用层从
 * ai_result_t 取出后传入（本协议层不依赖推理层）。返回 true = 发送成功 */
bool can_proto_send_ai(int cls, float conf, float peak_g, uint16_t seq);

/* 发送故障上报帧 0x102。故障标志与失败计数由应用层统计后传入，
 * 运行时长由本函数内部取 esp_timer。返回 true = 发送成功 */
bool can_proto_send_fault(uint8_t fault_bits, uint16_t imu_fails, uint16_t dht_fails);
