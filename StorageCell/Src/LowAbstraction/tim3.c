#include "LowAbstraction/tim3.h"

void tim3_init(void) {
	// 1. Тактирование TIM3
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

	// 2. Настройка времени срабатывания таймера
	TIM3->PSC = RCC_CLOCK_MHZ * 1000 - 1; // 1 000 Гц (1 мс)
	TIM3->ARR = TIM3_RESET_MILLISECOND_DELAY - 1; //Авто-сброс

	// 3. Разрешаем прерывание от TIM3
	TIM3->DIER |= TIM_DIER_UIE;
	NVIC_EnableIRQ(TIM3_IRQn);
	NVIC_SetPriority(TIM3_IRQn, 5);

	// 4. Обновляем таймер
	TIM3->EGR |= TIM_EGR_UG;
}

void tim3_start(void) {
	TIM3->CNT = 0; 				// Сбрасываем счетчик в 0
	TIM3->SR &= ~TIM_SR_UIF; 	// Очищаем флаг прерывания
	TIM3->CR1 |= TIM_CR1_CEN; 	//Включаем таймер
}

void TIM3_IRQHandler(void) {
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~TIM_SR_UIF; 	// Сброс флага
        TIM3->CR1 &= ~TIM_CR1_CEN; 	// Останавливаем таймер

        Callback_TIM3();
    }
}
