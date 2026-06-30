#pragma once

#include "main.h"
#include "rcc.h"

#define TIM4_RESET_MILLISECOND_DELAY 5000

void tim4_init(void);
void tim4_start(void);
void TIM4_IRQHandler(void);
