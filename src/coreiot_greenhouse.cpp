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
 *   s_localClient → Mosquitto local (192.168.x.x:1883) — NestJS
 */

#include "coreiot_greenhouse.h"
#include "iot_bridge.h"
#include "global_vars.h"
#include "actuator_handler.h"

// ============================================================================
// MQTT CLIENT — CoreIoT (cloud)
// ============================================================================
static WiFiClient s_wifiClient;
static PubSubClient s_mqttClient(s_wifiClient);

// ============================================================================
// MQTT CLIENT — Local Mosquitto (NestJS)
// ============================================================================
static WiFiClient s_localWifiClient;
static PubSubClient s_localClient(s_localWifiClient);

// IP máy tính chạy NestJS (cùng WiFi với ESP32)
// ĐỔI thành IP thực tế của máy bạn (xem bằng: ipconfig)
#define LOCAL_TOPIC_TELEMETRY "ecogreen/telemetry"

// Topics ThingsBoard/CoreIoT
static const char *TOPIC_TELEMETRY = "v1/devices/me/telemetry";
static const char *TOPIC_ATTRIBUTES = "v1/devices/me/attributes";
static const char *TOPIC_RPC_REQUEST = "v1/devices/me/rpc/request/+";

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
// HELPER: Gửi RPC response
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
// MQTT CALLBACK: Nhận RPC từ cloud
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
        String attrStr;
        serializeJson(attr, attrStr);
        s_mqttClient.publish(TOPIC_ATTRIBUTES, attrStr.c_str());

        return true;
    }

    Serial.printf(" FAILED (rc=%d)\n", s_mqttClient.state());
    return false;
}

// ============================================================================
// MQTT RECONNECT — Local Mosquitto
// ============================================================================
static bool localMqttReconnect()
{
    if (s_localClient.connected())
        return true;
    if (WiFi.status() != WL_CONNECTED)
        return false;

    Serial.print("[MQTT-LOCAL] Connecting to Mosquitto...");
    String clientId = "GH-LOCAL-" + String(random(0xffff), HEX);

    if (s_localClient.connect(clientId.c_str()))
    {
        Serial.println(" OK");
        return true;
    }

    Serial.printf(" FAILED (rc=%d)\n", s_localClient.state());
    return false;
}

// ============================================================================
// PUBLISH TELEMETRY — CoreIoT + Local
// ============================================================================
static void publishTelemetry(const TelemetryPacket_t &pkt)
{
    // --- Build JSON telemetry ---
    StaticJsonDocument<384> tele;
    tele["temperature"] = pkt.temperature;
    tele["humidity"] = pkt.humidity;
    tele["soilMoisture"] = pkt.soilMoisture;
    tele["lightLux"] = pkt.lightLux;
    tele["pump"] = pkt.pumpState;
    tele["fan"] = pkt.fanState;
    tele["autoMode"] = pkt.autoMode;

    String teleStr;
    serializeJson(tele, teleStr);

    // → Publish lên CoreIoT
    if (s_mqttClient.connected())
        s_mqttClient.publish(TOPIC_TELEMETRY, teleStr.c_str());

    // → Publish lên Mosquitto local (NestJS nhận)
    Serial.printf("[MQTT-LOCAL] connected=%d\n", s_localClient.connected());
    if (s_localClient.connected())
        s_localClient.publish(LOCAL_TOPIC_TELEMETRY, teleStr.c_str());

    // --- Attributes: thống kê và cảnh báo ---
    StaticJsonDocument<256> attr;
    attr["pumpCount"] = pkt.pumpCount;
    attr["totalPumpTimeSec"] = pkt.totalPumpTimeSec;
    attr["alertTemp"] = pkt.alertTemp;
    attr["alertHumidity"] = pkt.alertHumidity;
    attr["alertSoil"] = pkt.alertSoil;
    attr["alertLight"] = pkt.alertLight;
    attr["dhtError"] = pkt.dhtError;

    char ledHex[8];
    snprintf(ledHex, sizeof(ledHex), "#%02X%02X%02X",
             pkt.ledR, pkt.ledG, pkt.ledB);
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

    // Setup Local Mosquitto
    s_localClient.setServer(LOCAL_MQTT_HOST, atoi(LOCAL_MQTT_PORT));
    s_localClient.setKeepAlive(30);

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

        // MQTT loop
        if (s_mqttClient.connected())
            s_mqttClient.loop();

        if (s_localClient.connected())
            s_localClient.loop();

        // Đọc telemetry từ queue
        TelemetryPacket_t pkt;
        if (xQueueReceive(xTelemetryQueue, &pkt, 0) == pdTRUE)
        {
            publishTelemetry(pkt);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}