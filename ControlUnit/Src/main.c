#include "main.h"

//Init statuses
uint8_t rcc_init_status;

//FatFs
FATFS fs;          // Хендл файловой системы
FIL file;          // Хендл файла
FRESULT res;       // Код результата
UINT br;           // Счетчик прочитанных байт
char buffer[100];  // Буфер для текста

//MiniIni
#define CONFIG_NAME "config.ini"

int main(void)
{
	rcc_init_status = rcc_set_frequency(RCC_CLOCK_MHZ);
	systick_init();
	spi2_init();
	ILI9448_init();
	if (rcc_init_status != 0) {
		LCD_FillScreen(0xF81F); // Сиреневый экран при ошибке тактирования
		while(1) {}
	}
	XPT2046_init();
	can1_init();

    // 1. Монтируем файловую систему
    res = f_mount(&fs, "", 1);
    if (res == FR_OK) {

        // Зеленый экран на 500 мс означает успешную инициализацию SD-карты!
        LCD_FillScreen(0x07E0);
        Delay_ms(500);

    	// 1.1. Получаем id всех уже записанных модулей
    	char buf[128];
    	ini_gets("IDs", "List", "", buf, sizeof(buf), CONFIG_NAME);

    	int count = 0;

    	uint16_t ids[MODULES_BUFFER_SIZE];
    	char* token = strtok(buf, ",");
    	while (token && count < MODULES_BUFFER_SIZE) {
    	    ids[count++] = atoi(token);
    	    token = strtok(NULL, ",");
    	}

    	// 1.2. Создаём структуры модулей из уже записанных id
    	for (int i = 0; i < count; i++) {
    		uint16_t id = ids[i];

    	    Module* m = module_createModule(id);
    	    if (!m) continue;

    	    char section[16];
    	    sprintf(section, "%u", id);

    	    m->id = id;
    	    m->ohm       = ini_getl(section, "ohm",       0, CONFIG_NAME);
    	    m->smd_size  = ini_getl(section, "smd_size",  0, CONFIG_NAME);
    	    m->quaility  = ini_getl(section, "quaility",  0, CONFIG_NAME);
    	    m->step      = ini_getl(section, "step",      4, CONFIG_NAME);
    	    m->holes     = ini_getl(section, "holes",     0, CONFIG_NAME);
			m->need_save = 0;
    	}
    } else {
        // Красный экран при ошибке монтирования SD-карты
        LCD_FillScreen(0xF800);
        while(1) {}
    }

	GUI_Init();

	while(1) {
		CAN_Frame frame;
		while (can1_queue_pop(&frame)) {
			Callback_CAN(frame.id, frame.data, frame.dlc);
		}

		// Сохранение данных в SD карту
		uint16_t active_count = module_getActiveCount();
		for (uint16_t i = 0; i < active_count; i++) {
			Module* m = module_getActiveModule(i);
			if (m && m->need_save) {
				m->need_save = 0;
				module_saveModule(m);
			}
		}

		GUI_Update();

	}
}


void Callback_CAN(uint32_t id, const uint8_t *data, uint8_t dlc) {
	if (dlc < 1) return; // минимум 1 байт

	Module* m = module_findModule(id);
	if (m == 0) {
		m = module_createModule(id);
	}

	switch(data[0]) {
		case 1: // Модуль запросил информацию
			{
				m->last_update_time = systick_get_ticks();

				uint8_t newData[7];

				// Формируем пакет калибровки
				newData[0] = (uint8_t)(m->id >> 8);          // MSB адреса модуля
				newData[1] = (uint8_t)(m->id & 0xFF);        // LSB адреса модуля
				newData[2] = 0;                              // Код команды установки параметров
				newData[3] = (uint8_t)(m->holes >> 8);        // MSB текущего значения отверстий
				newData[4] = (uint8_t)(m->holes & 0xFF);      // LSB текущего значения отверстий
				newData[5] = (uint8_t)(m->holes_diff >> 8);   // MSB текущей разницы отверстий
				newData[6] = (uint8_t)(m->holes_diff & 0xFF); // LSB текущей разницы отверстий

				// Отправляем данные по запросу (7-байтовый пакет)
				can1_tx_data(newData, 7);
			}
			break;
		case 3: // Модуль сообщил об изменении
			if (dlc >= 5) {
				uint16_t new_holes = (uint16_t)(((uint16_t)data[1] << 8) | data[2]);
				int16_t new_holes_diff = (int16_t)(((uint16_t)data[3] << 8) | data[4]);
				
				m->holes = new_holes;
				m->holes_diff = new_holes_diff;
				m->last_update_time = systick_get_ticks();

				// Показываем уведомление на весь экран только при ненулевом изменении
				if (new_holes_diff != 0) {
					GUI_ShowNotification(m);
				}
			}
			break;
		case 4: //Сердцебиение модуля
		{
			m->last_update_time = systick_get_ticks();
		}
	}
}

void module_saveModule(const Module* m) {
	if (!m || m->id == 0) return;

	// todo Костыль, разобраться почему без повторного монтирования запись невозможна
	res = f_mount(&fs, "", 1);
	if (res == FR_OK) {
		char section[16];
		sprintf(section, "%u", m->id);

		ini_putl(section, "ohm", m->ohm, CONFIG_NAME);
		ini_putl(section, "smd_size", m->smd_size, CONFIG_NAME);
		ini_putl(section, "quaility", m->quaility, CONFIG_NAME);
		ini_putl(section, "step", m->step, CONFIG_NAME);
		ini_putl(section, "holes", m->holes, CONFIG_NAME);
	}
}

void module_saveIDList(void) {
	char buf[128] = "";
	int offset = 0;
	for (uint16_t i = 0; i < MODULES_BUFFER_SIZE; i++) {
		Module* m = module_getModuleByIndex(i);
		if (m && m->id != 0) {
			if (offset > 0) {
				offset += sprintf(buf + offset, ",");
			}
			offset += sprintf(buf + offset, "%u", m->id);
		}
	}
	ini_puts("IDs", "List", buf, CONFIG_NAME);
}
