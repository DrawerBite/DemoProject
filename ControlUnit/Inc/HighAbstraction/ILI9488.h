#pragma once

#include <stm32f1xx.h>
#include "spi1.h"

extern const uint16_t LCD_X_MAX;
extern const uint16_t LCD_Y_MAX;

void ILI9448_init();
void LCD_init();
void LCD_Command(uint8_t cmd);
void LCD_Data(uint8_t data);
void Delay_ms(uint32_t ms);

void LCD_WritePixel(uint16_t color565);
void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_FillScreen(uint16_t color565);
void LCD_FillSquare(uint16_t color565, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void LCD_DrawString(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);
void LCD_DrawChar7x11(uint16_t x, uint16_t y, uint8_t c, uint16_t color, uint16_t bg, uint8_t size);
void LCD_DrawString7x11(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size);



static inline void DC_set1() {
	GPIOA->BSRR = GPIO_BSRR_BS2;
}
static inline void DC_set0() {
	GPIOA->BSRR = GPIO_BSRR_BR2;
}
static inline void CS_set1() {
	GPIOA->BSRR = GPIO_BSRR_BS4;
}
static inline void CS_set0() {
	GPIOA->BSRR = GPIO_BSRR_BR4;
}
