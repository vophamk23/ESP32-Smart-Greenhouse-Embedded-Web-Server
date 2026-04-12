/*
 * task_webserver_greenhouse.h
 */

#ifndef WEBSERVER_GREENHOUSE_H
#define WEBSERVER_GREENHOUSE_H

#include <Arduino.h>
#include "iot_bridge.h"
#include "global_vars.h"

// Task entry point
// xTaskCreatePinnedToCore(greenhouse_webserver_task, "webserver", 8192, NULL, 1, NULL, 0);
void greenhouse_webserver_task(void *pvParameters);

// Hàm load lịch tưới từ LittleFS, được gọi trong setup() sau khi init iotBridge để có mutex
void loadSchedulesFromFS();

void loadThresholdFromFS();

void saveThresholdToFS();
void saveSchedulesToFS();
#endif // WEBSERVER_GREENHOUSE_H