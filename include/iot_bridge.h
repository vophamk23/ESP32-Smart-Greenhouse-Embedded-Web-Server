/*
 * iot_bridge.h - Cầu nối giữa Cooperative Scheduler và FreeRTOS IoT Tasks
 *
 * Vai trò:
 *   - Định nghĩa shared data queue và semaphore giữa hai core
 *   - Greenhouse scheduler (Core 1) ghi data vào queue
 *   - CoreIoT task / WebSocket task (Core 0) đọc từ queue để publish
 *   - RPC từ cloud → callback → cờ lệnh → scheduler đọc và thực thi
 *
 * Luồng dữ liệu:
 *   Sensor → g_xxx (global_vars) → Task_SendTelemetry → xTelemetryQueue
 *                                                      → coreiot_task → MQTT
 *                                                      → webserver_task → WS
 *   Cloud RPC → callback → g_rpc_xxx flags → Task_AutoControl đọc & thực thi
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
// QUEUES & SEMAPHORES (extern - khởi tạo trong iot_bridge.cpp)
// ============================================================================

// Queue chứa telemetry packet mới nhất (depth=1, overwrite nếu chưa đọc kịp)
extern QueueHandle_t xTelemetryQueue;

// Queue nhận lệnh RPC từ cloud (depth=8, scheduler poll định kỳ)
extern QueueHandle_t xRpcCommandQueue;

// Binary semaphore: WiFi connected → release để IoT task bắt đầu kết nối MQTT
extern EventGroupHandle_t xWiFiEventGroup;

// Mutex bảo vệ truy cập sensor data (gọi từ Task_SendTelemetry và các task đọc cảm biến)
extern SemaphoreHandle_t g_sensorMutex;

#define WIFI_CONNECTED_BIT  BIT0


// Thêm vào sau khai báo xWiFiEventGroup:


// 2 macro tiện dụng để dùng ở mọi nơi:
#define SENSOR_LOCK()    xSemaphoreTake(g_sensorMutex, portMAX_DELAY)
#define SENSOR_UNLOCK()  xSemaphoreGive(g_sensorMutex)

// ============================================================================
// HÀM KHỞI TẠO
// ============================================================================
void iotBridge_init();

// ============================================================================
// HÀM TIỆN ÍCH (gọi từ scheduler task)
// ============================================================================

// Gửi snapshot dữ liệu hiện tại vào queue (gọi từ Task_SendTelemetry)
void iotBridge_sendTelemetry();

// Đọc lệnh RPC pending và thực thi (gọi từ Task_AutoControl hoặc task riêng)
void iotBridge_processRpcCommands();

#endif // IOT_BRIDGE_H