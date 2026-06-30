#pragma once

#include <stm32f103xb.h>
#include "LowAbstraction/rcc.h"

typedef enum {
	IN,
	OUT,
	NONE
} EncoderDirection;

void encoder_init(void);
EncoderDirection encoder_getDirection(void);
int16_t encoder_getValue(void);
