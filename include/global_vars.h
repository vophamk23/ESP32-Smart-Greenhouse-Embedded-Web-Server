#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

/*
 * global_vars.h - Khai báo biến toàn cục hệ thống Cleanroom Bio-Chamber & Edge AI
 */

#include <Arduino.h>
#include <RTClib.h>
#include "iot_bridge.h"

// ============================================================================
// CONFIG NETWORK & CLOUD COREIOT
// ============================================================================
extern char WIFI_SSID[33];
extern char WIFI_PASS[65];
extern char CORE_IOT_TOKEN[64];
extern char CORE_IOT_SERVER[128];
extern char CORE_IOT_PORT[6];

// ============================================================================
// LỊCH PHUN SƯƠNG TẠO ẨM CHU KỲ
// ============================================================================
#define MAX_SCHEDULES 10

struct ScheduleEntry_t
{
    uint8_t hour;     // Giờ bật phun sương (0-23)
    uint8_t minute;   // Phút bật phun sương (0-59)
    uint8_t duration; // Thời lượng phun (phút, 1-120)
    uint8_t days;     // Bitmask: bit0=CN bit1=T2 ... bit6=T7
    bool enabled;     // Bật/tắt lịch này
};

extern ScheduleEntry_t g_schedules[MAX_SCHEDULES];
extern uint8_t g_scheduleCount;
extern bool g_scheduleEnabled;
extern unsigned long g_scheduleOffTime;
extern bool g_scheduleTriggered;

// ============================================================================
// KIỂU DỮ LIỆU MÀU RGB
// ============================================================================
struct RGBColor_t
{
    uint8_t r, g, b;
};

// ============================================================================
// DỮ LIỆU CẢM BIẾN
// ============================================================================
extern float g_temperature;
extern float g_humidity;
extern float g_soilMoisture;
extern float g_lightLux;
extern bool g_dhtError;
extern bool g_lightError;

// ============================================================================
// BIẾN TRÍ TUỆ NHÂN TẠO TINYML (EDGE AI)
// ============================================================================
extern uint8_t g_aiState;            // 0: COMFORTABLE, 1: HEAT_WARNING, 2: EXTREME_ALERT
extern float g_aiConfidence;         // Độ tin cậy mô hình (%)
extern uint32_t g_aiInferenceTimeUs; // Thời gian suy luận on-chip (µs)
extern char g_aiStateStr[32];        // Chuỗi trạng thái ("COMFORTABLE", ...)
extern char g_aiForecastStr[64];     // Chuỗi dự báo thời tiết 30 phút tới
extern bool g_aiAutoControl;         // Cho phép AI tự động ra quyết định kích hoạt quạt/bơm
extern float g_aiAnomalyScore;       // Điểm dị thường cảm biến (0.00 - 1.00)
extern bool g_aiIsAnomaly;           // Cờ báo dị thường (predictedAnomaly)
extern char g_aiAnomalyType[32];     // Chuỗi loại dị thường ("NORMAL", "FLAME_TAMPER", ...)
extern float g_aiOnlineAccuracy;     // Tỷ lệ dự đoán đúng tích lũy (%)

// ============================================================================
// TRẠNG THÁI THIẾT BỊ
// ============================================================================
extern bool g_pumpState;
extern bool g_fanState;
extern bool g_ledGrowState;
extern bool g_pumpManual;

// ============================================================================
// THỜI GIAN & COOLDOWN BƠM
// ============================================================================
extern unsigned long g_pumpStartTime;
extern unsigned long g_pumpLastOffTime;
extern bool g_pumpCooldown;

// ============================================================================
// HIỂN THỊ LCD
// ============================================================================
extern int g_lcdPage;
extern unsigned long g_lcdPageTimer;

// ============================================================================
// RTC DS3231 (Tùy chọn, không bắt buộc)
// ============================================================================
extern RTC_DS3231 g_rtc;
extern bool g_rtcError;

// ============================================================================
// CHẾ ĐỘ HỆ THỐNG
// ============================================================================
extern bool g_autoMode;

// ============================================================================
// CẢNH BÁO
// ============================================================================
extern bool g_alertTemp;
extern bool g_alertHumidity;
extern bool g_alertSoil;
extern bool g_alertLight;

// ============================================================================
// NGƯỠNG ĐIỀU KHIỂN
// ============================================================================
extern float g_soilDryThreshold;
extern float g_soilWetThreshold;
extern float g_tempHighThreshold;
extern float g_tempLowThreshold;
extern unsigned long g_pumpMaxOnMs;
extern unsigned long g_pumpCooldownMs;

// ============================================================================
// LED RGB MÀU HIỆN TẠI
// ============================================================================
extern RGBColor_t g_currentLEDColor;

// ============================================================================
// THỐNG KÊ
// ============================================================================
extern unsigned long g_totalPumpTime;
extern uint32_t g_pumpCount;

#endif // GLOBAL_VARS_H
