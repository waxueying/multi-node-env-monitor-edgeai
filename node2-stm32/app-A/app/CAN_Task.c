#include "CAN_Task.h"
#include "can_protocol.h"
#include "queue.h"

/* ==========================================================================
 * 节点2 的 CAN 任务
 *   发送：1Hz 上报状态帧(0x110) + 故障帧(0x112)；事件帧(0x111) 由队列触发立即发
 *   接收：中断收帧 -> FreeRTOS 队列 -> 任务里解析下行命令(0x210)
 *
 * 数据来源：g_node2（can_protocol.h 里定义，由传感器任务填写）。
 *           本文件不读传感器、不阻塞在 CAN 上：发送用邮箱+重试，接收走中断+队列。
 *
 * 注意：MX_CAN1_Init() 只做了 HAL_CAN_Init —— 既没有 HAL_CAN_Start()，也没有
 *       配置滤波器，所以外设一直停在初始化模式，总线上什么都发不出去。
 *       这两件事都放在 can_task_Init() 里做，避免改 CubeMX 生成的 can.c。
 * ========================================================================== */

#define CAN_TX_EVENT_Q_LEN      8       /* 事件队列深度 */
#define CAN_RX_FRAME_Q_LEN      8       /* 接收队列深度 */
#define CAN_TX_RETRY            3       /* 单帧发送重试次数 */

typedef struct
{
    uint8_t  type;
    uint16_t value;
} can_event_t;

typedef struct
{
    uint32_t id;
    uint8_t  data[8];
    uint8_t  len;
} can_rx_frame_t;

/* 上行数据（传感器任务填，CAN 任务读） */
node2_data_t g_node2 = {0};

static QueueHandle_t       s_event_q = NULL;
static QueueHandle_t       s_rx_q    = NULL;
static CAN_TxHeaderTypeDef s_tx_header;

/* CMSIS-RTOS2 的属性结构在创建时要保持有效，所以放文件作用域 */
static const osThreadAttr_t s_can_tx_attr = {"can_tx", 0, 0, 0, 0, 1024, (osPriority_t)osPriorityNormal7};
static const osThreadAttr_t s_can_rx_attr = {"can_rx", 0, 0, 0, 0, 1024, (osPriority_t)osPriorityNormal6};

/* ---------------------------------------------------------------- 底层发送 */
void BSP_CAN_SendData(uint32_t ID, uint8_t *Data, uint32_t Len)
{
    uint32_t mailbox = 0;
    uint8_t  retry   = CAN_TX_RETRY;

    if (Data == NULL || Len == 0)
    {
        return;
    }

    /* 经典 CAN 单帧最多 8 字节：原来的实现把 sizeof(结构体)=16 直接当 DLC，
     * 结果是每帧都在发无效数据。这里统一钳到 8。 */
    if (Len > 8)
    {
        Len = 8;
    }

    s_tx_header.StdId = ID;
    s_tx_header.IDE   = CAN_ID_STD;
    s_tx_header.RTR   = CAN_RTR_DATA;
    s_tx_header.DLC   = (uint32_t)Len;
    s_tx_header.TransmitGlobalTime = DISABLE;

    while (retry--)
    {
        if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0)
        {
            /* 邮箱满：中止挂起的请求腾位置 */
            HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
            osDelay(1);
        }

        if (HAL_CAN_AddTxMessage(&hcan1, &s_tx_header, Data, &mailbox) == HAL_OK)
        {
            return;
        }

        osDelay(1);
    }

    /* 重试仍失败只打印，不停机（原来这里对 CAN 错误直接 Error_Handler()，
     * 总线一断就把整个节点打死，不合适） */
    printf("[CAN] send failed, id=0x%03X\r\n", (unsigned)ID);
}

/* ---------------------------------------------------------------- 三个上报帧 */
bool can_proto_send_status(void)
{
    uint8_t data[8];

    data[0] = NODE2_ID;
    data[1] = (uint8_t)((g_node2.photo_bits & (N2_ST_PHOTO1 | N2_ST_PHOTO2)) |
                        (g_node2.vib_alarm ? N2_ST_VIB_ALARM : 0) |
                        (g_node2.calibrated ? N2_ST_CALIBRATED : 0));
    data[2] = (uint8_t)(g_node2.vib_level & 0xFF);
    data[3] = (uint8_t)((g_node2.vib_level >> 8) & 0xFF);
    data[4] = (uint8_t)(g_node2.photo_count & 0xFF);
    data[5] = (uint8_t)((g_node2.photo_count >> 8) & 0xFF);
    data[6] = (uint8_t)(g_node2.vib_count & 0xFF);
    data[7] = (uint8_t)((g_node2.vib_count >> 8) & 0xFF);

    BSP_CAN_SendData(CAN_ID_NODE2_STATUS, data, sizeof(data));
    return true;
}

bool can_proto_send_event(uint8_t evt_type, uint16_t value)
{
    uint8_t  data[8];
    uint32_t ts_ms = HAL_GetTick();

    data[0] = NODE2_ID;
    data[1] = evt_type;
    data[2] = (uint8_t)(value & 0xFF);
    data[3] = (uint8_t)((value >> 8) & 0xFF);
    data[4] = (uint8_t)(ts_ms & 0xFF);
    data[5] = (uint8_t)((ts_ms >> 8) & 0xFF);
    data[6] = (uint8_t)((ts_ms >> 16) & 0xFF);
    data[7] = (uint8_t)((ts_ms >> 24) & 0xFF);

    BSP_CAN_SendData(CAN_ID_NODE2_EVENT, data, sizeof(data));
    return true;
}

bool can_proto_send_fault(void)
{
    uint8_t  data[8];
    uint16_t uptime_s = (uint16_t)(HAL_GetTick() / 1000u);

    data[0] = NODE2_ID;
    data[1] = g_node2.fault_bits;
    data[2] = (uint8_t)(g_node2.photo_fails & 0xFF);
    data[3] = (uint8_t)((g_node2.photo_fails >> 8) & 0xFF);
    data[4] = (uint8_t)(g_node2.vib_fails & 0xFF);
    data[5] = (uint8_t)((g_node2.vib_fails >> 8) & 0xFF);
    data[6] = (uint8_t)(uptime_s & 0xFF);
    data[7] = (uint8_t)((uptime_s >> 8) & 0xFF);

    BSP_CAN_SendData(CAN_ID_NODE2_FAULT, data, sizeof(data));
    return true;
}

/* 事件投递：传感器任务调用（非阻塞，队列满就丢，绝不卡住采集） */
void can_proto_post_event(uint8_t evt_type, uint16_t value)
{
    can_event_t evt;

    evt.type      = evt_type;
    evt.value = value;

    if (s_event_q != NULL)
    {
        (void)xQueueSend(s_event_q, &evt, 0);
    }
}

/* ---------------------------------------------------------------- 接收中断 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rx_header;
    can_rx_frame_t      frame;
    BaseType_t          woken = pdFALSE;

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, frame.data) != HAL_OK)
    {
        return;
    }

    frame.id  = (rx_header.IDE == CAN_ID_STD) ? rx_header.StdId : rx_header.ExtId;
    frame.len = (uint8_t)rx_header.DLC;

    if (s_rx_q != NULL)
    {
        (void)xQueueSendFromISR(s_rx_q, &frame, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

/* ---------------------------------------------------------------- 两个任务 */
static void can_tx_task(void *argument)
{
    can_event_t evt;
    uint32_t    next_report;

    (void)argument;

    next_report = HAL_GetTick() + 1000u;

    while (1)
    {
        /* 等事件最多 100ms，同时兼顾 1Hz 周期上报 */
        if (xQueueReceive(s_event_q, &evt, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            can_proto_send_event(evt.type, evt.value);
        }

        if ((int32_t)(HAL_GetTick() - next_report) >= 0)
        {
            next_report += 1000u;

            can_proto_send_status();
            can_proto_send_fault();
        }
    }
}

static void can_rx_task(void *argument)
{
    can_rx_frame_t frame;

    (void)argument;

    while (1)
    {
        if (xQueueReceive(s_rx_q, &frame, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        /* 下行命令帧 0x210：B0=命令字，B1=参数（第一期只打印，动作后续加） */
        if (frame.id == CAN_ID_CMD_NODE2 && frame.len >= 2)
        {
            if (frame.data[0] == N2_CMD_SET_CONFIG)
            {
                printf("[CAN] cmd set config, arg=%u\r\n", (unsigned)frame.data[1]);
            }
            else
            {
                printf("[CAN] unknown cmd 0x%02X\r\n", (unsigned)frame.data[0]);
            }
        }
    }
}

/* ---------------------------------------------------------------- 初始化 */
void can_task_Init(void)
{
    CAN_FilterTypeDef filter;

    /* 1) 滤波器：全通、放 FIFO0，具体 ID 判断放在任务里（和节点1/主节点风格一致） */
    filter.FilterBank           = 0;
    filter.FilterMode           = CAN_FILTERMODE_IDMASK;
    filter.FilterScale          = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh         = 0x0000;
    filter.FilterIdLow          = 0x0000;
    filter.FilterMaskIdHigh     = 0x0000;
    filter.FilterMaskIdLow      = 0x0000;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation     = ENABLE;
    filter.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
    {
        printf("[CAN] config filter failed\r\n");
    }

    /* 2) 启动 CAN：不做这一步，外设停在初始化模式，总线上什么都发不出去 */
    if (HAL_CAN_Start(&hcan1) != HAL_OK)
    {
        printf("[CAN] start failed\r\n");
    }

    /* 3) 打开 RX FIFO0 挂起中断（NVIC 里的 CAN1_RX0_IRQn 已由 MspInit 使能） */
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        printf("[CAN] activate notification failed\r\n");
    }

    /* 4) 队列 + 两个任务 */
    s_event_q = xQueueCreate(CAN_TX_EVENT_Q_LEN, sizeof(can_event_t));
    s_rx_q    = xQueueCreate(CAN_RX_FRAME_Q_LEN, sizeof(can_rx_frame_t));

    if (s_event_q == NULL || s_rx_q == NULL)
    {
        printf("[CAN] create queue failed\r\n");
        return;
    }

    /* 两个都是常驻任务，句柄不再使用，所以不保存返回值 */
    (void)osThreadNew(can_tx_task, NULL, &s_can_tx_attr);
    (void)osThreadNew(can_rx_task, NULL, &s_can_rx_attr);

    printf("[CAN] node2 can task started\r\n");
}
