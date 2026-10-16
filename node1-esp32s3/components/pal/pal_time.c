#include "pal_time.h"

#include "esp_timer.h"

uint32_t pal_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

int64_t pal_time_us(void)
{
    return esp_timer_get_time();
}

