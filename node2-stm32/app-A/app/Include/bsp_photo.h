#ifndef __BSP_PHOTO_H__
#define __BSP_PHOTO_H__

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

/* ===========================================================================
 * 光电传感器 E3F-DS30C4 驱动（开关量，NPN 集电极开路输出）
 *
 * 接线（三线制）：
 *   棕 -> +12V / +24V（传感器自身供电，按铭牌）
 *   蓝 -> GND（必须和单片机共地）
 *   黑 -> PB0（信号脚）
 *
 * 电气注意：E3F-DS30C4 的供电是 10~30V，输出是 NPN 集电极开路。
 *   单片机侧用内部上拉把信号拉到 3.3V：没有物体时三极管截止，引脚=高；
 *   检测到物体时三极管导通，把引脚拉到 0V。
 *   演示阶段图省事也可以直接拿杜邦线把 PB0 碰一下 GND 来模拟触发。
 *
 * 电平约定：PHOTO_ACTIVE_LEVEL = 0（低电平 = 检测到物体）
 *   如果你的模块是高电平有效，把下面这行改成 1 就行。
 * =========================================================================== */

#define PHOTO_GPIO_PORT     GPIOB
#define PHOTO_GPIO_PIN      GPIO_PIN_0
#define PHOTO_ACTIVE_LEVEL  0

/* 把 PB0 配成带上拉的输入（上拉/下拉跟随 PHOTO_ACTIVE_LEVEL 自动选） */
void photo_init(void);

/* 读一次引脚：true = 当前检测到物体 */
bool photo_is_active(void);

#endif /* __BSP_PHOTO_H__ */
