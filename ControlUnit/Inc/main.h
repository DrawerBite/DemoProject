#pragma once

#include <stm32f1xx.h>
#include <ff.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <minIni.h>

#include "rcc.h"
#include "HighAbstraction/ILI9488.h"
#include "HighAbstraction/Modules.h"
#include "LowAbstraction/can1.h"
#include "spi2.h"
#include "HighAbstraction/XPT2046.h"
#include "HighAbstraction/GUI.h"
#include "LowAbstraction/systick.h"

#define HOLES_DIFF_RESET_DELAY 15000

/*
 * Вызывается при получении данных по CAN
 */
void Callback_CAN(uint32_t id, const uint8_t *data, uint8_t dlc);

/*
 * Вызывается при нажатии на сенсорный экран
 */
void Callback_TouchScreen(uint16_t x, uint16_t y);

void module_saveModule(const Module* m);
void module_saveIDList(void);
