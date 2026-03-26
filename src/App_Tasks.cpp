/*
 * app_tasks.cpp - Triển khai các task cho Cooperative Scheduler
 */

#include "app_tasks.h"
#include "sensor_handler.h"
#include "actuator_handler.h"
#include "lcd_display.h"
#include "global_vars.h"
#include "config.h"
#include "iot_bridge.h"

// Task 1: ĐỌC CẢM BIẾN KHÔNG KHÍ (2 giây)
void Task_ReadAirSensors(void)
{
    readDHT();
    readLightSensor();
}

// Task 2: ĐỌC CẢM BIẾN ĐẤT (5 giây)
void Task_ReadSoilSensor(void)
{
    readSoilMoisture();
}


// Task 3: CẬP NHẬT LCD (500ms)
void Task_UpdateLCD(void)
{
    updateLCDPage();
}

// Task 4: TỰ ĐỘNG CHUYỂN TRANG LCD (5 giây)
void Task_AutoSwitchLCDPage(void)
{
    nextLCDPage();
}

// Task 5: LOGIC ĐIỀU KHIỂN TỰ ĐỘNG (1 giây)
void Task_AutoControl(void)
{
    autoControlPump();
    autoControlFan();
}

// Task 6: CẬP NHẬT TRẠNG THÁI LED (500ms)
void Task_UpdateLEDStatus(void)
{
    autoControlLED();
}

// Task 7: KIỂM TRA CẢNH BÁO (1 giây)
void Task_CheckAlerts(void)
{
    updateAlerts();
}

// Task 8: HEARTBEAT (5 giây) - in tóm tắt trạng thái hệ thống
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
    if (g_pumpState)
    {
        unsigned long elapsed = millis() - g_pumpStartTime;

        // Ưu tiên 1: tắt theo lịch (duration)
        if (g_scheduleOffTime > 0 && millis() >= g_scheduleOffTime)
        {
            Serial.println("[WATCHDOG] Schedule duration ended -> pump OFF");
            g_scheduleOffTime = 0;
            pumpOff();
            return;
        }

        // Ưu tiên 2: tắt theo PUMP_MAX_ON_TIME_MS (bảo vệ phần cứng)
        if (elapsed >= PUMP_MAX_ON_TIME_MS)
        {
            Serial.printf("[WATCHDOG] Pump ran %lus >= max %lus -> force OFF\n",
                          elapsed / 1000,
                          (unsigned long)(PUMP_MAX_ON_TIME_MS / 1000));
            g_scheduleOffTime = 0;
            pumpOff();
        }
    }
    else
    {
        // Bơm đã tắt → reset scheduleOffTime nếu còn thừa
        if (g_scheduleOffTime > 0) g_scheduleOffTime = 0;
    }

    // Cooldown timeout
    if (g_pumpCooldown)
    {
        if (millis() - g_pumpLastOffTime >= PUMP_COOLDOWN_MS)
        {
            g_pumpCooldown = false;
            Serial.println("[WATCHDOG] Pump cooldown ended");
        }
    }
}
// ============================================================================
// TASK 10 : GỬI TELEMETRY LÊN CLOUD (mỗi 5 giây)
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
// TASK 11 : XỬ LÝ LỆNH RPC TỪ CLOUD (mỗi 200ms)
// Đọc các lệnh pending trong queue và thực thi
// Tách ra khỏi Task_AutoControl để không bị chặn bởi chế độ AUTO
// ============================================================================
void Task_ProcessRpc(void)
{
    iotBridge_processRpcCommands();
}


// ============================================================
// TASK 12: KIỂM TRA LỊCH TƯỚI (10 giây)
// So sánh giờ RTC với lịch đã lưu, bật bơm nếu khớp
// ============================================================
// void Task_CheckSchedule(void)
// {
//     if (!g_scheduleEnabled)  return;
//     if (g_scheduleCount == 0) return;
//     if (g_pumpState)          return;  // Bơm đang chạy, bỏ qua
//     if (g_rtcError)           return;  // RTC lỗi, không bật bơm

//     DateTime now       = getRTCTime();
//     uint8_t  todayBit  = (1 << now.dayOfTheWeek());
//     // RTClib: dayOfTheWeek() trả 0=CN, 1=T2 ... 6=T7

//     for (uint8_t i = 0; i < g_scheduleCount; i++)
//     {
//         ScheduleEntry_t &s = g_schedules[i];
//         if (!s.enabled)             continue;
//         if (!(s.days & todayBit))   continue;
//         if (s.hour   != now.hour()) continue;
//         if (s.minute != now.minute()) continue;

//         // Khớp lịch → bật bơm + set thời điểm tắt
//         Serial.printf("[SCHED] Match entry %d: %02d:%02d, %d min\n",
//                       i, s.hour, s.minute, s.duration);

//         pumpOn();
//         // Tắt sau đúng duration (phút) — watchdog sẽ check
//         g_scheduleOffTime = millis() + ((unsigned long)s.duration * 60000UL);
//         break;  // Chỉ kích hoạt 1 lịch mỗi lần check
//     }
// }



void Task_CheckSchedule(void)
{
    if (!g_scheduleEnabled)  { Serial.println("[SCHED] disabled");    return; }
    if (g_scheduleCount == 0){ Serial.println("[SCHED] no schedules"); return; }
    if (g_pumpState)         { Serial.println("[SCHED] pump on");      return; }
    if (g_rtcError)          { Serial.println("[SCHED] rtc error");    return; }

    DateTime now = getRTCTime();
    Serial.printf("[SCHED] now=%02d:%02d dow=%d count=%d\n",
                  now.hour(), now.minute(), now.dayOfTheWeek(), g_scheduleCount);

    for (uint8_t i = 0; i < g_scheduleCount; i++)
    {
        ScheduleEntry_t &s = g_schedules[i];
        Serial.printf("[SCHED] entry%d en=%d days=0x%02X h=%d m=%d\n",
                      i, s.enabled, s.days, s.hour, s.minute);

        if (!s.enabled)               continue;
        if (!(s.days & (1 << now.dayOfTheWeek()))) continue;
        if (s.hour   != now.hour())   continue;
        if (s.minute != now.minute()) continue;

        Serial.printf("[SCHED] Match entry %d: %02d:%02d, %d min\n",
                      i, s.hour, s.minute, s.duration);

        // Khớp lịch → bật bơm
        g_scheduleTriggered = true;  // Báo cho pumpOn() bỏ qua cooldown
        pumpOn();
        
        g_scheduleOffTime = millis() + ((unsigned long)s.duration * 60000UL);
        break;
    }
}