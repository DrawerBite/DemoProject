#include "HighAbstraction/Modules.h"
#include "LowAbstraction/systick.h"

#define MODULE_TIMEOUT_MS 3000

static Module modules[MODULES_BUFFER_SIZE];

static inline int32_t mathCore(int32_t holes, uint8_t step) {
    if (step == 0) return 0;
    return (holes * 4) / (int32_t)step;
}

Module* module_createModule(uint16_t id) {
    if (module_findModule(id) != 0) return 0;

	Module m = {
			.id = id,
	        .ohm = 0,
	        .smd_size = 0,
	        .quaility = 0,
	        .step = 4,
	        .holes = 0,
	        .holes_diff = 0,
	        .last_update_time = 0
	};

	for (uint16_t i = 0; i < MODULES_BUFFER_SIZE; i++) {
		if (modules[i].id == 0) {
			modules[i] = m;
			return &modules[i];
		}
	}
	return 0;
}

Module* module_findModule(uint16_t id) {
    for (uint16_t i = 0; i < MODULES_BUFFER_SIZE; i++) {
        if (modules[i].id == id)
            return &modules[i];
    }
    return 0;
}

uint32_t module_mathCount(uint16_t holes, uint8_t step) {
    return (uint32_t) mathCore((int32_t)holes, step);
}

int32_t module_mathCountDiff(int16_t holes_diff, uint8_t step) {
    return mathCore((int32_t)holes_diff, step);
}

Module* module_getActiveModule(uint16_t active_index) {
    uint16_t active_count = 0;
    for (uint16_t i = 0; i < MODULES_BUFFER_SIZE; i++) {
        if (modules[i].id != 0 && module_isConnected(&modules[i])) {
            if (active_count == active_index) {
                return &modules[i];
            }
            active_count++;
        }
    }
    return 0;
}

Module* module_getModuleByIndex(uint16_t index) {
    if (index >= MODULES_BUFFER_SIZE) {
        return 0;
    }
    return &modules[index];
}

uint16_t module_getActiveCount(void) {
    uint16_t count = 0;
    for (uint16_t i = 0; i < MODULES_BUFFER_SIZE; i++) {
        if (modules[i].id != 0 && module_isConnected(&modules[i])) {
            count++;
        }
    }
    return count;
}

uint8_t module_isConnected(const Module* m) {
    if (!m || m->id == 0) return 0;
    if (m->last_update_time == 0) return 0;
    return (systick_get_ticks() - m->last_update_time < MODULE_TIMEOUT_MS);
}

