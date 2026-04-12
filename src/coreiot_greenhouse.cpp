/*
 * task_coreiot_greenhouse.cpp - CoreIoT MQTT + WiFi Tasks cho Greenhouse IoT
 *
 * Kiến trúc dual-core:
 *   Core 1: Arduino loop() → Cooperative Scheduler → đọc cảm biến, điều khiển
 *   Core 0: FreeRTOS tasks → WiFi, MQTT publish, WebSocket, nhận RPC
 *
 * Giao tiếp giữa hai core:
 *   Core1 → xTelemetryQueue → Core0 (publish lên cloud + WebSocket)
 *   Core0 → xRpcCommandQueue → Core1 (thực thi lệnh từ cloud)
 *
 * Dual MQTT:
 *   s_mqttClient  → CoreIoT (app.coreiot.io:1883) — cloud
 *   s_localClient → broker.emqx.io (internet) — NestJS local
 */

#include "coreiot_greenhouse.h"
#include "iot_bridge.h"
#include "global_vars.h"
#include "actuator_handler.h"
#include "webserver_greenhouse.h" // gọi saveThresholdToFS(), saveSchedulesToFS()

// ============================================================================
// MQTT CLIENT — CoreIoT (cloud)
// ============================================================================
static WiFiClient s_wifiClient;
static PubSubClient s_mqttClient(s_wifiClient);

// ============================================================================
// MQTT CLIENT — Local / emqx (NestJS)
// ============================================================================
static WiFiClient s_localWifiClient;
static PubSubClient s_localClient(s_localWifiClient);

// Topics ThingsBoard/CoreIoT
static const char *TOPIC_TELEMETRY = "v1/devices/me/telemetry";
static const char *TOPIC_ATTRIBUTES = "v1/devices/me/attributes";
static const char *TOPIC_RPC_REQUEST = "v1/devices/me/rpc/request/+";

// ============================================================================
// HELPER: Lấy MAC không dấu ':' — dùng làm suffix topic
// Cache lại để không gọi WiFi.macAddress() nhiều lần trong publishTelemetry()
// ============================================================================
static String getMacFlat()
{
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return mac;
}

// ============================================================================
// HELPER: Lấy request ID từ topic RPC "v1/devices/me/rpc/request/<id>"
// ============================================================================
static const char *extractRequestId(const char *topic)
{
    const char *p = strrchr(topic, '/');
    if (!p || *(p + 1) == '\0')
        return nullptr;
    return p + 1;
}

// ============================================================================
// HELPER: Gửi RPC response về CoreIoT
// ============================================================================
static void sendRpcResponse(const char *requestId, const char *method,
                            bool success, const char *extraKey = nullptr,
                            const char *extraVal = nullptr)
{
    if (!requestId)
        return;

    char respTopic[64];
    snprintf(respTopic, sizeof(respTopic),
             "v1/devices/me/rpc/response/%s", requestId);

    StaticJsonDocument<128> resp;
    resp["method"] = method;
    resp["success"] = success;
    if (extraKey && extraVal)
        resp[extraKey] = extraVal;

    String payload;
    serializeJson(resp, payload);
    s_mqttClient.publish(respTopic, payload.c_str());
}

// ============================================================================
// MQTT CALLBACK: Nhận RPC từ CoreIoT cloud
// ============================================================================
static void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    char msg[256];
    length = (length > sizeof(msg) - 1) ? sizeof(msg) - 1 : length;
    memcpy(msg, payload, length);
    msg[length] = '\0';

    Serial.printf("[MQTT] RPC: %s\n", msg);

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg))
        return;

    const char *method = doc["method"];
    const char *requestId = extractRequestId(topic);
    if (!method)
        return;

    // RPC method "getStatus" để NestJS có thể lấy trạng thái hiện tại ngay khi cần, thay vì phải đợi telemetry tiếp theo (tối đa 1s)
    if (strcmp(method, "setPump") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = {state ? RPC_PUMP_ON : RPC_PUMP_OFF};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true, "pump", state ? "ON" : "OFF");
    }
    else if (strcmp(method, "setFan") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = {state ? RPC_FAN_ON : RPC_FAN_OFF};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true, "fan", state ? "ON" : "OFF");
    }
    else if (strcmp(method, "setMode") == 0)
    {
        bool isAuto = false;
        if (doc["params"].is<bool>())
            isAuto = doc["params"].as<bool>();
        else if (doc["params"].is<const char *>())
            isAuto = (strcmp(doc["params"].as<const char *>(), "AUTO") == 0);

        RpcPacket_t pkt = {isAuto ? RPC_MODE_AUTO : RPC_MODE_MANUAL};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true, "mode", isAuto ? "AUTO" : "MANUAL");
    }
    else if (strcmp(method, "getStatus") == 0)
    {
        StaticJsonDocument<256> resp;
        resp["method"] = "getStatus";
        resp["pump"] = (bool)g_pumpState;
        resp["fan"] = (bool)g_fanState;
        resp["mode"] = g_autoMode ? "AUTO" : "MANUAL";
        resp["temperature"] = g_temperature;
        resp["humidity"] = g_humidity;
        resp["soilMoisture"] = g_soilMoisture;

        if (requestId)
        {
            char respTopic[64];
            snprintf(respTopic, sizeof(respTopic),
                     "v1/devices/me/rpc/response/%s", requestId);
            String payload;
            serializeJson(resp, payload);
            s_mqttClient.publish(respTopic, payload.c_str());
        }
    }
}

// ============================================================================
// MQTT CALLBACK: Nhận lệnh từ NestJS qua topic "ecogreen/command/<MAC>"
// Không gửi RPC response — NestJS tự biết kết quả qua telemetry tiếp theo
// ============================================================================
static void onLocalMessage(char *topic, byte *payload, unsigned int length)
{
    char msg[512];
    length = (length > sizeof(msg) - 1) ? sizeof(msg) - 1 : length;
    memcpy(msg, payload, length);
    msg[length] = '\0';

    Serial.printf("[MQTT-LOCAL] CMD: %s\n", msg);

    // FIX: tăng document size để parse setSchedules
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, msg))
    {
        Serial.println("[MQTT-LOCAL] JSON parse failed");
        return;
    }

    const char *method = doc["method"];
    if (!method)
        return;

    // ---- setPump ----
    if (strcmp(method, "setPump") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = {state ? RPC_PUMP_ON : RPC_PUMP_OFF};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        Serial.printf("[MQTT-LOCAL] setPump → %s\n", state ? "ON" : "OFF");
    }
    // ---- setFan ----
    else if (strcmp(method, "setFan") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = {state ? RPC_FAN_ON : RPC_FAN_OFF};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        Serial.printf("[MQTT-LOCAL] setFan → %s\n", state ? "ON" : "OFF");
    }
    // ---- setMode ----
    else if (strcmp(method, "setMode") == 0)
    {
        bool isAuto = false;
        if (doc["params"].is<bool>())
            isAuto = doc["params"].as<bool>();
        else if (doc["params"].is<const char *>())
            isAuto = (strcmp(doc["params"].as<const char *>(), "AUTO") == 0);

        RpcPacket_t pkt = {isAuto ? RPC_MODE_AUTO : RPC_MODE_MANUAL};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        Serial.printf("[MQTT-LOCAL] setMode → %s\n", isAuto ? "AUTO" : "MANUAL");
    }
    // ---- setThreshold ----
    else if (strcmp(method, "setThreshold") == 0)
    {
        float soilDry = doc["params"]["soilDry"] | g_soilDryThreshold;
        float soilWet = doc["params"]["soilWet"] | g_soilWetThreshold;
        float tempHigh = doc["params"]["tempHigh"] | g_tempHighThreshold;
        int pumpMax = doc["params"]["pumpMax"] | (int)(g_pumpMaxOnMs / 1000UL);
        int pumpCool = doc["params"]["pumpCool"] | (int)(g_pumpCooldownMs / 1000UL);

        // Validate trước khi ghi
        if (soilDry >= soilWet || tempHigh <= 0.0f)
        {
            Serial.println("[MQTT-LOCAL] setThreshold: invalid values - rejected");
            return;
        }

        SENSOR_LOCK();
        g_soilDryThreshold = soilDry;
        g_soilWetThreshold = soilWet;
        g_tempHighThreshold = tempHigh;
        g_tempLowThreshold = tempHigh - 1.0f; // hysteresis cố định 1°C
        g_pumpMaxOnMs = (unsigned long)pumpMax * 1000UL;
        g_pumpCooldownMs = (unsigned long)pumpCool * 1000UL;
        SENSOR_UNLOCK();

        saveThresholdToFS(); // lưu xuống LittleFS
        Serial.printf("[MQTT-LOCAL] setThreshold: soilDry=%.0f soilWet=%.0f tempHigh=%.0f pumpMax=%ds pumpCool=%ds\n",
                      soilDry, soilWet, tempHigh, pumpMax, pumpCool);
    }
    // ---- setSchedules ----
    else if (strcmp(method, "setSchedules") == 0)
    {
        bool enabled = doc["params"]["enabled"] | g_scheduleEnabled;
        JsonArray arr = doc["params"]["schedules"].as<JsonArray>();
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
                h, m,
                (uint8_t)(constrain((int)(s["duration"] | 15), 1, 120)),
                dayMask,
                s["enabled"].as<bool>()};
        }

        SENSOR_LOCK();
        g_scheduleEnabled = enabled;
        g_scheduleCount = newCount;
        memcpy(g_schedules, tmp, sizeof(ScheduleEntry_t) * newCount);
        SENSOR_UNLOCK();

        saveSchedulesToFS(); // lưu xuống LittleFS
        Serial.printf("[MQTT-LOCAL] setSchedules: %d entries, enabled=%s\n",
                      newCount, enabled ? "YES" : "NO");
    }
    else
    {
        Serial.printf("[MQTT-LOCAL] Unknown method: %s\n", method);
    }
}

// ============================================================================
// MQTT RECONNECT — CoreIoT
// ============================================================================
static bool mqttReconnect()
{
    if (s_mqttClient.connected())
        return true;
    if (WiFi.status() != WL_CONNECTED)
        return false;

    Serial.print("[MQTT] Connecting to CoreIoT...");
    String clientId = "GH-ESP32-" + String(random(0xffff), HEX);

    if (s_mqttClient.connect(clientId.c_str(), CORE_IOT_TOKEN, nullptr))
    {
        Serial.println(" OK");
        s_mqttClient.subscribe(TOPIC_RPC_REQUEST);

        StaticJsonDocument<128> attr;
        attr["fw_version"] = "1.0.0";
        attr["device"] = "Greenhouse-ESP32";
        attr["mac"] = WiFi.macAddress();
        String attrStr;
        serializeJson(attr, attrStr);
        s_mqttClient.publish(TOPIC_ATTRIBUTES, attrStr.c_str());

        return true;
    }

    Serial.printf(" FAILED (rc=%d)\n", s_mqttClient.state());
    return false;
}

// ============================================================================
// MQTT RECONNECT — Local / emqx (NestJS)
// subscribe "ecogreen/command/<MAC>" sau khi connect
// ============================================================================
static bool localMqttReconnect()
{
    if (s_localClient.connected())
        return true;
    if (WiFi.status() != WL_CONNECTED)
        return false;

    Serial.print("[MQTT-LOCAL] Connecting...");
    String clientId = "GH-LOCAL-" + String(random(0xffff), HEX);

    if (s_localClient.connect(clientId.c_str()))
    {
        Serial.println(" OK");

        String cmdTopic = "ecogreen/command/" + getMacFlat();
        s_localClient.subscribe(cmdTopic.c_str());
        Serial.printf("[MQTT-LOCAL] Subscribed: %s\n", cmdTopic.c_str());

        return true;
    }

    Serial.printf(" FAILED (rc=%d)\n", s_localClient.state());
    return false;
}

// ============================================================================
// PUBLISH TELEMETRY — CoreIoT + Local
// FIX: bổ sung đầy đủ fields cho local broker (NestJS cần để build web)
// ============================================================================
static void publishTelemetry(const TelemetryPacket_t &pkt)
{
    // Cache MAC 1 lần, dùng cho cả topic và field "mac"
    String mac = WiFi.macAddress();
    String macFlat = getMacFlat();

    // FIX: tăng lên 512 để chứa thêm fields alert + ledColor
    StaticJsonDocument<512> tele;
    tele["mac"] = mac;
    tele["temperature"] = pkt.temperature;
    tele["humidity"] = pkt.humidity;
    tele["soilMoisture"] = pkt.soilMoisture;
    tele["lightLux"] = pkt.lightLux;
    tele["pump"] = pkt.pumpState;
    tele["fan"] = pkt.fanState;
    tele["autoMode"] = pkt.autoMode;
    tele["dhtError"] = pkt.dhtError;
    tele["alertTemp"] = pkt.alertTemp;
    tele["alertHumidity"] = pkt.alertHumidity;
    tele["alertSoil"] = pkt.alertSoil;
    tele["alertLight"] = pkt.alertLight;
    tele["pumpCount"] = (int)pkt.pumpCount;
    tele["totalPumpTimeSec"] = (long)pkt.totalPumpTimeSec;

    char ledHex[8];
    snprintf(ledHex, sizeof(ledHex), "#%02X%02X%02X",
             pkt.ledR, pkt.ledG, pkt.ledB);
    tele["ledColor"] = ledHex;

    String teleStr;
    serializeJson(tele, teleStr);

    // Publish lên CoreIoT (chỉ các field cơ bản, giữ nhỏ gọn)
    if (s_mqttClient.connected())
        s_mqttClient.publish(TOPIC_TELEMETRY, teleStr.c_str());

    // Publish lên local broker — đầy đủ fields cho NestJS
    if (s_localClient.connected())
    {
        String teleTopic = "ecogreen/telemetry/" + macFlat;
        s_localClient.publish(teleTopic.c_str(), teleStr.c_str());
        Serial.printf("[MQTT-LOCAL] Published → %s\n", teleTopic.c_str());
    }
    else
    {
        Serial.println("[MQTT-LOCAL] Not connected, skip publish");
    }

    // --- Attributes: thống kê và cảnh báo (chỉ lên CoreIoT) ---
    // FIX: tăng lên 384 để an toàn khi thêm field sau này
    StaticJsonDocument<384> attr;
    attr["pumpCount"] = pkt.pumpCount;
    attr["totalPumpTimeSec"] = pkt.totalPumpTimeSec;
    attr["alertTemp"] = pkt.alertTemp;
    attr["alertHumidity"] = pkt.alertHumidity;
    attr["alertSoil"] = pkt.alertSoil;
    attr["alertLight"] = pkt.alertLight;
    attr["dhtError"] = pkt.dhtError;
    attr["ledColor"] = ledHex;

    String attrStr;
    serializeJson(attr, attrStr);

    if (s_mqttClient.connected())
        s_mqttClient.publish(TOPIC_ATTRIBUTES, attrStr.c_str());

    Serial.printf("[MQTT] Published: T=%.1f H=%.0f Soil=%.0f Light=%.0f\n",
                  pkt.temperature, pkt.humidity,
                  pkt.soilMoisture, pkt.lightLux);
}

// ============================================================================
// COREIOT TASK (Core 0)
// ============================================================================
void greenhouse_coreiot_task(void *pvParameters)
{
    if (xWiFiEventGroup != nullptr)
        xEventGroupWaitBits(xWiFiEventGroup,
                            WIFI_CONNECTED_BIT,
                            pdFALSE,
                            pdTRUE,
                            pdMS_TO_TICKS(60000));

    Serial.println("[MQTT] Starting CoreIoT task...");

    // Setup CoreIoT
    s_mqttClient.setServer(CORE_IOT_SERVER, atoi(CORE_IOT_PORT));
    s_mqttClient.setCallback(onMqttMessage);
    s_mqttClient.setKeepAlive(30);
    s_mqttClient.setBufferSize(512); // FIX: tránh drop payload > 256 bytes

    // Setup Local
    s_localClient.setServer(LOCAL_MQTT_HOST, atoi(LOCAL_MQTT_PORT));
    s_localClient.setCallback(onLocalMessage);
    s_localClient.setKeepAlive(30);
    s_localClient.setBufferSize(512); // FIX: setSchedules payload có thể lớn

    unsigned long lastReconnectAttempt = 0;
    unsigned long lastLocalReconnectAttempt = 0;

    for (;;)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        unsigned long now = millis();

        // Reconnect CoreIoT
        if (!s_mqttClient.connected() && now - lastReconnectAttempt >= 5000)
        {
            lastReconnectAttempt = now;
            mqttReconnect();
        }

        // Reconnect Local
        if (!s_localClient.connected() && now - lastLocalReconnectAttempt >= 5000)
        {
            lastLocalReconnectAttempt = now;
            localMqttReconnect();
        }

        // MQTT loop — xử lý incoming messages và keepalive
        if (s_mqttClient.connected())
            s_mqttClient.loop();

        if (s_localClient.connected())
            s_localClient.loop();

        // Đọc telemetry từ queue và publish
        TelemetryPacket_t pkt;
        if (xQueueReceive(xTelemetryQueue, &pkt, 0) == pdTRUE)
        {
            publishTelemetry(pkt);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}