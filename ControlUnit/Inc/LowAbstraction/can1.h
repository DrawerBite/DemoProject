#pragma once

#include <stm32f1xx.h>
#include "main.h"

#define CAN_MAIN_ADRESS 0x00

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} CAN_Frame;

#define CAN_QUEUE_SIZE 16

void can1_init(void);

/*
 * Отправить данные по CAN
 */
void can1_tx_data(const uint8_t *data, uint8_t dlc);

/*
 * Извлечь кадр из приемной очереди CAN
 * Возвращает 1 в случае успеха, 0 если очередь пуста
 */
uint8_t can1_queue_pop(CAN_Frame *frame);

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
