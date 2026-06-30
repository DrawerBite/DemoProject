#include "HighAbstraction/led.h"

/*
 * LED
 *
 * GPIO - PA8
 */

void led_init(void) {
	// 1. Тактирование PA портов
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

	// 2. Формирование PA8 как Output Push-pull
	GPIOA->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
	GPIOA->CRH |= (0x02 << GPIO_CRH_MODE8_Pos) | (0x00 << GPIO_CRH_CNF8_Pos);
}

void led_set(uint8_t state) {
	GPIOA->BSRR = (state) ? GPIO_BSRR_BS8 : GPIO_BSRR_BR8;
}
