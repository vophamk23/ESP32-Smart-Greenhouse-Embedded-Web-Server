/*
 * task_webserver_greenhouse.h
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

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
#endif // WEB_SERVER_H