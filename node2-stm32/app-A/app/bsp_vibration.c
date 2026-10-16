#include "bsp_vibration.h"

void vib_sw_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin  = VIB_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    /* 低有效 => 平时靠上拉把电平拉高 */
    gpio.Pull = (VIB_ACTIVE_LEVEL == 0) ? GPIO_PULLUP : GPIO_PULLDOWN;
    HAL_GPIO_Init(VIB_GPIO_PORT, &gpio);
}

bool vib_sw_active(void)
{
    uint8_t level = (HAL_GPIO_ReadPin(VIB_GPIO_PORT, VIB_GPIO_PIN) == GPIO_PIN_SET) ? 1u : 0u;

    return (level == VIB_ACTIVE_LEVEL);
}
