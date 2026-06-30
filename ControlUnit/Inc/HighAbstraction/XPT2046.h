#pragma once

#include <stm32f1xx.h>
#include "spi2.h"

// Значения для калибровки
#define RAW_X_MIN 300
#define RAW_X_MAX 3500
#define RAW_Y_MIN 250
#define RAW_Y_MAX 3800

// Смещение калибровки (если нажатие сдвинуто на N пикселей)
#define TOUCH_X_OFFSET 0
#define TOUCH_Y_OFFSET 0

void XPT2046_init(void);
uint16_t XPT2046_getX(void);
uint16_t XPT2046_getY(void);
uint8_t XPT2046_isPressed(void);
