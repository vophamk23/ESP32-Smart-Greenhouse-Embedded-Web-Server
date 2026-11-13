/*
 * coreiot_client.h - CoreIoT MQTT Task cho Cleanroom Bio-Chamber IoT
 *
 * Kết nối ThingsBoard/CoreIOT platform qua MQTT:
 *   - Publish telemetry: nhiệt độ, độ ẩm không khí, độ ẩm giá thể, ánh sáng, trạng thái thiết bị
 *   - Publish attributes: pumpCount, totalPumpTime, autoMode, alerts
 *   - Subscribe RPC: setPump, setFan, setMode
 *   - WiFi reconnect tự động
 */

#ifndef COREIOT_CLIENT_H
#define COREIOT_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "iot_bridge.h"
#include "global_vars.h"

// ============================================================================
// CẤU HÌNH (đọc từ LittleFS /info.dat)
// ============================================================================
extern char WIFI_SSID      [33];
extern char WIFI_PASS      [65];
extern char CORE_IOT_TOKEN [64];
extern char CORE_IOT_SERVER[128];
extern char CORE_IOT_PORT  [6];

// ============================================================================
// TASK ENTRY POINT
// ============================================================================
// Chạy trên Core 0, stack 8KB
// xTaskCreatePinnedToCore(coreiot_task, "coreiot", 8192, NULL, 2, NULL, 0);
void coreiot_task(void *pvParameters);

// ============================================================================
// WIFI TASK
// ============================================================================
// Chạy trên Core 0, stack 4KB
// xTaskCreatePinnedToCore(wifi_task, "wifi", 4096, NULL, 3, NULL, 0);
void wifi_task(void *pvParameters);


#endif // COREIOT_CLIENT_H