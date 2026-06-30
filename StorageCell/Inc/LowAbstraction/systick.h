#pragma once

#include <stdint.h>
#include <stm32f103xb.h>
#include "LowAbstraction/rcc.h"

void systick_init(void);
uint32_t systick_get_ticks(void);
void Delay_ms(uint32_t ms);
