#include "main.h"

//Init statuses
uint8_t rcc_init_status;

// SUMMATOR
static uint16_t holes = 0;
int16_t holes_diff = 0;
static int16_t holes_compensation = HOLES_ADDITION;

// Photo Transistor
volatile uint8_t photo_transistor_triggered = 0; //Аппаратный флаг при срабатывании прерывания

//Encoder
EncoderDirection previousEncoderDirection = NONE;
static int16_t last_hole_encoder_val = 0;

//CAN
volatile uint8_t CAN_HELLO = 1;

int main(void) {
	// 1. Инициализация методов
	rcc_init_status = rcc_set_frequency(RCC_CLOCK_MHZ);
	led_init();
	if (rcc_init_status != 0) {
		// Быстрое мигание светодиодом при ошибке тактирования
		while (1) {
			led_set(1);
			for (volatile uint32_t i = 0; i < 100000; i++);
			led_set(0);
			for (volatile uint32_t i = 0; i < 100000; i++);
		}
	}
	systick_init();
	photoTransistor_init();
	can1_init();
	tim3_init();
	tim4_init();
    encoder_init();

	// 2. Запрос в главный модуль по CAN (рукопожатие)
	led_set(1);
	while(CAN_HELLO) {
		can1_tx_data((uint8_t[]){1}, 1);
		
		// Ожидаем ответа 800 мс неблокирующим образом
		uint32_t start = systick_get_ticks();
		while (CAN_HELLO && (systick_get_ticks() - start < 800)) {
			if (can_rx_flag) {
				uint8_t dlc = can_rx_dlc;
				uint8_t data[8];
				for (uint8_t i = 0; i < dlc; i++) {
					data[i] = can_rx_buf[i];
				}
				can_rx_flag = 0;
				Callback_CAN(data, dlc);
			}
			__WFI(); // Сон в режиме ожидания ответа
		}
	}
	led_set(0);

	// 3. Настройка начальных переменных
	last_hole_encoder_val = encoder_getValue();

    while(1) {
		// 3.0. Отправка периодического heartbeat (Команда 4) каждые 1000 мс
		static uint32_t last_heartbeat = 0;
		uint32_t current_time = systick_get_ticks();
		if (!CAN_HELLO && (current_time - last_heartbeat >= 1000)) {
			last_heartbeat = current_time;
			uint8_t heartbeatData[1];
			heartbeatData[0] = 4;
			can1_tx_data(heartbeatData, 1);
		}

		// 3.1. Обработка входящих CAN сообщений в основном потоке
		if (can_rx_flag) {
			uint8_t dlc = can_rx_dlc;
			uint8_t data[8];
			for (uint8_t i = 0; i < dlc; i++) {
				data[i] = can_rx_buf[i];
			}
			can_rx_flag = 0;
			Callback_CAN(data, dlc);
		}

		// 3.2. Обработка срабатывания датчика фототранзистора
		if (photo_transistor_triggered) {
			photo_transistor_triggered = 0;
			Callback_PhotoTransistor();
		}

		// 3.3. Энергосберегающий режим: спим до следующего прерывания
		if (!can_rx_flag && !photo_transistor_triggered) {
			__WFI();
		}
    }
}

void Callback_CAN(const uint8_t *data, uint8_t dlc) {
	// Команда 0: Установка настроек БУ -> фидеру
	if (dlc >= 7 && data[2] == 0) {
		uint16_t id = (uint16_t) ((uint16_t) data[0] << 8) | (uint16_t) data[1];
		if (CAN_MODULE_ADRESS == id) {
			holes = (uint16_t) ((uint16_t) data[3] << 8) | (uint16_t) data[4];
			holes_diff = (int16_t) ((uint16_t) data[5] << 8) | (uint16_t) data[6];
			last_hole_encoder_val = encoder_getValue(); // Синхронизируем положение энкодера
			CAN_HELLO = 0;
		}
	}
	// Команда 2: Подсветить светодиод
	if (dlc >= 3 && data[2] == 2) {
		uint16_t id = (uint16_t) ((uint16_t) data[0] << 8) | (uint16_t) data[1];
		if (CAN_MODULE_ADRESS == id) {
			led_set(1);
			tim4_start();
		}
	}
	// Команда 5: Очистить holes_diff
	if (dlc >= 3 && data[2] == 5) {
		uint16_t id = (uint16_t) ((uint16_t) data[0] << 8) | (uint16_t) data[1];
		if (CAN_MODULE_ADRESS == id) {
			holes_diff = 0;
		}
	}
}

void Callback_TIM3(void) {
    holes_diff = 0;
}

void Callback_TIM4(void) {
	led_set(0);
}

void Callback_PhotoTransistor(void) {
	int16_t previousHolesChange = holes_diff;

	int16_t current_encoder_val = encoder_getValue();
	int16_t diff = current_encoder_val - last_hole_encoder_val;

	if (diff >= ENCODER_FILTER_THRESHOLD) { // Движение вперед (IN)
		if (holes == 0) {
			int16_t compensation = diff / ENCODER_TICKS_PER_HOLE;
			if (compensation < 0) {
				compensation = 0;
			}
			holes_compensation = compensation;
			holes += holes_compensation;
			holes_diff += holes_compensation;
		}
		holes++;
		holes_diff++;
		tim3_start();

		last_hole_encoder_val = current_encoder_val;
		previousEncoderDirection = IN;
	}
	else if (diff <= -ENCODER_FILTER_THRESHOLD && holes > 0) { // Движение назад (OUT)
		if (previousEncoderDirection == IN) {
			holes--;
			holes_diff--;
		}
		holes--;
		holes_diff--;
		tim3_start();

		if (holes <= holes_compensation) {
			holes = 0;
			holes_diff = 0;
		}

		last_hole_encoder_val = current_encoder_val;
		previousEncoderDirection = OUT;
	}

	// Отправка новых данных по CAN при изменении
	if (previousHolesChange != holes_diff) {
		uint8_t newData[8];

		// Формируем дату (Команда 3)
		newData[0] = 3;
		newData[1] = holes >> 8;   // MSB
		newData[2] = holes & 0xFF; // LSB
		newData[3] = holes_diff >> 8;   // MSB
		newData[4] = holes_diff & 0xFF; // LSB

		// Отправляем данные БУ
		can1_tx_data(newData, 5);
	}
}

