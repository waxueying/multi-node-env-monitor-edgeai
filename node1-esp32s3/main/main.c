#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_dht11.h"            /* BSP ：DHT11 引脚初始化 */
#include "bsp_can.h"              /* BSP ：CAN 总线（can_init / can_receive_task） */
#include "bsp_imu.h"              /* BSP ：MPU6050 */

#include "can_protocol.h"          /* 服务：CAN 帧协议（帧 ID/字节格式在此定义） */
#include "environment_monitor.h"   /* 服务：仓储环境标准 */
#include "ai_infer.h"              /* 服务：AI 推理 */
#include "wifi_ota.h"              /* 服务：WiFi + 巴法云 OTA */
#include "capture.h"               /* 采集模式（CAPTURE_MODE 开关在 capture.h） */

#include "app_tasks.h"             /* 应用层任务入口（task_structure.c / task_env.c） */

/* ============================================================================
 * 节点1：结构安全 + 仓储环境监测节点  —— 应用入口
 *
 * 本文件只做两件事：**初始化外设** + **创建任务**。
 * 具体业务在各自的任务文件里：
 *     task_structure.c   100Hz 结构安全采样（撞击 / 倾斜 / 振动）
 *     task_env.c         1Hz 温湿度采集 + 环境合规判定
 *     wifi_ota.c         WiFi + 巴法云 OTA（含下载、切换、回滚确认）
 *     bsp_can.c          CAN 接收任务
 *
 * 分层结构（依赖只能从上往下）：
 *   ① 应用层   main/                  何时做什么
 *   ② 服务层   components/service/    算法、协议、网络、健康管理（与硬件无关）
 *   ③ BSP 层   components/bsp/        传感器 / 总线驱动（跟这块板子的接线绑定）
 *   ④ HAL 层   components/hal/        平台时间基（唯一跟平台绑定的地方）
 *
 * 硬件引脚（都在 bsp 层定义）：
 *   DHT11 DATA -> GPIO4
 *   MPU6050    -> SDA=GPIO5, SCL=GPIO6（AD0 接地，地址 0x68）
 *   CAN        -> TX=GPIO11, RX=GPIO12
 * ========================================================================== */

#define NODE_ID                 0x01    /* 节点1 */

/* 上电默认环境模式：按实际仓储货物选择
 *   ENV_PROFILE_FOOD_DRUG       食品药品（阴凉库 0~20℃，35~75%RH）
 *   ENV_PROFILE_ELECTRONICS     电子元件（5~35℃，30~60%RH）
 *   ENV_PROFILE_TEXTILE_PAPER   纺织品/纸箱（5~35℃，40~70%RH）
 *   ENV_PROFILE_CHEMICAL        化学品（5~30℃，20~75%RH）
 *   ENV_PROFILE_GENERAL         普通货物（常温库 5~35℃，30~75%RH） */
#define ENV_DEFAULT_PROFILE     ENV_PROFILE_GENERAL

void app_main(void)
{
#if CAPTURE_MODE
    capture_mode_run();             /* 采集模式：只读 MPU6050 并导出波形，本函数不返回 */
#else
    /* ---------------- 外设初始化 ---------------- */
    ESP_ERROR_CHECK(can_init());    /* CAN 总线 */
    can_proto_init(NODE_ID);        /* 协议层：记录节点ID + 注册命令帧解析回调 */

    dht11_init();                   /* DHT11 引脚 */

    /* 这里不能用 ESP_ERROR_CHECK：MPU6050 没接好时它会 abort()，而 abort 会重启，
     * 结果变成"一开机就无限重启"，比传感器坏本身严重得多。
     * 传感器故障交给 task_structure 统一处理（判故障 → 上报 → 1 秒重试）。 */
    if (imu_init() != ESP_OK)
        printf("[故障] MPU6050 初始化失败（检查接线），本次运行不启用结构监测\n");

    if (ai_infer_init() != ESP_OK)
        printf("[AI] 初始化失败，本次运行不启用 AI 判定\n");

    env_monitor_init(ENV_DEFAULT_PROFILE);
    /* 暂时静音（只留 AI 结果）：恢复时把下面整段注释打开
    const env_limits_t *lim = env_monitor_get_limits();
    printf("[环境] 当前模式：%s（温度 %.0f~%.0f℃，湿度 %.0f~%.0f%%RH）\n",
           env_monitor_profile_name(ENV_DEFAULT_PROFILE),
           lim->temp_min, lim->temp_max, lim->humi_min, lim->humi_max);
    */

    /* ---------------- 创建任务 ----------------
     * 优先级：采样(6) > CAN 接收(5) > OTA(4) > 环境(3)
     * 采样任务固定核心1：核心0 上 DHT11 读取会关中断约 25ms，别打扰采样。 */
    xTaskCreatePinnedToCore(task_structure, "structure", 4096, NULL, 6, NULL, 1);
    xTaskCreate(can_receive_task, "can_rx", 4096, NULL, 5, NULL);
    xTaskCreate(wifi_ota_task, "wifi_ota", 8192, NULL, 4, NULL);
    xTaskCreatePinnedToCore(task_env, "env", 4096, NULL, 3, NULL, 0);

    /* 业务都在上面的任务里；app_main 返回后 IDF 会自己删除 main 任务 */
#endif
}

