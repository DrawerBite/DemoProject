#pragma once

#include <CMSIS/stm32f1xx.h>
#include <stdint.h>

#define RCC_CLOCK_MHZ 72

uint8_t rcc_8mhz_init(void);
uint8_t rcc_set_frequency(uint8_t mhz);
