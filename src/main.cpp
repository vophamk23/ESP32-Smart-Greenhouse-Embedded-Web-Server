/*
 * main_updated.cpp - Greenhouse IoT System (+ CoreIoT + WebServer)
 *
 * Dual-core layout:
 *   Core 0: greenhouse_wifi_task (priority 3)
 *           greenhouse_coreiot_task (priority 2)
 *           greenhouse_webserver_task (priority 1)
 *   Core 1: Arduino loop() + Cooperative Scheduler (priority tự nhiên)
 */

#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "global_vars.h"
#include "scheduler.h"
#include "sensor_handler.h"
#include "actuator_handler.h"
#include "lcd_display.h"
#include "app_tasks.h"
#include "button_handler.h"
#include "iot_bridge.h"
#include "task_coreiot_greenhouse.h"
#include "task_webserver_greenhouse.h"
#include "task_wifi_greenhouse.h"

// ============================================================================
// CẤU HÌNH (đọc từ LittleFS)
// ============================================================================
String WIFI_SSID       = "";
String WIFI_PASS       = "";
String CORE_IOT_TOKEN  = "";
String CORE_IOT_SERVER = "";
String CORE_IOT_PORT   = "1883";

static void loadConfig()
{
    File file = LittleFS.open("/info.dat", "r");
    if (!file)
    {
        Serial.println("[CFG] /info.dat not found - using defaults");
        return;
    }

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, file) == DeserializationError::Ok)
    {
        if (doc["WIFI_SSID"].is<const char*>())   WIFI_SSID       = doc["WIFI_SSID"].as<String>();
        if (doc["WIFI_PASS"].is<const char*>())   WIFI_PASS       = doc["WIFI_PASS"].as<String>();
        if (doc["CORE_IOT_TOKEN"].is<const char*>()) CORE_IOT_TOKEN  = doc["CORE_IOT_TOKEN"].as<String>();
        if (doc["CORE_IOT_SERVER"].is<const char*>()) CORE_IOT_SERVER = doc["CORE_IOT_SERVER"].as<String>();
        if (doc["CORE_IOT_PORT"].is<const char*>()) CORE_IOT_PORT   = doc["CORE_IOT_PORT"].as<String>();
        Serial.printf("[CFG] Loaded: SSID=%s, Server=%s:%s\n",
                      WIFI_SSID.c_str(), CORE_IOT_SERVER.c_str(), CORE_IOT_PORT.c_str());
    }
    file.close();
}

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
    if (!LittleFS.begin(true))
        Serial.println("[INIT] LittleFS failed!");
    else
        loadConfig();

    // ===== 2. HARDWARE =====
    initLCD();
    initActuators();
    initSensors();
    initButtons();

    readDHT();
    readLightSensor();
    readSoilMoisture();

    
    // ===== 3. WIFI =====
    initWiFi();

    // ===== 4. IOT BRIDGE (queues + semaphores) =====
    iotBridge_init();

    // ===== 5. FREERTOS TASKS trên Core 0 =====
    // Priority: wifi(3) > coreiot(2) > webserver(1)
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

    // Đăng ký các task vào scheduler (Core 1)
    SCH_Add_Task(Task_ReadAirSensors,    0, TASK_SENSOR_READ_TICKS);
    SCH_Add_Task(Task_ReadSoilSensor,    0, TASK_SOIL_READ_TICKS);
    SCH_Add_Task(Task_CheckAlerts,       0, TASK_ALERT_CHECK_TICKS);
    SCH_Add_Task(Task_AutoControl,       0, TASK_AUTO_CONTROL_TICKS);
    SCH_Add_Task(Task_PumpWatchdog,      0, TASK_PUMP_WATCHDOG_TICKS);
    SCH_Add_Task(Task_UpdateLEDStatus,   0, TASK_LED_UPDATE_TICKS);
    SCH_Add_Task(Task_UpdateLCD,         0, TASK_LCD_UPDATE_TICKS);
    SCH_Add_Task(Task_AutoSwitchLCDPage, 0, TASK_LCD_PAGE_SWITCH_TICKS);
    SCH_Add_Task(Task_Heartbeat,         0, TASK_HEARTBEAT_TICKS);
    SCH_Add_Task(Task_ScanButtons,       0, BTN_DEBOUNCE_TICKS);

    // 
    SCH_Add_Task(Task_SendTelemetry,     0, TASK_TELEMETRY_TICKS);   // 5s
    SCH_Add_Task(Task_ProcessRpc,        0, TASK_RPC_POLL_TICKS);    // 200ms
}

// ============================================================================
// MAIN LOOP (Core 1)
// ============================================================================
void loop()
{
    SCH_Dispatch_Tasks();

    // Serial debug commands (giữ nguyên từ phiên bản cũ)
    if (Serial.available())
    {
        char cmd = Serial.read();
        switch (cmd)
        {
            case 'a': case 'A':
                g_autoMode = !g_autoMode;
                if (g_autoMode)
                    g_pumpManual = false;
                Serial.printf("[CMD] Mode -> %s\n", g_autoMode ? "AUTO" : "MANUAL");
                setLCDPage(2);
                break;
            case 'p': case 'P':
                if (!g_autoMode) { g_pumpManual = true; g_pumpState ? pumpOff() : pumpOn(); }
                else Serial.println("[CMD] Switch to MANUAL first");
                break;
            case 'f': case 'F':
                if (!g_autoMode) { g_fanState ? fanOff() : fanOn(); }
                else Serial.println("[CMD] Switch to MANUAL first");
                break;
            case 'n': case 'N':
                nextLCDPage();
                Serial.printf("[CMD] LCD Page -> %d\n", g_lcdPage);
                break;
            case 's': case 'S':
                printSensorData();
                break;
            case 'i': case 'I':
                // In thông tin IoT
                Serial.printf("[IOT] WiFi: %s | IP: %s\n",
                    WiFi.status() == WL_CONNECTED ? "OK" : "DISCONNECTED",
                    WiFi.localIP().toString().c_str());
                Serial.printf("[IOT] Server: %s:%s\n",
                    CORE_IOT_SERVER.c_str(), CORE_IOT_PORT.c_str());
                Serial.printf("[IOT] Free heap: %u\n", ESP.getFreeHeap());
                break;
            case '?':
                Serial.println("Commands: a=AutoToggle p=PumpToggle f=FanToggle n=NextPage s=Sensors i=IoTInfo");
                break;
            default: break;
        }
    }

    yield();
}