#include "wifi_greenhouse.h"
#include "iot_bridge.h"
#include "check_info.h"

// ============================================================================
// KHỞI TẠO WIFI (gọi từ setup())
// ============================================================================
void initWiFi()
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Greenhouse-AP", "12345678");
    Serial.printf("[WiFi] AP started: 192.168.4.1\n");

    if (WIFI_SSID[0] == '\0')
    {
        Serial.println("[WiFi] No STA config - AP only");
        return;
    }

    if (WIFI_PASS[0] == '\0') WiFi.begin(WIFI_SSID);
    else                      WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000)
        delay(200);

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        if (xWiFiEventGroup)
            xEventGroupSetBits(xWiFiEventGroup, WIFI_CONNECTED_BIT);
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

        if (WIFI_SSID[0] == '\0') continue;

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[WiFi] Disconnected! Reconnecting...");

            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP("Greenhouse-AP", "12345678");

            if (WIFI_PASS[0] == '\0') WiFi.begin(WIFI_SSID);
            else                      WiFi.begin(WIFI_SSID, WIFI_PASS);

            uint32_t t = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t < 15000)
                vTaskDelay(pdMS_TO_TICKS(500));

            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.printf("[WiFi] Reconnected! IP: %s\n",
                              WiFi.localIP().toString().c_str());
                if (xWiFiEventGroup)
                    xEventGroupSetBits(xWiFiEventGroup, WIFI_CONNECTED_BIT);
            }
            else
            {
                Serial.println("[WiFi] Reconnect failed - retry in 10s");
            }
        }
    }
}