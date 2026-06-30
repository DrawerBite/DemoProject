#include "HighAbstraction/photo_transistor.h"

/*
 * ======================================================
 * PHOTO TRANSISTOR
 * PB8
 */

void photoTransistor_init(void) {
	// 1. Тактирование PB портов и модуля альтернативной функции
	RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;

	// 2. Формирование порта как Input Push-up
    GPIOB->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
    GPIOB->CRH |= (0x02 << GPIO_CRH_CNF8_Pos); 		// Input with pull-up / pull-down
//    GPIOB->ODR &= ~(1 << 8);                   		// Подтяжка к GND
    GPIOB->ODR |= (1 << 8);  // Подтяжка к VCC

    // 3. Привязка EXTI8 к порту GPIOB
    // Регистр AFIO->EXTICR[2] отвечает за линии 8-11.
    // Запись 0x0001 в поле EXTI8 выбирает Port B.
    AFIO->EXTICR[2] &= ~AFIO_EXTICR3_EXTI8;
    AFIO->EXTICR[2] |= AFIO_EXTICR3_EXTI8_PB;

    // 4. Настройка самой линии EXTI8
    EXTI->IMR |= EXTI_IMR_MR8;      // Разрешить запрос прерывания (Interrupt Mask)
    EXTI->RTSR &= ~EXTI_RTSR_TR8;   // Отключаем прерывание по возрастанию (Rising edge)
    EXTI->FTSR |= EXTI_FTSR_TR8; 	// Прерывание по спаду (Falling edge)


    // 5. Включение прерывания в NVIC
    // Линии EXTI5..9 объединены в один вектор прерывания
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    NVIC_SetPriority(EXTI9_5_IRQn, 0);

    // 6. Сбрасываем аппаратный флаг
    photo_transistor_triggered = 0;
}

uint8_t photoTransistor_getStatus(void) {
	if (GPIOB->IDR & (1 << 8))
		return 0;
	else
    	return 1;
}

void EXTI9_5_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR8) {	// Проверяем, что это именно 8-я линия
        EXTI->PR |= EXTI_PR_PR8;	// Сброс флага в самом начале
        photo_transistor_triggered = 1;
    }
}
