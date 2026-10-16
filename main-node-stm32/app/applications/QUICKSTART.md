# 快速开始指南

## 文件结构

```
applications/
├── main.c                 # 主程序入口 - 集成所有模块
├── can_receiver.h/c       # CAN接收模块（线程1）
├── mqtt_client.h/c        # WiFi/MQTT模块（线程2&3）
├── sd_storage.h/c         # SD卡存储模块（线程4）
└── README.md              # 详细文档
```

## 三个模块简介

### 模块1：CAN接收 (`can_receiver.h/c`)

```c
// 在 can_receiver.h 中修改目标CAN ID
#define CAN_ID_1 0x001    // 第一个CAN ID
#define CAN_ID_2 0x002    // 第二个CAN ID

// 使用示例
can_data_t data;
if (can_receiver_get_data(&data, 100) == 0) {
    printf("CAN ID: 0x%03X, Data: ", data.can_id);
    for (int i = 0; i < data.dlc; i++) {
        printf("%02X ", data.data[i]);
    }
}
```

**功能：**
- 监听CAN总线
- 接收指定ID的数据
- 消息队列缓存（最多32条）

---

### 模块2：WiFi/MQTT (`mqtt_client.h/c`)

```c
// 在 main.c 中配置
mqtt_config_t mqtt_config = {
    .wifi_ssid = "YOUR_SSID",              // 修改WiFi名称
    .wifi_password = "YOUR_PASSWORD",      // 修改WiFi密码
    .mqtt_broker = "broker.address",       // MQTT服务器地址
    .mqtt_port = 1883,                     // MQTT端口
    .mqtt_client_id = "stm32_device",      // 客户端ID
    .mqtt_publish_topic = "stm32/data"     // 发布主题
};

// 特性：
// - AT指令驱动ESP8266
// - 自动WiFi连接
// - MQTT数据上传
// - WiFi断开时触发SD备份
// - WiFi监控线程自动重连
```

**功能：**
- ESP8266 WiFi连接
- MQTT协议支持
- 自动断线重连
- JSON格式数据上传

---

### 模块3：SD卡存储 (`sd_storage.h/c`)

```c
// SD卡数据结构
typedef struct {
    rt_uint32_t can_id;        // CAN ID
    rt_uint8_t data[8];        // 数据
    rt_uint8_t dlc;            // 数据长度
    rt_uint32_t timestamp;     // 时间戳
} sd_storage_record_t;

// 使用示例
sd_storage_record_t record;
record.can_id = 0x123;
rt_memcpy(record.data, buffer, 8);
sd_storage_save_record(&record);

// 文件位置：
// /sd/can_data.bin  - CAN数据（二进制）
// /sd/system.log    - 系统日志
```

**功能：**
- SD卡挂载/卸载
- 缓冲区管理（256条记录）
- 自动刷新到SD卡
- 网络断开时自动备份

---

## 数据流程

```
CAN总线
  ↓
can_receiver (线程1)
  ↓
data_forward (主线程)
  ├─ WiFi OK? → mqtt_client (线程2) → 云端
  └─ WiFi NO? → sd_storage (线程3/4) → SD卡
```

---

## 快速配置步骤

### 1️⃣ 配置WiFi和MQTT信息

编辑 `main.c` 约第32-40行：

```c
static mqtt_config_t mqtt_config = {
    .wifi_ssid = "家里的WiFi",              // ← 改这里
    .wifi_password = "WiFi密码",            // ← 改这里
    .mqtt_broker = "test.mosquitto.org",    // ← 改MQTT服务器
    .mqtt_port = 1883,
    .mqtt_client_id = "my_stm32",           // ← 改客户端ID
    .mqtt_publish_topic = "home/can/data"   // ← 改发布主题
};
```

### 2️⃣ 配置CAN ID

编辑 `can_receiver.h` 约第25-26行：

```c
#define CAN_ID_1 0x001    // ← 改为你的ID1
#define CAN_ID_2 0x002    // ← 改为你的ID2
```

### 3️⃣ 检查UART配置（如需更改）

编辑 `mqtt_client.c` 约第127行：

```c
mqtt_client.uart_dev = rt_device_find("uart2");  // ← 可改为uart1或其他
```

---

## 编译和测试

```bash
# 在RT-Thread Studio中
1. 选择项目 → 右键 → Build Project
2. 或使用命令行: scons

# 烧写
1. 连接开发板
2. 点击 Debug 或使用烧写工具

# 监控
1. 打开串口监视器（115200波特率）
2. 查看启动日志
```

---

## 运行时的日志输出示例

```
==================================================
STM32F407ZGT6 Application Started
==================================================
Initializing CAN receiver module...
CAN receiver module initialized
Initializing MQTT client module...
MQTT client module initialized
Initializing SD storage module...
SD storage module initialized
Starting CAN receiver thread...
Starting MQTT client thread...
Starting SD storage thread...
Starting data forward thread...
==================================================
All modules started successfully!
Application running, waiting for CAN data...
==================================================

[data_forward] Received CAN data - ID: 0x001, DLC: 8
[can_receiver] CAN ID: 0x001, DLC: 8
[mqtt_client] CAN data published to MQTT successfully
```

---

## 故障排查

| 问题 | 解决方案 |
|------|--------|
| CAN无法接收数据 | 检查CAN ID配置，确认硬件连接 |
| WiFi连接失败 | 检查SSID/密码，确认ESP8266连接 |
| SD卡无法写入 | 检查SD卡是否插入，SDIO驱动是否正确 |
| MQTT连接失败 | 检查broker地址/端口，网络连接 |
| 日志显示异常 | 查看详细日志级别，检查rtconfig.h配置 |

---

## 重要提示

⚠️ **ESP8266 UART连接：**
- 确保ESP8266与STM32的UART连接正确
- ESP8266波特率默认115200
- 需要3.3V电源（不能直接用5V）

⚠️ **WiFi连接：**
- WiFi监控线程每5秒检查一次连接状态
- 网络断开时会自动重连（最多3次，间隔递增）

⚠️ **SD卡：**
- 缓冲区满时会丢弃新数据
- 需要FAT32格式的SD卡

---

## 数据格式参考

### MQTT发布格式（JSON）：
```json
{
  "can_id": "0x001",
  "data": [0x12, 0x34, 0x56, 0x78, 0xAB, 0xCD, 0xEF, 0x00]
}
```

### SD卡存储格式（二进制）：
```
结构体: sd_storage_record_t
├── can_id (4字节)
├── data[8] (8字节)
├── dlc (1字节)
└── timestamp (4字节)
总计: 17字节/条记录
```

---

## 更多信息

详见 `README.md` 文件获取完整文档。
