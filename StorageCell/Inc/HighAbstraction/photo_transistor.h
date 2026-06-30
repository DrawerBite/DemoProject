#pragma once

#include <stdint.h>
#include <stm32f103xb.h>

extern volatile uint8_t photo_transistor_triggered; //Аппаратный флаг при срабатывании прерывания

void photoTransistor_init(void);
uint8_t photoTransistor_getStatus(void);
void EXTI9_5_IRQHandler(void);
