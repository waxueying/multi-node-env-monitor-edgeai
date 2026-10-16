# 节点2 · 仓储货架安全监测终端（STM32F407 + FreeRTOS + X-CUBE-AI + 巴法云 OTA）

这是一个工业仓储场景下的多节点安全监测系统的**从节点（节点2）**工程：
STM32F407VET6 采集光电 / 振动传感器，跑一个轻量神经网络做本地判断，
通过 CAN 把状态上报给主节点，并支持**巴法云 Wi-Fi 远程升级（OTA）**。

## 目录结构

```
bootloader/   —— 引导程序（裸机，无 HAL / 无 RTOS，只做选区和跳转）
app-A/        —— 应用固件 A 区（链接地址 0x08020000，版本 2.0.0）
tools/        —— 说明与脚本（例如 app-B 版本怎么生成）
```

app-A 与 app-B 是**同一套源码的两个链接版本**，只差三处：
链接地址（scatter + Keil IROM）、`SCB->VTOR`、以及 `app/Include/OTA.h` 里的 `MCU_VER`。

> app-B 没有放进仓库：它是同一套源码换链接地址后的产物（改 3 处即可），
> 生成步骤见 `tools/make_app_b.md`，这样仓库体积直接少一半。

## 硬件

| 部件 | 型号 | 接口 |
|---|---|---|
| 主控 | STM32F407VET6（512KB Flash / 192KB SRAM） | — |
| 光电开关 | E3F-DS30C4（NPN 输出） | PB0，低电平有效 |
| 振动开关 | SW-420（LM393 比较输出） | PB1，低电平有效 |
| 显示 | 0.96 寸 OLED（SSD1306） | I2C1 |
| 联网 | ESP8266（AT 固件，UART2） | 用于巴法云 OTA |
| 现场总线 | CAN1 | 与主节点/其他从节点通信 |

## Flash 分区（512KB）

```
0x08000000   64KB   bootloader        Sector 0~3
0x08010000   64KB   升级标志区        Sector 4
0x08020000  128KB   APP-A（默认运行）  Sector 5
0x08040000  128KB   APP-B（升级暂存）  Sector 6
```

应用程序编译后约 41KB（Code + RO-data ≈ 53KB 镜像），单区 128KB 余量充足。

## OTA 升级流程

```
上电 → bootloader 读标志区
        ├─ 无标志 / 校验不过        → 跳 APP-A
        ├─ 有 pending 且目标区合法  → 消费 pending，跳新固件试试
        └─ 有 pending 但目标区非法  → 清 pending，回滚到 active 区

App-A 里执行 ota 命令：
  1. HTTP GET 巴法云版本接口 → 拿 version / size / 下载 URL
  2. 与本地 MCU_VER 比较，云端更新才继续
  3. 擦除 APP-B（Sector 6）
  4. 分块 HTTP GET（Range: bytes=off-end，768B/块）→ 写 APP-B
  5. 校验 B 区镜像合法性（向量表 SP/PC）
  6. 写标志区：active=A, pending=B → 复位

新固件在 B 区启动 15 秒后自确认（写 active=B, pending=0）→ 正式生效；
如果 15 秒内崩溃/复位，pending 已消费但 active 仍是 A → 自动回到旧固件。
```

### 标志区记录（app 与 bootloader 必须一致）

```c
typedef struct {
    uint32_t magic;    /* 0x5A5AA5A5 */
    uint32_t active;   /* 0x41='A' / 0x42='B' */
    uint32_t pending;  /* 0 = 无待验证固件 */
    uint32_t check;    /* magic ^ active ^ pending */
} ota_flag_t;
```

## 编译

- Keil MDK 5（AC5 编译器，v5.06 update 5）
- 三个工程各自独立打开即可编译：
  - `bootloader/node2_boot.uvprojx` → 输出 `build/keil/boot.hex`（0x08000000）
  - `app-A/MDK-ARM/stm32f407vet6_Finalversion.uvprojx` → 0x08020000
  - `app-B`（按 `tools/make_app_b.md` 生成）→ 0x08040000

## 本地配置（密钥不进仓库）

1. 复制 `app-A/app/Include/secrets.template.h` 为 `secrets.h`（app-B 同理）
2. 填 WiFi SSID / 密码、巴法云私钥与 OTA 主题
3. `secrets.h` 已在 `.gitignore` 中，不会被提交

## 复现一次 OTA

1. 烧写 bootloader（0x08000000）与 app-A（0x08020000）
2. 把 app-B 编译出的 bin 上传到巴法云对应主题（本工程用的是 `self2ota`）
3. 串口发 `ota` → 观察下载进度 → 自动复位
4. 复位后串口应打印 `vtor=0x08040000 slot=B`，说明新固件已从 B 区运行
5. 再复位一次，若 `active=B` 则自确认成功，升级完成

## 踩过的坑（面试常问）

1. **APP 链接地址没改**：CubeMX 生成的工程默认链接 0x08000000，做双区必须先改 scatter + Keil IROM，
   否则 bootloader 跳过去就是 HardFault。
2. **VTOR 忘了重定位**：不改 `SCB->VTOR`，中断向量还是从 0x08000000 取，一进中断就跑飞。
3. **ESP8266 接收缓冲太小**：AT 固件的 `+IPD` 一帧可能上千字节，原工程 1KB 缓冲会截断，
   HTTP 头和 body 跨帧时解析直接失败。扩容到 2KB 并单独做下载抓取缓冲后稳定。
4. **HTTP 头必须和 body 一起解析**：只按 `Content-Length` 取字节会把 `+IPD,xxx:` 头也算进数据，
   写进 Flash 的镜像就是坏的 → bootloader 校验不过自动回滚（这正好验证了回滚机制有效）。
5. **bootloader 不能依赖复位后的干净状态**：Flash 错误标志、SysTick 使能位都可能在复位后残留，
   必须显式清零再操作，否则第一次能跑、第二次就卡死。
6. **FPU 要在 bootloader 里使能**（CPACR 的 CP10/CP11），否则带硬件浮点编译的库函数一执行就 UsageFault。

## 说明

- 仓库只保留工程必需文件：Keil 中间产物（`*.o/.axf/.crf`）、构建目录、
  `.eide/.cmsis/.ai` 等 IDE 缓存都在 `.gitignore` 里。
- `Drivers/CMSIS/DSP` 与 `Drivers/CMSIS/NN` 源码未编译（神经网络走 X-CUBE-AI 预编译库），
  已从仓库剔除；用 CubeMX 重新生成工程时它们会回来，不影响编译。
