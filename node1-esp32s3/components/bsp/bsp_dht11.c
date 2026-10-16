#include "bsp_dht11.h"

#include "driver/gpio.h"

/* DHT11 数据引脚，按你的接线修改 */
#define DHT11_GPIO GPIO_NUM_4

/* 初始化 DHT11 数据引脚（开漏 + 上拉，初始拉高）。
 * 注意：现在使用 esp-idf-lib/dht 现成库读取数据，
 * 库内部会自行配置 GPIO，此初始化作为兜底保险保留。 */
void dht11_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << DHT11_GPIO,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, /* 开漏输入输出，可读可写 */
        .pull_up_en = GPIO_PULLUP_ENABLE,  /* 内部上拉，总线空闲时为高 */
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io);

    gpio_set_level(DHT11_GPIO, 1); /* 初始释放总线（高电平） */
}
