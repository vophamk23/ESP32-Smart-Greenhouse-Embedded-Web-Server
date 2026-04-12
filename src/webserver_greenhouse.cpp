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
 *     "pumpCount": 3, "totalPumpTimeSec": 240,
 *     "cfg_soilDry": 30, "cfg_soilWet": 65,
 *     "cfg_tempHigh": 32, "cfg_pumpMax": 120, "cfg_pumpCool": 120
 *   }
 *
 * WebSocket JSON nhận từ browser:
 *   { "cmd": "setPump",    "value": true }
 *   { "cmd": "setFan",     "value": false }
 *   { "cmd": "setMode",    "value": "AUTO" }
 *   { "cmd": "setScheduleEnabled", "value": true }
 *   { "cmd": "setSchedules", "value": [...] }
 *   { "cmd": "setThreshold", "soilDry": 30, "soilWet": 65,
 *                             "tempHigh": 32, "pumpMax": 120, "pumpCool": 120 }
 */

#include "webserver_greenhouse.h"
#include "iot_bridge.h"
#include "global_vars.h"
#include "config.h"
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
static bool s_serverRunning = false;

// ============================================================================
// LƯU LỊCH XUỐNG LITTLEFS
// ============================================================================
void saveSchedulesToFS()
{
    File f = LittleFS.open("/schedules.dat", "w");
    if (!f)
    {
        Serial.println("[SCHED] Cannot write /schedules.dat");
        return;
    }

    // Snapshot trước khi serialize để không giữ lock trong lúc ghi file
    bool enabled;
    uint8_t count;
    ScheduleEntry_t tmp[MAX_SCHEDULES];

    SENSOR_LOCK();
    enabled = g_scheduleEnabled;
    count = g_scheduleCount;
    memcpy(tmp, g_schedules, sizeof(ScheduleEntry_t) * count);
    SENSOR_UNLOCK();

    DynamicJsonDocument doc(1024);
    doc["enabled"] = enabled;
    JsonArray arr = doc.createNestedArray("schedules");

    for (uint8_t i = 0; i < count; i++)
    {
        JsonObject o = arr.createNestedObject();
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                 tmp[i].hour, tmp[i].minute);
        o["time"] = timeBuf;
        o["duration"] = tmp[i].duration;
        o["days"] = tmp[i].days;
        o["enabled"] = tmp[i].enabled;
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

    // Parse vào biến tạm trước, rồi mới lock để ghi vào global
    bool newEnabled = doc["enabled"].as<bool>();
    uint8_t newCount = 0;
    ScheduleEntry_t tmp[MAX_SCHEDULES];

    JsonArray arr = doc["schedules"].as<JsonArray>();
    for (JsonObject o : arr)
    {
        if (newCount >= MAX_SCHEDULES)
            break;

        uint8_t h = 0, m = 0;
        const char *t = o["time"] | "00:00";
        sscanf(t, "%hhu:%hhu", &h, &m);

        tmp[newCount++] = {
            h,
            m,
            (uint8_t)(o["duration"] | 15),
            (uint8_t)(o["days"] | 0),
            o["enabled"].as<bool>()};
    }

    SENSOR_LOCK();
    g_scheduleEnabled = newEnabled;
    g_scheduleCount = newCount;
    memcpy(g_schedules, tmp, sizeof(ScheduleEntry_t) * newCount);
    SENSOR_UNLOCK();

    Serial.printf("[SCHED] Loaded %d schedules, enabled=%s\n",
                  newCount, newEnabled ? "YES" : "NO");
}

// ============================================================================
// LƯU NGƯỠNG XUỐNG LITTLEFS
// ============================================================================
void saveThresholdToFS()
{
    DynamicJsonDocument doc(256);

    SENSOR_LOCK();
    doc["soilDry"] = g_soilDryThreshold;
    doc["soilWet"] = g_soilWetThreshold;
    doc["tempHigh"] = g_tempHighThreshold;
    doc["tempLow"] = g_tempLowThreshold;
    doc["pumpMax"] = (int)(g_pumpMaxOnMs / 1000UL);
    doc["pumpCool"] = (int)(g_pumpCooldownMs / 1000UL);
    SENSOR_UNLOCK();

    File f = LittleFS.open("/threshold.dat", "w");
    if (!f)
    {
        Serial.println("[THRESH] Cannot write /threshold.dat");
        return;
    }
    serializeJson(doc, f);
    f.close();
    Serial.println("[THRESH] Saved to /threshold.dat");
}

// ============================================================================
// LOAD NGƯỠNG TỪ LITTLEFS
// ============================================================================
void loadThresholdFromFS()
{
    File f = LittleFS.open("/threshold.dat", "r");
    if (!f)
    {
        Serial.println("[THRESH] /threshold.dat not found - using defaults");
        return;
    }

    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, f))
    {
        f.close();
        return;
    }
    f.close();

    // Parse vào biến tạm, validate, rồi mới lock ghi global
    float soilDry = doc["soilDry"] | DEFAULT_SOIL_DRY;
    float soilWet = doc["soilWet"] | DEFAULT_SOIL_WET;
    float tempHigh = doc["tempHigh"] | DEFAULT_TEMP_HIGH;
    float tempLow = doc["tempLow"] | DEFAULT_TEMP_LOW;
    int pumpMax = doc["pumpMax"] | 120;
    int pumpCool = doc["pumpCool"] | 120;

    // Validate: soilDry phải nhỏ hơn soilWet, tempLow < tempHigh
    if (soilDry >= soilWet || tempLow >= tempHigh)
    {
        Serial.println("[THRESH] Invalid values in file - using defaults");
        return;
    }

    SENSOR_LOCK();
    g_soilDryThreshold = soilDry;
    g_soilWetThreshold = soilWet;
    g_tempHighThreshold = tempHigh;
    g_tempLowThreshold = tempLow;
    g_pumpMaxOnMs = (unsigned long)pumpMax * 1000UL;
    g_pumpCooldownMs = (unsigned long)pumpCool * 1000UL;
    SENSOR_UNLOCK();

    Serial.printf("[THRESH] Loaded: soilDry=%.0f soilWet=%.0f tempHigh=%.0f pumpMax=%ds pumpCool=%ds\n",
                  soilDry, soilWet, tempHigh, pumpMax, pumpCool);
}

// ============================================================================
// XỬ LÝ TIN NHẮN TỪ BROWSER → QUEUE LỆNH
// ============================================================================
static void handleBrowserMessage(const String &json)
{
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, json))
        return;

    const char *cmd = doc["cmd"];
    if (!cmd)
        return;

    RpcPacket_t pkt = {RPC_NONE};

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
        const char *val = doc["value"].as<const char *>();
        if (val)
            pkt.command = (strcmp(val, "AUTO") == 0) ? RPC_MODE_AUTO : RPC_MODE_MANUAL;
    }

    // Set bật/tắt lịch tưới tự động - đơn giản nên gộp chung vào "setMode" cũng được, nhưng tách riêng cho rõ ràng
    else if (strcmp(cmd, "setScheduleEnabled") == 0)
    {
        bool val = doc["value"].as<bool>();
        SENSOR_LOCK();
        g_scheduleEnabled = val;
        SENSOR_UNLOCK();

        saveSchedulesToFS();
        Serial.printf("[SCHED] Enabled: %s\n", val ? "ON" : "OFF");
    }

    // Set toàn bộ lịch tưới (danh sách) — payload phức tạp hơn nên tách riêng thành "setSchedules"
    else if (strcmp(cmd, "setSchedules") == 0)
    {
        JsonArray arr = doc["value"].as<JsonArray>();
        uint8_t newCount = 0;
        ScheduleEntry_t tmp[MAX_SCHEDULES];

        for (JsonObject s : arr)
        {
            if (newCount >= MAX_SCHEDULES)
                break;

            const char *timeStr = s["time"] | "00:00";
            uint8_t h = 0, m = 0;
            sscanf(timeStr, "%hhu:%hhu", &h, &m);

            uint8_t dayMask = 0;
            JsonArray daysArr = s["days"].as<JsonArray>();
            for (int d : daysArr)
                if (d >= 0 && d <= 6)
                    dayMask |= (1 << d);

            tmp[newCount++] = {
                h,
                m,
                (uint8_t)(constrain((int)(s["duration"] | 15), 1, 120)),
                dayMask,
                s["enabled"].as<bool>()};
        }

        SENSOR_LOCK();
        g_scheduleCount = newCount;
        memcpy(g_schedules, tmp, sizeof(ScheduleEntry_t) * newCount);
        SENSOR_UNLOCK();

        saveSchedulesToFS();
        Serial.printf("[SCHED] Saved %d schedules\n", newCount);
    }

    // Set ngưỡng điều khiển (soilDry, soilWet, tempHigh, tempLow, pumpMax, pumpCool)
    else if (strcmp(cmd, "setThreshold") == 0)
    {
        float soilDry = doc["soilDry"] | g_soilDryThreshold;
        float soilWet = doc["soilWet"] | g_soilWetThreshold;
        float tempHigh = doc["tempHigh"] | g_tempHighThreshold;
        float tempLow = tempHigh - 1.0f; // hysteresis cố định 1°C
        int pumpMax = doc["pumpMax"] | (int)(g_pumpMaxOnMs / 1000UL);
        int pumpCool = doc["pumpCool"] | (int)(g_pumpCooldownMs / 1000UL);

        // Validate trước khi ghi
        if (soilDry >= soilWet)
        {
            Serial.println("[THRESH] soilDry >= soilWet - rejected");
            return;
        }

        SENSOR_LOCK();
        g_soilDryThreshold = soilDry;
        g_soilWetThreshold = soilWet;
        g_tempHighThreshold = tempHigh;
        g_tempLowThreshold = tempLow;
        g_pumpMaxOnMs = (unsigned long)pumpMax * 1000UL;
        g_pumpCooldownMs = (unsigned long)pumpCool * 1000UL;
        SENSOR_UNLOCK();

        saveThresholdToFS();
        Serial.printf("[THRESH] Updated: soilDry=%.0f soilWet=%.0f tempHigh=%.0f pumpMax=%ds pumpCool=%ds\n",
                      soilDry, soilWet, tempHigh, pumpMax, pumpCool);
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
    s_server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req)
                {
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
        req->send(200, "application/json", json); });

    // 3. REST endpoint: lấy danh sách lịch tưới
    s_server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest *req)
                {
        bool enabled;
        uint8_t count;
        ScheduleEntry_t tmp[MAX_SCHEDULES];

        SENSOR_LOCK();
        enabled = g_scheduleEnabled;
        count   = g_scheduleCount;
        memcpy(tmp, g_schedules, sizeof(ScheduleEntry_t) * count);
        SENSOR_UNLOCK();

        DynamicJsonDocument doc(1024);
        doc["enabled"] = enabled;
        JsonArray arr  = doc.createNestedArray("schedules");

        for (uint8_t i = 0; i < count; i++)
        {
            JsonObject o = arr.createNestedObject();
            char timeBuf[6];
            snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                     tmp[i].hour, tmp[i].minute);
            o["time"]     = timeBuf;
            o["duration"] = tmp[i].duration;
            o["days"]     = tmp[i].days;
            o["enabled"]  = tmp[i].enabled;
        }

        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json); });

    // 4. REST endpoint: lấy ngưỡng điều khiển hiện tại
    s_server.on("/api/threshold", HTTP_GET, [](AsyncWebServerRequest *req)
                {
        StaticJsonDocument<256> doc;

        SENSOR_LOCK();
        doc["soilDry"]  = g_soilDryThreshold;
        doc["soilWet"]  = g_soilWetThreshold;
        doc["tempHigh"] = g_tempHighThreshold;
        doc["tempLow"]  = g_tempLowThreshold;
        doc["pumpMax"]  = (int)(g_pumpMaxOnMs  / 1000UL);
        doc["pumpCool"] = (int)(g_pumpCooldownMs / 1000UL);
        SENSOR_UNLOCK();

        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json); });

    // 5. Serve toàn bộ file tĩnh từ LittleFS
    s_server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 6. ElegantOTA
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

                // Tăng lên 640 để chứa thêm cfg_* fields
                StaticJsonDocument<640> doc;

                uint8_t ledR, ledG, ledB;
                float cfgSoilDry, cfgSoilWet, cfgTempHigh;
                int cfgPumpMax, cfgPumpCool;

                SENSOR_LOCK();
                doc["temperature"] = g_temperature;
                doc["humidity"] = g_humidity;
                doc["soilMoisture"] = g_soilMoisture;
                doc["lightLux"] = g_lightLux;
                doc["pump"] = g_pumpState;
                doc["fan"] = g_fanState;
                doc["autoMode"] = g_autoMode;
                doc["alertTemp"] = g_alertTemp;
                doc["alertHumidity"] = g_alertHumidity;
                doc["alertSoil"] = g_alertSoil;
                doc["alertLight"] = g_alertLight;
                doc["pumpCount"] = (int)g_pumpCount;
                doc["totalPumpTimeSec"] = (long)(g_totalPumpTime / 1000UL);
                doc["dhtError"] = g_dhtError;
                doc["lcdPage"] = g_lcdPage;
                doc["scheduleEnabled"] = g_scheduleEnabled;
                ledR = g_currentLEDColor.r;
                ledG = g_currentLEDColor.g;
                ledB = g_currentLEDColor.b;
                cfgSoilDry = g_soilDryThreshold;
                cfgSoilWet = g_soilWetThreshold;
                cfgTempHigh = g_tempHighThreshold;
                cfgPumpMax = (int)(g_pumpMaxOnMs / 1000UL);
                cfgPumpCool = (int)(g_pumpCooldownMs / 1000UL);
                SENSOR_UNLOCK();

                // thread-safe, không cần lock
                doc["wifiRssi"] = WiFi.RSSI();
                doc["freeHeap"] = ESP.getFreeHeap();

                char ledHex[8];
                snprintf(ledHex, sizeof(ledHex), "#%02X%02X%02X", ledR, ledG, ledB);
                doc["ledColor"] = ledHex;

                // Gửi ngưỡng hiện tại để frontend sync slider khi load trang
                doc["cfg_soilDry"] = cfgSoilDry;
                doc["cfg_soilWet"] = cfgSoilWet;
                doc["cfg_tempHigh"] = cfgTempHigh;
                doc["cfg_pumpMax"] = cfgPumpMax;
                doc["cfg_pumpCool"] = cfgPumpCool;

                String json;
                serializeJson(doc, json);
                s_ws.textAll(json);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}