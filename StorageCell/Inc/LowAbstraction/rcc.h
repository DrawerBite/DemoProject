#pragma once

#include <stdint.h>
#include <stm32f103xb.h>

#define RCC_CLOCK_MHZ 16

uint8_t rcc_8mhz_init(void);
uint8_t rcc_set_frequency(uint8_t mhz);
