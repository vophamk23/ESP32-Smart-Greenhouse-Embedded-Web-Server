/*
 * app_tasks.cpp - Triển khai các task cho Cooperative Scheduler
 *
 * IMPROVEMENTS:
 *   1. Task_PumpWatchdog xử lý cooldown timeout
 *   2. Task_Heartbeat ngắn gọn hơn, đủ thông tin
 *   3. Guard rõ ràng trong mỗi task
 */

#include "app_tasks.h"
#include "sensor_handler.h"
#include "actuator_handler.h"
#include "lcd_display.h"
#include "global_vars.h"
#include "config.h"
#include "iot_bridge.h"

// ============================================================================
// TASK 1: ĐỌC CẢM BIẾN KHÔNG KHÍ (3 giây)
// ============================================================================
void Task_ReadAirSensors(void)
{
    readDHT();
    readLightSensor();
}

// ============================================================================
// TASK 2: ĐỌC ĐỘ ẨM ĐẤT (5 giây)
// ============================================================================
void Task_ReadSoilSensor(void)
{
    readSoilMoisture();
}


// ============================================================================
// TASK 3: CẬP NHẬT LCD (500ms)
// ============================================================================
void Task_UpdateLCD(void)
{
    updateLCDPage();
}

// ============================================================================
// TASK 4: TỰ ĐỘNG CHUYỂN TRANG LCD (5 giây)
// ============================================================================
void Task_AutoSwitchLCDPage(void)
{
    nextLCDPage();
}

// ============================================================================
// TASK 5: LOGIC TỰ ĐỘNG (1 giây)
// ============================================================================
void Task_AutoControl(void)
{
    autoControlPump();
    autoControlFan();
}

// ============================================================================
// TASK 6: CẬP NHẬT MÀU LED (500ms)
// ============================================================================
void Task_UpdateLEDStatus(void)
{
    autoControlLED();
}

// ============================================================================
// TASK 7: KIỂM TRA CẢNH BÁO (1 giây)
// ============================================================================
void Task_CheckAlerts(void)
{
    updateAlerts();
}

// ============================================================================
// TASK 8: HEARTBEAT (5 giây) - in tóm tắt trạng thái hệ thống
// ============================================================================
void Task_Heartbeat(void)
{
    Serial.println("========== GREENHOUSE STATUS ==========");
    Serial.printf("  Temp:%.1fC%s  Humi:%.0f%%\n",
                  g_temperature, g_dhtError ? "[ERR]" : "",
                  g_humidity);
    Serial.printf("  Soil:%.0f%%  Light:%.0flux(%s)\n",
                  g_soilMoisture, g_lightLux,
                  g_lightLux > 0 ? "BRIGHT" : "DARK");
    Serial.printf("  PUMP:%-3s  FAN:%-3s  LED_GROW:%-3s\n",
                  g_pumpState    ? "ON"  : "OFF",
                  g_fanState     ? "ON"  : "OFF",
                  g_ledGrowState ? "ON"  : "OFF");
    Serial.printf("  Mode:%s  PumpManual:%s  Cooldown:%s\n",
                  g_autoMode   ? "AUTO" : "MAN",
                  g_pumpManual ? "Y"    : "N",
                  g_pumpCooldown ? "Y"  : "N");
    Serial.printf("  Alert[T:%s H:%s S:%s L:%s]\n",
                  g_alertTemp     ? "!" : "-",
                  g_alertHumidity ? "!" : "-",
                  g_alertSoil     ? "!" : "-",
                  g_alertLight    ? "!" : "-");
    Serial.printf("  PumpCount:%lu  TotalTime:%lus\n",
                  (unsigned long)g_pumpCount,
                  g_totalPumpTime / 1000);
    Serial.println("=======================================");
}

// ============================================================================
// TASK 9: PUMP WATCHDOG (1 giây)
// Bảo vệ bơm: tắt cưỡng bức nếu chạy quá PUMP_MAX_ON_TIME_MS
// Reset cooldown sau PUMP_COOLDOWN_MS
// ============================================================================
void Task_PumpWatchdog(void)
{
    // Bơm chạy quá lâu -> force OFF
    if (g_pumpState)
    {
        unsigned long elapsed = millis() - g_pumpStartTime;
        if (elapsed >= PUMP_MAX_ON_TIME_MS)
        {
            Serial.printf("[WATCHDOG] Pump ran %lus >= max %lus -> force OFF\n",
                          elapsed / 1000,
                          (unsigned long)(PUMP_MAX_ON_TIME_MS / 1000));
            pumpOff();
        }
    }

    // Cooldown timeout -> clear flag (cũng được xử lý trong pumpOn() nhưng double-check)
    if (g_pumpCooldown)
    {
        if (millis() - g_pumpLastOffTime >= PUMP_COOLDOWN_MS)
        {
            g_pumpCooldown = false;
            Serial.println("[WATCHDOG] Pump cooldown ended - ready to pump again");
        }
    }
}

// ============================================================================
// TASK MỚI 1: GỬI TELEMETRY LÊN CLOUD (mỗi 5 giây)
// Đóng gói snapshot dữ liệu hiện tại vào queue cho CoreIoT task đọc
// ============================================================================
void Task_SendTelemetry(void)
{
    iotBridge_sendTelemetry();
    // Log nhẹ để debug không spam
    Serial.printf("[TELE] Queued: T=%.1f H=%.0f Soil=%.0f\n",
                  g_temperature, g_humidity, g_soilMoisture);
}

// ============================================================================
// TASK MỚI 2: XỬ LÝ LỆNH RPC TỪ CLOUD (mỗi 200ms)
// Đọc các lệnh pending trong queue và thực thi
// Tách ra khỏi Task_AutoControl để không bị chặn bởi chế độ AUTO
// ============================================================================
void Task_ProcessRpc(void)
{
    iotBridge_processRpcCommands();
}