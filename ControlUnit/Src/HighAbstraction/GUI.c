#include "HighAbstraction/GUI.h"
#include "HighAbstraction/ILI9488.h"
#include "HighAbstraction/XPT2046.h"
#include "HighAbstraction/Modules.h"
#include "LowAbstraction/can1.h"
#include "main.h"
#include "HighAbstraction/font7x11.h"
#include <stdio.h>
#include "LowAbstraction/systick.h"

// Цветовая палитра (RGB565)
#define COLOR_BG        0x0821  // Очень темный индиго (фон)
#define COLOR_NAVY      0x18E3  // Средне-темный синий (заголовок/подвал)
#define COLOR_CARD_BG   0x2104  // Темно-серый синий (карточка)
#define COLOR_CARD_BD   0x5AEB  // Голубой (рамка карточки)
#define COLOR_TEXT_W    0xFFFF  // Белый
#define COLOR_TEXT_Y    0xFD20  // Янтарный/оранжевый
#define COLOR_TEXT_C    0x07FF  // Голубой
#define COLOR_GREEN      0x07E0  // Ярко-зелёный
#define COLOR_SOFT_GREEN 0x4D6A  // Нежно-зелёный (#4CAF50)
#define COLOR_RED        0xF800  // Красный

static uint8_t current_page = 0;
static uint16_t last_active_count = 0;

static uint16_t cached_holes[MODULES_BUFFER_SIZE];
static int16_t cached_holes_diff[MODULES_BUFFER_SIZE];
static Module* cached_modules[4]; // Кеш указателей на модули в каждом слоте текущей страницы

typedef enum {
    FEEDBACK_NONE,
    FEEDBACK_PREV,
    FEEDBACK_NEXT,
    FEEDBACK_LED
} FeedbackType;

typedef struct {
    FeedbackType type;
    uint32_t start_time;
    uint8_t slot;
    Module* module;
} NonBlockingFeedback;

static NonBlockingFeedback active_feedback = {FEEDBACK_NONE, 0, 0, NULL};

typedef enum {
    GUI_STATE_MAIN,
    GUI_STATE_NOTIFICATION
} GUIState;

static GUIState gui_state = GUI_STATE_MAIN;
static uint32_t notification_start_time = 0;
static uint32_t last_dismissal_time = 0;

static uint16_t notification_mod_id = 0;
static uint16_t notification_ohm = 0;
static uint16_t notification_smd_size = 0;
static uint16_t notification_quaility = 0;
static uint16_t notification_holes = 0;
static int16_t notification_holes_diff = 0;
static uint8_t notification_step = 4;

// Объявление локальных функций
static void GUI_DrawHeader(void);
static void GUI_DrawFooter(void);
static void GUI_DrawModuleCard(uint8_t slot, Module* m);
static void GUI_UpdateCardCount(uint8_t slot, Module* m);
static void GUI_DrawPage(void);
static void GUI_HandleTouch(uint16_t tx, uint16_t ty);
static void GUI_DrawNotificationScreen(void);
static void GUI_UpdateNotificationValues(void);
static void format_ohm(uint32_t ohm, char* buf);
static int utf8_strlen(const char* str);

int utf8_strlen(const char* str) {
    int len = 0;
    while (*str) {
        if (((uint8_t)*str & 0xC0) != 0x80) {
            len++;
        }
        str++;
    }
    return len;
}

void GUI_Init(void) {
    // Первоначальное заполнение кэша
    for (uint16_t i = 0; i < MODULES_BUFFER_SIZE; i++) {
        cached_holes[i] = 0xFFFF; // Невалидные значения для первой отрисовки
        cached_holes_diff[i] = 0x7FFF;
    }

    last_active_count = module_getActiveCount();
    
    // Рисуем весь интерфейс
    LCD_FillScreen(COLOR_BG);
    GUI_DrawHeader();
    GUI_DrawPage();
}

static void GUI_DrawHeader(void) {
    // Заливаем область заголовка
    LCD_FillSquare(COLOR_NAVY, 0, 0, 479, 35);
    // Разделительная полоса
    LCD_FillSquare(COLOR_CARD_BD, 0, 35, 479, 37);
    // Текст заголовка по центру
    LCD_DrawString7x11(88, 10, "МОНИТОР CAN-МОДУЛЕЙ", COLOR_TEXT_W, COLOR_NAVY, 2);
}

static void GUI_DrawFooter(void) {
    // Заливаем область подвала
    LCD_FillSquare(COLOR_NAVY, 0, 245, 479, 319);
    // Разделительная полоса
    LCD_FillSquare(COLOR_CARD_BD, 0, 243, 479, 244);

    // Расчет страниц (используем кешированное значение, уже обновлённое в GUI_Update)
    uint16_t total_modules = last_active_count;
    uint8_t total_pages = (total_modules + 3) / 4;
    if (total_pages == 0) total_pages = 1;

    // Кнопка Предыдущей страницы
    LCD_FillSquare(COLOR_CARD_BD, 10, 260, 120, 300);
    LCD_DrawString7x11(49, 272, "ПРЕД", COLOR_BG, COLOR_CARD_BD, 1);

    // Индикатор страницы
    char page_str[32];
    sprintf(page_str, "Стр. %d/%d", current_page + 1, total_pages);
    uint16_t page_x = 240 - (utf8_strlen(page_str) * 8) / 2;
    LCD_DrawString7x11(page_x, 272, page_str, COLOR_TEXT_W, COLOR_NAVY, 1);

    // Кнопка Следующей страницы
    LCD_FillSquare(COLOR_CARD_BD, 360, 260, 470, 300);
    LCD_DrawString7x11(399, 272, "СЛЕД", COLOR_BG, COLOR_CARD_BD, 1);
}

uint16_t debug_holes = -1;

static void GUI_DrawModuleCard(uint8_t slot, Module* m) {
    uint8_t col = slot % 2;
    uint8_t row = slot / 2;
    uint16_t x0 = 10 + col * 235;
    uint16_t y0 = 45 + row * 98;

    if (m == 0) {
        // Очищаем слот, если модуля нет
        LCD_FillSquare(COLOR_BG, x0, y0, x0 + 225, y0 + 90);
        return;
    }

    // Сохраняем значения в кэш
    uint16_t idx = current_page * 4 + slot;
    cached_holes[idx] = m->holes;
    cached_holes_diff[idx] = m->holes_diff;

    // 1. Рисуем фон карточки
    LCD_FillSquare(COLOR_CARD_BG, x0, y0, x0 + 225, y0 + 90);

    // 2. Рисуем тонкую рамку вокруг карточки
    LCD_FillSquare(COLOR_CARD_BD, x0, y0, x0 + 225, y0 + 1);           // Верх
    LCD_FillSquare(COLOR_CARD_BD, x0, y0 + 89, x0 + 225, y0 + 90);     // Низ
    LCD_FillSquare(COLOR_CARD_BD, x0, y0, x0 + 1, y0 + 90);             // Лево
    LCD_FillSquare(COLOR_CARD_BD, x0 + 224, y0, x0 + 225, y0 + 90);     // Право

    // Вертикальный разделитель между левым и правым блоком
    LCD_FillSquare(COLOR_CARD_BD, x0 + 118, y0 + 2, x0 + 119, y0 + 88);

    // 3. ID модуля — верхняя строка, на всю ширину
    char id_str[32];
    sprintf(id_str, "МОД %03u", m->id);
    LCD_DrawString7x11(x0 + 5, y0 + 5, id_str, COLOR_TEXT_Y, COLOR_CARD_BG, 1);

    // Горизонтальный разделитель под ID
    LCD_FillSquare(COLOR_CARD_BD, x0 + 2, y0 + 17, x0 + 223, y0 + 18);

    // 4. Левый блок: Ом, Разм, Кач — всё размером 1
    char ohm_val[16];
    format_ohm(m->ohm, ohm_val);
    char ohm_str[32];
    sprintf(ohm_str, "Ом: %s", ohm_val);
    LCD_DrawString7x11(x0 + 5, y0 + 22, ohm_str, COLOR_TEXT_W, COLOR_CARD_BG, 1);

    char size_str[32];
    sprintf(size_str, "Разм: %04u", m->smd_size);
    LCD_DrawString7x11(x0 + 5, y0 + 37, size_str, COLOR_TEXT_W, COLOR_CARD_BG, 1);

    char qual_str[32];
    sprintf(qual_str, "Кач: +-%u%%", m->quaility);
    LCD_DrawString7x11(x0 + 5, y0 + 52, qual_str, COLOR_TEXT_W, COLOR_CARD_BG, 1);

    // 5. Правый блок: КОЛ-ВО + значение size 2 (правая колонка 105px шириной)
    LCD_DrawString7x11(x0 + 122, y0 + 22, "КОЛ-ВО", COLOR_TEXT_C, COLOR_CARD_BG, 1);

    debug_holes = m->holes;
    uint32_t count = module_mathCount(m->holes, m->step);
    char count_str[16];
    sprintf(count_str, "%lu", count);
    LCD_DrawString7x11(x0 + 122, y0 + 36, count_str, COLOR_TEXT_C, COLOR_CARD_BG, 2);

    // Подсказка внизу правого блока (одна строка, приглушенный цвет)
    LCD_DrawString7x11(x0 + 122, y0 + 68, "[подсветить]", COLOR_CARD_BD, COLOR_CARD_BG, 1);

    // Вся карточка кликабельна (обрабатывается в GUI_HandleTouch)
}

/*
 * Частичное обновление карточки: перерисовывает только правый блок (КОЛ-ВО).
 * Вызывается вместо GUI_DrawModuleCard когда изменились только holes/holes_diff.
 */
static void GUI_UpdateCardCount(uint8_t slot, Module* m) {
    uint8_t col = slot % 2;
    uint8_t row = slot / 2;
    uint16_t x0 = 10 + col * 235;
    uint16_t y0 = 45 + row * 98;

    // Очищаем только правый блок (от вертикального разделителя до правой границы)
    LCD_FillSquare(COLOR_CARD_BG, x0 + 120, y0 + 20, x0 + 223, y0 + 88);

    // Перерисовываем динамическую часть правого блока
    LCD_DrawString7x11(x0 + 122, y0 + 22, "КОЛ-ВО", COLOR_TEXT_C, COLOR_CARD_BG, 1);

    uint32_t count = module_mathCount(m->holes, m->step);
    char count_str[16];
    sprintf(count_str, "%lu", count);
    LCD_DrawString7x11(x0 + 122, y0 + 36, count_str, COLOR_TEXT_C, COLOR_CARD_BG, 2);

    LCD_DrawString7x11(x0 + 122, y0 + 68, "[подсветить]", COLOR_CARD_BD, COLOR_CARD_BG, 1);
}

static void GUI_DrawPage(void) {
    // Рисуем карточки для текущей страницы и обновляем кеш указателей
    for (uint8_t slot = 0; slot < 4; slot++) {
        uint16_t active_idx = current_page * 4 + slot;
        Module* m = module_getActiveModule(active_idx);
        cached_modules[slot] = m; // Обновляем кеш указателей
        GUI_DrawModuleCard(slot, m);
    }
    // Рисуем подвал (обновляет номера страниц и кнопки)
    GUI_DrawFooter();
}

void GUI_Update(void) {
    uint32_t current_time = systick_get_ticks();

    // 0. Обрабатываем состояние уведомления
    if (gui_state == GUI_STATE_NOTIFICATION) {
        // Проверяем таймаут 10 секунд (10000 мс)
        if (current_time - notification_start_time >= 10000) {
            //Очищаем holes_diff
    		uint16_t active_count = module_getActiveCount();
    		for (uint16_t i = 0; i < active_count; i++) {
    			Module* m = module_getActiveModule(i);
    			if (m && m->holes_diff != 0) {
    				m->holes_diff = 0;
    				//Принудительно сообщаем, чтобы модуль сбросил holes_diff
    				// Формируем пакет калибровки
    				uint8_t newData[3];
    				newData[0] = (uint8_t)(m->id >> 8);          // MSB адреса модуля
    				newData[1] = (uint8_t)(m->id & 0xFF);        // LSB адреса модуля
    				newData[2] = 5;                              // Код команды очистки holes_diff

    				// Отправляем данные по запросу (3-байтовый пакет)
    				can1_tx_data(newData, 3);

    				//Сохраняем данные на SD карту
    				m->need_save = 1;
    			}
    		}

            gui_state = GUI_STATE_MAIN;
            GUI_Init();
            return;
        }

        // Проверяем нажатие на экран для выхода
        if (XPT2046_isPressed()) {
            //Очищаем holes_diff
    		uint16_t active_count = module_getActiveCount();
    		for (uint16_t i = 0; i < active_count; i++) {
    			Module* m = module_getActiveModule(i);
    			if (m && m->holes_diff != 0) {
    				m->holes_diff = 0;
    				//Принудительно сообщаем, чтобы модуль сбросил holes_diff
    				// Формируем пакет калибровки
    				uint8_t newData[3];
    				newData[0] = (uint8_t)(m->id >> 8);          // MSB адреса модуля
    				newData[1] = (uint8_t)(m->id & 0xFF);        // LSB адреса модуля
    				newData[2] = 5;                              // Код команды очистки holes_diff

    				// Отправляем данные по запросу (3-байтовый пакет)
    				can1_tx_data(newData, 3);

    				//Сохраняем данные на SD карту
    				m->need_save = 1;
    			}
    		}

            gui_state = GUI_STATE_MAIN;
            GUI_Init();
            last_dismissal_time = current_time;
            return;
        }
        return;
    }

    // 0.1. Обрабатываем неблокирующее снятие подсветки кнопок после задержки
    if (active_feedback.type != FEEDBACK_NONE) {
        if (current_time - active_feedback.start_time >= 150) {
            FeedbackType prev_type = active_feedback.type;
            active_feedback.type = FEEDBACK_NONE; // Сбрасываем заранее

            if (prev_type == FEEDBACK_PREV) {
                current_page--;
                GUI_DrawPage();
            } else if (prev_type == FEEDBACK_NEXT) {
                current_page++;
                GUI_DrawPage();
            } else if (prev_type == FEEDBACK_LED) {
                GUI_DrawModuleCard(active_feedback.slot, active_feedback.module);
            }
        }
    }

    // 1. Ограничиваем опрос тачскрина и обновление экрана до 20 мс (50 Гц)
    static uint32_t last_poll_time = 0;
    if (current_time - last_poll_time < 20) {
        return;
    }
    last_poll_time = current_time;

    // 2. Проверяем изменения количества активных модулей (вставка/извлечение ленты)
    uint16_t active_count = module_getActiveCount();
    if (active_count != last_active_count) {
        last_active_count = active_count;
        uint8_t total_pages = (active_count + 3) / 4;
        if (total_pages == 0) total_pages = 1;
        if (current_page >= total_pages) {
            current_page = total_pages - 1;
            GUI_DrawPage();
        } else {
            // Перерисовываем точечно только слоты, где сменился модуль — вместо GUI_DrawPage()
            for (uint8_t slot = 0; slot < 4; slot++) {
                uint16_t idx = current_page * 4 + slot;
                Module* m = module_getActiveModule(idx);
                if (m != cached_modules[slot]) {
                    cached_modules[slot] = m;
                    GUI_DrawModuleCard(slot, m); // Обновляет cached_holes/cached_holes_diff внутри
                }
            }
            GUI_DrawFooter(); // Обновляем счётчик страниц
        }
        return;
    }

    // 3. Проверяем изменения значений внутри видимых карточек (частичное обновление)
    for (uint8_t slot = 0; slot < 4; slot++) {
        uint16_t idx = current_page * 4 + slot;
        Module* m = module_getActiveModule(idx);
        if (m) {
            // Если карточка сейчас подсвечена (нажатие), не трогаем её
            if (active_feedback.type == FEEDBACK_LED && active_feedback.slot == slot) {
                continue;
            }
            if (m->holes != cached_holes[idx] || m->holes_diff != cached_holes_diff[idx]) {
                // Обновляем кеш и перерисовываем только правый блок (КОЛ-ВО)
                cached_holes[idx] = m->holes;
                cached_holes_diff[idx] = m->holes_diff;
                GUI_UpdateCardCount(slot, m);
            }
        }
    }

    // 4. Обрабатываем нажатия на тачскрин
    uint8_t pressed = XPT2046_isPressed();
    uint16_t tx = XPT2046_getX();
    uint16_t ty = XPT2046_getY();

    if (pressed) {
        // Игнорируем нажатия в течение 500 мс после закрытия уведомления
        if (current_time - last_dismissal_time > 500) {
            GUI_HandleTouch(tx, ty);
        }
    }
}

static void GUI_HandleTouch(uint16_t tx, uint16_t ty) {
    // Если визуальный отклик уже обрабатывается, игнорируем новые нажатия
    if (active_feedback.type != FEEDBACK_NONE) {
        return;
    }

    // 1. Проверяем нажатие на кнопки пагинации в подвале
    if (ty >= 260 && ty <= 300) {
        // Кнопка предыдущей страницы
        if (tx >= 10 && tx <= 120) {
            if (current_page > 0) {
                // Визуальный отклик (красный цвет)
                LCD_FillSquare(COLOR_RED, 10, 260, 120, 300);
                LCD_DrawString7x11(49, 272, "ПРЕД", COLOR_TEXT_W, COLOR_RED, 1);
                
                active_feedback.type = FEEDBACK_PREV;
                active_feedback.start_time = systick_get_ticks();
            }
            return;
        }
        // Кнопка Следующей страницы
        if (tx >= 360 && tx <= 470) {
            uint16_t total_modules = module_getActiveCount();
            uint8_t total_pages = (total_modules + 3) / 4;
            if (current_page + 1 < total_pages) {
                // Визуальный отклик (красный цвет)
                LCD_FillSquare(COLOR_RED, 360, 260, 470, 300);
                LCD_DrawString7x11(399, 272, "СЛЕД", COLOR_TEXT_W, COLOR_RED, 1);
                
                active_feedback.type = FEEDBACK_NEXT;
                active_feedback.start_time = systick_get_ticks();
            }
            return;
        }
    }

    // 2. Проверяем нажатия на кнопки LED в карточках
    for (uint8_t slot = 0; slot < 4; slot++) {
        uint16_t idx = current_page * 4 + slot;
        Module* m = module_getActiveModule(idx);
        if (m == 0) continue;

        uint8_t col = slot % 2;
        uint8_t row = slot / 2;
        uint16_t x0 = 10 + col * 235;
        uint16_t y0 = 45 + row * 98;

        // Вся карточка целиком — кликабельная зона
        if (tx >= x0 && tx <= x0 + 225 && ty >= y0 && ty <= y0 + 90) {
            // Визуальный отклик: вся карточка вспыхивает нежно-зелёным
            LCD_FillSquare(COLOR_SOFT_GREEN, x0 + 1, y0 + 1, x0 + 224, y0 + 89);

            // Перерисовываем весь текст поверх красного фона
            char fb_id[32];
            sprintf(fb_id, "МОД %03u", m->id);
            LCD_DrawString7x11(x0 + 5, y0 + 5, fb_id, COLOR_TEXT_Y, COLOR_SOFT_GREEN, 1);

            char fb_ohm_val[16];
            format_ohm(m->ohm, fb_ohm_val);
            char fb_ohm[32];
            sprintf(fb_ohm, "Ом: %s", fb_ohm_val);
            LCD_DrawString7x11(x0 + 5, y0 + 22, fb_ohm, COLOR_TEXT_W, COLOR_SOFT_GREEN, 1);

            char fb_size[32];
            sprintf(fb_size, "Разм: %04u", m->smd_size);
            LCD_DrawString7x11(x0 + 5, y0 + 37, fb_size, COLOR_TEXT_W, COLOR_SOFT_GREEN, 1);

            char fb_qual[32];
            sprintf(fb_qual, "Кач: +-%u%%", m->quaility);
            LCD_DrawString7x11(x0 + 5, y0 + 52, fb_qual, COLOR_TEXT_W, COLOR_SOFT_GREEN, 1);

            uint32_t fb_count = module_mathCount(m->holes, m->step);
            char fb_count_str[16];
            sprintf(fb_count_str, "%lu", fb_count);
            LCD_DrawString7x11(x0 + 122, y0 + 22, "КОЛ-ВО", COLOR_TEXT_W, COLOR_SOFT_GREEN, 1);
            LCD_DrawString7x11(x0 + 122, y0 + 36, fb_count_str, COLOR_TEXT_W, COLOR_SOFT_GREEN, 2);

            // CAN-передача
            uint8_t can_data[3];
            can_data[0] = (m->id >> 8);
            can_data[1] = (m->id & 0xFF);
            can_data[2] = 2;
            can1_tx_data(can_data, 3);

            // Неблокирующий таймер для восстановления карточки
            active_feedback.type = FEEDBACK_LED;
            active_feedback.start_time = systick_get_ticks();
            active_feedback.slot = slot;
            active_feedback.module = m;
            return;
        }
    }
}

void GUI_ShowNotification(Module* m) {
    if (!m) return;
    
    uint8_t same_module = (gui_state == GUI_STATE_NOTIFICATION && notification_mod_id == m->id);
    
    notification_mod_id = m->id;
    notification_ohm = m->ohm;
    notification_smd_size = m->smd_size;
    notification_quaility = m->quaility;
    notification_holes = m->holes;
    notification_holes_diff = m->holes_diff;
    notification_step = m->step;
    notification_start_time = systick_get_ticks();
    
    if (same_module) {
        // Отрисовываем только изменившиеся значения для скорости и плавной отрисовки
        GUI_UpdateNotificationValues();
    } else {
        gui_state = GUI_STATE_NOTIFICATION;
        GUI_DrawNotificationScreen();
    }
}

static void GUI_DrawNotificationScreen(void) {
    LCD_FillScreen(COLOR_BG);
    
    // Рисуем красивую рамку/карточку в центре
    LCD_FillSquare(COLOR_CARD_BG, 20, 20, 460, 300);
    // Рамка
    LCD_FillSquare(COLOR_CARD_BD, 20, 20, 460, 22);   // Верх
    LCD_FillSquare(COLOR_CARD_BD, 20, 298, 460, 300); // Низ
    LCD_FillSquare(COLOR_CARD_BD, 20, 20, 22, 300);   // Лево
    LCD_FillSquare(COLOR_CARD_BD, 458, 20, 460, 300); // Право
    
    // Заголовок
    char title[64];
    sprintf(title, "ОБНОВЛЕНИЕ МОД %03u", notification_mod_id);
    uint16_t title_x = 240 - (utf8_strlen(title) * 16) / 2;
    LCD_DrawString7x11(title_x, 40, title, COLOR_TEXT_Y, COLOR_CARD_BG, 2);
    
    // Разделительная линия
    LCD_FillSquare(COLOR_CARD_BD, 40, 75, 440, 76);
    
    // Левый блок информации (размер 2)
    char ohm_val[16];
    format_ohm(notification_ohm, ohm_val);
    char ohm_str[32];
    sprintf(ohm_str, "Ом:   %s", ohm_val);
    LCD_DrawString7x11(50, 100, ohm_str, COLOR_TEXT_W, COLOR_CARD_BG, 2);
    
    char size_str[32];
    sprintf(size_str, "Разм: %04u", notification_smd_size);
    LCD_DrawString7x11(50, 130, size_str, COLOR_TEXT_W, COLOR_CARD_BG, 2);

    char qual_str[32];
    sprintf(qual_str, "Кач:  +-%u%%", notification_quaility);
    LCD_DrawString7x11(50, 160, qual_str, COLOR_TEXT_W, COLOR_CARD_BG, 2);

    uint32_t count = module_mathCount(notification_holes, notification_step);
    char count_str[32];
    sprintf(count_str, "Кол-во: %lu", count);
    LCD_DrawString7x11(50, 190, count_str, COLOR_TEXT_C, COLOR_CARD_BG, 2);

    // Правый блок информации (разница)
    LCD_DrawString7x11(280, 100, "РАЗН:", COLOR_TEXT_Y, COLOR_CARD_BG, 2);
    
    char diff_str[16];
    if (notification_holes_diff > 0) {
        sprintf(diff_str, "+%d", notification_holes_diff);
    } else {
        sprintf(diff_str, "%d", notification_holes_diff);
    }
    
    uint16_t diff_color = (notification_holes_diff < 0) ? COLOR_RED : COLOR_GREEN;
    
    // Выравниваем число diff_str по центру правой области
    // Символ размера 4 имеет ширину 32 пикселя.
    uint16_t diff_len = strlen(diff_str);
    uint16_t diff_x = 360 - (diff_len * 32) / 2;
    LCD_DrawString7x11(diff_x, 140, diff_str, diff_color, COLOR_CARD_BG, 4);
    
    // Инструкция для выхода внизу экрана
    LCD_DrawString7x11(160, 265, "Нажмите для возврата", COLOR_TEXT_W, COLOR_CARD_BG, 1);
}

static void GUI_UpdateNotificationValues(void) {
    // 1. Очищаем только области динамических данных на экране
    // Область для Count: x=50..260, y=190..214 (высота шрифта 24 пикселя)
    LCD_FillSquare(COLOR_CARD_BG, 50, 190, 260, 214);
    // Область для Diff: x=240..455, y=130..200
    LCD_FillSquare(COLOR_CARD_BG, 240, 130, 455, 200);

    // 2. Рисуем новые значения
    uint32_t count = module_mathCount(notification_holes, notification_step);
    char count_str[32];
    sprintf(count_str, "Кол-во: %lu", count);
    LCD_DrawString7x11(50, 190, count_str, COLOR_TEXT_C, COLOR_CARD_BG, 2);

    char diff_str[16];
    if (notification_holes_diff > 0) {
        sprintf(diff_str, "+%d", notification_holes_diff);
    } else {
        sprintf(diff_str, "%d", notification_holes_diff);
    }
    
    uint16_t diff_color = (notification_holes_diff < 0) ? COLOR_RED : COLOR_GREEN;
    
    // Выравниваем число diff_str по центру правой области
    // Символ размера 4 имеет ширину 32 пикселя.
    uint16_t diff_len = strlen(diff_str);
    uint16_t diff_x = 360 - (diff_len * 32) / 2;
    LCD_DrawString7x11(diff_x, 140, diff_str, diff_color, COLOR_CARD_BG, 4);
}

static void format_ohm(uint32_t ohm, char* buf) {
    if (ohm >= 1000000) {
        uint32_t integer_part = ohm / 1000000;
        uint32_t fractional_part = (ohm % 1000000) / 100000;
        if (fractional_part == 0) {
            sprintf(buf, "%luM", integer_part);
        } else {
            sprintf(buf, "%lu.%luM", integer_part, fractional_part);
        }
    } else if (ohm >= 1000) {
        uint32_t integer_part = ohm / 1000;
        uint32_t fractional_part = (ohm % 1000) / 100;
        if (fractional_part == 0) {
            sprintf(buf, "%luK", integer_part);
        } else {
            sprintf(buf, "%lu.%luK", integer_part, fractional_part);
        }
    } else {
        sprintf(buf, "%lu", ohm);
    }
}
