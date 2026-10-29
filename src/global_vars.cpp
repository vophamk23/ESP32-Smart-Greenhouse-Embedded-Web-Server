/*
 * global_vars.cpp - Định nghĩa biến toàn cục hệ thống Greenhouse IoT & Edge AI
 */
#include "global_vars.h"
#include "config.h"

// ============================================================================
// CONFIG NETWORK & CLOUD COREIOT
// ============================================================================
char WIFI_SSID[33] = "";
char WIFI_PASS[65] = "";
char CORE_IOT_TOKEN[64] = "";
char CORE_IOT_SERVER[128] = "app.coreiot.io";
char CORE_IOT_PORT[6] = "1883";

// ==================== DỮ LIỆU CẢM BIẾN ====================
float g_temperature = 25.0f; // Khởi tạo an toàn
float g_humidity = 60.0f;
float g_soilMoisture = 50.0f;
float g_lightLux = 800.0f;
bool g_dhtError = false;
bool g_lightError = false;

// ==================== BIẾN TRÍ TUỆ NHÂN TẠO TINYML (EDGE AI) ====================
uint8_t g_aiState = 0;              // 0: COMFORTABLE
float g_aiConfidence = 88.5f;       // 88.5%
uint32_t g_aiInferenceTimeUs = 50;  // ~50 µs
char g_aiStateStr[32] = "COMFORTABLE";
char g_aiForecastStr[64] = "Vi khi hau on dinh";
bool g_aiAutoControl = true;        // Mặc định bật chế độ tự động AI
float g_aiAnomalyScore = 0.05f;     // Điểm dị thường khởi tạo thấp (bình thường)
bool g_aiIsAnomaly = false;
char g_aiAnomalyType[32] = "NORMAL";
float g_aiOnlineAccuracy = 100.0f;  // Khởi tạo 100%

// ==================== RTC DS3231 ====================
RTC_DS3231 g_rtc;
bool g_rtcError = false;

// ==================== TRẠNG THÁI THIẾT BỊ ====================
bool g_pumpState = false;
bool g_fanState = false;
bool g_ledGrowState = false;
bool g_pumpManual = false;

// ==================== LỊCH TƯỚI ====================
ScheduleEntry_t g_schedules[MAX_SCHEDULES] = {};
uint8_t g_scheduleCount = 0;
bool g_scheduleEnabled = false;
unsigned long g_scheduleOffTime = 0;
bool g_scheduleTriggered = false;

// ==================== THỜI GIAN BƠM ====================
unsigned long g_pumpStartTime = 0;
unsigned long g_pumpLastOffTime = 0;
bool g_pumpCooldown = false;

// ==================== HIỂN THỊ LCD ====================
int g_lcdPage = 0;
unsigned long g_lcdPageTimer = 0;

// ==================== CHẾ ĐỘ HỆ THỐNG ====================
bool g_autoMode = true;

// ==================== CẢNH BÁO ====================
bool g_alertTemp = false;
bool g_alertHumidity = false;
bool g_alertSoil = false;
bool g_alertLight = false;

// ==================== LED RGB ====================
RGBColor_t g_currentLEDColor = {0, 0, 0};

// ==================== THỐNG KÊ ====================
unsigned long g_totalPumpTime = 0;
uint32_t g_pumpCount = 0;

// ==================== NGƯỠNG ĐIỀU KHIỂN ====================
float g_soilDryThreshold = DEFAULT_SOIL_DRY;
float g_soilWetThreshold = DEFAULT_SOIL_WET;
float g_tempHighThreshold = DEFAULT_TEMP_HIGH;
float g_tempLowThreshold = DEFAULT_TEMP_LOW;
unsigned long g_pumpMaxOnMs = DEFAULT_PUMP_MAX_MS;
unsigned long g_pumpCooldownMs = DEFAULT_PUMP_COOLDOWN_MS;
