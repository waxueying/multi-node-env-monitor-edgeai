# API 参考手册

## CAN接收模块 (can_receiver.h/c)

### 数据结构

```c
typedef struct {
    rt_uint32_t can_id;              /* CAN标识符 */
    rt_uint8_t data[8];              /* CAN数据字节 */
    rt_uint8_t dlc;                  /* 数据长度码 (0-8) */
    rt_uint32_t timestamp;           /* 系统时间戳 (ms) */
} can_data_t;
```

### 常量

```c
#define CAN_RX_QUEUE_SIZE 32         /* 接收队列容量 */
#define CAN_ID_1 0x001               /* 目标CAN ID 1 */
#define CAN_ID_2 0x002               /* 目标CAN ID 2 */
```

### 函数

#### `int can_receiver_init(void)`

**功能：** 初始化CAN接收模块

**参数：** 无

**返回值：**
- `0` - 成功
- `-1` - 失败（创建消息队列失败）

**说明：** 必须在启动接收线程前调用。创建接收消息队列。

**示例：**
```c
if (can_receiver_init() != 0) {
    printf("Failed to init CAN receiver\n");
    return -1;
}
```

---

#### `int can_receiver_start(void)`

**功能：** 启动CAN接收线程

**参数：** 无

**返回值：**
- `0` - 成功
- `-1` - 失败（创建或启动线程失败）

**说明：** 创建并启动后台接收线程。线程将持续监听CAN总线。

**示例：**
```c
if (can_receiver_start() != 0) {
    printf("Failed to start CAN receiver\n");
    return -1;
}
```

---

#### `int can_receiver_get_data(can_data_t *data, rt_int32_t timeout)`

**功能：** 从接收队列获取一条CAN数据

**参数：**
- `data` - 指向接收数据缓冲的指针（不能为NULL）
- `timeout` - 超时时间，单位毫秒
  - `RT_WAITING_FOREVER` 表示无限等待
  - `0` 表示不等待，立即返回
  - 正数表示等待的毫秒数

**返回值：**
- `0` - 成功获取数据
- `-1` - 失败或超时

**说明：** 如果队列中有数据立即返回，否则等待。

**示例：**
```c
can_data_t data;
int ret = can_receiver_get_data(&data, 100);  // 等待100ms

if (ret == 0) {
    printf("CAN ID: 0x%03X, DLC: %d, Data: ", data.can_id, data.dlc);
    for (int i = 0; i < data.dlc; i++) {
        printf("%02X ", data.data[i]);
    }
    printf("\n");
}
```

---

#### `rt_uint32_t can_receiver_get_count(void)`

**功能：** 获取接收队列中的数据个数

**参数：** 无

**返回值：** 当前队列中的数据条数（0-32）

**说明：** 可用于监测缓冲状态。

**示例：**
```c
rt_uint32_t count = can_receiver_get_count();
if (count > 20) {
    printf("Warning: CAN buffer nearly full (%d)\n", count);
}
```

---

## MQTT客户端模块 (mqtt_client.h/c)

### 数据结构

```c
typedef enum {
    MQTT_STATUS_DISCONNECTED = 0,    /* 未连接 */
    MQTT_STATUS_CONNECTING,          /* 连接中 */
    MQTT_STATUS_CONNECTED,           /* 已连接 */
    MQTT_STATUS_RECONNECTING         /* 重新连接中 */
} mqtt_status_t;

typedef struct {
    const char *wifi_ssid;           /* WiFi网络名称 */
    const char *wifi_password;       /* WiFi密码 */
    const char *mqtt_broker;         /* MQTT broker地址 */
    rt_uint16_t mqtt_port;           /* MQTT port (通常1883) */
    const char *mqtt_client_id;      /* MQTT客户端ID */
    const char *mqtt_user;           /* MQTT用户名 */
    const char *mqtt_password;       /* MQTT密码 */
    const char *mqtt_publish_topic;  /* MQTT发布主题 */
} mqtt_config_t;

typedef struct {
    mqtt_config_t config;
    mqtt_status_t status;
    /* 内部字段... */
} mqtt_client_t;
```

### 常量

```c
#define MQTT_THREAD_PRIORITY 12      /* MQTT线程优先级 */
#define MQTT_RECONNECT_TIMEOUT 30    /* 重连超时时间(秒) */
#define MQTT_CONNECT_TIMEOUT 10      /* 连接超时时间(秒) */
```

### 函数

#### `int mqtt_client_init(mqtt_config_t *config)`

**功能：** 初始化MQTT客户端模块

**参数：**
- `config` - 指向MQTT配置结构的指针

**返回值：**
- `0` - 成功
- `-1` - 失败

**说明：** 设置WiFi和MQTT参数。必须在启动服务前调用。

**示例：**
```c
mqtt_config_t cfg = {
    .wifi_ssid = "MyWiFi",
    .wifi_password = "12345678",
    .mqtt_broker = "test.mosquitto.org",
    .mqtt_port = 1883,
    .mqtt_client_id = "stm32_001",
    .mqtt_user = "",
    .mqtt_password = "",
    .mqtt_publish_topic = "home/sensor/data"
};

if (mqtt_client_init(&cfg) != 0) {
    printf("MQTT init failed\n");
    return -1;
}
```

---

#### `int mqtt_client_start(void)`

**功能：** 启动MQTT服务（创建处理线程）

**参数：** 无

**返回值：**
- `0` - 成功
- `-1` - 失败

**说明：** 启动MQTT和WiFi监控线程。

**示例：**
```c
if (mqtt_client_start() != 0) {
    printf("Failed to start MQTT\n");
    return -1;
}
```

---

#### `int mqtt_client_publish(const char *topic, const void *payload, rt_size_t payload_len)`

**功能：** 发布任意消息到MQTT

**参数：**
- `topic` - 发布主题
- `payload` - 消息内容指针
- `payload_len` - 消息长度

**返回值：**
- `0` - 成功
- `-1` - 失败（未连接或参数错误）

**说明：** 仅在连接状态下有效。

**示例：**
```c
const char *message = "Hello MQTT";
mqtt_client_publish("test/topic", message, strlen(message));
```

---

#### `int mqtt_client_publish_can_data(rt_uint32_t can_id, const rt_uint8_t *data, rt_uint8_t len)`

**功能：** 将CAN数据发送到MQTT（自动JSON格式化）

**参数：**
- `can_id` - CAN标识符
- `data` - 数据指针
- `len` - 数据长度（1-8）

**返回值：**
- `0` - 成功
- `-1` - 失败

**说明：** 自动转换为JSON格式发送。

**发送格式：**
```json
{
  "can_id": "0x001",
  "data": [0x12, 0x34, 0x56, 0x78]
}
```

**示例：**
```c
rt_uint8_t can_data[8] = {0x12, 0x34, 0x56, 0x78, 0xAA, 0xBB, 0xCC, 0xDD};
mqtt_client_publish_can_data(0x001, can_data, 8);
```

---

#### `mqtt_status_t mqtt_client_get_status(void)`

**功能：** 获取MQTT连接状态

**参数：** 无

**返回值：**
```c
MQTT_STATUS_DISCONNECTED   // 0 - 未连接
MQTT_STATUS_CONNECTING     // 1 - 连接中
MQTT_STATUS_CONNECTED      // 2 - 已连接
MQTT_STATUS_RECONNECTING   // 3 - 重新连接中
```

**示例：**
```c
mqtt_status_t status = mqtt_client_get_status();
if (status == MQTT_STATUS_CONNECTED) {
    printf("MQTT is connected\n");
}
```

---

#### `rt_bool_t mqtt_client_is_wifi_connected(void)`

**功能：** 检查是否已连接到WiFi

**参数：** 无

**返回值：**
- `RT_TRUE` (1) - 已连接
- `RT_FALSE` (0) - 未连接

**示例：**
```c
if (mqtt_client_is_wifi_connected()) {
    printf("WiFi connected\n");
} else {
    printf("WiFi not connected\n");
}
```

---

#### `int mqtt_client_stop(void)`

**功能：** 停止MQTT服务

**参数：** 无

**返回值：**
- `0` - 成功
- `-1` - 失败

**示例：**
```c
mqtt_client_stop();
```

---

## SD卡存储模块 (sd_storage.h/c)

### 数据结构

```c
typedef struct {
    rt_uint32_t can_id;              /* CAN标识符 */
    rt_uint8_t data[8];              /* 数据字节 */
    rt_uint8_t dlc;                  /* 数据长度 */
    rt_uint32_t timestamp;           /* 时间戳 */
} sd_storage_record_t;

typedef enum {
    SD_STATUS_UNMOUNTED = 0,         /* 未挂载 */
    SD_STATUS_MOUNTED,               /* 已挂载 */
    SD_STATUS_ERROR                  /* 错误 */
} sd_status_t;
```

### 常量

```c
#define SD_MOUNT_POINT "/sd"         /* 挂载点 */
#define SD_DATA_FILE "/sd/can_data.bin"  /* 数据文件 */
#define SD_LOG_FILE "/sd/system.log"     /* 日志文件 */
#define SD_BUFFER_SIZE 256           /* 缓冲区大小 */
```

### 函数

#### `int sd_storage_init(void)`

**功能：** 初始化SD卡存储模块

**参数：** 无

**返回值：**
- `0` - 成功
- `-1` - 失败

**说明：** 创建互斥锁和信号量。必须在启动线程前调用。

**示例：**
```c
if (sd_storage_init() != 0) {
    printf("Failed to init SD storage\n");
    return -1;
}
```

---

#### `int sd_storage_start(void)`

**功能：** 启动SD卡存储线程

**参数：** 无

**返回值：**
- `0` - 成功
- `-1` - 失败

**说明：** 创建并启动存储线程，自动挂载SD卡。

**示例：**
```c
if (sd_storage_start() != 0) {
    printf("Failed to start SD storage\n");
    return -1;
}
```

---

#### `int sd_storage_save_record(sd_storage_record_t *record)`

**功能：** 保存一条数据记录到缓冲区

**参数：**
- `record` - 指向数据记录的指针

**返回值：**
- `0` - 成功
- `-1` - 失败（缓冲区满或参数错误）

**说明：** 将数据添加到内存缓冲区，不立即写入SD卡。

**示例：**
```c
sd_storage_record_t rec;
rec.can_id = 0x001;
rec.dlc = 8;
rec.timestamp = rt_tick_get();
rt_memcpy(rec.data, data, 8);

if (sd_storage_save_record(&rec) != 0) {
    printf("Buffer full, data lost\n");
}
```

---

#### `int sd_storage_save_batch(sd_storage_record_t *records, rt_uint32_t count)`

**功能：** 批量保存数据记录

**参数：**
- `records` - 数据记录数组指针
- `count` - 记录个数

**返回值：** 实际保存的记录数

**示例：**
```c
sd_storage_record_t batch[10];
/* 填充batch数组... */
int saved = sd_storage_save_batch(batch, 10);
printf("Saved %d records\n", saved);
```

---

#### `int sd_storage_read_records(sd_storage_record_t *records, rt_uint32_t max_count)`

**功能：** 从SD卡读取数据记录

**参数：**
- `records` - 接收数据的缓冲区
- `max_count` - 最多读取的记录数

**返回值：** 实际读取的记录数

**说明：** 从 `/sd/can_data.bin` 读取数据。

**示例：**
```c
sd_storage_record_t read_buf[50];
int count = sd_storage_read_records(read_buf, 50);
printf("Read %d records from SD\n", count);

for (int i = 0; i < count; i++) {
    printf("ID: 0x%03X, DLC: %d\n", read_buf[i].can_id, read_buf[i].dlc);
}
```

---

#### `sd_status_t sd_storage_get_status(void)`

**功能：** 获取SD卡挂载状态

**参数：** 无

**返回值：**
```c
SD_STATUS_UNMOUNTED   // 0 - 未挂载
SD_STATUS_MOUNTED     // 1 - 已挂载
SD_STATUS_ERROR       // 2 - 错误
```

**示例：**
```c
if (sd_storage_get_status() == SD_STATUS_MOUNTED) {
    printf("SD card mounted\n");
}
```

---

#### `rt_bool_t sd_storage_is_mounted(void)`

**功能：** 检查SD卡是否已挂载

**参数：** 无

**返回值：**
- `RT_TRUE` (1) - 已挂载
- `RT_FALSE` (0) - 未挂载

**示例：**
```c
if (sd_storage_is_mounted()) {
    printf("SD card ready\n");
} else {
    printf("SD card not ready\n");
}
```

---

#### `rt_uint32_t sd_storage_get_buffer_count(void)`

**功能：** 获取缓冲区中待保存的记录数

**参数：** 无

**返回值：** 缓冲区中的记录数（0-256）

**示例：**
```c
rt_uint32_t pending = sd_storage_get_buffer_count();
printf("Pending records in buffer: %d\n", pending);
```

---

#### `int sd_storage_stop(void)`

**功能：** 停止SD卡存储服务

**参数：** 无

**返回值：**
- `0` - 成功
- `-1` - 失败

**说明：** 刷新所有缓冲数据到SD卡并卸载。

**示例：**
```c
sd_storage_stop();
```

---

## 完整使用示例

### 简单的数据采集和上传

```c
int main(void)
{
    // 配置MQTT
    mqtt_config_t cfg = {
        .wifi_ssid = "MyWiFi",
        .wifi_password = "password123",
        .mqtt_broker = "broker.example.com",
        .mqtt_port = 1883,
        .mqtt_client_id = "stm32_device",
        .mqtt_publish_topic = "device/sensors/can"
    };

    // 初始化所有模块
    can_receiver_init();
    mqtt_client_init(&cfg);
    sd_storage_init();

    // 启动所有服务
    can_receiver_start();
    mqtt_client_start();
    sd_storage_start();

    // 主循环
    while (1)
    {
        can_data_t data;
        
        // 获取CAN数据（非阻塞）
        if (can_receiver_get_data(&data, 100) == 0)
        {
            // 尝试通过MQTT发送
            if (mqtt_client_is_wifi_connected())
            {
                mqtt_client_publish_can_data(data.can_id, 
                                            data.data, 
                                            data.dlc);
            }
            else
            {
                // WiFi未连接，保存到SD卡
                sd_storage_record_t rec;
                rec.can_id = data.can_id;
                rec.dlc = data.dlc;
                rec.timestamp = rt_tick_get();
                rt_memcpy(rec.data, data.data, data.dlc);
                
                sd_storage_save_record(&rec);
            }
        }
        
        rt_thread_mdelay(100);
    }

    return 0;
}
```

---

## 错误处理

所有函数返回值说明：

| 返回值 | 含义 | 处理建议 |
|--------|------|--------|
| 0 | 操作成功 | 继续处理 |
| -1 | 操作失败 | 查看日志或重试 |
| RT_EOK | 操作成功 | 继续处理 |
| 其他 | 错误代码 | 参考rt-thread文档 |

---

## 线程安全说明

- CAN接收队列：线程安全（rt_mq）
- SD缓冲区：受互斥锁保护（rt_mutex）
- MQTT配置：初始化后不修改

---

## 注意事项

1. **初始化顺序：** 必须先 init 再 start
2. **内存管理：** 不支持动态分配大型缓冲
3. **实时性：** 某些操作可能阻塞，注意线程优先级
4. **调试模式：** 启用 RT_USING_DEBUG 查看详细日志
