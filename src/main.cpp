// clang-format off
/*
 * ============================================================================
 * CLEANROOM BIO-CHAMBER EDGE-AI TELEMETRY & CONTROL SYSTEM
 * Kiến trúc: ESP32-S3 (Yolo:UNO) Dual-Core FreeRTOS + TinyML + Fog Gateway
 *
 * PHÂN HỆ FIRMWARE & TRIỂN KHAI:
 *   - Modbus RS485 / I2C Fail-safe Acquisition -> sensor_handler.cpp
 *   - On-Chip TinyML Inference (MLP + Forest)   -> tinyml_engine.cpp
 *   - Model Training & C Header Export         -> tools/train_tinyml/
 *   - Embedded Async Web Server (LittleFS SPA) -> web_server.cpp
 *   - Python Multi-Threaded Fog Gateway        -> gateway/gateway_core.py
 *   - Fog Predictive Analytics (Z-Score/OLS)   -> gateway/gateway_ai.py
 *   - BIO-SCADA Desktop Control GUI (Tkinter)  -> gateway/gateway_gui.py
 *   - Cloud IoT MQTT & Bidirectional RPC       -> coreiot_client.cpp
 *   - Event Anomaly Logging (CSV) & OTA Update -> web_server.cpp & data/js/
 * ============================================================================
 */
// clang-format on

#include <Arduino.h>
#include "config.h"
#include "global_vars.h"
#include "scheduler.h"
#include "sensor_handler.h"
#include "actuator_handler.h"
#include "lcd_display.h"
#include "scheduler_tasks.h"
#include "button_handler.h"
#include "iot_bridge.h"
#include "config_manager.h"
#include "coreiot_client.h"
#include "web_server.h"
#include "wifi_manager.h"

// ============================================================================
// SETUP
// ============================================================================
void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==============================================");
    Serial.println(" Cleanroom Bio-Chamber Edge AI — Yolo:UNO S3  ");
    Serial.println("==============================================");

    // ===== 1. LittleFS + Config =====
    check_info_File(false);

    // ===== 2. IOT BRIDGE TRƯỚC — tạo mutex trước khi đọc sensor =====
    iotBridge_init();

    // Load lịch phun sương và ngưỡng điều khiển từ LittleFS
    loadSchedulesFromFS();
    loadThresholdFromFS();

    // ===== 3. NGOẠI VI & PHẦN CỨNG =====
    initSensors();
    initActuators();
    initLCD();
    initButtons();
    initRTC();

    // Đọc sensor lần đầu để có dữ liệu ngay khi khởi động
    readDHT();
    readLightSensor();
    readSoilMoisture();

    // ===== 4. WIFI & NETWORK (Core 0) =====
    initWiFi();

    // Tạo task WiFi monitor trên Core 0
    xTaskCreatePinnedToCore(wifi_task, "wifi", 4096, nullptr, 3, nullptr, 0);

    // ===== 5. FREERTOS TASKS (Core 0) =====
    xTaskCreatePinnedToCore(
        // FreeRTOS CoreIOT Task (MQTT Telemetry, RuleChain, RPC)
        coreiot_task, "coreiot", 8192, nullptr, 2, nullptr, 0);

    xTaskCreatePinnedToCore(
        // FreeRTOS Web Server Task (LittleFS SPA, ElegantOTA, Event Log CSV)
        webserver_task, "webserver", 8192, nullptr, 1, nullptr, 0);

    // ===== 6. COOPERATIVE SCHEDULER (Core 1) =====
    SCH_Init();
    SCH_Init_Timer();

    // Đăng ký các tác vụ định kỳ vào Cooperative Scheduler
    SCH_Add_Task(Task_ReadAirSensors, 0, TASK_SENSOR_READ_TICKS);        // Đọc Modbus RS485 / DHT20 & LDR mỗi 0.5s
    SCH_Add_Task(Task_ReadSoilSensor, 0, TASK_SOIL_READ_TICKS);          // Đọc độ ẩm giá thể mỗi 1s
    SCH_Add_Task(Task_TinyML_Inference, 0, TASK_AI_INFERENCE_TICKS);     // Suy luận AI TinyML on-chip mỗi 3s
    SCH_Add_Task(Task_CheckAlerts, 0, TASK_ALERT_CHECK_TICKS);           // Kiểm tra cảnh báo mỗi 1s
    SCH_Add_Task(Task_AutoControl, 0, TASK_AUTO_CONTROL_TICKS);          // Logic tự động (phun sương + quạt) mỗi 1s
    SCH_Add_Task(Task_PumpWatchdog, 0, TASK_PUMP_WATCHDOG_TICKS);        // Giám sát bảo vệ bơm phun sương mỗi 1s
    SCH_Add_Task(Task_UpdateLEDStatus, 0, TASK_LED_UPDATE_TICKS);        // Cập nhật LED NeoPixel mỗi 500ms
    SCH_Add_Task(Task_UpdateLCD, 0, TASK_LCD_UPDATE_TICKS);              // Cập nhật LCD mỗi 500ms
    SCH_Add_Task(Task_AutoSwitchLCDPage, 0, TASK_LCD_PAGE_SWITCH_TICKS); // Tự động lật trang LCD mỗi 5s
    SCH_Add_Task(Task_Heartbeat, 0, TASK_HEARTBEAT_TICKS);               // In log trạng thái mỗi 15s
    SCH_Add_Task(Task_ScanButtons, 0, BTN_DEBOUNCE_TICKS);               // Quét nút bấm mỗi 50ms
    SCH_Add_Task(Task_SendTelemetry, 0, TASK_TELEMETRY_TICKS);           // Gửi telemetry CoreIOT mỗi 3s
    SCH_Add_Task(Task_ProcessRpc, 0, TASK_RPC_POLL_TICKS);               // Quét lệnh RPC mỗi 200ms
    SCH_Add_Task(Task_CheckSchedule, 0, TASK_SCHEDULE_CHECK_TICKS);      // Kiểm tra lịch hẹn chu kỳ mỗi 5s
}

// ============================================================================
// MAIN LOOP (Core 1)
// ============================================================================
void loop()
{
    SCH_Dispatch_Tasks();

    if (Serial.available())
    {
        char cmd = Serial.read();
        switch (cmd)
        {
        case 'a':
        case 'A':
            g_autoMode = !g_autoMode;
            if (g_autoMode)
                g_pumpManual = false;
            Serial.printf("[CMD] Mode -> %s\n", g_autoMode ? "AUTO" : "MANUAL");
            setLCDPage(2);
            break;
        case 'p':
        case 'P':
            if (!g_autoMode)
            {
                g_pumpManual = true;
                g_pumpState ? pumpOff() : pumpOn();
            }
            else
                Serial.println("[CMD] Switch to MANUAL first");
            break;
        case 'f':
        case 'F':
            if (!g_autoMode)
            {
                g_fanState ? fanOff() : fanOn();
            }
            else
                Serial.println("[CMD] Switch to MANUAL first");
            break;
        case 'n':
        case 'N':
            nextLCDPage();
            Serial.printf("[CMD] LCD Page -> %d\n", g_lcdPage);
            break;
        case 's':
        case 'S':
            printSensorData();
            break;
        case 'j': // Trả về gói JSON Telemetry cho Fog IoT Gateway qua Serial
        case 'J':
        {
            // Trả về gói JSON Telemetry cho IoT Gateway trên PC qua cổng Serial
            SENSOR_LOCK();
            Serial.printf("{\"temperature\":%.1f,\"humidity\":%.1f,\"soilMoisture\":%.1f,\"lightLux\":%.0f,"
                          "\"pumpState\":%s,\"fanState\":%s,\"autoMode\":%s,\"aiStateStr\":\"%s\",\"anomalyScore\":%."
                          "2f,\"isAnomaly\":%s,\"anomalyType\":\"%s\"}\n",
                          g_temperature, g_humidity, g_soilMoisture, g_lightLux, g_pumpState ? "true" : "false",
                          g_fanState ? "true" : "false", g_autoMode ? "true" : "false", g_aiStateStr, g_aiAnomalyScore,
                          g_aiIsAnomaly ? "true" : "false", g_aiAnomalyType);
            SENSOR_UNLOCK();
            break;
        }
        case 'i':
        case 'I':
            Serial.printf("[IOT] WiFi: %s | IP: %s\n", WiFi.status() == WL_CONNECTED ? "OK" : "DISCONNECTED",
                          WiFi.localIP().toString().c_str());
            Serial.printf("[IOT] CoreIOT Server: %s:%s | Token: %s\n", CORE_IOT_SERVER, CORE_IOT_PORT, CORE_IOT_TOKEN);
            Serial.printf("[IOT] AI State: %s (%.1f%%, %luus)\n", g_aiStateStr, g_aiConfidence,
                          (unsigned long)g_aiInferenceTimeUs);
            Serial.printf("[IOT] Free heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
            break;
        case '?':
            Serial.println("Commands: a=AutoToggle p=PumpToggle f=FanToggle n=NextPage s=Sensors j=JSON i=IoTInfo");
            break;
        default:
            break;
        }
    }

    yield();
}
