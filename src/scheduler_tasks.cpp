/*
 * app_tasks.cpp - Triển khai các task cho Cooperative Scheduler & Edge AI
 */

#include "scheduler_tasks.h"
#include "sensor_handler.h"
#include "actuator_handler.h"
#include "lcd_display.h"
#include "global_vars.h"
#include "config.h"
#include "iot_bridge.h"
#include "ntp_sync.h"
#include "tinyml_engine.h"
#include <WiFi.h>

// Task 1: ĐỌC CẢM BIẾN KHÔNG KHÍ (DHT20 I2C + LDR)
void Task_ReadAirSensors(void)
{
    readDHT();
    readLightSensor();
}

// Task 2: ĐỌC CẢM BIẾN ĐẤT
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

// Task 6: CẬP NHẬT TRẠNG THÁI LED NEOPIXEL (500ms)
void Task_UpdateLEDStatus(void)
{
    autoControlLED();
}

// Task 7: KIỂM TRA CẢNH BÁO (1 giây)
void Task_CheckAlerts(void)
{
    updateAlerts();
}

// ============================================================================
// TASK: SUY LUẬN AI TINYML ON-DEVICE & ĐÁNH GIÁ VI KHÍ HẬU (Mỗi 3 giây)
// ============================================================================
void Task_TinyML_Inference(void)
{
    static float s_lastTemp = -999.0f;
    static float s_lastHum = -999.0f;
    static unsigned long s_lastTime = 0;

    float curTemp, curHum, curLight;
    bool curDhtError;
    SENSOR_LOCK();
    curTemp = g_temperature;
    curHum = g_humidity;
    curLight = g_lightLux;
    curDhtError = g_dhtError;
    SENSOR_UNLOCK();

    unsigned long now = millis();
    float deltaTempRate = 0.0f;
    float deltaHumRate = 0.0f;
    if (s_lastTemp > -900.0f && (now > s_lastTime))
    {
        float dtSec = (float)(now - s_lastTime) / 1000.0f;
        if (dtSec > 0.5f)
        {
            deltaTempRate = (curTemp - s_lastTemp) / dtSec; // °C/giây
            deltaHumRate  = (curHum - s_lastHum) / dtSec;   // %/giây
        }
    }
    s_lastTemp = curTemp;
    s_lastHum  = curHum;
    s_lastTime = now;

    // Chạy bộ suy luận TinyML on-device (Bao gồm Anomaly Detection)
    TinyML_Result_t res = TinyML_Predict(curTemp, curHum, curLight, deltaTempRate, deltaHumRate, curDhtError);

    // Cập nhật kết quả vào biến toàn cục được bảo vệ bằng Semaphore Mutex
    SENSOR_LOCK();
    g_aiState = (uint8_t)res.state;
    g_aiConfidence = res.confidence;
    g_aiInferenceTimeUs = res.inferenceTimeUs;
    snprintf(g_aiStateStr, sizeof(g_aiStateStr), "%s", res.stateStr);
    snprintf(g_aiForecastStr, sizeof(g_aiForecastStr), "%s", res.forecastStr);
    g_aiAnomalyScore = res.anomalyScore;
    g_aiIsAnomaly = res.isAnomaly;
    g_aiOnlineAccuracy = res.onlineAccuracy;
    snprintf(g_aiAnomalyType, sizeof(g_aiAnomalyType), "%s", res.anomalyType);

    // Cảnh báo nếu phát hiện Dị thường / Hơ lửa phá hoại (FLAME_TAMPER)
    if (res.isAnomaly)
    {
        if (strcmp(res.anomalyType, "FLAME_TAMPER") == 0)
        {
            setAllLEDs(255, 0, 0); // Nháy đỏ khẩn cấp
            Serial.println("[ANOMALY-ALERT] !!! FLAME OR THERMAL TAMPER DETECTED !!!");
        }
        else
        {
            setAllLEDs(255, 60, 0); // Cam đậm
            Serial.printf("[ANOMALY-ALERT] Sensor Anomaly: %s (Score: %.2f)\n", res.anomalyType, res.anomalyScore);
        }
    }

    // Nếu ở chế độ AUTO & cho phép AI tự động can thiệp
    if (g_autoMode && g_aiAutoControl)
    {
        if (res.shouldTriggerFan && !g_fanState)
        {
            fanOn();
            Serial.println("[AI-ACTION] AI detected Heat Warning / Anomaly -> Fan turned ON automatically");
        }
        else if (!res.shouldTriggerFan && g_fanState && curTemp < g_tempLowThreshold && !res.isAnomaly)
        {
            fanOff();
            Serial.println("[AI-ACTION] Environment normalized -> Fan turned OFF");
        }
    }
    SENSOR_UNLOCK();

    Serial.printf("[TINYML-AI] State: %s (%.1f%%) | Anomaly: %s (score=%.2f, acc=%.1f%%) | Latency: %luus\n",
                  res.stateStr, res.confidence, res.anomalyType, res.anomalyScore,
                  res.onlineAccuracy, (unsigned long)res.inferenceTimeUs);
}

// ============================================================================
// TASK: PUMP WATCHDOG (1 giây)
// Bảo vệ bơm: tắt cưỡng bức nếu chạy quá PUMP_MAX_ON_TIME_MS
// ============================================================================
void Task_PumpWatchdog(void)
{
    if (g_pumpState)
    {
        unsigned long elapsed = millis() - g_pumpStartTime;

        // Ưu tiên 1: tắt theo lịch
        if (g_scheduleOffTime > 0 && millis() >= g_scheduleOffTime)
        {
            Serial.println("[WATCHDOG] Schedule duration ended -> pump OFF");
            g_scheduleOffTime = 0;
            g_scheduleTriggered = false;
            pumpOff();
            return;
        }

        // Ưu tiên 2: force OFF theo PUMP_MAX_ON_TIME_MS
        if (g_scheduleOffTime == 0 && elapsed >= g_pumpMaxOnMs)
        {
            Serial.printf("[WATCHDOG] Pump ran %lus >= max %lus -> force OFF\n",
                          elapsed / 1000,
                          (unsigned long)(g_pumpMaxOnMs / 1000));
            g_scheduleOffTime = 0;
            g_scheduleTriggered = false;
            pumpOff();
        }
    }
    else
    {
        if (g_scheduleOffTime > 0)
            g_scheduleOffTime = 0;
        if (g_scheduleTriggered)
            g_scheduleTriggered = false;
    }

    // Cooldown timeout
    if (g_pumpCooldown)
    {
        if (millis() - g_pumpLastOffTime >= g_pumpCooldownMs)
        {
            g_pumpCooldown = false;
            Serial.println("[WATCHDOG] Pump cooldown ended");
        }
    }
}

// ============================================================================
// TASK: GỬI TELEMETRY LÊN CLOUD (mỗi 3 giây)
// Đóng gói snapshot dữ liệu hiện tại vào queue cho CoreIoT task đọc
// ============================================================================
void Task_SendTelemetry(void)
{
    iotBridge_sendTelemetry();
    Serial.printf("[TELE] Queued: T=%.1f H=%.0f Soil=%.0f AI=%s (%.1f%%)\n",
                  g_temperature, g_humidity, g_soilMoisture, g_aiStateStr, g_aiConfidence);
}

// ============================================================================
// TASK: XỬ LÝ LỆNH RPC TỪ CLOUD (mỗi 200ms)
// ============================================================================
void Task_ProcessRpc(void)
{
    iotBridge_processRpcCommands();
}

// ============================================================
// TASK: KIỂM TRA LỊCH TƯỚI (5 giây)
// ============================================================
void Task_CheckSchedule(void)
{
    static uint32_t s_lastTriggeredMinute = 0xFFFFFFFF;

    bool enabled;
    uint8_t count;
    ScheduleEntry_t tmp[MAX_SCHEDULES];

    SENSOR_LOCK();
    enabled = g_scheduleEnabled;
    count = g_scheduleCount;
    memcpy(tmp, g_schedules, sizeof(ScheduleEntry_t) * count);
    SENSOR_UNLOCK();

    if (!enabled || count == 0 || g_pumpState)
        return;

    DateTime now = getRTCTime();
    if (now.year() < 2024)
        return; // Chưa có thời gian hợp lệ thì bỏ qua

    uint32_t currentMinute = now.hour() * 60 + now.minute();
    if (currentMinute == s_lastTriggeredMinute)
        return;

    for (uint8_t i = 0; i < count; i++)
    {
        ScheduleEntry_t &s = tmp[i];
        if (!s.enabled)
            continue;
        if (!(s.days & (1 << now.dayOfTheWeek())))
            continue;
        if (s.hour != now.hour())
            continue;
        if (s.minute != now.minute())
            continue;

        Serial.printf("[SCHED] Match entry %d: %02d:%02d, %d min\n",
                      i, s.hour, s.minute, s.duration);

        s_lastTriggeredMinute = currentMinute;
        g_scheduleTriggered = true;
        pumpOn();
        g_scheduleOffTime = millis() + ((unsigned long)s.duration * 60000UL);
        break;
    }
}

// ============================================================
// TASK: HEARTBEAT (5 giây) - in tóm tắt trạng thái hệ thống
// ============================================================
void Task_Heartbeat(void)
{
    DateTime now = getRTCTime();

    Serial.println("========== GREENHOUSE & AI STATUS ==========");
    Serial.printf("  Time: %02d/%02d/%04d %02d:%02d:%02d\n",
                  now.day(), now.month(), now.year(),
                  now.hour(), now.minute(), now.second());
    Serial.printf("  WiFi: %s  IP: %s\n",
                  WiFi.status() == WL_CONNECTED ? "OK" : "LOST",
                  WiFi.localIP().toString().c_str());
    Serial.println("--------------------------------------------");
    Serial.printf("  Temp:%.1fC%s  Humi:%.0f%%\n",
                  g_temperature, g_dhtError ? "[ERR]" : "",
                  g_humidity);
    Serial.printf("  Soil:%.0f%%  Light:%.0flux\n",
                  g_soilMoisture, g_lightLux);
    Serial.printf("  PUMP:%-3s  FAN:%-3s  Mode:%s\n",
                  g_pumpState ? "ON" : "OFF",
                  g_fanState ? "ON" : "OFF",
                  g_autoMode ? "AUTO" : "MAN");
    Serial.printf("  AI State: %s (Conf: %.1f%% | %luus)\n",
                  g_aiStateStr, g_aiConfidence, (unsigned long)g_aiInferenceTimeUs);
    Serial.printf("  AI Forecast: %s\n", g_aiForecastStr);
    Serial.printf("  Free Heap: %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.println("============================================");
}
