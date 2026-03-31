#include "ntp_sync.h"
#include "global_vars.h"
#include <WiFi.h>
#include <time.h>

// ==================== CONFIG ====================
static const char *NTP_SERVER_1 = "pool.ntp.org";
static const char *NTP_SERVER_2 = "time.google.com";
static const char *NTP_SERVER_3 = "time.cloudflare.com";
static const long GMT_OFFSET_SEC = 7 * 3600; // UTC+7 Việt Nam
static const int DAYLIGHT_OFFSET = 0;        // Không có DST

// ==================== INTERNAL ====================
static bool waitForNTP(uint32_t timeoutMs)
{
    struct tm timeinfo;
    uint32_t start = millis();

    while (!getLocalTime(&timeinfo, 1000))
    {
        if (millis() - start >= timeoutMs)
            return false;
        Serial.print(".");
        delay(500);
    }
    Serial.println();
    return true;
}

// ==================== PUBLIC ====================
NTPSyncResult syncRTCfromNTP(uint8_t maxRetries, uint32_t timeoutMs)
{
    // 1. Kiểm tra WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("[NTP] ✗ No WiFi connection - skip sync");
        return NTPSyncResult::NO_WIFI;
    }

    // 2. Config NTP (3 server fallback)
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET,
               NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);

    // 3. Retry loop
    struct tm timeinfo;
    bool synced = false;

    for (uint8_t attempt = 1; attempt <= maxRetries; attempt++)
    {
        Serial.printf("[NTP] Attempt %d/%d - waiting for time", attempt, maxRetries);

        if (waitForNTP(timeoutMs))
        {
            getLocalTime(&timeinfo);
            synced = true;
            break;
        }

        Serial.printf("[NTP] ✗ Attempt %d timed out\n", attempt);

        if (attempt < maxRetries)
        {
            Serial.println("[NTP] Retrying in 2s...");
            delay(2000);
        }
    }

    if (!synced)
    {
        Serial.println("[NTP] ✗ All attempts failed");
        return NTPSyncResult::TIMEOUT;
    }

    // 4. Ghi vào DS3231
    DateTime ntpTime(
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

    if (!g_rtc.begin())
    {
        Serial.println("[NTP] ✗ RTC not found");
        return NTPSyncResult::RTC_ERROR;
    }

    g_rtc.adjust(ntpTime);

    // 5. Verify: đọc lại DS3231 để xác nhận ghi thành công
    DateTime verify = g_rtc.now();
    int32_t drift = (int32_t)verify.unixtime() - (int32_t)ntpTime.unixtime();

    Serial.printf("[NTP] ✓ Synced: %02d/%02d/%04d %02d:%02d:%02d (UTC+7)\n",
                  ntpTime.day(), ntpTime.month(), ntpTime.year(),
                  ntpTime.hour(), ntpTime.minute(), ntpTime.second());
    Serial.printf("[NTP] ✓ Drift after write: %+ds\n", drift);

    return NTPSyncResult::SUCCESS;
}

const char *ntpResultStr(NTPSyncResult result)
{
    switch (result)
    {
    case NTPSyncResult::SUCCESS:
        return "SUCCESS";
    case NTPSyncResult::NO_WIFI:
        return "NO_WIFI";
    case NTPSyncResult::TIMEOUT:
        return "TIMEOUT";
    case NTPSyncResult::RTC_ERROR:
        return "RTC_ERROR";
    default:
        return "UNKNOWN";
    }
}