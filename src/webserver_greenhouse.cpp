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
static AsyncWebServer s_server(80);   // HTTP server cổng 80
static AsyncWebSocket s_ws("/ws");    // WebSocket endpoint: ws://<ip>/ws
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

    // Ánh xạ lệnh text → enum RPC để Core 1 xử lý
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
/*
   onWsEvent()
   Callback của AsyncWebSocket — xử lý 3 loại sự kiện:
     CONNECT    → log client mới
     DISCONNECT → log client ngắt
     DATA       → chỉ xử lý text frame, bỏ qua binary
 */
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
/* 
   Đăng ký tất cả route rồi khởi động server.
   Thứ tự đăng ký quan trọng:
     1. WebSocket handler
     2. REST /api/status   ← phải trước serveStatic
     3. Static files (LittleFS)
     4. ElegantOTA
 */
static void startWebServer()
{
    // 1. Gắn WebSocket vào server
    s_ws.onEvent(onWsEvent);
    s_server.addHandler(&s_ws);

    // 2. REST endpoint: snapshot trạng thái hiện tại
    // Browser gọi ngay khi mở trang, không cần chờ WebSocket push lần đầu
    s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
        StaticJsonDocument<256> doc;

        SENSOR_LOCK();
        doc["temperature"]  = g_temperature;
        doc["humidity"]     = g_humidity;
        doc["soilMoisture"] = g_soilMoisture;
        doc["lightLux"]     = g_lightLux;
        doc["pump"]         = g_pumpState;
        doc["fan"]          = g_fanState;
        doc["autoMode"]     = g_autoMode;
        SENSOR_UNLOCK();

        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    // 3. Serve toàn bộ file tĩnh từ LittleFS: index.html, styles.css, js/*.js ...
    // setDefaultFile("index.html") tự xử lý route "/"
    s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 4. ElegantOTA: truy cập /update để flash firmware mới qua browser
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

// Đợi WiFi kết nối trước khi bắt đầu (tối đa 30s, nếu không có WiFi thì vẫn chạy nhưng chỉ có AP)
if (xWiFiEventGroup != nullptr)
    xEventGroupWaitBits(xWiFiEventGroup,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,            // KHÔNG clear bit
                        pdTRUE,
                        pdMS_TO_TICKS(30000));

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

                StaticJsonDocument<384> doc;

                SENSOR_LOCK();
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
                uint8_t ledR = g_currentLEDColor.r;
                uint8_t ledG = g_currentLEDColor.g;
                uint8_t ledB = g_currentLEDColor.b;
                SENSOR_UNLOCK();

                doc["wifiRssi"] = WiFi.RSSI();
                doc["freeHeap"] = ESP.getFreeHeap();
                doc["lcdPage"]  = g_lcdPage;

                char ledHex[8];
                snprintf(ledHex, sizeof(ledHex), "#%02X%02X%02X", ledR, ledG, ledB);
                doc["ledColor"] = ledHex;

                String json;
                serializeJson(doc, json);
                s_ws.textAll(json);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms loop
    }
}