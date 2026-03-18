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
 */

#include "coreiot_greenhouse.h"
#include "iot_bridge.h"
#include "global_vars.h"      // [FIX] Thêm include để dùng g_temperature, g_pumpState...
#include "actuator_handler.h" // pumpOn(), pumpOff(), fanOn(), fanOff()

// ============================================================================
// MQTT CLIENT
// ============================================================================
static WiFiClient   s_wifiClient;
static PubSubClient s_mqttClient(s_wifiClient);

// Topics ThingsBoard/CoreIoT
static const char* TOPIC_TELEMETRY   = "v1/devices/me/telemetry";
static const char* TOPIC_ATTRIBUTES  = "v1/devices/me/attributes";
static const char* TOPIC_RPC_REQUEST = "v1/devices/me/rpc/request/+";

// ============================================================================
// HELPER: Lấy request ID từ topic RPC "v1/devices/me/rpc/request/<id>"
// ============================================================================
static const char* extractRequestId(const char *topic)
{
    const char *p = strrchr(topic, '/');
    if (!p || *(p + 1) == '\0') return nullptr;
    return p + 1;
}

// ============================================================================
// HELPER: Gửi RPC response
// ============================================================================
static void sendRpcResponse(const char *requestId, const char *method,
                            bool success, const char *extraKey = nullptr,
                            const char *extraVal = nullptr)
{
    if (!requestId) return;

    char respTopic[64];
    snprintf(respTopic, sizeof(respTopic),
             "v1/devices/me/rpc/response/%s", requestId);

    StaticJsonDocument<128> resp;
    resp["method"]  = method;
    resp["success"] = success;
    if (extraKey && extraVal) resp[extraKey] = extraVal;

    String payload;
    serializeJson(resp, payload);
    s_mqttClient.publish(respTopic, payload.c_str());
}

// ============================================================================
// MQTT CALLBACK: Nhận RPC từ cloud
// Chạy trong context của s_mqttClient.loop() → Core 0
// Đẩy lệnh vào xRpcCommandQueue để Core 1 thực thi an toàn
// ============================================================================
static void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    // Parse JSON
    char msg[256];
    length = (length > sizeof(msg) - 1) ? sizeof(msg) - 1 : length;
    memcpy(msg, payload, length);
    msg[length] = '\0';

    Serial.printf("[MQTT] RPC: %s\n", msg);

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg)) return;

    const char *method    = doc["method"];
    const char *requestId = extractRequestId(topic);
    if (!method) return;

    // ---- setPump ----
    if (strcmp(method, "setPump") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = { state ? RPC_PUMP_ON : RPC_PUMP_OFF };
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true,
                        "pump", state ? "ON" : "OFF");
    }
    // ---- setFan ----
    else if (strcmp(method, "setFan") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = { state ? RPC_FAN_ON : RPC_FAN_OFF };
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true,
                        "fan", state ? "ON" : "OFF");
    }
    // ---- setMode ----
    else if (strcmp(method, "setMode") == 0)
    {
        // params: "AUTO" hoặc "MANUAL" hoặc true/false
        bool isAuto = false;
        if (doc["params"].is<bool>())
            isAuto = doc["params"].as<bool>();
        else if (doc["params"].is<const char*>())
            isAuto = (strcmp(doc["params"].as<const char*>(), "AUTO") == 0);

        RpcPacket_t pkt = { isAuto ? RPC_MODE_AUTO : RPC_MODE_MANUAL };
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true,
                        "mode", isAuto ? "AUTO" : "MANUAL");
    }
    // ---- getStatus (GET request) ----
    else if (strcmp(method, "getStatus") == 0)
    {
        // Không cần queue, chỉ cần đọc global vars an toàn
        // (chỉ đọc, không ghi → không cần mutex cho ESP32 single-writer)
        StaticJsonDocument<256> resp;
        resp["method"]       = "getStatus";
        resp["pump"]         = (bool)g_pumpState;  // extern từ global_vars
        resp["fan"]          = (bool)g_fanState;
        resp["mode"]         = g_autoMode ? "AUTO" : "MANUAL";
        resp["temperature"]  = g_temperature;
        resp["humidity"]     = g_humidity;
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
// MQTT RECONNECT
// ============================================================================
static bool mqttReconnect()
{
    if (s_mqttClient.connected()) return true;
    if (WiFi.status() != WL_CONNECTED) return false;

    Serial.print("[MQTT] Connecting...");
    String clientId = "GH-ESP32-" + String(random(0xffff), HEX);

    if (s_mqttClient.connect(clientId.c_str(),
                         CORE_IOT_TOKEN, nullptr))
    {
        Serial.println(" OK");
        s_mqttClient.subscribe(TOPIC_RPC_REQUEST);

        // Gửi attributes ngay khi connect
        StaticJsonDocument<128> attr;
        attr["fw_version"] = "1.0.0";
        attr["device"]     = "Greenhouse-ESP32";
        String attrStr;
        serializeJson(attr, attrStr);
        s_mqttClient.publish(TOPIC_ATTRIBUTES, attrStr.c_str());

        return true;
    }

    Serial.printf(" FAILED (rc=%d)\n", s_mqttClient.state());
    return false;
}

// ============================================================================
// PUBLISH TELEMETRY từ queue
// ============================================================================
static void publishTelemetry(const TelemetryPacket_t &pkt)
{
    // --- Telemetry: sensor data ---
    StaticJsonDocument<384> tele;
    tele["temperature"]  = pkt.temperature;
    tele["humidity"]     = pkt.humidity;
    tele["soilMoisture"] = pkt.soilMoisture;
    tele["lightLux"]     = pkt.lightLux;
    tele["pump"]         = pkt.pumpState;
    tele["fan"]          = pkt.fanState;
    tele["autoMode"]     = pkt.autoMode;

    String teleStr;
    serializeJson(tele, teleStr);
    s_mqttClient.publish(TOPIC_TELEMETRY, teleStr.c_str());

    // --- Attributes: thống kê và cảnh báo (ít thay đổi hơn) ---
    StaticJsonDocument<256> attr;
    attr["pumpCount"]        = pkt.pumpCount;
    attr["totalPumpTimeSec"] = pkt.totalPumpTimeSec;
    attr["alertTemp"]        = pkt.alertTemp;
    attr["alertHumidity"]    = pkt.alertHumidity;
    attr["alertSoil"]        = pkt.alertSoil;
    attr["alertLight"]       = pkt.alertLight;
    attr["dhtError"]         = pkt.dhtError;

    // LED color dạng hex string "#RRGGBB"
    char ledHex[8];
    snprintf(ledHex, sizeof(ledHex), "#%02X%02X%02X",
             pkt.ledR, pkt.ledG, pkt.ledB);
    attr["ledColor"] = ledHex;

    String attrStr;
    serializeJson(attr, attrStr);
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
    // Đợi WiFi kết nối trước khi bắt đầu MQTT (tối đa 60s, nếu không có WiFi thì vẫn chạy nhưng chỉ có AP)
     if (xWiFiEventGroup != nullptr)
        xEventGroupWaitBits(xWiFiEventGroup,
                            WIFI_CONNECTED_BIT,
                            pdFALSE,               // KHÔNG clear bit sau khi wait
                            pdTRUE,
                            pdMS_TO_TICKS(60000));

    Serial.println("[MQTT] Starting CoreIoT task...");

    s_mqttClient.setServer(CORE_IOT_SERVER, atoi(CORE_IOT_PORT));
    s_mqttClient.setCallback(onMqttMessage);
    s_mqttClient.setKeepAlive(30);

    unsigned long lastReconnectAttempt = 0;

    for (;;)
    {
        // WiFi check
        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // MQTT reconnect (non-blocking: chỉ thử mỗi 5s nếu thất bại)
        if (!s_mqttClient.connected())
        {
            unsigned long now = millis();
            if (now - lastReconnectAttempt >= 5000)
            {
                lastReconnectAttempt = now;
                mqttReconnect();
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // MQTT loop: nhận RPC, keepalive
        s_mqttClient.loop();

        // Đọc telemetry từ queue (non-blocking)
        TelemetryPacket_t pkt;
        if (xQueueReceive(xTelemetryQueue, &pkt, 0) == pdTRUE)
        {
            publishTelemetry(pkt);
        }

        // Nhường CPU, 10ms đủ nhanh để nhận RPC kịp thời
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}






// void greenhouse_wifi_task(void *pvParameters)
// {
//     Serial.println("[WiFi] Starting WiFi task...");

//     // LUÔN bật AP trước (dù có hay không có SSID)
//     WiFi.mode(WIFI_AP_STA);
//     WiFi.softAP("Greenhouse-AP", "12345678");
//     Serial.printf("[WiFi] AP started: Greenhouse-AP / IP: %s\n",
//                   WiFi.softAPIP().toString().c_str());

//     // Nếu có config → kết nối thêm WiFi nhà
//     if (!WIFI_SSID.isEmpty())
//     {
//         WiFi.begin(WIFI_SSID.c_str(),
//                    WIFI_PASS.isEmpty() ? nullptr : WIFI_PASS.c_str());

//         Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID.c_str());

//         uint32_t t0 = millis();
//         while (WiFi.status() != WL_CONNECTED)
//         {
//             vTaskDelay(pdMS_TO_TICKS(200));
//             if (millis() - t0 > 20000)
//             {
//                 // Timeout nhưng KHÔNG restart
//                 // Vẫn giữ AP để truy cập web qua 192.168.4.1
//                 Serial.println("[WiFi] STA timeout - AP only mode");
//                 vTaskDelete(nullptr);
//                 return;
//             }
//         }

//         Serial.printf("[WiFi] STA connected! IP: %s\n",
//                       WiFi.localIP().toString().c_str());

//             if (xWiFiEventGroup)
//                 xEventGroupSetBits(xWiFiEventGroup, WIFI_CONNECTED_BIT);
//     }

//     // Giám sát reconnect STA
//     for (;;)
//     {
//         vTaskDelay(pdMS_TO_TICKS(10000));

//         if (!WIFI_SSID.isEmpty() && WiFi.status() != WL_CONNECTED)
//         {
//             Serial.println("[WiFi] STA disconnected! Reconnecting...");
//             WiFi.reconnect();

//             uint32_t t = millis();
//             while (WiFi.status() != WL_CONNECTED && millis() - t < 15000)
//                 vTaskDelay(pdMS_TO_TICKS(500));

//             if (WiFi.status() == WL_CONNECTED)
//             {
//                 Serial.printf("[WiFi] Reconnected! IP: %s\n",
//                               WiFi.localIP().toString().c_str());
//                 if (xSemaphoreWiFiReady)
//                     xSemaphoreGive(xSemaphoreWiFiReady);
//             }
//         }
//     }
// }