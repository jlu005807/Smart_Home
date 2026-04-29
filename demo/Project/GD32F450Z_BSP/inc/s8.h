#ifndef S8_H
#define S8_H

#include "i2c.h"

#define SHT35_SOFT_RESET_MSB      0x30
#define SHT35_SOFT_RESET_LSB      0xA2
#define SHT35_MEASURE_MSB         0x2C
#define SHT35_MEASURE_LSB         0x0D

i2c_addr_def s8_init(uint8_t address);
uint8_t s8_read_temp_humi(uint32_t i2c_periph, uint8_t i2c_addr, float *temperature, float *humidity);

#endif
