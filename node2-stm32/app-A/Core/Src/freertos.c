/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "OLED.h"
#include "ESP8266.h"
#include "OTA.h"
#include "BAIFA_OTA.h"
#include "ota_flag.h"
#include "CAN_Task.h"
#include "Sensor_reading.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  /* 512*4 = 2048 字节：这个任务要跑 OLED/printf/OTA 轮询，512 字节不够 */
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  static uint8_t s_confirmed = 0;
  extern volatile uint8_t g_baifa_ota_request;
  /* USER CODE BEGIN StartDefaultTask */
  /* 以下内容原来写在 main.c 的 osKernelStart() 之后 —— 那段是死代码（调度器启动后
   * 不会返回），所以节点2 的 OLED / ESP8266 / 传感器任务 / CAN 任务从来没跑起来过。
   * 现在搬到这里：这里是任务上下文，ESP_Init() 里用到的 osDelay 才成立。
   * 顺序上把传感器/CAN 任务放在 ESP_Init() 之前 —— 没插 ESP8266 时 CAN 照样上报。 */
  OLED_Init();

  char line1[16] = {0};
  sprintf(line1, "appled %s", MCU_VER);
  OLED_ShowString(1, 1, line1);

  Baifa_OTA_Dump_Flag();
  printf("[MAIN] node2 app ver=%s vtor=0x%08X slot=%c\r\n",
         MCU_VER, (unsigned)(*(volatile uint32_t *)0xE000ED08UL),
         (ota_flag_my_slot() == OTA_SLOT_B) ? 0x42 : 0x41);

  Sensor_reading_task_Init();
  can_task_Init();

  /* 初始化巴法云OTA模块 */
  Baifa_OTA_Init();
  ESP_Init();

  /* Infinite loop */
  for(;;)
  {
    /* 启动 15 秒后自确认（清掉 pending），未确认的固件下次复位自动回滚 */
    if (!s_confirmed && (HAL_GetTick() > 15000U)) {
      s_confirmed = 1;
      ota_flag_confirm();
    }

    /* 串口收到 ota 命令后执行一次升级检查 */
    if (g_baifa_ota_request) {
      int r;

      g_baifa_ota_request = 0;
      printf("[MAIN] OTA request, start...\n");
      OLED_Clear_Line(2);
      OLED_ShowString(2, 1, "OTA checking");

      r = Baifa_OTA_Start();

      if (r == BAIFA_OTA_OK) {
        printf("[MAIN] OTA staged, rebooting\n");
        OLED_Clear_Line(2);
        OLED_ShowString(2, 1, "OTA OK, reboot");
        osDelay(500);
        HAL_NVIC_SystemReset();
      } else {
        printf("[MAIN] OTA failed: %s\n", Baifa_OTA_Last_Error());
        OLED_Clear_Line(2);
        OLED_ShowString(2, 1, "OTA failed");
      }
    }

    osDelay(500);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */