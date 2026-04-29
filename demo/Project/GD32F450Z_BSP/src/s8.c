#include "s8.h"
#include "main.h"

static void s8_write_command(uint32_t i2c_periph, uint8_t i2c_addr, uint8_t msb, uint8_t lsb)
{
    i2c_byte_write(i2c_periph, i2c_addr, msb, lsb);
}

i2c_addr_def s8_init(uint8_t address)
{
    i2c_addr_def s8_address;

    s8_address = get_board_address(address);
    if (s8_address.flag) {
        s8_write_command(s8_address.periph, s8_address.addr, SHT35_SOFT_RESET_MSB, SHT35_SOFT_RESET_LSB);
        delay_ms(20);
    }

    return s8_address;
}

uint8_t s8_read_temp_humi(uint32_t i2c_periph, uint8_t i2c_addr, float *temperature, float *humidity)
{
    uint8_t buffer[6] = {0};
    uint16_t temp_raw;
    uint16_t humi_raw;

    if (temperature == 0 || humidity == 0) {
        return 0;
    }

    s8_write_command(i2c_periph, i2c_addr, SHT35_MEASURE_MSB, SHT35_MEASURE_LSB);
    delay_ms(20);
    i2c_direct_read(i2c_periph, i2c_addr, buffer, 6);

    temp_raw = ((uint16_t)buffer[0] << 8) | buffer[1];
    humi_raw = ((uint16_t)buffer[3] << 8) | buffer[4];

    if (temp_raw == 0xFFFFU && humi_raw == 0xFFFFU) {
        return 0;
    }

    *temperature = ((float)temp_raw * 175.0f / 65535.0f) - 45.0f;
    *humidity = (float)humi_raw * 100.0f / 65535.0f;

    return 1;
}
