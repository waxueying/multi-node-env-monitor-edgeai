# 速查表 (Cheat Sheet)

## 🔧 快速配置

### 1. WiFi 和 MQTT 配置 (main.c 第32-40行)

```c
mqtt_config_t mqtt_config = {
    .wifi_ssid = "家里的WiFi",           // ← 改这里
    .wifi_password = "WiFi密码",          // ← 改这里
    .mqtt_broker = "test.mosquitto.org",  // ← MQTT服务器
    .mqtt_port = 1883,                    // ← MQTT端口
    .mqtt_client_id = "stm32_001",        // ← 设备ID
    .mqtt_publish_topic = "home/data"     // ← 发布主题
};
```

### 2. CAN ID 配置 (can_receiver.h 第25-26行)

```c
#define CAN_ID_1 0x001    // ← 改你的ID1
#define CAN_ID_2 0x002    // ← 改你的ID2
```

### 3. UART 配置 (mqtt_client.c 第127行)

```c
mqtt_client.uart_dev = rt_device_find("uart2");  // ← uart1/uart2/...
```

---

## 📖 常用 API

### CAN 接收

```c
// 初始化和启动
can_receiver_init();
can_receiver_start();

// 获取数据
can_data_t data;
can_receiver_get_data(&data, 100);  // 等待100ms

// 检查状态
rt_uint32_t count = can_receiver_get_count();
```

### WiFi/MQTT

```c
// 初始化和启动
mqtt_client_init(&mqtt_config);
mqtt_client_start();

// 发送数据
mqtt_client_publish_can_data(0x001, buffer, 8);

// 检查状态
if (mqtt_client_is_wifi_connected()) { }
mqtt_status_t status = mqtt_client_get_status();
```

### SD 卡存储

```c
// 初始化和启动
sd_storage_init();
sd_storage_start();

// 保存数据
sd_storage_record_t rec = {
    .can_id = 0x001,
    .dlc = 8,
    .timestamp = rt_tick_get()
};
rt_memcpy(rec.data, buffer, 8);
sd_storage_save_record(&rec);

// 检查状态
rt_uint32_t pending = sd_storage_get_buffer_count();
```

---

## 🎯 常见任务

### 发送一条CAN数据到MQTT

```c
can_data_t data = { /* 初始化 */ };
mqtt_client_publish_can_data(data.can_id, data.data, data.dlc);
```

### 保存一条CAN数据到SD卡

```c
sd_storage_record_t rec;
rec.can_id = data.can_id;
rec.dlc = data.dlc;
rec.timestamp = rt_tick_get();
rt_memcpy(rec.data, data.data, data.dlc);
sd_storage_save_record(&rec);
```

### 检查WiFi连接

```c
if (mqtt_client_is_wifi_connected()) {
    printf("WiFi connected\n");
}
```

### 读取SD卡中的数据

```c
sd_storage_record_t records[50];
int count = sd_storage_read_records(records, 50);
```

---

## ⚡ 线程优先级

| 线程名称 | 优先级 | 说明 |
|---------|--------|------|
| can_rx | 15 | CAN接收（最高） |
| mqtt_thread | 12 | MQTT处理 |
| wifi_monitor | 13 | WiFi监控 |
| sd_storage | 14 | SD存储 |
| data_forward | 11 | 数据转发 |

---

## 📊 数据结构

### CAN 数据
```c
typedef struct {
    rt_uint32_t can_id;        // CAN ID
    rt_uint8_t data[8];        // 数据 (0-8字节)
    rt_uint8_t dlc;            // 数据长度码
    rt_uint32_t timestamp;     // 时间戳(ms)
} can_data_t;
```

### SD 记录
```c
typedef struct {
    rt_uint32_t can_id;        // CAN ID
    rt_uint8_t data[8];        // 数据
    rt_uint8_t dlc;            // 数据长度
    rt_uint32_t timestamp;     // 时间戳
} sd_storage_record_t;
```

---

## 🔌 硬件连接

### ESP8266 → STM32

```
ESP8266                STM32F407
  TX  ───────────────► RX2 (PD6)
  RX  ◄───────────────  TX2 (PD5)
  GND ───────────────► GND
  VCC ───────────────► 3.3V
```

### 注意：需要 3.3V 电平转换！

---

## 🔍 日志输出

### 启用详细日志 (rtconfig.h)
```c
#define RT_USING_DEBUG
#define DBG_LOG
```

### 调整日志级别
在各源文件开头：
```c
#define DBG_LVL DBG_LOG      // 改为 DBG_INFO, DBG_WARNING, DBG_ERROR
```

### 查看日志
使用串口工具（115200 8N1）连接到 UART1

---

## 🐛 故障排查快速表

| 问题 | 可能原因 | 解决方案 |
|------|--------|--------|
| CAN无数据 | ID配置错误 | 检查 CAN_ID_1/2 定义 |
| WiFi连接失败 | SSID/密码错误 | 检查配置 |
| MQTT不上线 | 网络/broker问题 | 检查地址和端口 |
| SD卡无法写 | 未挂载 | 检查SD卡/SDIO驱动 |
| 编译错误 | 文件路径/头文件 | 查看 COMPILE.md |

---

## 💡 调试技巧

### 监测缓冲区状态
```c
printf("CAN Q: %d, SD Buf: %d\n", 
       can_receiver_get_count(),
       sd_storage_get_buffer_count());
```

### 检查所有模块状态
```c
printf("CAN: %d, MQTT: %d, SD: %d\n",
       can_receiver_get_count(),
       mqtt_client_get_status(),
       sd_storage_get_buffer_count());
```

### 强制保存SD数据
```c
// 定期检查缓冲区是否过大
if (sd_storage_get_buffer_count() > 128) {
    // 缓冲区即将满，可考虑主动处理
}
```

---

## 📱 MQTT 消息格式

### 发布格式 (JSON)
```json
{
  "can_id": "0x001",
  "data": [0x12, 0x34, 0x56, 0x78]
}
```

### 订阅示例 (MQTTX)
```
Topic: stm32/can/data
QoS: 1
```

---

## 🔐 重要设置

### 缓冲区大小
```c
#define CAN_RX_QUEUE_SIZE 32      // CAN队列
#define SD_BUFFER_SIZE 256        // SD缓冲
```

### 超时时间
```c
#define MQTT_RECONNECT_TIMEOUT 30 // 重连超时(秒)
#define MQTT_CONNECT_TIMEOUT 10   // 连接超时(秒)
```

### 监控周期
```c
wifi_monitor_thread: 5000ms      // 5秒检查WiFi
sd_storage_thread: 10000ms       // 10秒刷新SD
```

---

## 🎯 最小化配置

只需修改这 3 个地方即可运行：

**1. main.c:**
```c
.wifi_ssid = "YOUR_SSID";
.wifi_password = "PASSWORD";
```

**2. can_receiver.h:**
```c
#define CAN_ID_1 0x001
```

**3. mqtt_client.c:**
```c
mqtt_client.uart_dev = rt_device_find("uart2");
```

完成！

---

## 📋 编译命令

```bash
# 编译
scons -j4

# 清理
scons --clean

# 重新编译
scons -f

# 并行编译（快速）
scons -j8
```

---

## 🚀 启动步骤

```bash
1. 修改配置 (见上方)
2. scons -j4        # 编译
3. 烧写到开发板
4. 连接串口看输出
5. 测试各功能
```

---

## 📞 快速参考

| 需要 | 文件 |
|------|------|
| 快速开始 | QUICKSTART.md |
| API 详情 | API_REFERENCE.md |
| 编译方法 | COMPILE.md |
| 完整文档 | README.md |
| 项目信息 | SUMMARY.md |
| 问题导航 | INDEX.md |

---

## ⏱️ 时间参考

| 任务 | 预计时间 |
|------|----------|
| 阅读文档 | 30分钟 |
| 修改配置 | 5分钟 |
| 编译项目 | 2分钟 |
| 烧写程序 | 1分钟 |
| 首次测试 | 10分钟 |
| **总计** | **~50分钟** |

---

**💡 提示：** 保存此页面为书签，以便快速参考！

**最后更新：** 2025-12-02
