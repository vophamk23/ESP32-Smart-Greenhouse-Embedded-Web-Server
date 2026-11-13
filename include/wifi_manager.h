#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

void initWiFi();
void wifi_task(void *pvParameters);

#endif // WIFI_MANAGER_H