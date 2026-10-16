# STM32F407ZGT6 应用层模块文档

## 项目概述

本项目为STM32F407ZGT6微控制器添加了三个主要应用层模块，实现了CAN数据接收、WiFi/MQTT云连接和SD卡数据备份功能。

## 模块说明

### 1. CAN接收模块 (`can_receiver.c/h`)

**功能描述：**
- 支持接收两个不同的CAN ID数据（CAN_ID_1=0x001, CAN_ID_2=0x002）
- 使用消息队列缓存接收到的CAN数据
- 后台线程持续监听CAN总线

**主要函数：**
```c
int can_receiver_init(void);              // 初始化模块
int can_receiver_start(void);             // 启动接收线程
int can_receiver_get_data(can_data_t *data, rt_int32_t timeout);  // 获取数据
rt_uint32_t can_receiver_get_count(void); // 获取队列中数据个数
```

**修改说明：**
- 修改 `CAN_ID_1` 和 `CAN_ID_2` 定义为你的实际CAN ID
- 修改 `CAN_RX_QUEUE_SIZE` 调整缓冲区大小

---

### 2. WiFi/MQTT模块 (`mqtt_client.c/h`)

**功能描述：**
- 通过AT指令驱动ESP8266模块
- 自动连接配置的WiFi网络
- 建立MQTT连接到指定的broker
- 发送CAN数据到云端
- 自动检测WiFi连接状态，网络断开时触发本地备份

**主要函数：**
```c
int mqtt_client_init(mqtt_config_t *config);              // 初始化
int mqtt_client_start(void);                              // 启动服务
int mqtt_client_publish_can_data(rt_uint32_t can_id, 
                                  const rt_uint8_t *data, 
                                  rt_uint8_t len);        // 发布CAN数据
mqtt_status_t mqtt_client_get_status(void);               // 获取连接状态
rt_bool_t mqtt_client_is_wifi_connected(void);            // 检查WiFi连接
int mqtt_client_stop(void);                               // 停止服务
```

**关键配置（main.c中）：**
```c
mqtt_config.wifi_ssid = "your_wifi_name";           // 修改为你的WiFi名称
mqtt_config.wifi_password = "your_wifi_password";   // 修改为你的WiFi密码
mqtt_config.mqtt_broker = "your.mqtt.broker";       // MQTT broker地址
mqtt_config.mqtt_port = 1883;                       // MQTT端口
mqtt_config.mqtt_publish_topic = "stm32/can/data";  // 发布主题
```

**实现特性：**
- WiFi监控线程每5秒检查一次连接状态
- 网络断开时自动重连（指数退避算法，最长等待30秒）
- CAN数据以JSON格式发送：`{"can_id":"0x001","data":[0xXX,0xXX,...]}`

---

### 3. SD卡存储模块 (`sd_storage.c/h`)

**功能描述：**
- 自动挂载SD卡（如果存在）
- 数据缓冲机制，降低闪存写入频率
- WiFi断开时自动保存CAN数据到SD卡
- 支持批量读写操作

**主要函数：**
```c
int sd_storage_init(void);                          // 初始化
int sd_storage_start(void);                         // 启动线程
int sd_storage_save_record(sd_storage_record_t *record);  // 保存单条记录
int sd_storage_save_batch(sd_storage_record_t *records, 
                          rt_uint32_t count);       // 保存批量记录
int sd_storage_read_records(sd_storage_record_t *records, 
                            rt_uint32_t max_count); // 读取记录
sd_status_t sd_storage_get_status(void);            // 获取挂载状态
rt_bool_t sd_storage_is_mounted(void);              // 检查是否挂载
rt_uint32_t sd_storage_get_buffer_count(void);      // 获取缓冲区记录数
int sd_storage_stop(void);                          // 停止服务
```

**存储文件：**
- 数据文件：`/sd/can_data.bin` - 二进制格式存储CAN数据
- 日志文件：`/sd/system.log` - 系统日志（预留）

**缓冲机制：**
- 缓冲区大小：256条记录
- 缓冲区半满时自动刷新到SD卡
- 定期刷新（10秒）
- 程序停止时刷新所有待保存数据

---

## 数据流程图

```
┌─────────────┐
│ CAN总线     │
└──────┬──────┘
       │
       ▼
┌─────────────────────┐
│ can_receiver        │ ◄─ CAN接收线程
│ 消息队列(32条)      │
└────────┬────────────┘
         │
         ▼
┌──────────────────────────────┐
│ data_forward_thread          │ ◄─ 数据转发线程
│ (从CAN队列取数据)            │
└───┬──────────────┬───────────┘
    │              │
    ▼              ▼
  [WiFi连接？]  [保存到SD]
   ├─是 ──────►  MQTT发送到云端
   └─否 ────────► SD卡备份
                   ▼
              ┌──────────────┐
              │ SD卡存储线程 │
              │ 定期刷新数据 │
              └──────────────┘
```

---

## 线程说明

| 线程名称 | 优先级 | 功能 | 栈大小 |
|---------|--------|------|--------|
| can_rx | 15 | CAN数据接收 | 1024 |
| mqtt_thread | 12 | MQTT/WiFi处理 | 2048 |
| wifi_monitor | 13 | WiFi连接监控 | 1024 |
| sd_storage | 14 | SD卡数据处理 | 1024 |
| data_forward | 11 | 数据转发决策 | 1024 |

---

## 使用步骤

### 1. 硬件连接

**ESP8266连接：**
- TX ──► STM32 RX (UART2/UART1)
- RX ──► STM32 TX
- GND ──► GND
- VCC ──► 3.3V

**SD卡连接：** 
- 使用已配置的SDIO驱动

### 2. 修改配置

编辑 `main.c` 中的 `mqtt_config` 结构体：

```c
static mqtt_config_t mqtt_config = {
    .wifi_ssid = "YOUR_WIFI_SSID",
    .wifi_password = "YOUR_PASSWORD",
    .mqtt_broker = "test.mosquitto.org",  // 或你的MQTT服务器
    .mqtt_port = 1883,
    .mqtt_client_id = "stm32_client_001",
    .mqtt_publish_topic = "stm32/can/data"
};
```

### 3. 编译和烧写

```bash
# 编译项目
scons -j4

# 烧写到设备
# （使用你的烧写工具）
```

### 4. 监控运行状态

使用串口工具连接到UART1（波特率115200），观察系统输出：

```
==============================================
STM32F407ZGT6 Application Started
==============================================
Features:
  1. CAN Receiver with dual ID support
  2. WiFi/MQTT cloud connectivity
  3. SD card data backup on WiFi disconnect
==============================================
Initializing CAN receiver module...
CAN receiver module initialized
...
All modules started successfully!
Application running, waiting for CAN data...
```

---

## 常见问题

### Q1: 如何修改CAN ID？
在 `can_receiver.h` 中修改：
```c
#define CAN_ID_1 0x001  // 修改为你的ID
#define CAN_ID_2 0x002  // 修改为你的ID
```

### Q2: 如何使用不同的UART？
在 `mqtt_client.c` 中找到 UART 初始化代码，修改设备名称：
```c
mqtt_client.uart_dev = rt_device_find("uart2");  // 改为 uart1, uart3 等
```

### Q3: SD卡未挂载怎么办？
- 检查SD卡是否插入
- 检查SDIO驱动配置
- 查看调试输出中的错误信息

### Q4: MQTT连接失败？
- 检查WiFi是否连接成功
- 确认MQTT broker地址和端口正确
- 检查防火墙是否阻止1883端口

### Q5: 如何自定义MQTT消息格式？
修改 `mqtt_client.c` 中的 `mqtt_client_publish_can_data()` 函数

---

## 日志级别

支持的日志级别（在 rtconfig.h 中配置）：
- DBG_LOG - 详细日志（调试）
- DBG_INFO - 信息日志
- DBG_WARNING - 警告日志
- DBG_ERROR - 错误日志

编辑每个源文件顶部的 `DBG_LVL` 宏可调整该模块的日志级别。

---

## 性能指标

- CAN接收队列：32条消息
- SD缓冲区：256条记录（~8KB）
- WiFi监控周期：5秒
- SD自动刷新周期：10秒
- 最大MQTT连接重试：3次

---

## 许可证

Apache License 2.0

## 修改日期

- 2025-12-02：初始版本
