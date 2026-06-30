#include "spi1.h"

/*
 * PA4 (CS)
 * PA5 (SCK)
 * PA6 (MISO) - не используется
 * PA7 (MOSI)
 */

void spi1_init() {
	// 1. Тактирование SPI
	RCC->APB2ENR |= RCC_APB2ENR_SPI1EN; //Тактирование SPI1
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; //Тактирование GPIO PA портов
	RCC->APB2ENR |= RCC_APB2ENR_AFIOEN; //Тактирование альтернативной функции

	// 2. Очищаем настройки для пинов PA4, PA5, PA6, PA7
	GPIOA->CRL &= ~(GPIO_CRL_MODE4 | GPIO_CRL_CNF4 |
					GPIO_CRL_MODE5 | GPIO_CRL_CNF5 |
					GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
					GPIO_CRL_MODE7 | GPIO_CRL_CNF7);

	// 3. Настраиваем PA4 как General Purpose Output Push-Pull (50MHz)
	// MODE = 11 (50MHz), CNF = 00 (Push-Pull)
	GPIOA->CRL |= (GPIO_CRL_MODE4_1 | GPIO_CRL_MODE4_0);

	// 4. Настраиваем PA5 (SCK) и PA7 (MOSI) как Alternate Function Push-Pull (50MHz)
	// MODE = 11 (50MHz), CNF = 10 (AF Push-Pull)
	GPIOA->CRL |= (GPIO_CRL_MODE5_1 | GPIO_CRL_MODE5_0 | GPIO_CRL_CNF5_1) |
	              (GPIO_CRL_MODE7_1 | GPIO_CRL_MODE7_0 | GPIO_CRL_CNF7_1);

	// 5. Настройка SPI
	SPI1->CR1 |= SPI_CR1_MSTR | SPI_CR1_SSI | SPI_CR1_SSM; // Master, Soft NSS
	SPI1->CR1 &= ~SPI_CR1_BR; // Сброс
//	SPI1->CR1 |= (0x02 << SPI_CR1_BR_Pos); // 001: fPCLK / 4 = 18 MHz

	// 6. Включить SPI
	SPI1->CR1 |= SPI_CR1_SPE;
}

void spi1_sendByte(uint8_t data) {
    while (!(SPI1->SR & SPI_SR_TXE)); // Ждем готовности
    SPI1->DR = data;
    while (SPI1->SR & SPI_SR_BSY);    // Ждем окончания передачи
}

void spi1_sendData(uint8_t *data, uint8_t size) {
	for (int i = 0; i < size; i++) {
	    while (!(SPI1->SR & SPI_SR_TXE)); // Ждем готовности
	    SPI1->DR = data[i];
	}

}


