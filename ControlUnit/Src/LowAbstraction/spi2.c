#include "spi2.h"

qwertyuiop;


/*
 * APB1 = 32 MHz
 * SPI2 = 2 MHz
 *
 * PB12 (CS)
 * PB13 (SCK)
 * PB14 (MISO)
 * PB15 (MOSI)
 */

void spi2_init() {
    // 1. Тактирование SPI и GPIO
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; // Тактиров	ание SPI2
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; // Тактирование GPIO PB
    RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; // Тактирование альтернативных функций

	// 2. Настраиваем пины PB8..PB15 через регистр CRH:
	// PB15 (MOSI)   -> Alternate Function Output Push-Pull 50MHz (0xB)
	// PB14 (MISO)   -> Input with pull-up/pull-down (0x8)
	// PB13 (SCK)    -> Alternate Function Output Push-Pull 50MHz (0xB)
	// PB12 (SD_CS)  -> General Purpose Output Push-Pull 50MHz (0x3)
	// PB11 (Unused) -> Input Floating (0x4)
	// PB10 (Unused) -> Input Floating (0x4)
	// PB9  (T_CS)   -> General Purpose Output Push-Pull 50MHz (0x3)
	// PB8  (T_IRQ)  -> Input with pull-up/pull-down (0x8)
	GPIOB->CRH = 0xB8B34438;

	// 3. Включаем внутреннюю подтяжку (pull-up) для MISO (PB14) и T_IRQ (PB8),
	// а также устанавливаем высокий уровень (HIGH) по умолчанию на линиях CS (PB12, PB9)
	GPIOB->ODR |= (1 << 8) | (1 << 9) | (1 << 12) | (1 << 14);

	// 5. Настройка SPI
	SPI2->CR1 |= SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM; // Master, Soft NSS
	SPI2->CR1 &= ~SPI_CR1_BR; // Сброс
	SPI2->CR1 |= (0x03 << SPI_CR1_BR_Pos); // 011: fPCLK / 16 = 2 MHz

	// 6. Включить SPI
	SPI2->CR1 |= SPI_CR1_SPE;
}

void spi2_sendByte(uint8_t data) {
    while (!(SPI2->SR & SPI_SR_TXE));
    SPI2->DR = data;
    while (SPI2->SR & SPI_SR_BSY);
}

uint8_t spi2_transmit(uint8_t data) {
    volatile uint32_t dummy;
    dummy = SPI2->DR; // Очистка DR перед передачей
    dummy = SPI2->SR; // Очистка флагов ошибок (в т.ч. OVR)

    while (!(SPI2->SR & SPI_SR_TXE));
    SPI2->DR = data;

    while (!(SPI2->SR & SPI_SR_RXNE));
    return (uint8_t)SPI2->DR;
}

void spi2_set_speed(uint16_t prescaler) {
    SPI2->CR1 &= ~SPI_CR1_SPE; // Выключаем SPI
    SPI2->CR1 &= ~SPI_CR1_BR;  // Очищаем делитель
    SPI2->CR1 |= (prescaler & SPI_CR1_BR); // Устанавливаем новый
    SPI2->CR1 |= SPI_CR1_SPE;  // Включаем обратно
}

