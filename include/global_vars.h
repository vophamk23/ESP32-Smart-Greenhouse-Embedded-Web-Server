# ifndef GLOBAL_VARS_H
# define GLOBAL_VARS_H
/*
 * global_vars.h - Khai báo biến toàn cục có volatile cho thread safety
 *
 * Tất cả biến được đọc/ghi từ nhiều context (ISR + loop) phải là volatile.
 * Các biến chỉ dùng trong loop() không cần volatile nhưng khai báo chung cho đồng nhất.
 */

#include <Arduino.h>
#include <RTClib.h> 
#include <iot_bridge.h>

// ============================================================================
// LỊCH TƯỚI
// ============================================================================
#define MAX_SCHEDULES 10

struct ScheduleEntry_t {
    uint8_t  hour;        // Giờ bật bơm (0-23)
    uint8_t  minute;      // Phút bật bơm (0-59)
    uint8_t  duration;    // Thời lượng tưới (phút, 1-120)
    uint8_t  days;        // Bitmask: bit0=CN bit1=T2 ... bit6=T7
    bool     enabled;     // Bật/tắt lịch này
};

extern ScheduleEntry_t g_schedules[MAX_SCHEDULES];
extern uint8_t         g_scheduleCount;
extern bool            g_scheduleEnabled;
extern unsigned long   g_scheduleOffTime;  // millis() thời điểm cần tắt bơm theo lịch
extern bool g_scheduleTriggered; // true nếu bơm đang chạy do lịch tưới, dùng để bypass cooldown

// ============================================================================
// KIỂU DỮ LIỆU
// ============================================================================
struct RGBColor_t {
    uint8_t r, g, b;
};

// ============================================================================
// DỮ LIỆU CẢM BIẾN (chỉ ghi trong loop/task, không phải ISR -> không cần volatile)
// ============================================================================
extern float g_temperature;
extern float g_humidity;
extern float g_soilMoisture;
extern float g_lightLux;
extern bool  g_dhtError;
extern bool  g_lightError;

// ============================================================================
// TRẠNG THÁI THIẾT BỊ
// ============================================================================
extern bool g_pumpState;
extern bool g_fanState;
extern bool g_ledGrowState;
extern bool g_pumpManual;       // true = bơm do lệnh manual, không auto off theo soil

// ============================================================================
// THỜI GIAN BƠM
// ============================================================================
extern unsigned long g_pumpStartTime;
extern unsigned long g_pumpLastOffTime;
extern bool          g_pumpCooldown;

// ============================================================================
// HIỂN THỊ LCD
// ============================================================================
extern int           g_lcdPage;
extern unsigned long g_lcdPageTimer;

// ============================================================================
// RTC DS3231
// Dùng chung I2C bus SDA=21, SCL=22 với LCD
// ============================================================================
extern RTC_DS3231 g_rtc;
extern bool       g_rtcError;   // true nếu RTC không tìm thấy hoặc mất nguồn

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
// LED RGB MÀU HIỆN TẠI
// ============================================================================
extern RGBColor_t g_currentLEDColor;

// ============================================================================
// THỐNG KÊ
// ============================================================================
extern unsigned long g_totalPumpTime;
extern uint32_t      g_pumpCount;

# endif // GLOBAL_VARS_H