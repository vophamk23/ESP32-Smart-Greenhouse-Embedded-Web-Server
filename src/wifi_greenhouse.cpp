#include "wifi_greenhouse.h"
#include "iot_bridge.h" // xWiFiEventGroup, WIFI_CONNECTED_BIT
#include "check_info.h" // WIFI_SSID, WIFI_PASS (đọc từ /info.dat trên LittleFS)
#include "ntp_sync.h"
/* ============================================================
   wifi_greenhouse.cpp
   Quản lý toàn bộ kết nối WiFi cho hệ thống Greenhouse IoT.

   Gồm 2 phần:
     1. initWiFi()              — Gọi từ setup(), kết nối lần đầu
     2. greenhouse_wifi_task()  — FreeRTOS task Core 0, tự reconnect
============================================================ */

// ============================================================================
// KHỞI TẠO WIFI (gọi từ setup())
// ============================================================================
void initWiFi()
{
    // ── Bật đồng thời cả 2 mode: AP + STA ──
    // AP (Access Point): ESP32 phát WiFi riêng "Greenhouse-AP"
    //   → dùng để cấu hình lần đầu hoặc khi mất mạng
    // STA (Station): ESP32 kết nối vào router nhà
    //   → dùng để truy cập internet, MQTT, WebSocket từ xa
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Greenhouse-AP", "12345678");
    Serial.printf("[WiFi] AP started: 192.168.4.1\n");

    // ── Kiểm tra đã có config STA chưa ──
    // WIFI_SSID được load từ file /info.dat (LittleFS) trong check_info_File()
    // Nếu chưa cấu hình thì SSID rỗng → chỉ chạy AP, không thử kết nối
    if (WIFI_SSID[0] == '\0')
    {
        Serial.println("[WiFi] No STA config - AP only");
        return;
    }

    // ── Bắt đầu kết nối STA vào router ──
    // Hỗ trợ cả mạng có password và mạng mở (không password)
    if (WIFI_PASS[0] == '\0')
        WiFi.begin(WIFI_SSID);
    else
        WiFi.begin(WIFI_SSID, WIFI_PASS);

    Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000)
        delay(200);

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        if (xWiFiEventGroup)
            xEventGroupSetBits(xWiFiEventGroup, WIFI_CONNECTED_BIT);

        // Đồng bộ thời gian ngay sau khi kết nối để có thời gian chính xác cho các log sau này
        NTPSyncResult r = syncRTCfromNTP(3, 8000);
        Serial.printf("[NTP] Initial sync: %s\n", ntpResultStr(r));
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

        if (WIFI_SSID[0] == '\0')
            continue;

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[WiFi] Disconnected! Reconnecting...");

            // Reset WiFi về AP+STA và bật lại AP trước
            // (AP có thể bị tắt khi mất kết nối trên một số firmware)
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP("Greenhouse-AP", "12345678");

            // Thử kết nối lại STA với cùng config cũ
            if (WIFI_PASS[0] == '\0')
                WiFi.begin(WIFI_SSID);
            else
                WiFi.begin(WIFI_SSID, WIFI_PASS);

            uint32_t t = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - t < 15000)
                vTaskDelay(pdMS_TO_TICKS(500));

            if (WiFi.status() == WL_CONNECTED)
            {
                Serial.printf("[WiFi] Reconnected! IP: %s\n",
                              WiFi.localIP().toString().c_str());
                if (xWiFiEventGroup)
                    xEventGroupSetBits(xWiFiEventGroup, WIFI_CONNECTED_BIT);

                // Đồng bộ thời gian ngay sau khi reconnect để đảm bảo thời gian chính xác cho các log và hoạt động tiếp theo
                NTPSyncResult r = syncRTCfromNTP(2, 5000);
                Serial.printf("[NTP] Post-reconnect sync: %s\n", ntpResultStr(r));
            }
            else
            {
                Serial.println("[WiFi] Reconnect failed - retry in 10s");
            }
        }
    }
}