/*
 * iot_bridge.cpp - Cầu nối Cooperative Scheduler ↔ FreeRTOS IoT Tasks
 */

#include "iot_bridge.h"
#include "global_vars.h"
#include "actuator_handler.h"

// ============================================================================
// KHỞI TẠO QUEUES & SEMAPHORES
// ============================================================================
QueueHandle_t    xTelemetryQueue    = nullptr;
QueueHandle_t    xRpcCommandQueue   = nullptr;  // 
EventGroupHandle_t xWiFiEventGroup  = nullptr;   // Semaphore bảo vệ truy cập sensor data (gọi từ Task_SendTelemetry và các task đọc cảm biến)
SemaphoreHandle_t  g_sensorMutex    = nullptr;  // 

void iotBridge_init()
{
    // Queue telemetry: depth=1 (chỉ cần gói mới nhất)
    // Nếu queue đầy (CoreIoT chưa đọc kịp), Task_SendTelemetry sẽ overwrite
    xTelemetryQueue = xQueueCreate(1, sizeof(TelemetryPacket_t));

    // Queue RPC: depth=8 (buffer vài lệnh liên tiếp)
    xRpcCommandQueue = xQueueCreate(8, sizeof(RpcPacket_t));

    // Semaphore WiFi ready: ban đầu chưa kết nối, Task_IoT sẽ block chờ signal
    xWiFiEventGroup = xEventGroupCreate();

    // Mutex bảo vệ truy cập sensor data (gọi từ Task_SendTelemetry và các task đọc cảm biến)
    g_sensorMutex    = xSemaphoreCreateMutex();

    if (!xTelemetryQueue || !xRpcCommandQueue || !xWiFiEventGroup || !g_sensorMutex)
    {
        Serial.println("[BRIDGE] ERROR: Queue/Semaphore creation failed!");
    }
    else
    {
        Serial.println("[BRIDGE] Queues initialized OK");
    }
}

// ============================================================================
// GỬI TELEMETRY (gọi từ Task_SendTelemetry trong scheduler)
// ============================================================================
void iotBridge_sendTelemetry()
{
    if (!xTelemetryQueue) return;

    TelemetryPacket_t pkt;

    // Snapshot toàn bộ sensor data trong 1 lần lock
    SENSOR_LOCK();
    pkt.temperature      = g_temperature;
    pkt.humidity         = g_humidity;
    pkt.soilMoisture     = g_soilMoisture;
    pkt.lightLux         = g_lightLux;
    pkt.pumpState        = g_pumpState;
    pkt.fanState         = g_fanState;
    pkt.autoMode         = g_autoMode;
    pkt.dhtError         = g_dhtError;
    pkt.alertTemp        = g_alertTemp;
    pkt.alertHumidity    = g_alertHumidity;
    pkt.alertSoil        = g_alertSoil;
    pkt.alertLight       = g_alertLight;
    pkt.pumpCount        = g_pumpCount;
    pkt.totalPumpTimeSec = g_totalPumpTime / 1000UL;
    pkt.ledR             = g_currentLEDColor.r;
    pkt.ledG             = g_currentLEDColor.g;
    pkt.ledB             = g_currentLEDColor.b;
    SENSOR_UNLOCK();

    // Overwrite nếu queue đầy (không block) - giữ dữ liệu mới nhất
    if (xQueueSend(xTelemetryQueue, &pkt, 0) == errQUEUE_FULL)
    {
        TelemetryPacket_t dummy;
        xQueueReceive(xTelemetryQueue, &dummy, 0);
        xQueueSend(xTelemetryQueue, &pkt, 0);
    }
}

// ============================================================================
// XỬ LÝ LỆNH RPC TỪ CLOUD (gọi từ Task_AutoControl trong scheduler)
// Chỉ thực thi khi ở chế độ phù hợp
// ============================================================================
void iotBridge_processRpcCommands()
{
    if (!xRpcCommandQueue) return;

    RpcPacket_t pkt;
    // Đọc hết tất cả lệnh pending (non-blocking)
    while (xQueueReceive(xRpcCommandQueue, &pkt, 0) == pdTRUE)
    {
        switch (pkt.command)
        {
            case RPC_PUMP_ON:
                if (!g_autoMode)
                {
                    g_pumpManual = true;
                    pumpOn();
                    Serial.println("[BRIDGE] RPC: Pump ON");
                }
                break;

            case RPC_PUMP_OFF:
                pumpOff();
                Serial.println("[BRIDGE] RPC: Pump OFF");
                break;

            case RPC_FAN_ON:
                if (!g_autoMode)
                {
                    fanOn();
                    Serial.println("[BRIDGE] RPC: Fan ON");
                }
                break;

            case RPC_FAN_OFF:
                fanOff();
                Serial.println("[BRIDGE] RPC: Fan OFF");
                break;

            case RPC_MODE_AUTO:
                g_autoMode = true;
                Serial.println("[BRIDGE] RPC: Mode -> AUTO");
                break;

            case RPC_MODE_MANUAL:
                g_autoMode = false;
                Serial.println("[BRIDGE] RPC: Mode -> MANUAL");
                break;

            default:
                break;
        }
    }
}