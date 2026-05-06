#ifndef C3_WIFI_H
#define C3_WIFI_H

#include "gd32f4xx.h"

#define WIFI_RX_BUFFER_SIZE   256U

typedef enum {
    WIFI_CMD_NONE = 0,
    WIFI_CMD_LED_ON,
    WIFI_CMD_LED_OFF,
    WIFI_CMD_FAN_ON,
    WIFI_CMD_FAN_OFF,
    WIFI_CMD_AUTO_MODE,
    WIFI_CMD_MANUAL_MODE,
    WIFI_CMD_READ_STATUS
} wifi_cmd_t;

void Wifi_Init(void);
void Wifi_SendSensorData(float temperature, float humidity, uint16_t light, uint8_t fan_on, uint8_t led_on, uint8_t mode);
void Wifi_ReceiveCommand(void);
wifi_cmd_t Wifi_GetPendingCommand(void);

#endif
