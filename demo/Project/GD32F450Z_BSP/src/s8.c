#include "s8.h"
#include "main.h"

#define S8_RESET_DELAY_MS     200U
#define S8_MEASURE_DELAY_MS   20U
#define S8_READ_RETRY_COUNT   2U

static uint8_t s8_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFU;
    uint8_t i;
    uint8_t bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8U; bit++) {
            if (crc & 0x80U) {
                crc = (uint8_t)((crc << 1) ^ 0x31U);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

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
        /* s8_init is called before timer3_init, so delay_ms would block forever here. */
        i2c_delay((uint32_t)S8_RESET_DELAY_MS * 12000U);
    }

    return s8_address;
}

uint8_t s8_read_temp_humi(uint32_t i2c_periph, uint8_t i2c_addr, float *temperature, float *humidity)
{
    uint8_t buffer[6];
    uint8_t retry;
    uint16_t temp_raw;
    uint16_t humi_raw;

    if (temperature == 0 || humidity == 0) {
        return 0;
    }

    for (retry = 0U; retry < S8_READ_RETRY_COUNT; retry++) {
        s8_write_command(i2c_periph, i2c_addr, SHT35_MEASURE_MSB, SHT35_MEASURE_LSB);
        delay_ms(S8_MEASURE_DELAY_MS);
        i2c_direct_read(i2c_periph, i2c_addr, buffer, 6);

        if (s8_crc8(&buffer[0], 2U) != buffer[2] || s8_crc8(&buffer[3], 2U) != buffer[5]) {
            continue;
        }

        temp_raw = ((uint16_t)buffer[0] << 8) | buffer[1];
        humi_raw = ((uint16_t)buffer[3] << 8) | buffer[4];

        if ((temp_raw == 0xFFFFU && humi_raw == 0xFFFFU) ||
            (temp_raw == 0x0000U && humi_raw == 0x0000U)) {
            continue;
        }

        *temperature = ((float)temp_raw * 175.0f / 65535.0f) - 45.0f;
        *humidity = (float)humi_raw * 100.0f / 65535.0f;
        return 1;
    }

    return 0;
}
