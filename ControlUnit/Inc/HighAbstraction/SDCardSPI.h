#pragma once

#include <stm32f1xx.h>
#include "spi2.h"

uint8_t sd_init(void); //Вызывается в diskio.c
uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc);
uint8_t sd_read_sector(uint32_t sector_addr, uint8_t *buffer);
uint8_t sd_write_sector(uint32_t sector_addr, const uint8_t *buffer);
