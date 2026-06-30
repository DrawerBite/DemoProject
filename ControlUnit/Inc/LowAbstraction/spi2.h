#pragma once

#include <stm32f1xx.h>

void spi2_init();
void spi2_sendByte(uint8_t data);
uint8_t spi2_transmit(uint8_t data);
void spi2_set_speed(uint16_t prescaler);
