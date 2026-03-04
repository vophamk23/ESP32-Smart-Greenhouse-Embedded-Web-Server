#ifndef TASK_WIFI_GREENHOUSE_H
#define TASK_WIFI_GREENHOUSE_H

#include <Arduino.h>
#include <WiFi.h>

void initWiFi();
void greenhouse_wifi_task(void *pvParameters);

#endif