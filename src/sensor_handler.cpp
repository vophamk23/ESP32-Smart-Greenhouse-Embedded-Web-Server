/*
 * sensor_handler.cpp - Đọc cảm biến DHT11/22, LDR DO, Soil ADC
 *
 * FIXES & IMPROVEMENTS:
 *   1. readSoilMoisture() bỏ delay(2) -> không chặn scheduler
 *   2. DHT retry logic: bỏ qua giá trị NaN thay vì xóa toàn bộ dữ liệu
 *   3. Thêm lọc low-pass đơn giản cho soil ADC (EMA filter)
 *   4. GPIO ADC-only (34, 35) không set pinMode INPUT (gây warning trên ESP32)
 */

#include "sensor_handler.h"
#include "config.h"
#include "global_vars.h"

// ==================== ĐỐI TƯỢNG CẢM BIẾN ====================
static DHT dht(DHT_PIN, DHT_TYPE);

// EMA (Exponential Moving Average) filter cho soil ADC
// alpha = 0.2: giữ 80% giá trị cũ, 20% giá trị mới -> lọc nhiễu tốt
static const float SOIL_EMA_ALPHA = 0.2f;
static float s_soilEMA = -1.0f;    // -1 = chưa khởi tạo

// ==================== KHỞI TẠO ====================
void initSensors()
{
    // ---- DHT11/22 ----
    dht.begin();
    Serial.printf("[SENSOR] DHT%s initialized (Pin GPIO%d)\n",
                  (DHT_TYPE == DHT11) ? "11" : "22", DHT_PIN);

    // ---- Cảm biến ánh sáng LDR (ADC) ----
    // GPIO34: input-only, không gọi pinMode
    //  ADC config: 12-bit, 11dB attenuation (đọc 0~3.3V full range)
    analogReadResolution(12);       // 12-bit: 0-4095
    analogSetAttenuation(ADC_11db); // Đọc full range 0-3.3V
    Serial.printf("[SENSOR] Light ADC initialized (Pin GPIO%d, 12-bit)\n",
                LIGHT_SENSOR_PIN);
    Serial.printf("[SENSOR] Light calibration: DARK=%d, BRIGHT=%d\n",
                LIGHT_ADC_DARK, LIGHT_ADC_BRIGHT);


    // ---- Cảm biến đất (ADC) ----
    // GPIO35: input-only, không gọi pinMode
    // ADC config: 12-bit, 11dB attenuation (đọc 0~3.3V full range)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.printf("[SENSOR] Soil ADC initialized (Pin GPIO%d, 12-bit, 11dB)\n",
                  SOIL_MOISTURE_PIN);
    Serial.printf("[SENSOR] Soil calibration: DRY=%d, WET=%d\n",
                  SOIL_ADC_DRY, SOIL_ADC_WET);
}

// ==================== ĐỌC DHT11/22 ====================
// Giữ nguyên giá trị cũ nếu lỗi thay vì trả 0 (safer for control logic)
void readDHT()
{
    float temp = dht.readTemperature();
    float humi = dht.readHumidity();

    if (isnan(temp) || isnan(humi))
    {
        // Lỗi đọc: giữ giá trị cũ, set flag
        g_dhtError = true;
        Serial.println("[SENSOR] DHT read FAILED - keeping last values");
        return;
    }

    // Sanity check: loại bỏ giá trị vô lý (DHT11: 0-50°C, DHT22: -40-80°C)
    if (temp < -40.0f || temp > 85.0f || humi < 0.0f || humi > 100.0f)
    {
        g_dhtError = true;
        Serial.printf("[SENSOR] DHT out of range: T=%.1f H=%.1f - skipped\n", temp, humi);
        return;
    }

    g_temperature = temp;
    g_humidity    = humi;
    g_dhtError    = false;
}

// ==================== ĐỌC ÁNH SÁNG LDR (DO Digital) ====================
// void readLightSensor()
// {
//     int doValue = digitalRead(LIGHT_SENSOR_PIN);

//     // DO=LOW  -> đủ sáng (quang trở dẫn, LM393 output kéo xuống GND)
//     // DO=HIGH -> tối    (quang trở kháng cao, LM393 output HIGH)
//     g_lightLux   = (doValue == LOW) ? LIGHT_LUX_BRIGHT : LIGHT_LUX_DARK;
//     g_lightError = false;   // LDR DO không có lỗi hw, chỉ có 2 trạng thái

//     Serial.printf("[SENSOR] LDR DO=%d -> %.0flux (%s)\n",
//                   doValue, g_lightLux, (doValue == LOW) ? "BRIGHT" : "DARK");
// }

// ==================== ĐỌC ÁNH SÁNG LDR (AO Analog) ====================
void readLightSensor()
{
    long sum = 0;
    for (int i = 0; i < LIGHT_ADC_SAMPLES; i++)
        sum += analogRead(LIGHT_SENSOR_PIN);
    int avgADC = (int)(sum / LIGHT_ADC_SAMPLES);

    // constrain đúng thứ tự: min trước, max sau
    int clamped = constrain(avgADC, LIGHT_ADC_DARK, LIGHT_ADC_BRIGHT);
    //                               500              4000  ← min < max ✅

    // ADC cao = sáng, ADC thấp = tối → map thuận chiều
    g_lightLux = ((float)(clamped - LIGHT_ADC_DARK) /
                  (float)(LIGHT_ADC_BRIGHT - LIGHT_ADC_DARK)) * LIGHT_LUX_BRIGHT;

    g_lightLux   = constrain(g_lightLux, LIGHT_LUX_DARK, LIGHT_LUX_BRIGHT);
    g_lightError = false;

    Serial.printf("[SENSOR] Light ADC=%d clamped=%d -> %.0flux (%s)\n",
                  avgADC, clamped, g_lightLux,
                  g_lightLux >= LIGHT_LOW_THRESHOLD ? "BRIGHT" : "DARK");
}



// ==================== MAP ADC -> % ĐỘ ẨM ĐẤT ====================
// Capacitive sensor: ADC cao = khô, ADC thấp = ướt (NGƯỢC với resistive)
static float mapSoilADC(int adcValue)
{
    // Clamp trong vùng calibration
    int clamped = constrain(adcValue, SOIL_ADC_WET, SOIL_ADC_DRY);
    // Map ngược: DRY=0%, WET=100%
    float moisture = ((float)(SOIL_ADC_DRY - clamped) /
                      (float)(SOIL_ADC_DRY - SOIL_ADC_WET)) * 100.0f;
    return constrain(moisture, 0.0f, 100.0f);
}

// ==================== ĐỌC ĐỘ ẨM ĐẤT (ADC) ====================
// Không dùng delay() để không chặn scheduler
// Đọc SOIL_ADC_SAMPLES lần liên tiếp -> tính trung bình -> EMA filter
void readSoilMoisture()
{
    // Đọc nhiều mẫu không có delay -> vẫn giảm được nhiễu ADC ngắn hạn
    long sum = 0;
    for (int i = 0; i < SOIL_ADC_SAMPLES; i++)
    {
        sum += analogRead(SOIL_MOISTURE_PIN);
        // Không delay() ở đây - chấp nhận thêm nhiễu, bù lại bằng EMA filter
    }
    int avgADC = (int)(sum / SOIL_ADC_SAMPLES);
    float rawMoisture = mapSoilADC(avgADC);

    // EMA filter: làm mượt tín hiệu qua nhiều lần đọc
    if (s_soilEMA < 0.0f)
        s_soilEMA = rawMoisture; // Khởi tạo lần đầu
    else
        s_soilEMA = SOIL_EMA_ALPHA * rawMoisture + (1.0f - SOIL_EMA_ALPHA) * s_soilEMA;

    g_soilMoisture = s_soilEMA;

    Serial.printf("[SENSOR] Soil ADC=%d, Raw=%.1f%%, EMA=%.1f%%\n",
                  avgADC, rawMoisture, s_soilEMA);
}

// ==================== GETTER ====================
DHT* getDHT() { return &dht; }

// ==================== IN DỮ LIỆU SERIAL ====================
void printSensorData()
{
    Serial.println("===== SENSOR DATA =====");
    Serial.printf("  Temp    : %.1f C  %s\n",
                  g_temperature, g_dhtError ? "[ERR]" : "[OK]");
    Serial.printf("  Humidity: %.1f %%\n", g_humidity);
    Serial.printf("  Soil    : %.1f %% (EMA filtered)\n", g_soilMoisture);
    Serial.printf("  Light   : %.0f lux -> %s\n",
                  g_lightLux, (g_lightLux > 0) ? "BRIGHT" : "DARK");
    Serial.println("=======================");
}