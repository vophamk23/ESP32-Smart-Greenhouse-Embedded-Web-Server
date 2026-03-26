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
 *   { "cmd": "setPump",    "value": true }
 *   { "cmd": "setFan",     "value": false }
 *   { "cmd": "setMode",    "value": "AUTO" }
 *   { "cmd": "setScheduleEnabled", "value": true }
 *   { "cmd": "setSchedules", "value": [...] }
 */

#include "webserver_greenhouse.h"
#include "iot_bridge.h"
#include "global_vars.h"
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
// LƯU LỊCH XUỐNG LITTLEFS
// ============================================================================
static void saveSchedulesToFS()
{
    File f = LittleFS.open("/schedules.dat", "w");
    if (!f)
    {
        Serial.println("[SCHED] Cannot write /schedules.dat");
        return;
    }

    DynamicJsonDocument doc(1024);
    doc["enabled"] = g_scheduleEnabled;
    JsonArray arr  = doc.createNestedArray("schedules");

    for (uint8_t i = 0; i < g_scheduleCount; i++)
    {
        JsonObject o = arr.createNestedObject();
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                 g_schedules[i].hour, g_schedules[i].minute);
        o["time"]     = timeBuf;
        o["duration"] = g_schedules[i].duration;
        o["days"]     = g_schedules[i].days;
        o["enabled"]  = g_schedules[i].enabled;
    }

    serializeJson(doc, f);
    f.close();
    Serial.println("[SCHED] Saved to /schedules.dat");
}

// ============================================================================
// LOAD LỊCH TỪ LITTLEFS
// ============================================================================
void loadSchedulesFromFS()
{
    File f = LittleFS.open("/schedules.dat", "r");
    if (!f)
    {
        Serial.println("[SCHED] /schedules.dat not found");
        return;
    }

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, f))
    {
        f.close();
        return;
    }
    f.close();

    g_scheduleEnabled = doc["enabled"].as<bool>();
    g_scheduleCount   = 0;

    JsonArray arr = doc["schedules"].as<JsonArray>();
    for (JsonObject o : arr)
    {
        if (g_scheduleCount >= MAX_SCHEDULES) break;

        uint8_t h = 0, m = 0;
        const char *t = o["time"] | "00:00";
        sscanf(t, "%hhu:%hhu", &h, &m);

        g_schedules[g_scheduleCount++] = {
            h,
            m,
            (uint8_t)(o["duration"] | 15),
            (uint8_t)(o["days"]     | 0),
            o["enabled"].as<bool>()
        };
    }

    Serial.printf("[SCHED] Loaded %d schedules, enabled=%s\n",
                  g_scheduleCount, g_scheduleEnabled ? "YES" : "NO");
}

// ============================================================================
// XỬ LÝ TIN NHẮN TỪ BROWSER → QUEUE LỆNH
// ============================================================================
static void handleBrowserMessage(const String &json)
{
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, json)) return;

    const char *cmd = doc["cmd"];
    if (!cmd) return;

    RpcPacket_t pkt = { RPC_NONE };

    // ---- setPump ----
    if (strcmp(cmd, "setPump") == 0)
    {
        pkt.command = doc["value"].as<bool>() ? RPC_PUMP_ON : RPC_PUMP_OFF;
    }
    // ---- setFan ----
    else if (strcmp(cmd, "setFan") == 0)
    {
        pkt.command = doc["value"].as<bool>() ? RPC_FAN_ON : RPC_FAN_OFF;
    }
    // ---- setMode ----
    else if (strcmp(cmd, "setMode") == 0)
    {
        const char *val = doc["value"].as<const char*>();
        if (val)
            pkt.command = (strcmp(val, "AUTO") == 0) ? RPC_MODE_AUTO : RPC_MODE_MANUAL;
    }
    // ---- setScheduleEnabled ----
    else if (strcmp(cmd, "setScheduleEnabled") == 0)
    {
        g_scheduleEnabled = doc["value"].as<bool>();
        saveSchedulesToFS();
        Serial.printf("[SCHED] Enabled: %s\n", g_scheduleEnabled ? "ON" : "OFF");
    }
    // ---- setSchedules ----
    else if (strcmp(cmd, "setSchedules") == 0)
    {
        JsonArray arr   = doc["value"].as<JsonArray>();
        g_scheduleCount = 0;

        for (JsonObject s : arr)
        {
            if (g_scheduleCount >= MAX_SCHEDULES) break;

            const char *timeStr = s["time"] | "00:00";
            uint8_t h = 0, m = 0;
            sscanf(timeStr, "%hhu:%hhu", &h, &m);

            // Parse days array [0,1,3] → bitmask
            uint8_t dayMask  = 0;
            JsonArray daysArr = s["days"].as<JsonArray>();
            for (int d : daysArr)
                if (d >= 0 && d <= 6)
                    dayMask |= (1 << d);

            g_schedules[g_scheduleCount++] = {
                h,
                m,
                (uint8_t)(constrain((int)(s["duration"] | 15), 1, 120)),
                dayMask,
                s["enabled"].as<bool>()
            };
        }

        saveSchedulesToFS();
        Serial.printf("[SCHED] Saved %d schedules\n", g_scheduleCount);
    }

    // Gửi lệnh vào queue nếu có
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
    // 1. Gắn WebSocket vào server
    s_ws.onEvent(onWsEvent);
    s_server.addHandler(&s_ws);

    // 2. REST endpoint: snapshot trạng thái hiện tại
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

    // 3. REST endpoint: lấy danh sách lịch tưới
    s_server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest *req) {
        DynamicJsonDocument doc(1024);
        doc["enabled"] = g_scheduleEnabled;
        JsonArray arr  = doc.createNestedArray("schedules");

        for (uint8_t i = 0; i < g_scheduleCount; i++)
        {
            JsonObject o = arr.createNestedObject();
            char timeBuf[6];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                     g_schedules[i].hour, g_schedules[i].minute);
            o["time"]     = timeBuf;
            o["duration"] = g_schedules[i].duration;
            o["days"]     = g_schedules[i].days;
            o["enabled"]  = g_schedules[i].enabled;
        }

        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    // 4. Serve toàn bộ file tĩnh từ LittleFS
    s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 5. ElegantOTA
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
    // Đợi WiFi kết nối trước khi bắt đầu (tối đa 30s)
    if (xWiFiEventGroup != nullptr)
        xEventGroupWaitBits(xWiFiEventGroup,
                            WIFI_CONNECTED_BIT,
                            pdFALSE,
                            pdTRUE,
                            pdMS_TO_TICKS(30000));

    // Load lịch tưới đã lưu từ LittleFS
    loadSchedulesFromFS();

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

                StaticJsonDocument<512> doc;

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

                doc["wifiRssi"]         = WiFi.RSSI();
                doc["freeHeap"]         = ESP.getFreeHeap();
                doc["lcdPage"]          = g_lcdPage;
                doc["scheduleEnabled"]  = g_scheduleEnabled;

                char ledHex[8];
                snprintf(ledHex, sizeof(ledHex), "#%02X%02X%02X", ledR, ledG, ledB);
                doc["ledColor"] = ledHex;

                String json;
                serializeJson(doc, json);
                s_ws.textAll(json);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}