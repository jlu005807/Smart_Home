#ifndef E2_H
#define E2_H

#include "i2c.h"

#define PCA9685_ADDRESS_E2    0xC8
#define PCA9685_MODE1         0x00
#define LED0_ON_L             0x06
#define LED0_ON_H             0x07
#define LED0_OFF_L            0x08
#define LED0_OFF_H            0x09
#define ALLLED_ON_L           0xFA
#define ALLLED_ON_H           0xFB
#define ALLLED_OFF_L          0xFC
#define ALLLED_OFF_H          0xFD

i2c_addr_def e2_init(uint8_t address);
void e2_fan_set_duty(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t duty);
void e2_fan_on(uint32_t i2c_periph, uint8_t i2c_addr);
void e2_fan_off(uint32_t i2c_periph, uint8_t i2c_addr);

#endif
