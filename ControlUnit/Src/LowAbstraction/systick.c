#include "LowAbstraction/systick.h"

static volatile uint32_t ms_ticks = 0;

void systick_init(void) {
    // 72 MHz system clock, reload value for 1 ms = 72000
    SysTick_Config(72000000 / 1000);
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
        // Простое ожидание без сна (без __WFI)
    }
}
