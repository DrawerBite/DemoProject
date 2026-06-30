#include "LowAbstraction/systick.h"

static volatile uint32_t ms_ticks = 0;

void systick_init(void) {
    // Вычисляем значение перезапуска исходя из текущей частоты в МГц
    SysTick_Config(RCC_CLOCK_MHZ * 1000000 / 1000);
}

uint32_t systick_get_ticks(void) {
    return ms_ticks;
}

void SysTick_Handler(void) {
    ms_ticks++;
}

void Delay_ms(uint32_t ms) {
    uint32_t start = ms_ticks;
    while ((ms_ticks - start) < ms) {
        __WFI(); // Входим в режим сна для энергосбережения
    }
}
