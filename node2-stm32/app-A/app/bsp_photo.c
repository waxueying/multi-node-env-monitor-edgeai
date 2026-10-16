#include "bsp_photo.h"

void photo_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin  = PHOTO_GPIO_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    /* 低有效 => 平时靠上拉把电平拉高，触发时被传感器拉到地 */
    gpio.Pull = (PHOTO_ACTIVE_LEVEL == 0) ? GPIO_PULLUP : GPIO_PULLDOWN;
    HAL_GPIO_Init(PHOTO_GPIO_PORT, &gpio);
}

bool photo_is_active(void)
{
    uint8_t level = (HAL_GPIO_ReadPin(PHOTO_GPIO_PORT, PHOTO_GPIO_PIN) == GPIO_PIN_SET) ? 1u : 0u;

    return (level == PHOTO_ACTIVE_LEVEL);
}
