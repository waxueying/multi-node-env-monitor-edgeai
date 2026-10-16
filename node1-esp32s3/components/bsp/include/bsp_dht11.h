#pragma once

#include <stdint.h>

/* DHT11 温湿度传感器驱动
 *
 * 接线说明：
 *   VCC  -> 3.3V
 *   GND  -> GND
 *   DATA -> GPIO（引脚在 DHT11.c 中定义）
 *
 * 注意：读取数据请使用 esp-idf-lib/dht 现成库
 *   #include "dht.h"
 *   float humi, temp;
 *   dht_read_float_data(DHT_TYPE_DHT11, GPIO_NUM_4, &humi, &temp);
 */

/* 初始化 DHT11 数据引脚（开漏 + 上拉，作为兜底保险） */
void dht11_init(void);
