#include <HighAbstraction/XPT2046.h>

uint16_t x_raw;
uint16_t y_raw;

void XPT2046_init() {
	spi2_init();
}

static uint16_t XPT2046_ReadAxis(uint8_t cmd) {
    uint16_t raw_data = 0;

    // Снижаем скорость SPI2 до 2 МГц
    spi2_set_speed(SPI_CR1_BR_1 | SPI_CR1_BR_0);

    GPIOB->BSRR = GPIO_BSRR_BR9; // CS = LOW (Выбираем тачскрин PB9)

    spi2_transmit(cmd);

    // Читаем два байта. XPT2046 отдает 12 бит, выровненных по левому краю
    uint8_t high = spi2_transmit(0x00);
    uint8_t low  = spi2_transmit(0x00);

    GPIOB->BSRR = GPIO_BSRR_BS9; // CS = HIGH (Отключаем PB9)

    // Возвращаем скорость SPI2 для работы с SD-картой (8 МГц)
    spi2_set_speed(SPI_CR1_BR_0);

    // Склеиваем 12 бит: старшие 7 бит из high и 5 бит из low
    raw_data = ((high << 8) | low) >> 3;

    return raw_data;
}

static uint16_t map(uint16_t val, uint16_t in_min, uint16_t in_max, uint16_t out_max) {
    if (val < in_min) val = in_min;
    if (val > in_max) val = in_max;
    return (val - in_min) * out_max / (in_max - in_min);
}

uint16_t XPT2046_getX() {
	y_raw = XPT2046_ReadAxis(0x90); // В ландшафтном режиме ось Y соответствует X экрана
	int16_t x = 480 - map(y_raw, RAW_Y_MIN, RAW_Y_MAX, 480) + TOUCH_X_OFFSET;
	if (x < 0) return 0;
	if (x >= 480) return 479;
	return x;
}

uint16_t XPT2046_getY() {
	x_raw = XPT2046_ReadAxis(0xD0); // В ландшафтном режиме ось X соответствует Y экрана
	int16_t y = 320 - map(x_raw, RAW_X_MIN, RAW_X_MAX, 320) + TOUCH_Y_OFFSET;
	if (y < 0) return 0;
	if (y >= 320) return 319;
	return y;
}

uint8_t XPT2046_isPressed() {
    // PB8 (T_IRQ) равен LOW при касании
    return (GPIOB->IDR & (1 << 8)) == 0;
}
