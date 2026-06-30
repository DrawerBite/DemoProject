#pragma once

#include "main.h"
#include "rcc.h"

#define TIM3_RESET_MILLISECOND_DELAY 15000

extern int16_t holes_diff;

void tim3_init(void);
void tim3_start(void);
void TIM3_IRQHandler(void);
