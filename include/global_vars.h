#pragma once
/*
 * global_vars.h - Khai báo biến toàn cục có volatile cho thread safety
 *
 * Tất cả biến được đọc/ghi từ nhiều context (ISR + loop) phải là volatile.
 * Các biến chỉ dùng trong loop() không cần volatile nhưng khai báo chung cho đồng nhất.
 */

#include <Arduino.h>

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