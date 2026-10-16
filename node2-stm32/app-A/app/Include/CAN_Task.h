#ifndef CAN_Task_H
#define CAN_Task_H

#include "main.h"
#include "cmsis_os.h"
#include "can.h"
#include "task.h"
#include "FreeRTOS.h"
#include <stdio.h>
#include <string.h>

/* 节点2 的 CAN 任务接口。
 * 帧定义（CAN ID、各字节含义）在 can_protocol.h 里，上行数据放在 g_node2。 */

/* 保留原来的结构体定义，避免影响其它引用它的文件。
 * 注意：上行帧已经改成 8 字节的协议帧（见 can_protocol.h），不再直接发这个结构体。 */
typedef struct CAN_TX_Data_t{
	float Raw_data[3];
	float Computing_data;
}CAN_TX_Data_t;

/* 底层发送：一帧标准帧，DLC 最大 8（超了会被钳到 8） */
void BSP_CAN_SendData(uint32_t ID, uint8_t* Data, uint32_t Len);

/* 启动节点2 的 CAN 发送/接收任务，系统初始化时调用一次 */
void can_task_Init(void);

#endif
