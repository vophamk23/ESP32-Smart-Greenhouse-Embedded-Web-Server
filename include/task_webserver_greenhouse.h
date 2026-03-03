/*
 * task_webserver_greenhouse.h
 */

#ifndef TASK_WEBSERVER_GREENHOUSE_H
#define TASK_WEBSERVER_GREENHOUSE_H

#include <Arduino.h>
#include "iot_bridge.h"
#include "global_vars.h"

// Task entry point
// xTaskCreatePinnedToCore(greenhouse_webserver_task, "webserver", 8192, NULL, 1, NULL, 0);
void greenhouse_webserver_task(void *pvParameters);

#endif // TASK_WEBSERVER_GREENHOUSE_H