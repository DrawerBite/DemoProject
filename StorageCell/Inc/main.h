#pragma once

#include <stm32f103xb.h>
#include <stdint.h>

#include "HighAbstraction/encoder.h"
#include "HighAbstraction/photo_transistor.h"
#include "HighAbstraction/led.h"
#include "LowAbstraction/can1.h"
#include "LowAbstraction/rcc.h"
#include "LowAbstraction/tim3.h"
#include "LowAbstraction/tim4.h"
#include "LowAbstraction/systick.h"

#define HOLES_ADDITION 11
#define ENCODER_TICKS_PER_HOLE 3
#define ENCODER_FILTER_THRESHOLD 1






/*
 * Вызывается при получении данных по CAN
 */
void Callback_CAN(const uint8_t *data, uint8_t dlc);

/*
 * Вызывается когда TIM3 сбрасывается
 */
void Callback_TIM3(void);

/*
 * Вызывается когда TIM4 сбрасывается
 */
void Callback_TIM4(void);


/*
 * Вызывается при изменении состояния фототранзистора
 */
void Callback_PhotoTransistor(void);
