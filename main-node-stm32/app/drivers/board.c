/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-02     RealThread   first version
 */

#include <rtthread.h>
#include <board.h>
#include <drv_common.h>
#define RT_WEAK    rt_weak
RT_WEAK void rt_hw_board_init()
{
    extern void hw_board_init(char *clock_src, int32_t clock_src_freq, int32_t clock_target_freq);

    /* Heap initialization */
#if defined(RT_USING_HEAP)
    rt_system_heap_init((void *) HEAP_BEGIN, (void *) HEAP_END);
#endif

    hw_board_init(BSP_CLOCK_SOURCE, BSP_CLOCK_SOURCE_FREQ_MHZ, BSP_CLOCK_SYSTEM_FREQ_MHZ);

    /* Set the shell console output device */
#if defined(RT_USING_DEVICE) && defined(RT_USING_CONSOLE)
    rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif

    /* Board underlying hardware initialization */
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif

}

#ifdef BSP_USING_CAN1
/* CAN1 底层初始化：外设时钟 + GPIO。HAL_CAN_Init() 首次初始化时会回调到这里 */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hcan->Instance == CAN1)
    {
        __HAL_RCC_CAN1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA11 -> CAN1_RX, PA12 -> CAN1_TX, 复用功能 AF9 */
        GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_NOPULL;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}
#endif /* BSP_USING_CAN1 */

#ifdef BSP_USING_SDIO
/* SDIO 底层初始化：外设时钟 + GPIO。drv_sdio.c 的 rt_hw_sdio_init() 会回调到这里，
 * 之后它自己开 SDIO_IRQn 和 DMA 时钟（DMA2 Stream3 收 / Stream6 发，见 sdio_config.h）。
 *
 * SDIO 的时钟来自 PLL48CLK：drv_clk.c 里 PLLN=144 / PLLQ=3 → 144/3 = 48MHz，
 * 正好是 SDIO 要求的频率，所以这里不用动时钟树。
 *
 * 引脚是 F407 板子的通用接法（4 位模式）：
 *   PC8=D0  PC9=D1  PC10=D2  PC11=D3  PC12=CK  PD2=CMD
 * 如果卡检测不到，第一个要怀疑的就是这组引脚和板子实际接线是否一致。 */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hsd->Instance == SDIO)
    {
        __HAL_RCC_SDIO_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /* PC8~PC12：数据线与时钟，复用功能 AF12，上拉（SD 总线空闲时需要上拉） */
        GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                                    GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* PD2：命令线 */
        GPIO_InitStruct.Pin       = GPIO_PIN_2;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    }
}
#endif /* BSP_USING_SDIO */
