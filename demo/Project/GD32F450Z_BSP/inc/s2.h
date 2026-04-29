#ifndef S2_H
#define S2_H

#include "i2c.h"

#define BH1750_POWER_DOWN          0x00
#define BH1750_POWER_ON            0x01
#define BH1750_RESET               0x07
#define BH1750_CONT_H_RES_MODE     0x10
#define BH1750_ONE_H_RES_MODE      0x20

i2c_addr_def s2_init(uint8_t address);
uint8_t s2_read_light(uint32_t i2c_periph, uint8_t i2c_addr, uint16_t *lux);

#endif
