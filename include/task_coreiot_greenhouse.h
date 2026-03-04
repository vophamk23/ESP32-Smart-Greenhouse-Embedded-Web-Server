/*
 * task_coreiot_greenhouse.h - CoreIoT MQTT Task cho Greenhouse IoT
 *
 * Kết nối ThingsBoard/CoreIoT platform qua MQTT:
 *   - Publish telemetry: nhiệt độ, độ ẩm, đất, ánh sáng, trạng thái thiết bị
 *   - Publish attributes: pumpCount, totalPumpTime, autoMode, alerts
 *   - Subscribe RPC: setPump, setFan, setMode
 *   - WiFi reconnect tự động
 */

#ifndef TASK_COREIOT_GREENHOUSE_H
#define TASK_COREIOT_GREENHOUSE_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "iot_bridge.h"
#include "global_vars.h"

// ============================================================================
// CẤU HÌNH (đọc từ LittleFS /info.dat)
// ============================================================================
extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

// ============================================================================
// TASK ENTRY POINT
// ============================================================================
// Chạy trên Core 0, stack 8KB
// xTaskCreatePinnedToCore(greenhouse_coreiot_task, "coreiot", 8192, NULL, 2, NULL, 0);
void greenhouse_coreiot_task(void *pvParameters);

// ============================================================================
// WIFI TASK
// ============================================================================
// Chạy trên Core 0, stack 4KB
// xTaskCreatePinnedToCore(greenhouse_wifi_task, "wifi", 4096, NULL, 3, NULL, 0);
void greenhouse_wifi_task(void *pvParameters);


#endif // TASK_COREIOT_GREENHOUSE_H