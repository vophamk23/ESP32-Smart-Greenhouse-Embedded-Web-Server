/*
 * web_server.h - Async WebServer Task cho Cleanroom Bio-Chamber
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include "iot_bridge.h"
#include "global_vars.h"

// Task entry point
// xTaskCreatePinnedToCore(webserver_task, "webserver", 8192, NULL, 1, NULL, 0);
void webserver_task(void *pvParameters);

// Hàm load lịch phun sương từ LittleFS, được gọi trong setup() sau khi init iotBridge để có mutex
void loadSchedulesFromFS();

void loadThresholdFromFS();

void saveThresholdToFS();
void saveSchedulesToFS();
#endif // WEB_SERVER_H