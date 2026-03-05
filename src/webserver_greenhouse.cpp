/*
 * webserver_greenhouse.cpp - WebServer + WebSocket Task cho Greenhouse IoT
 *
 * Chức năng:
 *   - Serve web UI từ LittleFS (/index.html, /script.js, /styles.css)
 *   - WebSocket /ws: đẩy telemetry real-time mỗi 1s
 *   - Nhận lệnh điều khiển từ browser qua WebSocket → xRpcCommandQueue
 *   - ElegantOTA: cập nhật firmware qua browser
 *
 * WebSocket JSON format gửi đến browser:
 *   {
 *     "temperature": 28.5, "humidity": 65.0,
 *     "soilMoisture": 45.0, "lightLux": 1000.0,
 *     "pump": true, "fan": false, "autoMode": true,
 *     "alertTemp": false, "alertSoil": true,
 *     "ledColor": "#00FF00",
 *     "pumpCount": 3, "totalPumpTimeSec": 240
 *   }
 *
 * WebSocket JSON nhận từ browser:
 *   { "cmd": "setPump",  "value": true }
 *   { "cmd": "setFan",   "value": false }
 *   { "cmd": "setMode",  "value": "AUTO" }
 */

#include "webserver_greenhouse.h"
#include "iot_bridge.h"
#include "global_vars.h"          // g_temperature, g_humidity, g_pumpState...
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// ============================================================================
// WEB SERVER & WEBSOCKET
// ============================================================================
static AsyncWebServer s_server(80);
static AsyncWebSocket s_ws("/ws");
static bool           s_serverRunning = false;

// ============================================================================
// XỬ LÝ TIN NHẮN TỪ BROWSER → QUEUE LỆNH
// ============================================================================
static void handleBrowserMessage(const String &json)
{
    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, json)) return;

    const char *cmd = doc["cmd"];
    if (!cmd) return;

    RpcPacket_t pkt = { RPC_NONE };

    if (strcmp(cmd, "setPump") == 0)
        pkt.command = doc["value"].as<bool>() ? RPC_PUMP_ON : RPC_PUMP_OFF;
    else if (strcmp(cmd, "setFan") == 0)
        pkt.command = doc["value"].as<bool>() ? RPC_FAN_ON : RPC_FAN_OFF;
    else if (strcmp(cmd, "setMode") == 0)
    {
        const char *val = doc["value"].as<const char*>();
        if (val) pkt.command = (strcmp(val, "AUTO") == 0) ? RPC_MODE_AUTO : RPC_MODE_MANUAL;
    }

    if (pkt.command != RPC_NONE && xRpcCommandQueue)
        xQueueSend(xRpcCommandQueue, &pkt, 0);
}

// ============================================================================
// WEBSOCKET EVENT HANDLER
// ============================================================================
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("[WS] Client #%u connected: %s\n",
                      client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_TEXT && len > 0)
        {
            String msg = String((char *)data).substring(0, len);
            handleBrowserMessage(msg);
        }
    }
}

// ============================================================================
// KHỞI ĐỘNG WEB SERVER
// ============================================================================
static void startWebServer()
{
    s_ws.onEvent(onWsEvent);
    s_server.addHandler(&s_ws);

    // Static files từ LittleFS
    s_server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/index.html", "text/html");
    });
    s_server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/script.js", "application/javascript");
    });
    s_server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/styles.css", "text/css");
    });

    // REST API: lấy trạng thái hiện tại (cho load lần đầu)
    s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        StaticJsonDocument<256> doc;
        doc["temperature"]  = g_temperature;
        doc["humidity"]     = g_humidity;
        doc["soilMoisture"] = g_soilMoisture;
        doc["lightLux"]     = g_lightLux;
        doc["pump"]         = g_pumpState;
        doc["fan"]          = g_fanState;
        doc["autoMode"]     = g_autoMode;
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    ElegantOTA.begin(&s_server);
    s_server.begin();
    s_serverRunning = true;
    Serial.println("[WS] Web server started on port 80");
}

// ============================================================================
// WEBSERVER TASK (Core 0)
// ============================================================================
void greenhouse_webserver_task(void *pvParameters)
{
    // Chờ WiFi sẵn sàng trước khi start server (tối đa 30s)
    // LittleFS đã mount trong setup() trên Core 1 trước khi task này chạy
    if (xSemaphoreWiFiReady)
        xSemaphoreTake(xSemaphoreWiFiReady, pdMS_TO_TICKS(30000));

    // Nhả semaphore lại cho CoreIoT task cũng đang chờ
    if (xSemaphoreWiFiReady)
        xSemaphoreGive(xSemaphoreWiFiReady);

    startWebServer();

    unsigned long lastPushTime = 0;

    for (;;)
    {
        ElegantOTA.loop();

        // Cleanup WebSocket clients định kỳ (tránh memory leak)
        s_ws.cleanupClients();

        // Đẩy dữ liệu real-time mỗi 1 giây nếu có client kết nối
        if (s_ws.count() > 0)
        {
            unsigned long now = millis();
            if (now - lastPushTime >= 1000)
            {
                lastPushTime = now;

                // Đọc telemetry từ queue (peek, không remove - để CoreIoT cũng đọc được)
                // Thực ra ta đọc thẳng từ global vars vì WebSocket chỉ cần hiển thị
                StaticJsonDocument<384> doc;
                doc["temperature"]      = g_temperature;
                doc["humidity"]         = g_humidity;
                doc["soilMoisture"]     = g_soilMoisture;
                doc["lightLux"]         = g_lightLux;
                doc["pump"]             = g_pumpState;
                doc["fan"]              = g_fanState;
                doc["autoMode"]         = g_autoMode;
                doc["alertTemp"]        = g_alertTemp;
                doc["alertHumidity"]    = g_alertHumidity;
                doc["alertSoil"]        = g_alertSoil;
                doc["alertLight"]       = g_alertLight;
                doc["pumpCount"]        = (int)g_pumpCount;
                doc["totalPumpTimeSec"] = (long)(g_totalPumpTime / 1000UL);
                doc["dhtError"]         = g_dhtError;
                doc["wifiRssi"]         = WiFi.RSSI();         
                doc["freeHeap"]         = ESP.getFreeHeap();   
                doc["lcdPage"]          = g_lcdPage;

                char ledHex[8];
                snprintf(ledHex, sizeof(ledHex), "#%02X%02X%02X",
                         g_currentLEDColor.r,
                         g_currentLEDColor.g,
                         g_currentLEDColor.b);
                doc["ledColor"] = ledHex;

                String json;
                serializeJson(doc, json);
                s_ws.textAll(json);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms loop
    }
}



/*
ESP32 khởi động
    │
    ├─ Phát WiFi "Greenhouse-AP" (192.168.4.1) → LUÔN có
    │       └─ Điện thoại/laptop kết nối vào → mở 192.168.4.1
    │
    └─ Đồng thời kết nối WiFi nhà (nếu có info.dat)
            └─ Có internet → gửi dữ liệu lên CoreIoT cloud
     
            
*/