#pragma once

#include "stdint.h"

#define MODULES_BUFFER_SIZE 20

typedef struct{
	uint16_t id;

	uint16_t ohm;
	uint16_t smd_size;
	uint16_t quaility;
	uint8_t step;

	uint16_t holes;
	int16_t holes_diff;

	uint32_t last_update_time;

	uint8_t need_save;
} Module;

Module* module_createModule(uint16_t id);
Module* module_findModule(uint16_t id);
Module* module_getActiveModule(uint16_t active_index);
Module* module_getModuleByIndex(uint16_t index);
uint16_t module_getActiveCount(void);
uint8_t module_isConnected(const Module* m);

/*
 * Подсчитать количество компонентов на SMD ленте
 */
uint32_t module_mathCount(uint16_t holes, uint8_t step);

/*
 * Подсчитать количество изменившихся компонентов на SMD ленте
 */
int32_t module_mathCountDiff(int16_t holes_diff, uint8_t step);
