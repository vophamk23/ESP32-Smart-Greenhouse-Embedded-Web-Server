#include "wifi_greenhouse.h"
#include "iot_bridge.h"

extern String WIFI_SSID;
extern String WIFI_PASS;

// ============================================================================
// KHỞI TẠO WIFI (gọi từ setup())
// ============================================================================
void initWiFi()
{
    // Luôn bật AP + STA cùng lúc
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Greenhouse-AP", "12345678");
    Serial.printf("[WiFi] AP started: 192.168.4.1\n");

    if (WIFI_SSID.isEmpty())
    {
        Serial.println("[WiFi] No STA config - AP only");
        return;
    }

    if (WIFI_PASS.isEmpty()) WiFi.begin(WIFI_SSID.c_str());
    else                     WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

    Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID.c_str());

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000)
        delay(200);

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        if (xSemaphoreWiFiReady) xSemaphoreGive(xSemaphoreWiFiReady);
    }
    else
    {
        Serial.println("[WiFi] STA timeout - AP only");
    }
}

// ============================================================================
// WIFI MONITOR TASK (Core 0)
// ============================================================================
void greenhouse_wifi_task(void *pvParameters)
{
    Serial.println("[WiFi] Monitor task started");

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));

        if (WIFI_SSID.isEmpty()) continue;

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[WiFi] Disconnected! Reconnecting...");

            // Giữ AP_STA mode, không để bị reset về STA
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP("Greenhouse-AP", "12345678");

            // Dùng begin() thay vì reconnect() để tránh mất AP
            if (WIFI_PASS.isEmpty()) WiFi.begin(WIFI_SSID.c_str());
            else                     WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

            uint32_t t = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t < 15000)
                vTaskDelay(pdMS_TO_TICKS(500));

            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.printf("[WiFi] Reconnected! IP: %s\n",
                              WiFi.localIP().toString().c_str());
                if (xSemaphoreWiFiReady)
                    xSemaphoreGive(xSemaphoreWiFiReady);
            }
            else
            {
                Serial.println("[WiFi] Reconnect failed - retry in 10s");
            }
        }
    }
}