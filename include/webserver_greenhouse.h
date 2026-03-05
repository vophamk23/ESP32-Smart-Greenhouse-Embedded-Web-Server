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

#endif // WEBSERVER_GREENHOUSE_H