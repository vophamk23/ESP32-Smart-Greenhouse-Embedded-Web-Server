/*
 * iot_bridge.h - Cầu nối giữa Cooperative Scheduler và FreeRTOS IoT Tasks
 */

#ifndef IOT_BRIDGE_H
#define IOT_BRIDGE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ============================================================================
// CẤU TRÚC DỮ LIỆU TELEMETRY (gửi từ scheduler → IoT tasks)
// ============================================================================
struct TelemetryPacket_t
{
    // Sensor
    float temperature;
    float humidity;
    float soilMoisture;
    float lightLux;

    // Device state
    bool  pumpState;
    bool  fanState;
    bool  autoMode;
    bool  dhtError;

    // Alerts
    bool  alertTemp;
    bool  alertHumidity;
    bool  alertSoil;
    bool  alertLight;

    // Stats
    uint32_t      pumpCount;
    unsigned long totalPumpTimeSec;

    // LED color
    uint8_t ledR;
    uint8_t ledG;
    uint8_t ledB;

    // AI Insights (TinyML & Anomaly Detection)
    uint8_t       aiState;
    float         aiConfidence;
    uint32_t      aiInferenceTimeUs;
    float         aiAnomalyScore;
    bool          aiIsAnomaly;
    float         aiOnlineAccuracy;
    char          aiAnomalyType[32];
};

// ============================================================================
// CẤU TRÚC LỆNH RPC (gửi từ IoT task → scheduler thực thi)
// ============================================================================
enum RpcCommand_t : uint8_t
{
    RPC_NONE       = 0,
    RPC_PUMP_ON    = 1,
    RPC_PUMP_OFF   = 2,
    RPC_FAN_ON     = 3,
    RPC_FAN_OFF    = 4,
    RPC_MODE_AUTO  = 5,
    RPC_MODE_MANUAL= 6,
};

struct RpcPacket_t
{
    RpcCommand_t command;
};

// ============================================================================
// QUEUES & SEMAPHORES
// ============================================================================
extern QueueHandle_t xTelemetryQueue;
extern QueueHandle_t xRpcCommandQueue;
extern EventGroupHandle_t xWiFiEventGroup;
extern SemaphoreHandle_t g_sensorMutex;

#define WIFI_CONNECTED_BIT  BIT0

#define SENSOR_LOCK()    xSemaphoreTake(g_sensorMutex, portMAX_DELAY)
#define SENSOR_UNLOCK()  xSemaphoreGive(g_sensorMutex)

// ============================================================================
// HÀM KHỞI TẠO & TIỆN ÍCH
// ============================================================================
void iotBridge_init();
void iotBridge_sendTelemetry();
void iotBridge_processRpcCommands();

#endif // IOT_BRIDGE_H
