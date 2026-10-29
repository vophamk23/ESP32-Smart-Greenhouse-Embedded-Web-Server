/*
 * coreiot_greenhouse.cpp - CoreIoT Cloud MQTT Platform cho Greenhouse IoT
 * Chạy trên Core 0 (FreeRTOS Task), kết nối trực tiếp app.coreiot.io:1883
 * 
 * Đã lược bỏ hoàn toàn phần Backend cũ (NestJS / emqx) để tối ưu hiệu năng:
 *   - 1 MQTT Client duy nhất kết nối CoreIOT Cloud của Thầy Lê Trọng Nhân.
 *   - Bắn Telemetry đầy đủ thông số cảm biến và kết quả AI TinyML.
 *   - Lắng nghe RPC 2 chiều từ Dashboard điều khiển Bơm/Quạt và phản hồi tức thì.
 */

#include "coreiot_client.h"
#include "iot_bridge.h"
#include "global_vars.h"
#include "actuator_handler.h"
#include "web_server.h"

// ============================================================================
// MQTT CLIENT — CoreIoT (app.coreiot.io:1883)
// ============================================================================
static WiFiClient s_wifiClient;
static PubSubClient s_mqttClient(s_wifiClient);

// Topics ThingsBoard / CoreIoT tiêu chuẩn
static const char *TOPIC_TELEMETRY   = "v1/devices/me/telemetry";
static const char *TOPIC_ATTRIBUTES  = "v1/devices/me/attributes";
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
// HELPER: Gửi RPC response về CoreIoT để cập nhật nút gạt trên Dashboard
// ============================================================================
static void sendRpcResponse(const char *requestId, const char *method,
                            bool success, const char *extraKey = nullptr,
                            const char *extraVal = nullptr)
{
    if (!requestId)
        return;

    char respTopic[64];
    snprintf(respTopic, sizeof(respTopic), "v1/devices/me/rpc/response/%s", requestId);

    StaticJsonDocument<128> resp;
    resp["method"]  = method;
    resp["success"] = success;
    if (extraKey && extraVal)
        resp[extraKey] = extraVal;

    String payload;
    serializeJson(resp, payload);
    s_mqttClient.publish(respTopic, payload.c_str());
    Serial.printf("[CoreIoT] RPC Response sent -> %s: %s\n", respTopic, payload.c_str());
}

// ============================================================================
// MQTT CALLBACK: Nhận và thực thi lệnh RPC từ Cloud CoreIOT Dashboard
// ============================================================================
static void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
    char msg[256];
    length = (length > sizeof(msg) - 1) ? sizeof(msg) - 1 : length;
    memcpy(msg, payload, length);
    msg[length] = '\0';

    Serial.printf("[CoreIoT] Incoming RPC: %s\n", msg);

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, msg))
    {
        Serial.println("[CoreIoT] JSON parse error in RPC");
        return;
    }

    const char *method    = doc["method"];
    const char *requestId = extractRequestId(topic);
    if (!method)
        return;

    // 1. Điều khiển Bơm nước
    if (strcmp(method, "setPump") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = {state ? RPC_PUMP_ON : RPC_PUMP_OFF};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true, "pump", state ? "ON" : "OFF");
    }
    // 2. Điều khiển Quạt làm mát
    else if (strcmp(method, "setFan") == 0)
    {
        bool state = doc["params"].as<bool>();
        RpcPacket_t pkt = {state ? RPC_FAN_ON : RPC_FAN_OFF};
        xQueueSend(xRpcCommandQueue, &pkt, 0);
        sendRpcResponse(requestId, method, true, "fan", state ? "ON" : "OFF");
    }
    // 3. Chuyển đổi chế độ AUTO / MANUAL
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
    // 4. Bật/Tắt chế độ AI Adaptive (setAiMode)
    else if (strcmp(method, "setAiMode") == 0)
    {
        bool enable = doc["params"].as<bool>();
        SENSOR_LOCK();
        g_aiAutoControl = enable;
        SENSOR_UNLOCK();
        sendRpcResponse(requestId, method, true, "ai_mode", enable ? "ENABLED" : "DISABLED");
        Serial.printf("[CoreIoT] RPC: AI Auto Control -> %s\n", enable ? "ENABLED" : "DISABLED");
    }
    // 4. Lấy trạng thái hiện tại (getStatus)
    else if (strcmp(method, "getStatus") == 0)
    {
        StaticJsonDocument<384> resp;
        resp["method"]       = "getStatus";
        resp["pump"]         = (bool)g_pumpState;
        resp["fan"]          = (bool)g_fanState;
        resp["mode"]         = g_autoMode ? "AUTO" : "MANUAL";
        resp["temperature"]  = g_temperature;
        resp["humidity"]     = g_humidity;
        resp["soilMoisture"] = g_soilMoisture;
        resp["ai_state"]     = g_aiStateStr;
        resp["anomaly_pred"] = g_aiIsAnomaly;
        resp["anomaly_type"] = g_aiAnomalyType;
        resp["online_acc"]   = g_aiOnlineAccuracy;

        if (requestId)
        {
            char respTopic[64];
            snprintf(respTopic, sizeof(respTopic), "v1/devices/me/rpc/response/%s", requestId);
            String payload;
            serializeJson(resp, payload);
            s_mqttClient.publish(respTopic, payload.c_str());
        }
    }
}

// ============================================================================
// MQTT RECONNECT — CoreIoT Platform
// ============================================================================
static bool mqttReconnect()
{
    if (s_mqttClient.connected())
        return true;
    if (WiFi.status() != WL_CONNECTED)
        return false;

    Serial.printf("[CoreIoT] Connecting to %s:%s (Token: %s)...\n",
                  CORE_IOT_SERVER, CORE_IOT_PORT, CORE_IOT_TOKEN);
    String clientId = "YoloUNO-GH-" + String(random(0xffff), HEX);

    // Xác thực bằng Device Access Token của ThingsBoard/CoreIOT
    if (s_mqttClient.connect(clientId.c_str(), CORE_IOT_TOKEN, nullptr))
    {
        Serial.println("[CoreIoT] Connected OK!");
        s_mqttClient.subscribe(TOPIC_RPC_REQUEST);

        // Gửi thông tin thiết bị (Device Attributes)
        StaticJsonDocument<192> attr;
        attr["firmware"]   = "YoloUNO-AI-v2.0";
        attr["board"]      = "ESP32-S3 (Yolo:UNO)";
        attr["ip"]         = WiFi.localIP().toString();
        attr["mac"]        = WiFi.macAddress();
        attr["ai_engine"]  = "TinyML-DecisionForest";
        String attrStr;
        serializeJson(attr, attrStr);
        s_mqttClient.publish(TOPIC_ATTRIBUTES, attrStr.c_str());

        return true;
    }

    Serial.printf("[CoreIoT] Connect FAILED (rc=%d)\n", s_mqttClient.state());
    return false;
}

// ============================================================================
// PUBLISH TELEMETRY — Gửi số đo cảm biến & kết quả AI lên CoreIOT
// ============================================================================
static void publishTelemetry(const TelemetryPacket_t &pkt)
{
    if (!s_mqttClient.connected())
        return;

    // Đóng gói JSON Telemetry theo chuẩn CoreIOT
    StaticJsonDocument<512> tele;
    tele["temperature"]   = pkt.temperature;
    tele["humidity"]      = pkt.humidity;
    tele["soilMoisture"]  = pkt.soilMoisture;
    tele["lightLux"]      = pkt.lightLux;
    tele["pump"]          = pkt.pumpState;
    tele["fan"]           = pkt.fanState;
    tele["autoMode"]      = pkt.autoMode;
    tele["dhtError"]      = pkt.dhtError;
    tele["pumpCount"]     = pkt.pumpCount;
    tele["totalPumpSec"]  = pkt.totalPumpTimeSec;

    // Bổ sung các trường AI TinyML & Anomaly Detection
    tele["ai_state"]      = g_aiStateStr;
    tele["ai_confidence"] = pkt.aiConfidence;
    tele["ai_latency_us"] = pkt.aiInferenceTimeUs;
    tele["anomaly_score"] = pkt.aiAnomalyScore;
    tele["anomaly_pred"]  = pkt.aiIsAnomaly;
    tele["anomaly_type"]  = pkt.aiAnomalyType;
    tele["online_acc"]    = pkt.aiOnlineAccuracy;

    String teleStr;
    serializeJson(tele, teleStr);
    s_mqttClient.publish(TOPIC_TELEMETRY, teleStr.c_str());

    Serial.printf("[CoreIoT] Telemetry Sent: T=%.1f H=%.0f Soil=%.0f AI=%s Anom=%s(%.2f) Acc=%.1f%%\n",
                  pkt.temperature, pkt.humidity, pkt.soilMoisture,
                  g_aiStateStr, pkt.aiAnomalyType, pkt.aiAnomalyScore, pkt.aiOnlineAccuracy);
}

// ============================================================================
// COREIOT TASK (Core 0 - FreeRTOS)
// ============================================================================
void greenhouse_coreiot_task(void *pvParameters)
{
    // Đợi WiFi kết nối thành công trước khi bắt đầu
    if (xWiFiEventGroup != nullptr)
    {
        xEventGroupWaitBits(xWiFiEventGroup,
                            WIFI_CONNECTED_BIT,
                            pdFALSE,
                            pdTRUE,
                            pdMS_TO_TICKS(60000));
    }

    Serial.println("[CoreIoT] Task started on Core 0");

    int port = atoi(CORE_IOT_PORT);
    if (port <= 0) port = 1883;

    s_mqttClient.setServer(CORE_IOT_SERVER, port);
    s_mqttClient.setCallback(onMqttMessage);
    s_mqttClient.setKeepAlive(30);
    s_mqttClient.setBufferSize(512);

    unsigned long lastReconnectAttempt = 0;

    for (;;)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            vTaskDelay(pdMS_TO_TICKS(3000));
            continue;
        }

        unsigned long now = millis();

        // Tự động kết nối lại nếu rớt mạng
        if (!s_mqttClient.connected() && (now - lastReconnectAttempt >= 5000))
        {
            lastReconnectAttempt = now;
            mqttReconnect();
        }

        // Duy trì kết nối MQTT và xử lý RPC đến
        if (s_mqttClient.connected())
        {
            s_mqttClient.loop();
        }

        // Lấy gói Telemetry từ Queue (do Core 1 đẩy sang) và phát lên Cloud
        TelemetryPacket_t pkt;
        if (xQueueReceive(xTelemetryQueue, &pkt, 0) == pdTRUE)
        {
            publishTelemetry(pkt);
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
