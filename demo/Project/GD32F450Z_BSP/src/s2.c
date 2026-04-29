#include "s2.h"

i2c_addr_def s2_init(uint8_t address)
{
    i2c_addr_def s2_address;

    s2_address = get_board_address(address);
    if (s2_address.flag) {
        i2c_cmd_write(s2_address.periph, s2_address.addr, BH1750_POWER_ON);
        i2c_cmd_write(s2_address.periph, s2_address.addr, BH1750_CONT_H_RES_MODE);
    }

    return s2_address;
}

uint8_t s2_read_light(uint32_t i2c_periph, uint8_t i2c_addr, uint16_t *lux)
{
    uint8_t buffer[2] = {0};
    uint32_t raw;
    uint32_t value;

    if (lux == 0) {
        return 0;
    }

    i2c_direct_read(i2c_periph, i2c_addr, buffer, 2);
    raw = ((uint32_t)buffer[0] << 8) | buffer[1];

    if (raw == 0xFFFFU) {
        return 0;
    }

    value = (raw * 10U) / 12U;
    if (value > 65535U) {
        value = 65535U;
    }

    *lux = (uint16_t)value;
    return 1;
}
