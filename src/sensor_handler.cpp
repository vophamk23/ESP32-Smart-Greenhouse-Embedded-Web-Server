/*
 * sensor_handler.cpp - Đọc cảm biến DHT20 (I2C) / DHT11, LDR, Soil Moisture
 * Hỗ trợ chuyển đổi tự động cho Yolo:UNO (ESP32-S3) và cơ chế Fail-Safe.
 */

#include "sensor_handler.h"
#include "config.h"
#include "global_vars.h"
#include "iot_bridge.h"
#include "ntp_sync.h"
#include <Wire.h>

// ==================== ĐỐI TƯỢNG CẢM BIẾN ====================
#if USE_DHT20
  static DHT20 s_dht20;
#else
  static DHT s_dht(DHT_PIN, DHT_TYPE);
#endif

// EMA (Exponential Moving Average) filter cho soil ADC
static const float SOIL_EMA_ALPHA = 0.2f;
static float s_soilEMA = -1.0f;

// ==================== KHỞI TẠO CẢM BIẾN ====================
void initSensors()
{
    // 1. Khởi tạo bus I2C cho DHT20 và LCD
    Wire.begin(SDA_PIN, SCL_PIN);

#if USE_DHT20
    s_dht20.begin();
    Serial.printf("[SENSOR] DHT20 I2C initialized on SDA=%d, SCL=%d (Addr=0x38)\n", SDA_PIN, SCL_PIN);
#else
    s_dht.begin();
    Serial.printf("[SENSOR] DHT%d initialized on Pin GPIO%d\n", DHT_TYPE, DHT_PIN);
#endif

    // 2. Cảm biến ánh sáng LDR (ADC)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.printf("[SENSOR] Light ADC initialized (Pin GPIO%d, 12-bit)\n", LIGHT_SENSOR_PIN);

    // 3. Cảm biến độ ẩm đất (ADC)
    Serial.printf("[SENSOR] Soil ADC initialized (Pin GPIO%d, 12-bit)\n", SOIL_MOISTURE_PIN);
}

// ==================== KHỞI TẠO RTC DS3231 ====================
void initRTC()
{
    // Kiểm tra mềm: không dùng while(1) để tránh treo hệ thống nếu kit không có DS3231
    if (!g_rtc.begin())
    {
        Serial.println("[RTC] DS3231 not detected on I2C - using ESP32 internal clock & NTP fallback");
        g_rtcError = true;
        return;
    }

    g_rtcError = false;

    if (g_rtc.lostPower())
    {
        Serial.println("[RTC] Lost power - using compile time as fallback");
        g_rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    DateTime now = g_rtc.now();
    Serial.printf("[RTC] DS3231 ready: %02d/%02d/%04d %02d:%02d:%02d\n",
                  now.day(), now.month(), now.year(),
                  now.hour(), now.minute(), now.second());
}

// ==================== ĐỌC THỜI GIAN RTC ====================
DateTime getRTCTime()
{
    if (!g_rtcError)
    {
        return g_rtc.now();
    }

    // Fallback thông minh: Đọc giờ từ đồng hồ phần cứng nội bộ của ESP32-S3 (đồng bộ qua WiFi NTP)
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10))
    {
        return DateTime(
            timeinfo.tm_year + 1900,
            timeinfo.tm_mon + 1,
            timeinfo.tm_mday,
            timeinfo.tm_hour,
            timeinfo.tm_min,
            timeinfo.tm_sec);
    }

    // Nếu chưa có kết nối WiFi: dùng thời gian biên dịch + thời gian chip chạy millis()
    uint32_t sec = millis() / 1000UL;
    return DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(sec);
}

// ==================== ĐỌC CẢM BIẾN NHIỆT ĐỘ & ĐỘ ẨM ====================
void readDHT()
{
#if USE_DHT20
    int status = s_dht20.read();
    float temp = s_dht20.getTemperature();
    float humi = s_dht20.getHumidity();

    // Fail-Safe logic kiểm tra tính hợp lệ của DHT20
    if (status != 0 || isnan(temp) || isnan(humi) || (temp == 0.0f && humi == 0.0f))
    {
        g_dhtError = true;
        Serial.printf("[SENSOR] DHT20 read FAILED (status=%d) - keeping last values\n", status);

        // Khôi phục tự động bus I2C nếu tuột dây và cắm lại
        static uint8_t retryCount = 0;
        if (++retryCount >= 3)
        {
            Wire.begin(SDA_PIN, SCL_PIN);
            s_dht20.begin();
            retryCount = 0;
        }
        return;
    }
#else
    float temp = s_dht.readTemperature();
    float humi = s_dht.readHumidity();

    if (isnan(temp) || isnan(humi))
    {
        g_dhtError = true;
        Serial.println("[SENSOR] DHT read FAILED - keeping last values");
        return;
    }
#endif

    // Sanity check: loại bỏ giá trị vô lý
    if (temp < -20.0f || temp > 85.0f || humi < 0.0f || humi > 100.0f)
    {
        g_dhtError = true;
        Serial.printf("[SENSOR] DHT out of range: T=%.1f H=%.1f\n", temp, humi);
        return;
    }

    // Ghi dữ liệu vào biến toàn cục được bảo vệ bằng Semaphore Mutex
    SENSOR_LOCK();
    g_temperature = temp;
    g_humidity = humi;
    g_dhtError = false;
    SENSOR_UNLOCK();
}

// ==================== ĐỌC ÁNH SÁNG LDR ====================
void readLightSensor()
{
    long sum = 0;
    for (int i = 0; i < LIGHT_ADC_SAMPLES; i++)
        sum += analogRead(LIGHT_SENSOR_PIN);
    int avgADC = (int)(sum / LIGHT_ADC_SAMPLES);

    int clamped = constrain(avgADC, LIGHT_ADC_DARK, LIGHT_ADC_BRIGHT);

    float lux = ((float)(clamped - LIGHT_ADC_DARK) /
                 (float)(LIGHT_ADC_BRIGHT - LIGHT_ADC_DARK)) *
                LIGHT_LUX_BRIGHT;
    lux = constrain(lux, LIGHT_LUX_DARK, LIGHT_LUX_BRIGHT);

    SENSOR_LOCK();
    g_lightLux = lux;
    g_lightError = false;
    SENSOR_UNLOCK();
}

// ==================== MAP ADC -> % ĐỘ ẨM ĐẤT ====================
static float mapSoilADC(int adcValue)
{
    int clamped = constrain(adcValue, SOIL_ADC_WET, SOIL_ADC_DRY);
    float moisture = ((float)(SOIL_ADC_DRY - clamped) /
                      (float)(SOIL_ADC_DRY - SOIL_ADC_WET)) *
                     100.0f;
    return constrain(moisture, 0.0f, 100.0f);
}

// ==================== ĐỌC ĐỘ ẨM ĐẤT (ADC + BỘ LỌC EMA) ====================
void readSoilMoisture()
{
    long sum = 0;
    for (int i = 0; i < SOIL_ADC_SAMPLES; i++)
    {
        sum += analogRead(SOIL_MOISTURE_PIN);
    }
    int avgADC = (int)(sum / SOIL_ADC_SAMPLES);
    float rawMoisture = mapSoilADC(avgADC);

    // Bộ lọc Exponential Moving Average (EMA) làm mịn tín hiệu
    if (s_soilEMA < 0.0f)
        s_soilEMA = rawMoisture;
    else
        s_soilEMA = SOIL_EMA_ALPHA * rawMoisture + (1.0f - SOIL_EMA_ALPHA) * s_soilEMA;

    SENSOR_LOCK();
    g_soilMoisture = s_soilEMA;
    SENSOR_UNLOCK();
}

// ==================== IN DỮ LIỆU SERIAL ====================
void printSensorData()
{
    Serial.println("===== SENSOR & AI DATA =====");
    Serial.printf("  Temp    : %.1f C  %s\n", g_temperature, g_dhtError ? "[ERR]" : "[OK]");
    Serial.printf("  Humidity: %.1f %%\n", g_humidity);
    Serial.printf("  Soil    : %.1f %% (EMA)\n", g_soilMoisture);
    Serial.printf("  Light   : %.0f lux\n", g_lightLux);
    Serial.printf("  AI State: %s (Conf: %.1f%%, Latency: %luus)\n",
                  g_aiStateStr, g_aiConfidence, (unsigned long)g_aiInferenceTimeUs);
    Serial.println("============================");
}
