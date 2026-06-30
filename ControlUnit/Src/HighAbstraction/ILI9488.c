#include <HighAbstraction/ILI9488.h>
#include "HighAbstraction/font5x7.h"
#include "HighAbstraction/font7x11.h"


/*
 * SPI1:
 * PA4 (CS)
 * PA5 (SCK)
 * PA6 (MISO)
 * PA7 (MOSI)
 *
 * ILI9488:
 * PA2 (DC/RS)
 * PA3 (RST)
 */

const uint16_t LCD_X_MAX = 479;
const uint16_t LCD_Y_MAX = 319;

void ILI9448_init() {
	// 1. Инициализация SPI1
	spi1_init();

	// 2. Тактирование
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; //Тактирование GPIO PA портов

	// 3. Очищаем настройки для пинов PA2, PA3
	GPIOA->CRL &= ~(GPIO_CRL_MODE2 | GPIO_CRL_CNF2 |
					GPIO_CRL_MODE3 | GPIO_CRL_CNF3);

	// 4. Настраиваем PA2, PA3 как General Purpose Output Push-Pull (50MHz)
	// MODE = 11 (50MHz), CNF = 00 (Push-Pull)
	GPIOA->CRL |= (GPIO_CRL_MODE2_1 | GPIO_CRL_MODE2_0) |
	              (GPIO_CRL_MODE3_1 | GPIO_CRL_MODE3_0);

	// 5. Сброс
	GPIOA->BSRR = GPIO_BSRR_BR3; // RST Low
	Delay_ms(100);
	GPIOA->BSRR = GPIO_BSRR_BS3; // RST High
	Delay_ms(120);

	// 6. Установка настроек LCD
	LCD_init();
}

void LCD_init() {
	// 1. Software Reset (0x01):
	LCD_Command(0x01);
	Delay_ms(100);

	// 2. Interface Mode Control (0xB0):
	LCD_Command(0xB0);
	LCD_Data(0x00);

	// 3. Interface Pixel Format (0x3A)
	LCD_Command(0x3A);
	LCD_Data(0x66); // 18-bit color для SPI

	// 4. Sleep Out (0x11)
	LCD_Command(0x11);
	Delay_ms(120);

	// 5. Настройка порядка цветов и ориентации (горизонтальная)
	LCD_Command(0x36);
	LCD_Data(0x28); // MV=1, MX=0, MY=0, BGR=1 -> 0x28 (стандартная горизонтальная ориентация)

	// 6. Display ON (0x29):
	LCD_Command(0x29);
}

void LCD_Command(uint8_t cmd) {
    DC_set0();
    CS_set0();
    spi1_sendByte(cmd);
    CS_set1();
}

void LCD_Data(uint8_t data) {
    DC_set1();
    CS_set0();
    spi1_sendByte(data);
    CS_set1();
}






void LCD_WritePixel(uint16_t color565) {
    // Разделяем 16-битный цвет на компоненты и расширяем до 6 бит
    uint8_t r = (color565 >> 11) << 3; // 5 бит красного -> верхние 6 бит байта
    uint8_t g = ((color565 >> 5) & 0x3F) << 2; // 6 бит зеленого
    uint8_t b = (color565 & 0x1F) << 3; // 5 бит синего

    LCD_Data(r);
    LCD_Data(g);
    LCD_Data(b);
}

void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    LCD_Command(0x2A); // Column Address Set
    LCD_Data(x0 >> 8); LCD_Data(x0 & 0xFF);
    LCD_Data(x1 >> 8); LCD_Data(x1 & 0xFF);

    LCD_Command(0x2B); // Page Address Set
    LCD_Data(y0 >> 8); LCD_Data(y0 & 0xFF);
    LCD_Data(y1 >> 8); LCD_Data(y1 & 0xFF);

    LCD_Command(0x2C); // Memory Write (команда записи в GRAM)
}

void LCD_FillScreen(uint16_t color565) {
    // 1. Устанавливаем окно на весь экран (0,0) - (319, 479)
    LCD_SetAddressWindow(0, 0, LCD_X_MAX, LCD_Y_MAX);

    // 2. Подготавливаем компоненты цвета RGB666 (18-бит)
    // Разделяем 16-битный цвет на R, G, B байты
    uint8_t r = (color565 >> 11) << 3;
    uint8_t g = ((color565 >> 5) & 0x3F) << 2;
    uint8_t b = (color565 & 0x1F) << 3;

    // 3. Прямое управление пинами
    DC_set1(); // DC = 1 (Данные)
    CS_set0(); // CS = 0 (Выбор чипа)

    // 4. Цикл заливки всех пикселей (320 * 480)
    for (uint32_t i = 0; i < 320 * 480; i++) {
        // Ждем готовности передатчика и кидаем 3 байта подряд
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = r;
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = g;
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = b;
    }

    // 5. Ждем окончания физической передачи последнего байта
    while (SPI1->SR & SPI_SR_BSY);
    CS_set1();
}

void LCD_FillSquare(uint16_t color565, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // 1. Устанавливаем окно
    LCD_SetAddressWindow(x0, y0, x1, y1);

    // 2. Подготавливаем компоненты цвета RGB666 (18-бит)
    // Разделяем 16-битный цвет на R, G, B байты
    uint8_t r = (color565 >> 11) << 3;
    uint8_t g = ((color565 >> 5) & 0x3F) << 2;
    uint8_t b = (color565 & 0x1F) << 3;

    // 3. Прямое управление пинами
    DC_set1(); // DC = 1 (Данные)
    CS_set0(); // CS = 0 (Выбор чипа)

    // 4. Вычисляем количество пикселей для заполнения
    uint32_t num_pixels = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1);

    // 5. Цикл заливки пикселей
    for (uint32_t i = 0; i < num_pixels; i++) {
        // Ждем готовности передатчика и кидаем 3 байта подряд
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = r;
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = g;
        while (!(SPI1->SR & SPI_SR_TXE));
        SPI1->DR = b;
    }

    // 6. Ждем окончания физической передачи последнего байта
    while (SPI1->SR & SPI_SR_BSY);
    CS_set1();
}

void LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 126) c = '?';
    uint8_t c_idx = c - 32;

    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = font5x7[c_idx][i];
        for (int8_t j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                if (size == 1) {
                    LCD_SetAddressWindow(x + i, y + j, x + i, y + j);
                    LCD_WritePixel(color);
                } else {
                    LCD_FillSquare(color, x + i * size, y + j * size, x + (i + 1) * size - 1, y + (j + 1) * size - 1);
                }
            } else if (bg != color) {
                if (size == 1) {
                    LCD_SetAddressWindow(x + i, y + j, x + i, y + j);
                    LCD_WritePixel(bg);
                } else {
                    LCD_FillSquare(bg, x + i * size, y + j * size, x + (i + 1) * size - 1, y + (j + 1) * size - 1);
                }
            }
        }
    }
}

void LCD_DrawString(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size) {
    while (*str) {
        LCD_DrawChar(x, y, *str, color, bg, size);
        x += 6 * size; // 5 пикселей символ + 1 пиксель интервал
        if (x + 6 * size > 480) {
            break; // Выход за границу экрана
        }
        str++;
    }
}

static uint8_t utf8_decode_to_cp1251(const char* str, int* bytes_consumed) {
    uint8_t b = (uint8_t)str[0];
    if (b < 0x80) {
        *bytes_consumed = 1;
        return b;
    }
    if (b == 0xD0) {
        uint8_t next = (uint8_t)str[1];
        if (next >= 0x90 && next <= 0xBF) {
            *bytes_consumed = 2;
            return next + 0x30;
        }
        if (next == 0x81) {
            *bytes_consumed = 2;
            return 0xA8; // Ё
        }
        *bytes_consumed = 2;
        return 0x20; // fallback to space
    }
    if (b == 0xD1) {
        uint8_t next = (uint8_t)str[1];
        if (next >= 0x80 && next <= 0x8F) {
            *bytes_consumed = 2;
            return next + 0x70;
        }
        if (next == 0x91) {
            *bytes_consumed = 2;
            return 0xB8; // ё
        }
        *bytes_consumed = 2;
        return 0x20; // fallback to space
    }
    if (b == 0xC2) {
        uint8_t next = (uint8_t)str[1];
        if (next == 0xB0) { *bytes_consumed = 2; return 0xB0; } // ° (degree)
        if (next == 0xA7) { *bytes_consumed = 2; return 0xA7; } // §
        if (next == 0xAB) { *bytes_consumed = 2; return 0xAB; } // «
        if (next == 0xBB) { *bytes_consumed = 2; return 0xBB; } // »
        if (next == 0xB1) { *bytes_consumed = 2; return 0xB1; } // ±
        if (next == 0xB6) { *bytes_consumed = 2; return 0xB6; } // ¶
        if (next == 0xB7) { *bytes_consumed = 2; return 0xB7; } // ·
        *bytes_consumed = 2;
        return 0x20;
    }
    if (b == 0xE2) {
        uint8_t next1 = (uint8_t)str[1];
        uint8_t next2 = (uint8_t)str[2];
        if (next1 == 0x84 && next2 == 0x96) {
            *bytes_consumed = 3;
            return 0xB9; // №
        }
        *bytes_consumed = 3;
        return 0x20;
    }
    *bytes_consumed = 1;
    return 0x20;
}

void LCD_DrawChar7x11(uint16_t x, uint16_t y, uint8_t c, uint16_t color, uint16_t bg, uint8_t size) {
    uint8_t c_idx = 0;
    if (c >= 0x20 && c <= 0x7F) {
        c_idx = c - 0x20;
    } else if (c >= 0xA0 && c <= 0xFF) {
        c_idx = c - 0xA0 + 96;
    } else {
        c_idx = 0; // space (0x20 is at index 0)
    }

    // Предварительно вычисляем RGB666 для переднего и заднего плана
    uint8_t r_fg = (color >> 11) << 3;
    uint8_t g_fg = ((color >> 5) & 0x3F) << 2;
    uint8_t b_fg = (color & 0x1F) << 3;
    uint8_t r_bg = (bg >> 11) << 3;
    uint8_t g_bg = ((bg >> 5) & 0x3F) << 2;
    uint8_t b_bg = (bg & 0x1F) << 3;

    // Устанавливаем адресное окно на весь символ
    LCD_SetAddressWindow(x, y, x + 7 * size - 1, y + 11 * size - 1);

    DC_set1();
    CS_set0();

    for (int8_t row = 0; row < 11; row++) {
        uint8_t row_val = Font7x11[c_idx * 11 + row];
        for (uint8_t rep_r = 0; rep_r < size; rep_r++) {
            for (int8_t col = 0; col < 7; col++) {
                uint8_t is_fg = (row_val >> col) & 1;
                uint8_t pr = is_fg ? r_fg : r_bg;
                uint8_t pg = is_fg ? g_fg : g_bg;
                uint8_t pb = is_fg ? b_fg : b_bg;
                for (uint8_t rep_c = 0; rep_c < size; rep_c++) {
                    while (!(SPI1->SR & SPI_SR_TXE)); SPI1->DR = pr;
                    while (!(SPI1->SR & SPI_SR_TXE)); SPI1->DR = pg;
                    while (!(SPI1->SR & SPI_SR_TXE)); SPI1->DR = pb;
                }
            }
        }
    }

    while (SPI1->SR & SPI_SR_BSY);
    CS_set1();
}

void LCD_DrawString7x11(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg, uint8_t size) {
    while (*str) {
        int bytes_consumed = 0;
        uint8_t cp1251_char = utf8_decode_to_cp1251(str, &bytes_consumed);
        if (cp1251_char != 0) {
            LCD_DrawChar7x11(x, y, cp1251_char, color, bg, size);
            x += 8 * size; // 7 pixels char width + 1 pixel space
            if (x + 8 * size > 480) {
                break;
            }
        }
        str += bytes_consumed;
    }
}
