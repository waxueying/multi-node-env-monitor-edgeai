# 主节点 · 工业货架安全监测系统（STM32F407 + RT-Thread + CAN/MQTT + 双区 OTA）

多节点安全监测系统的主节点：通过 CAN 汇聚各从节点的传感器状态，
经 MQTT 上云、断网时落 SD 卡并在恢复后自动补传，同时支持巴法云 Wi-Fi 远程升级。

## 目录结构

```
bootloader/   引导程序（裸机，无 HAL / 无 RTOS，只做选区与跳转）
app/          应用固件（RT-Thread 5.1.0 + Keil MDK）
  applications/   业务代码：CAN 接收、MQTT、SD 存储、OLED、OTA
  rt-thread/      内核与组件
  libraries/      STM32F4 HAL / CMSIS
  packages/       at_device(ESP8266) / pahomqtt 等软件包
```

## 硬件

| 部件 | 型号 | 说明 |
|---|---|---|
| 主控 | STM32F407ZGT6（1MB Flash / 192KB SRAM） | — |
| 显示 | 0.96 寸 OLED（SSD1306） | 软件 I2C，PB8/PB9 |
| 联网 | ESP8266（AT 固件，UART2） | at_device 驱动，用于 MQTT 与 OTA |
| 存储 | MicroSD 卡（SDIO） | 断网期间缓存 CAN 数据 |
| 总线 | CAN1 | 接收从节点上报 |

## Flash 分区与双区升级（1MB）

```
0x08000000   48KB   bootloader      Sector 0~2
0x0800C000   16KB   升级标志区      Sector 3
0x08010000  448KB   APP-A           Sector 4~7
0x08080000  512KB   APP-B           Sector 8~11
```

bootloader 只负责选区和跳转，**不做固件搬运**：

```
上电 → 读标志区
  ├─ 标志无效            → 跳 APP-A
  ├─ 有 pending 且镜像合法 → 消费 pending，跳新固件试用
  └─ 有 pending 但镜像非法 → 清 pending，回滚 active
```

新固件启动后需自确认（写 `active=新分区, pending=0`）才正式生效，
未确认前挂了就自动回滚，因此升级失败不会变砖。

## 主要功能

| 模块 | 文件 | 说明 |
|---|---|---|
| CAN 接收 | `applications/can_receiver.c` | 双 ID 白名单、队列缓存 |
| MQTT 上云 | `applications/mqtt_client.c` | paho MQTT，QoS1，断线自动重连 |
| 断网续传 | `applications/sd_storage.c` | 内存缓冲 → 批量落 SD → 重连后按游标补传 |
| OLED 显示 | `applications/oled_display.c` | 多页轮显（状态/诊断/AI 事件） |
| OTA 下载 | `applications/ota_download.c` | 巴法云版本查询 + 分块下载写 B 区 |
| 标志区 | `applications/ota_flag.c` | 与 bootloader 共用的标志记录 |

### 断网续传的三级缓冲

```
实时转发成功        → 直接上云
转发失败 / 未联网    → 写入内存缓冲（攒批）
缓冲满一批（30 条）  → 落盘 /sd/can_data.bin
网络恢复            → 每 2 秒补传一批，游标推进，补完删文件
```

串口命令：`sdstat` 查看积压、`bootflag` 查看/切换启动区、`ota_check` 手动触发升级检查。

## 编译

1. 复制 `app/applications/secrets.template.h` 为 `app/applications/secrets.h`，填 WiFi 与 MQTT 配置
2. Keil MDK 打开 `app/maintest_rt.uvprojx` 编译（A 区版本）
3. bootloader 打开 `bootloader/maintest_boot.uvprojx` 编译
4. 生成 B 区版本：复制 app 目录后改链接地址到 `0x08080000` 与 `VECT_TAB_OFFSET`，再编译

## 踩过的坑

1. **RT-Thread 时钟配置**：直接套用裸机版的 SysTick 配置会和内核节拍冲突，需按 RT-Thread 的 `RT_TICK_PER_SECOND` 重新配。
2. **CAN 收不到数据**：先看 `list device` 里 `can1` 是否注册、再看中断与滤波器配置，最后才是波特率；
   本项目最终定位到的是 CAN 模式与滤波器使能顺序问题。
3. **MQTT 发不出去**：paho 内部用 512 字节管道把消息投递给 worker 线程，
   worker 卡住时管道瞬间写满，日志会刷 `Publish failed`——本质是链路卡顿而不是丢包，
   此时数据会被落盘补传，属于设计内行为。
4. **SD 卡写挂系统**：SDIO + DMA 与中断优先级冲突会导致死锁，需要把 SD 操作放到独立线程并限制优先级。
5. **OTA 读到一半 CRC 失败**：app 与 bootloader 的标志区结构必须完全一致，
   版本号/大小也要和云端一致，否则会出现「下载完成但校验不过」的假故障。

## 安全说明

WiFi 与 MQTT 配置存放在 `secrets.h`，已被 `.gitignore` 排除，仓库只保留 `secrets.template.h`。
