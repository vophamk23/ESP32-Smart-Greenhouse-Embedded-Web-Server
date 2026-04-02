#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include <Arduino.h>

// Kết quả sync NTP
enum class NTPSyncResult
{
    SUCCESS,
    NO_WIFI,
    TIMEOUT,
    RTC_ERROR
};

// Sync RTC từ NTP server
// Gọi sau khi WiFi đã connected
NTPSyncResult syncRTCfromNTP(uint8_t maxRetries = 3, uint32_t timeoutMs = 5000);

// Lấy chuỗi mô tả kết quả
const char *ntpResultStr(NTPSyncResult result);

#endif // NTP_SYNC_H