# 多节点工业货架安全监测系统（边缘 AI + 多协议 + 全节点 OTA）

一套面向工业仓储货架的分布式安全监测系统：三个节点分工协作，CAN 总线互联、MQTT 上云、边缘 AI 本地预警，三个节点都支持远程 OTA 升级。

## 系统架构

```
        云平台（巴法云 OTA / MQTT broker）
                  │
          Wi-Fi（ESP8266 / ESP32-S3）
                  │
        ┌─────────┴─────────┐
        │   主节点 STM32F407 │  RT-Thread
        │  CAN 汇聚 + MQTT   │  OLED + SD 断网续传 + 双区 OTA
        └─────────┬─────────┘
                  │
            CAN 总线（500kbps）
        ┌─────────┴─────────┐
        ▼                   ▼
  ┌──────────────┐   ┌──────────────┐
  │ 节点1 结构安全 │   │ 节点2 环境安全 │
  │ ESP32-S3+AI  │   │ STM32F407    │
  │ MPU6050/DHT11│   │ 光电+振动开关  │
  │ Wi-Fi OTA    │   │ Wi-Fi OTA    │
  └──────────────┘   └──────────────┘
```

## 三个节点

| 目录 | 角色 | 主控 | 系统 | 关键能力 |
|---|---|---|---|---|
| `node1-esp32s3/` | 结构安全感知 | ESP32-S3 | ESP-IDF | TFLite 边缘 AI 识别撞击/推压、DHT11 温湿度、A/B 双分区 OTA |
| `node2-stm32/` | 环境/防入侵感知 | STM32F407 + ESP8266 | FreeRTOS | 光电 E3F-DS30C4 + 振动 SW-420、X-CUBE-AI、双区 OTA |
| `main-node-stm32/` | 数据汇聚与上云 | STM32F407 | RT-Thread | CAN 汇聚、MQTT、SD 断网续传、OLED 多页显示、双区 OTA |

## 通信设计

**CAN（节点 ↔ 主节点）**：单帧 8 字节、小端、标准帧，按 ID 区分数据类型。

| ID | 来源 | 内容 |
|---|---|---|
| 0x100 | 节点1 | 结构状态：倾角 / 振动 RMS / 撞击计数 |
| 0x102 | 节点1 | 环境数据：温度 / 湿度 |
| 0x103 | 节点1 | AI 事件：类别 + 置信度 + 峰值 |
| 0x110 | 节点2 | 状态帧（1Hz） |
| 0x111 | 节点2 | 事件帧（光电遮挡 / 振动触发） |
| 0x112 | 节点2 | 故障帧（传感器卡死 / 断线） |
| 0x456 | 全节点 | 心跳 / 健康状态 |

**MQTT（主节点 ↔ 云）**：发布 `maintest/yao-shelf/can` 上报 CAN 帧 JSON，订阅 `maintest/yao-shelf/cmd` 接收下行命令。

**断网续传**：MQTT 未连接或发送失败时，CAN 帧先写入内存缓冲，攒够一批落 SD 卡；网络恢复后按游标分批补传，补完自动清理文件。

## 边缘 AI（节点1）

```
MPU6050 100Hz 采样 → 50 帧窗口（0.5s）→ 扣重力 → 归一化
   → TensorFlow Lite Micro 推理（int8，约 50KB）
   → idle 干扰 / impact 撞击 / push 推压
```

模型是自己训练的：用采集固件录 idle/tap/push/hit 四类动作，Python 侧训练后导出 TFLite，再部署到设备上推理。

对比纯阈值判断：阈值法只能识别超过门限的冲击；模型能区分短促撞击与持续推压，并把走路、搬货这类环境振动归为干扰，明显减少误报。

## 远程升级（三节点统一思路）

三个节点芯片和系统都不同，但升级设计是同一套思路：**双分区 + 校验 + 自确认 + 可回滚**。

| 节点 | 引导方式 | 分区 | 回滚机制 |
|---|---|---|---|
| 节点1 ESP32-S3 | ESP-IDF bootloader | ota_0 / ota_1 各 2MB | 新镜像 5 秒内未自确认则由 bootloader 回滚 |
| 节点2 STM32 | 自写裸机 bootloader | APP-A 0x08020000 / APP-B 0x08040000 | pending 只消费一次，非法镜像自动回滚 |
| 主节点 STM32 | 自写裸机 bootloader | APP-A 0x08010000 / APP-B 0x08080000 | 同上，再加启动自确认 |

以 STM32 双区为例的升级流程：

```
云端版本接口 → 与本地版本比较 → 下载完整固件 → 写入非当前分区
   → 校验镜像 → 写标志区 pending → 复位 → bootloader 跳新分区
   → 新固件 15 秒内自确认 → 正式生效；否则下次复位回到旧分区
```

## 硬件清单

| 部件 | 型号 | 用途 |
|---|---|---|
| 主控 | ESP32-S3（16MB Flash） | 节点1，边缘 AI |
| 主控 | STM32F407VET6（512KB） | 节点2 |
| 主控 | STM32F407ZGT6（1MB） | 主节点 |
| 温湿度 | DHT11 | 环境监测 |
| IMU | MPU6050 | 三轴加速度 / 角速度 |
| 光电 | E3F-DS30C4 | 货架遮挡 / 越界检测 |
| 振动 | SW-420 | 振动 / 冲击开关 |
| 联网 | ESP8266（AT 固件） | STM32 侧 Wi-Fi |
| 显示 | OLED SSD1306 | 本地状态显示 |
| 存储 | MicroSD | 断网数据缓存 |
| 总线 | CAN 收发器 | 节点互联 |

## 构建与烧写

```bash
# 节点1（ESP-IDF 5.5）
cd node1-esp32s3
# 先复制 secrets.template.h 为 secrets.h 并填 WiFi 与云平台私钥
idf.py build
idf.py -p COMx flash monitor

# 节点2 / 主节点（Keil MDK，AC5 编译器）
#   各自打开 *.uvprojx 编译；
#   主节点先烧 bootloader（0x08000000），再烧 app（A 区）；
#   B 区版本：复制工程、改链接地址与版本号后编译（各目录说明里有步骤）
```

每个工程目录下还有单独的 README，写明了该节点的引脚、协议与复现步骤。

## 目录

```
node1-esp32s3/     ESP32-S3 节点：边缘 AI + CAN + Wi-Fi OTA
node2-stm32/       STM32F407 节点：光电 / 振动 + CAN + Wi-Fi OTA
main-node-stm32/   STM32F407 主节点：RT-Thread + CAN/MQTT + SD 续传 + OTA
```

> 仓库内不包含任何 WiFi 密码或云平台私钥：各工程只提供 `secrets.template.h`，复制为 `secrets.h` 并填入自己的配置即可编译。
