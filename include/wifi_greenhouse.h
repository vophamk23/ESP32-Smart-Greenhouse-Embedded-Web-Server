#ifndef WIFI_GREENHOUSE_H
#define WIFI_GREENHOUSE_H

#include <Arduino.h>
#include <WiFi.h>

void initWiFi();
void greenhouse_wifi_task(void *pvParameters);

#endif // WIFI_GREENHOUSE_H