#include "e2.h"
#include "smart_home_config.h"

static void e2_set_pwm(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t num, uint16_t on, uint16_t off)
{
    i2c_byte_write(i2c_periph, i2c_addr, LED0_ON_L + 4U * num, (uint8_t)on);
    i2c_byte_write(i2c_periph, i2c_addr, LED0_ON_H + 4U * num, (uint8_t)(on >> 8));
    i2c_byte_write(i2c_periph, i2c_addr, LED0_OFF_L + 4U * num, (uint8_t)off);
    i2c_byte_write(i2c_periph, i2c_addr, LED0_OFF_H + 4U * num, (uint8_t)(off >> 8));
}

static void e2_all_pwm_off(uint32_t i2c_periph, uint8_t i2c_addr)
{
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_ON_L, 0);
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_ON_H, 0);
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_OFF_L, 0);
    i2c_byte_write(i2c_periph, i2c_addr, ALLLED_OFF_H, 0x10);
}

i2c_addr_def e2_init(uint8_t address)
{
    i2c_addr_def e2_address;
    uint8_t i;

    e2_address.flag = 0;
    e2_address.periph = 0;
    e2_address.addr = 0;

    for (i = 0; i < 4; i++) {
        e2_address = get_board_address(address + i * 2U);
        if (e2_address.flag) {
            i2c_delay_byte_write(e2_address.periph, e2_address.addr, PCA9685_MODE1, 0x00);
            e2_all_pwm_off(e2_address.periph, e2_address.addr);
            break;
        }
    }

    return e2_address;
}

void e2_fan_set_duty(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t duty)
{
    uint16_t off;

    if (duty > 100U) {
        duty = 100U;
    }

    if (duty == 0U) {
        e2_set_pwm(i2c_periph, i2c_addr, 0, 0, 0);
        return;
    }

    off = (uint16_t)((4095U * duty) / 100U);
    e2_set_pwm(i2c_periph, i2c_addr, 0, 0, off);
}

void e2_fan_on(uint32_t i2c_periph, uint8_t i2c_addr)
{
    e2_fan_set_duty(i2c_periph, i2c_addr, FAN_ON_DUTY);
}

void e2_fan_off(uint32_t i2c_periph, uint8_t i2c_addr)
{
    e2_fan_set_duty(i2c_periph, i2c_addr, 0);
}
