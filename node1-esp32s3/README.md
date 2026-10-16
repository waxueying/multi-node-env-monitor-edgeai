# 节点1 · 货架结构安全感知节点（ESP32-S3 + TensorFlow Lite + CAN + Wi-Fi OTA）

多节点安全监测系统的从节点（节点1）：本地采集温湿度与三轴加速度，
用轻量化神经网络识别 撞击 / 推压 / 干扰 事件，通过 CAN 上报主节点，
并支持巴法云 Wi-Fi 远程升级（A/B 双分区 + 回滚）。

## 目录结构

```
components/
  bsp/       硬件驱动层：CAN、DHT11、MPU6050
  pal/       平台适配层：时间等基础服务
  service/   业务服务层：AI 推理、环境监测、结构监测、CAN 协议、健康检查、Wi-Fi OTA
main/        任务编排：task_env / task_structure / capture（采集固件）
```

分层原则：bsp 只碰外设，service 只做业务，main 只做任务编排与调度。

## 硬件

| 部件 | 型号 | 接口 |
|---|---|---|
| 主控 | ESP32-S3（16MB Flash） | — |
| 温湿度 | DHT11 | GPIO |
| 六轴 IMU | MPU6050 | I2C（SDA=5 / SCL=6） |
| 总线 | CAN（外挂收发器） | 上报主节点 |
| 板载 LED | GPIO48 | OTA 状态指示 |

## 边缘 AI：撞击 / 推压识别

```
MPU6050 100Hz 采样 → 50 帧窗口（0.5s）→ 扣重力 → 归一化
        → TFLite Micro 推理（int8，约 50KB 模型）
        → 三类输出：idle 干扰 / impact 撞击 / push 推压
```

- 模型：`anomaly_detection_model.tflite`（结构见 `model_info.txt`）
- 推理代码：`components/service/ai_infer.cpp`（TFLite Micro，静态张量，无动态内存）
- 数据集是自己用采集固件录的：idle / tap / push / hit 四类动作，训练后导出 TFLite 再移植

与直接读传感器阈值判断的区别：阈值法只能判断超过门限的冲击；
模型能区分短促的撞击与持续的低频推压，并把走路、搬货这类环境振动归为 idle，减少误报。

## CAN 上报协议

| ID | 方向 | 内容 |
|---|---|---|
| 0x100 | 节点1 → 主节点 | 结构状态（倾角 / 振动 RMS / 撞击计数） |
| 0x102 | 节点1 → 主节点 | 环境数据（温度 / 湿度） |
| 0x103 | 节点1 → 主节点 | AI 事件（类别 + 置信度 + 峰值） |
| 0x456 | 节点1 → 主节点 | 心跳 / 健康状态 |

单帧 8 字节小端，与主节点 `can_receiver.c` 的白名单一一对应。

## Flash 分区与 OTA（16MB）

```
0x000000   bootloader
0x008000   partition-table
0x00f000   otadata
0x020000   ota_0   2MB
0x220000   ota_1   2MB
0x420000   storage
```

升级流程：

```
连 WiFi → 拉巴法云版本接口（version/size/url）→ 与本地版本比较
  → 有新版本：HTTP 下载完整 bin → 写入非当前分区 → 校验 → 置启动标志 → 重启
  → 新分区启动 5 秒内自确认 → 正式生效；期间崩溃则 bootloader 自动回滚
```

代码：`components/service/wifi_ota.c`；版本号与主题在 `wifi_ota_config.h`。

## 编译与烧写

```bash
# 1) 准备本地密钥（不进仓库）
cp components/service/include/secrets.template.h components/service/include/secrets.h
#    然后填 WiFi SSID / 密码与巴法云私钥

# 2) 编译
. $HOME/esp/esp-idf/export.sh
idf.py build

# 3) 烧写
idf.py -p COMx flash monitor
```

## 采集固件（自造数据集）

`main/capture.c` 是采集模式：串口发 0/1/2/3 切类别（idle / tap / push / hit），
触发动作后自动导出 100 行 CSV，再交给 Python 训练脚本生成 TFLite 模型。

## 踩过的坑

1. **TFLite 输入张量形状**：先看 model_info.txt 的输入输出形状。一开始按 8 通道加时间窗喂数据，
   实际模型是 [1,50,3] 的三轴加速度，喂错就会输出常数。
2. **置信度恒为 0%**：典型原因是输入没做扣重力与归一化，训练与部署两侧的预处理必须完全一致。
3. **I2C 失败刷屏**：MPU6050 掉线时 i2cdev 会反复重试打印，需要限制日志次数，否则串口被刷爆。
4. **Wi-Fi 发射瞬间 Brownout 复位**：把发射功率从 20dBm 降到 11dBm 后稳定
   （`wifi_ota.c` 里的 `esp_wifi_set_max_tx_power`）。
5. **OTA 包必须是完整镜像**：上传的 bin 要包含 bootloader + 分区表 + 应用；
   升级失败时靠 A/B 分区自动回滚，不会变砖。

## 安全说明

WiFi 凭据与巴法云私钥放在 `components/service/include/secrets.h`，已被 .gitignore 排除；
仓库只保留 `secrets.template.h` 模板。
