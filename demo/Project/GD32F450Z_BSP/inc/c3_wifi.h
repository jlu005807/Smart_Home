#ifndef C3_WIFI_H
#define C3_WIFI_H

#include "gd32f4xx.h"

void Wifi_Init(void);
void Wifi_SendSensorData(float temperature, float humidity, uint16_t light);
void Wifi_ReceiveCommand(void);

#endif
