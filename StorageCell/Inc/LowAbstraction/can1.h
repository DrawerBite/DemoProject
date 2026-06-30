#pragma once

#include <stdint.h>
#include <stm32f103xb.h>

#define CAN_MODULE_ADRESS 0x01
#define CAN_MAIN_ADRESS 0x00

void can1_init(void);

/*
 * Отправить данные по CAN
 */
void can1_tx_data(const uint8_t *data, uint8_t dlc);

extern volatile uint8_t can_rx_flag;
extern volatile uint8_t can_rx_dlc;
extern volatile uint8_t can_rx_buf[8];

/*
 * Прерывание при приёме сигнала из FIFO1
 */
void CAN_RX1_IRQHandler(void);


/*
 * CAN DATA
 *
 * 1, 2 байты:
 * Адресс обращения
 *
 * 3й байт:
 * 0 - Установка данных модулю
 * 1 - Запрос данных о модуле в БУ
 * 2 - Подсветка светодиода модуля
 * 3 - Сообщение БУ об изменении значений
 * 4 - Сердцебиение модуля
 * 5 - Модуль должен сбросить holes_diff
 *
 * 5, 6 байты:
 * Кол-во отверстий в модуле
 * 7, 8 байты:
 * Изменение кол-ва отверстий в модуле
 */
