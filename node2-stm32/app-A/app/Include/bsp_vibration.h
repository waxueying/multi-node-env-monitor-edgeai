#ifndef __BSP_VIBRATION_H__
#define __BSP_VIBRATION_H__

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

/* ===========================================================================
 * 振动传感器 SW-420 驱动（开关量，板载 LM393 比较器）
 *
 * 接线（只用数字脚，四线模块的 A0 不用）：
 *   VCC -> 3.3V（5V 也能用）
 *   GND -> GND
 *   DO  -> PB1
 *
 * 原理：SW-420 内部是一个弹簧式振动开关，静止时断开，一有振动就频繁通断，
 *   经 LM393 整形后从 DO 输出方波。所以它给不出加速度数值，只能回答
 *   "这一刻在不在振"——想要强度，就用一段时间内的有效电平占比来近似
 *   （见 Sensor_reading.c 里的 vib_level）。
 *   模块上的蓝色电位器是灵敏度，振动不明显时逆时针调小。
 *
 * 电平约定：VIB_ACTIVE_LEVEL = 0（有振动时 DO 拉低）
 *   常见的 SW-420 模块都是低有效；如果你的模块相反，改成 1。
 * =========================================================================== */

#define VIB_GPIO_PORT       GPIOB
#define VIB_GPIO_PIN        GPIO_PIN_1
#define VIB_ACTIVE_LEVEL    0

/* 把 PB1 配成带上拉的输入 */
void vib_sw_init(void);

/* 读一次 DO：true = 当前检测到振动 */
bool vib_sw_active(void);

#endif /* __BSP_VIBRATION_H__ */
