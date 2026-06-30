#include "LowAbstraction/tim4.h"

void tim4_init(void) {
	// 1. Тактирование TIM4
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

	// 2. Настройка времени срабатывания таймера
	TIM4->PSC = RCC_CLOCK_MHZ * 1000 - 1; // 1 000 Гц (1 мс)
	TIM4->ARR = TIM4_RESET_MILLISECOND_DELAY - 1; //Авто-сброс

	// 3. Разрешаем прерывание от TIM4
	TIM4->DIER |= TIM_DIER_UIE;
	NVIC_EnableIRQ(TIM4_IRQn);
	NVIC_SetPriority(TIM4_IRQn, 6);

	// 4. Обновляем таймер
	TIM4->EGR |= TIM_EGR_UG;
}

void tim4_start(void) {
	TIM4->CNT = 0; 				// Сбрасываем счетчик в 0
	TIM4->SR &= ~TIM_SR_UIF; 	// Очищаем флаг прерывания
	TIM4->CR1 |= TIM_CR1_CEN; 	//Включаем таймер
}

void TIM4_IRQHandler(void) {
    if (TIM4->SR & TIM_SR_UIF) {
        TIM4->SR &= ~TIM_SR_UIF; 	// Сброс флага
        TIM4->CR1 &= ~TIM_CR1_CEN; 	// Останавливаем таймер

        Callback_TIM4();
    }
}
