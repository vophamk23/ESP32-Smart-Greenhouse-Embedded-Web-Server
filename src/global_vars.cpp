/*
 * global_vars.cpp - Định nghĩa biến toàn cục
 */
#include "global_vars.h"
#include "config.h"

// ============================================================================
// CONFIG NETWORK
// ============================================================================
char WIFI_SSID[33] = ""; // max SSID 32 chars + null
char WIFI_PASS[65] = ""; // max WPA2 64 chars + null
char CORE_IOT_TOKEN[64] = "";
char CORE_IOT_SERVER[128] = "";
char CORE_IOT_PORT[6] = "1883";
char LOCAL_MQTT_HOST[64] = "";
char LOCAL_MQTT_PORT[6] = "1883";

// ==================== DỮ LIỆU CẢM BIẾN ====================
float g_temperature = 25.0f; // Giá trị khởi tạo an toàn
float g_humidity = 60.0f;
float g_soilMoisture = 50.0f;
float g_lightLux = 1000.0f; // Mặc định "sáng" để tránh bật grow light ngay
bool g_dhtError = true;     // true cho đến khi đọc thành công lần đầu
bool g_lightError = false;

// ==================== RTC DS3231 ====================
RTC_DS3231 g_rtc;
bool g_rtcError = true;

// ==================== TRẠNG THÁI THIẾT BỊ ====================
bool g_pumpState = false;
bool g_fanState = false;
bool g_ledGrowState = false;
bool g_pumpManual = false;

// ==================== LỊCH TƯỚI ====================
ScheduleEntry_t g_schedules[MAX_SCHEDULES] = {};
uint8_t g_scheduleCount = 0;
bool g_scheduleEnabled = false;
unsigned long g_scheduleOffTime = 0; // 0 = không có lịch đang chạy
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
unsigned long g_pumpCooldownMs = DEFAULT_PUMP_COOL_MS;