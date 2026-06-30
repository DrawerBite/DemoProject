#include "HighAbstraction/encoder.h"

/*
 * ENCODER
 * PA0 - CH_A
 * PA1 - CH_B
 * TIM2 (Hardware Encoder Mode)
 */

static void tim2_init_encoder(void);
static void encoder_gpio_init(void);

void encoder_init(void) {
    encoder_gpio_init();
    tim2_init_encoder();
}

static void tim2_init_encoder(void) {
    // 1. Включаем тактирование TIM2
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR; // Задержка для стабилизации тактирования (критично!)

    // 2. Сбрасываем управляющие регистры
    TIM2->CR1 = 0;
    TIM2->SMCR = 0;
    TIM2->PSC = 0;      // Предделитель = 0 (максимальное разрешение)

    // 3. Настраиваем каналы входа TI1 и TI2
    // CC1S = 01 (канал настроен на вход TI1)
    // IC1F = 0011 (цифровой фильтр F_DTS/8, N=4) для фильтрации дребезга
    // CC2S = 01 (канал настроен на вход TI2)
    // IC2F = 0011 (цифровой фильтр F_DTS/8, N=4) для фильтрации дребезга
    TIM2->CCMR1 = (0x01 << 0) | (0x03 << 4) | (0x01 << 8) | (0x03 << 12);

    // 4. Полярность входов по умолчанию (активный высокий уровень)
    TIM2->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P);

    // 5. Выбираем режим энкодера 3 (SMS = 011) — счет по обоим фронтам обоих каналов
    TIM2->SMCR |= 0x03;

    // 6. Устанавливаем максимальный период (16-битный автоперезапуск)
    TIM2->ARR = 0xFFFF;

    // 7. Сбрасываем счетчик в 0
    TIM2->CNT = 0;

    // 8. Запускаем таймер
    TIM2->CR1 |= TIM_CR1_CEN;
}

static void encoder_gpio_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;

    // PA0 = A, PA1 = B (input pull-up)
    GPIOA->CRL &= ~(
        GPIO_CRL_MODE0 | GPIO_CRL_CNF0 |
        GPIO_CRL_MODE1 | GPIO_CRL_CNF1
    );

    // CNF = 10 (Input with pull-up / pull-down), MODE = 00 (Input mode)
    GPIOA->CRL |=
        (0x02 << GPIO_CRL_CNF0_Pos) | 
        (0x02 << GPIO_CRL_CNF1_Pos);

    GPIOA->ODR |= (1 << 0) | (1 << 1); // Подтяжка к VCC (Pull-up)
}

EncoderDirection encoder_getDirection(void) {
    if (TIM2->CR1 & TIM_CR1_DIR) {
        return OUT;
    } else {
        return IN;
    }
}

int16_t encoder_getValue(void) {
    return (int16_t)TIM2->CNT;
}
