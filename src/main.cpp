/*
 * main.cpp - Greenhouse IoT System (+ CoreIoT + WebServer)
 *
 * Dual-core layout:
 *   Core 0: greenhouse_wifi_task (priority 3)
 *           greenhouse_coreiot_task (priority 2)
 *           greenhouse_webserver_task (priority 1)
 *   Core 1: Arduino loop() + Cooperative Scheduler (priority tự nhiên)
 */
// ====================== Build Commands ======================================================
// pio run -t erase #xóa toàn bộ flash (kể cả LittleFS) - dùng khi muốn reset hoàn toàn về trạng thái mới
// pio run -t clean #xóa build cache
// pio run #build firmware
// pio run -t upload #upload firmware lên ESP32
// pio run -t uploadfs #upload LittleFS(web files) - uploadfs cái file trong mục data/ (nếu có) lên ESP32, sẽ mount vào LittleFS để webserver đọc
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "global_vars.h"
#include "scheduler.h"
#include "sensor_handler.h"
#include "actuator_handler.h"
#include "lcd_display.h"
#include "app_tasks.h"
#include "button_handler.h"
#include "iot_bridge.h"
#include "check_info.h"
#include "coreiot_greenhouse.h"
#include "webserver_greenhouse.h"
#include "wifi_greenhouse.h"

// ============================================================================
// SETUP
// ============================================================================
void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println("\n==============================");
    Serial.println("  Greenhouse IoT + CoreIoT ");
    Serial.println("==============================");

    // ===== 1. LittleFS + Config =====
    // check_info_File(false): mount LittleFS + load /info.dat
    // Nếu chưa có config → tự động bật AP để cấu hình
    check_info_File(false);

    // ===== 2. IOT BRIDGE TRƯỚC — tạo mutex trước khi đọc sensor =====
    iotBridge_init();

    // Load lịch tưới từ FS (nếu có) sau khi đã có mutex
    loadSchedulesFromFS();

    // Load ngưỡng điều khiển từ FS (nếu có), dùng DEFAULT_* nếu chưa có file
    loadThresholdFromFS();

    // Đọc sensor lần đầu để có dữ liệu hiển thị ngay khi khởi động
    readDHT();
    readLightSensor();
    readSoilMoisture();

    // ===== 3. HARDWARE =====
    initLCD();
    initRTC();
    initActuators();
    initSensors();
    initButtons();

    // ===== 4. WIFI =====
    initWiFi();

    // ===== 5. FREERTOS TASKS trên Core 0 =====
    xTaskCreatePinnedToCore(
        greenhouse_wifi_task, "wifi",
        4096, nullptr, 3, nullptr, 0);

    xTaskCreatePinnedToCore(
        greenhouse_coreiot_task, "coreiot",
        8192, nullptr, 2, nullptr, 0);

    xTaskCreatePinnedToCore(
        greenhouse_webserver_task, "webserver",
        8192, nullptr, 1, nullptr, 0);

    // ===== 6. COOPERATIVE SCHEDULER (Core 1) =====
    SCH_Init();
    SCH_Init_Timer();

    // Thêm các task vào scheduler với khoảng thời gian định kỳ
    SCH_Add_Task(Task_ReadAirSensors, 0, TASK_SENSOR_READ_TICKS);        // Đọc cảm biến không khí mỗi 2 giây vì thay đổi nhanh hơn
    SCH_Add_Task(Task_ReadSoilSensor, 0, TASK_SOIL_READ_TICKS);          // Đọc đất mỗi 5 giây vì thay đổi chậm hơn
    SCH_Add_Task(Task_CheckAlerts, 0, TASK_ALERT_CHECK_TICKS);           // Kiểm tra cảnh báo mỗi 1 giây để phản hồi nhanh với thay đổi trạng thái
    SCH_Add_Task(Task_AutoControl, 0, TASK_AUTO_CONTROL_TICKS);          // Task điều khiển tự động (bơm + quạt) mỗi 1 giây
    SCH_Add_Task(Task_PumpWatchdog, 0, TASK_PUMP_WATCHDOG_TICKS);        // Giám sát bơm mỗi 1 giây
    SCH_Add_Task(Task_UpdateLEDStatus, 0, TASK_LED_UPDATE_TICKS);        // Cập nhật LED mỗi 500ms để phản hồi nhanh với thay đổi trạng thái
    SCH_Add_Task(Task_UpdateLCD, 0, TASK_LCD_UPDATE_TICKS);              // Task cập nhật LCD mỗi 500ms
    SCH_Add_Task(Task_AutoSwitchLCDPage, 0, TASK_LCD_PAGE_SWITCH_TICKS); // Task tự động chuyển trang LCD mỗi 5s
    SCH_Add_Task(Task_Heartbeat, 0, TASK_HEARTBEAT_TICKS);               // Task nhẹ, in trạng thái hệ thống mỗi 5s
    SCH_Add_Task(Task_ScanButtons, 0, BTN_DEBOUNCE_TICKS);               // Task quét nút bấm với debounce
    SCH_Add_Task(Task_SendTelemetry, 0, TASK_TELEMETRY_TICKS);           // Task: gửi telemetry lên cloud mỗi 5s
    SCH_Add_Task(Task_ProcessRpc, 0, TASK_RPC_POLL_TICKS);               // Task: poll lệnh RPC từ cloud mỗi 200ms
    SCH_Add_Task(Task_CheckSchedule, 0, TASK_SCHEDULE_CHECK_TICKS);      // Task: kiểm tra lịch tưới mỗi 10s
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
        case 'i':
        case 'I':
            Serial.printf("[IOT] WiFi: %s | IP: %s\n",
                          WiFi.status() == WL_CONNECTED ? "OK" : "DISCONNECTED",
                          WiFi.localIP().toString().c_str());
            Serial.printf("[IOT] Server: %s:%s\n",
                          CORE_IOT_SERVER, CORE_IOT_PORT);
            Serial.printf("[IOT] Free heap: %u\n", ESP.getFreeHeap());
            break;
        case '?':
            Serial.println("Commands: a=AutoToggle p=PumpToggle f=FanToggle n=NextPage s=Sensors i=IoTInfo");
            break;
        default:
            break;
        }
    }

    yield();
}