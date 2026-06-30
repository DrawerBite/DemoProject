#include <HighAbstraction/SDCardSPI.h>

#define SD_CS_LOW()  GPIOB->BSRR = GPIO_BSRR_BR12
#define SD_CS_HIGH() GPIOB->BSRR = GPIO_BSRR_BS12

void Delay_ms(uint32_t ms);

uint8_t sd_init() {
    uint8_t res;

    spi2_init(); // Запустится на безопасной частоте ~140 кГц

    SD_CS_HIGH();

    // 1. Подача 120 тактов (15 байт 0xFF) при HIGH CS для надежного пробуждения
    for (int i = 0; i < 15; i++) spi2_transmit(0xFF);

    // 2. CMD0 (Software Reset) с повторными попытками для надежности (синхронизация битов при сбросе)
    for (int retry = 0; retry < 100; retry++) {
        SD_CS_LOW();
        res = sd_send_cmd(0, 0, 0x95); // CRC 0x95
        SD_CS_HIGH();
        spi2_transmit(0xFF); // Даем карте освободить MISO
        
        if (res == 0x01) break;
        Delay_ms(1);
    }

    if (res != 0x01) return 1; // Ошибка CMD0

    // 3. CMD8 (Проверка поддержки современных карт)
    SD_CS_LOW();
    res = sd_send_cmd(8, 0x1AA, 0x87);
    if (res == 0x01) {
        for(int i=0; i<4; i++) spi2_transmit(0xFF);
    }
    SD_CS_HIGH();
    spi2_transmit(0xFF); // Даем карте освободить MISO

    // 4. Цикл ожидания готовности ACMD41
    uint8_t ready = 0;
    for (volatile int i = 0; i < 1000; i++) {
        SD_CS_LOW();
        sd_send_cmd(55, 0, 0xFF); // Префикс ACMD
        SD_CS_HIGH();
        spi2_transmit(0xFF); // Даем карте освободить MISO

        SD_CS_LOW();
        res = sd_send_cmd(41, 0x40000000, 0xFF); // Аргумент HCS
        SD_CS_HIGH();
        spi2_transmit(0xFF); // Даем карте освободить MISO

        if (res == 0x00) {
            ready = 1;
            break;
        }
        Delay_ms(1); // Даем карте время обработать команду
    }

    if (!ready) return 2; // Карта не инициализировалась

    // 5. Инициализация успешна. Переключаемся на высокую скорость работы.
    // Делитель /8 даст 36MHz / 8 = 4.5 MHz (очень надежно)
    spi2_set_speed(SPI_CR1_BR_1);

    return 0; // Успех
}

uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc) {
    uint8_t res;

    spi2_transmit(0xFF); // Синхронизация перед командой

    spi2_transmit(cmd | 0x40);
    spi2_transmit(arg >> 24);
    spi2_transmit(arg >> 16);
    spi2_transmit(arg >> 8);
    spi2_transmit(arg);
    spi2_transmit(crc);

    // 4. Ждем ответа (обычно до 8 байт "пустышек" 0xFF, но может быть дольше при инициализации)
    for (int i = 0; i < 128; i++) {
        res = spi2_transmit(0xFF);
        if (!(res & 0x80)) return res;
    }
    return res;
}

uint8_t sd_read_sector(uint32_t sector_addr, uint8_t *buffer) {
    uint8_t res;
    uint16_t timeout;

    SD_CS_LOW();

    res = sd_send_cmd(17, sector_addr, 0xFF);
    if (res != 0x00) {
        SD_CS_HIGH();
        spi2_transmit(0xFF);
        return 1; // Ошибка команды
    }

    timeout = 0xFFFF;
    while (spi2_transmit(0xFF) != 0xFE) {
        if (--timeout == 0) {
            SD_CS_HIGH();
            spi2_transmit(0xFF);
            return 2; // Таймаут
        }
    }

    for (uint16_t i = 0; i < 512; i++) {
        buffer[i] = spi2_transmit(0xFF);
    }

    spi2_transmit(0xFF); // Пропуск CRC
    spi2_transmit(0xFF);

    SD_CS_HIGH();
    spi2_transmit(0xFF);

    return 0;
}

uint8_t sd_write_sector(uint32_t sector_addr, const uint8_t *buffer) {
    uint8_t res;
    uint16_t timeout;

    SD_CS_LOW();

    res = sd_send_cmd(24, sector_addr, 0xFF);
    if (res != 0x00) {
        SD_CS_HIGH();
        spi2_transmit(0xFF);
        return 1;
    }

    spi2_transmit(0xFF);
    spi2_transmit(0xFE); // Маркер начала данных

    for (uint16_t i = 0; i < 512; i++) {
        spi2_transmit(buffer[i]);
    }

    spi2_transmit(0xFF); // Пакет CRC
    spi2_transmit(0xFF);

    res = spi2_transmit(0xFF);
    if ((res & 0x1F) != 0x05) {
        SD_CS_HIGH();
        spi2_transmit(0xFF);
        return 2; // Карта не приняла данные
    }

    timeout = 0xFFFF;
    while (spi2_transmit(0xFF) == 0x00) {
        if (--timeout == 0) {
            SD_CS_HIGH();
            spi2_transmit(0xFF);
            return 3; // Тайм-аут записи
        }
    }

    SD_CS_HIGH();
    spi2_transmit(0xFF);
    return 0;
}
