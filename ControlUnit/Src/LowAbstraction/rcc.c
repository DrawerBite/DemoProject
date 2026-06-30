#include "LowAbstraction/rcc.h"

uint8_t rcc_8mhz_init(void)
{
    __IO uint32_t StartUpCounter;

    // 1. Включаем внешний кварц HSE = 8 МГц
    RCC->CR |= RCC_CR_HSEON;

    // 2. Ждём готовности HSE
    for (StartUpCounter = 0; ; StartUpCounter++)
    {
        if (RCC->CR & RCC_CR_HSERDY)
            break;

        if (StartUpCounter > 0x100000)
        {
            RCC->CR &= ~RCC_CR_HSEON;
            return 1; // HSE не запустился
        }
    }

    // 3. Отключаем PLL (он нам не нужен)
    RCC->CR &= ~RCC_CR_PLLON;

    // 4. Настройки FLASH (при 8 МГц — 0 wait states)
    FLASH->ACR &= ~FLASH_ACR_LATENCY;

    // 5. Делители шин: всё = /1
    RCC->CFGR &= ~(RCC_CFGR_HPRE  |
                   RCC_CFGR_PPRE1 |
                   RCC_CFGR_PPRE2);

    // 6. Переключаем SYSCLK = HSE
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |=  (0x01 << RCC_CFGR_SW_Pos); // SW = HSE

    // 7. Ждём, пока переключится
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != (0x01 << RCC_CFGR_SWS_Pos)) {}

    // 8. Отключаем HSI (не нужен)
    RCC->CR &= ~RCC_CR_HSION;

    return 0;
}

/**
 * @brief  Универсальная настройка системной частоты на базе HSE (8 МГц).
 *         Допустимые частоты: 8, 16, 24, 32, 40, 48, 56, 64, 72 МГц.
 *
 * @param  mhz: Желаемая частота в МГц (например: 8, 16, 32, 72).
 *
 * @retval 0 - Успешно настроено.
 *         1 - Ошибка: Не запустился внешний кварц HSE.
 *         2 - Ошибка: Не запустился PLL.
 *         3 - Ошибка: Не удалось переключить SYSCLK на выбранный источник.
 *         4 - Ошибка: Передана недопустимая частота (не кратна 8 или вне диапазона 8..72).
 */
uint8_t rcc_set_frequency(uint8_t mhz) {
    // 1. Валидация входных параметров
    if (mhz < 8 || mhz > 72 || (mhz % 8) != 0)
    {
        return 4; // Недопустимая частота
    }

    __IO uint32_t timeout = 0;

    // 2. Безопасный переход на HSI (внутренний генератор) перед переконфигурацией
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {} // Ожидаем готовности HSI

    // Переключаем системную шину (SYSCLK) на HSI
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0x00) {} // Ожидаем подтверждения переключения

    // Выключаем PLL и HSE для их безопасной настройки
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);
    RCC->CIR = 0x00000000;
    RCC->CFGR = 0x00000000;

    // 3. Запуск внешнего кварца HSE (8 МГц)
    RCC->CR |= RCC_CR_HSEON;
    for (timeout = 0; ; timeout++)
    {
        if (RCC->CR & RCC_CR_HSERDY)
        {
            break; // Внешний кварц готов к работе
        }
        if (timeout > 0x100000)
        {
            RCC->CR &= ~RCC_CR_HSEON;
            return 1; // Аппаратная ошибка запуска HSE
        }
    }

    // 4. Настройка задержки Flash-памяти (Latency) в зависимости от частоты
    FLASH->ACR |= FLASH_ACR_PRFTBE; // Включаем буфер предвыборки (Prefetch)
    FLASH->ACR &= ~FLASH_ACR_LATENCY;
    if (mhz <= 24)
    {
        FLASH->ACR |= FLASH_ACR_LATENCY_0; // 0 wait states для частот до 24 МГц
    }
    else if (mhz <= 48)
    {
        FLASH->ACR |= FLASH_ACR_LATENCY_1; // 1 wait state для частот от 24 до 48 МГц
    }
    else
    {
        FLASH->ACR |= FLASH_ACR_LATENCY_2; // 2 wait states для частот свыше 48 МГц
    }

    // 5. Динамический расчет делителей шин
    uint32_t cfgr_reg = 0;

    // Шина AHB (HCLK) всегда работает на полной частоте ядра (/1)
    cfgr_reg |= RCC_CFGR_HPRE_DIV1;

    // Шина APB1 (PCLK1) не должна превышать 36 МГц
    if (mhz <= 36)
    {
        cfgr_reg |= RCC_CFGR_PPRE1_DIV1; // Делитель /1
    }
    else
    {
        cfgr_reg |= RCC_CFGR_PPRE1_DIV2; // Делитель /2 (например, 72 МГц / 2 = 36 МГц)
    }

    // Шина APB2 (PCLK2) работает на полной частоте (/1, максимум 72 МГц)
    cfgr_reg |= RCC_CFGR_PPRE2_DIV1;

    // Тактирование ADC не должно превышать 14 МГц. Подбираем делитель:
    if (mhz <= 28)
    {
        cfgr_reg |= RCC_CFGR_ADCPRE_DIV2; // Например, 24 МГц / 2 = 12 МГц
    }
    else if (mhz <= 56)
    {
        cfgr_reg |= RCC_CFGR_ADCPRE_DIV4; // Например, 48 МГц / 4 = 12 МГц
    }
    else
    {
        cfgr_reg |= RCC_CFGR_ADCPRE_DIV6; // Например, 72 МГц / 6 = 12 МГц
    }

    // Настройка делителя для модуля USB (требуется ровно 48 МГц)
    if (mhz == 72)
    {
        cfgr_reg &= ~RCC_CFGR_USBPRE; // Делитель PLLCLK / 1.5 (72 / 1.5 = 48 МГц)
    }
    else if (mhz == 48)
    {
        cfgr_reg |= RCC_CFGR_USBPRE;  // Делитель PLLCLK / 1 (48 / 1 = 48 МГц)
    }

    // Записываем сформированные делители в регистр конфигурации
    RCC->CFGR = cfgr_reg;

    // 6. Выбор источника тактирования
    if (mhz == 8)
    {
        // Для частоты 8 МГц умножитель PLL не нужен — работаем напрямую от HSE
        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_HSE;

        for (timeout = 0; ; timeout++)
        {
            if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_HSE)
            {
                break; // Успешно переключились напрямую на HSE
            }
            if (timeout > 0x100000)
            {
                return 3; // Ошибка переключения
            }
        }
    }
    else
    {
        // Для частот 16..72 МГц рассчитываем множитель PLL
        // Множитель равен: mhz / 8
        // В регистр записывается значение: (Множитель - 2)
        uint32_t pll_multiplier = ((mhz / 8) - 2) << RCC_CFGR_PLLMULL_Pos;

        // Настраиваем PLL: источник HSE (без деления) + рассчитанный множитель
        RCC->CFGR |= (RCC_CFGR_PLLSRC | pll_multiplier);

        // Включаем PLL
        RCC->CR |= RCC_CR_PLLON;

        for (timeout = 0; ; timeout++)
        {
            if (RCC->CR & RCC_CR_PLLRDY)
            {
                break; // PLL захватил частоту
            }
            if (timeout > 0x100000)
            {
                RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON);
                return 2; // Ошибка запуска PLL
            }
        }

        // Переключаем SYSCLK на PLL
        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_PLL;

        for (timeout = 0; ; timeout++)
        {
            if ((RCC->CFGR & RCC_CFGR_SWS) == RCC_CFGR_SWS_PLL)
            {
                break; // Успешно переключились на работу от PLL
            }
            if (timeout > 0x100000)
            {
                RCC->CFGR &= ~RCC_CFGR_SW;
                RCC->CR &= ~RCC_CR_PLLON;
                return 3; // Ошибка переключения
            }
        }
    }

    // 7. Отключение встроенного HSI для экономии энергии
    RCC->CR &= ~RCC_CR_HSION;

    return 0; // Инициализация выполнена успешно
}
