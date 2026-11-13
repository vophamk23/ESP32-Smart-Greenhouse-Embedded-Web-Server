/*
 * sensor_handler.cpp - Đọc cảm biến Modbus RS485 / DHT20 (I2C), LDR, Substrate Moisture
 * Hỗ trợ chuyển đổi tự động cho Yolo:UNO (ESP32-S3) và cơ chế Fail-Safe.
 */

#include "sensor_handler.h"
#include "config.h"
#include "global_vars.h"
#include "iot_bridge.h"
#include "ntp_sync.h"
#include <Wire.h>

// ==================== ĐỐI TƯỢNG CẢM BIẾN ====================
#if USE_MODBUS_RS485
  static HardwareSerial s_rs485Serial(1); // Sử dụng UART1 phần cứng trên ESP32-S3
  static ModbusMaster s_modbusNode;
#endif

#if USE_DHT20
  static DHT20 s_dht20;
#else
  static DHT s_dht(DHT_PIN, DHT_TYPE);
#endif

// EMA (Exponential Moving Average) filter cho soil ADC
static const float SOIL_EMA_ALPHA = 0.2f;
static float s_soilEMA = -1.0f;

// ============================================================================
// KHỞI TẠO TẤT CẢ CÁC CẢM BIẾN (I2C, ADC, MODBUS RS485)
// ============================================================================
void initSensors()
{
    // 1. Khởi tạo bus I2C nội bộ cho DHT20 (0x38), RTC DS3231 (0x68) và LCD 1602 (0x21/0x27)
    Wire.begin(SDA_PIN, SCL_PIN);

#if USE_MODBUS_RS485
    // 2. Khởi tạo UART1 phần cứng cho Cảm biến Công nghiệp Modbus RS485
    s_rs485Serial.begin(RS485_BAUDRATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    s_modbusNode.begin(RS485_SLAVE_ID, s_rs485Serial);
    Serial.printf("[SENSOR] Modbus RS485 initialized (RX=%d, TX=%d, Baud=%d, SlaveID=%d)\n",
                  RS485_RX_PIN, RS485_TX_PIN, RS485_BAUDRATE, RS485_SLAVE_ID);
#endif

#if USE_DHT20
    s_dht20.begin();
    Serial.printf("[SENSOR] DHT20 I2C initialized on SDA=%d, SCL=%d (Addr=0x38)\n", SDA_PIN, SCL_PIN);
#else
    s_dht.begin();
    Serial.printf("[SENSOR] DHT%d initialized on Pin GPIO%d\n", DHT_TYPE, DHT_PIN);
#endif

    // 3. Cảm biến ánh sáng LDR: Cấu hình ADC 12-bit (dải 0 - 4095, suy hao ADC_11db = 0 - 3.3V)
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    Serial.printf("[SENSOR] Light ADC initialized (Pin GPIO%d, 12-bit)\n", LIGHT_SENSOR_PIN);

    // 4. Cảm biến độ ẩm giá thể/phôi điện dung: Cấu hình ADC 12-bit kết hợp bộ lọc EMA (Alpha = 0.2)
    Serial.printf("[SENSOR] Substrate Moisture ADC initialized (Pin GPIO%d, 12-bit)\n", SOIL_MOISTURE_PIN);
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

// ============================================================================
// HÀM ĐỌC CẢM BIẾN NHIỆT ĐỘ & ĐỘ ẨM KHÔNG KHÍ (TÍCH HỢP ĐA GIAO THỨC & FAIL-SAFE)
// - Kênh chính: Cảm biến Modbus RS485 công nghiệp (RTU Function Code 03)
// - Dự phòng song song: Cảm biến I2C DHT20 (0x38) / DHT11 (1-Wire)
// - Đồng bộ liên nhân an toàn luồng: Khóa SemaphoreMutex SENSOR_LOCK()
// ============================================================================
void readDHT()
{
    float temp = 0.0f;
    float humi = 0.0f;
    bool readSuccess = false;

#if USE_MODBUS_RS485
    // ------------------------------------------------------------------------
    // [ƯU TIÊN 1] Đọc cảm biến công nghiệp Modbus RS485
    // - Chuẩn giao thức: Modbus RTU qua bus vi sai RS485 (UART1: RX=GPIO44, TX=GPIO43)
    // - Function Code: 03 (Read Holding Registers)
    // - Địa chỉ bắt đầu: RS485_REG_HUMI (0), số lượng đọc: 2 thanh ghi 16-bit
    // ------------------------------------------------------------------------
    uint8_t result = s_modbusNode.readHoldingRegisters(RS485_REG_HUMI, 2);
    if (result == s_modbusNode.ku8MBSuccess)
    {
        // Thanh ghi 0: Độ ẩm tương đối (%RH) - Độ phân giải 0.1% (Ví dụ: 654 -> 65.4%RH)
        humi = (uint16_t)s_modbusNode.getResponseBuffer(0) / 10.0f;

        // Thanh ghi 1: Nhiệt độ không khí (°C) - Ép kiểu int16_t để xử lý cả nhiệt độ âm
        temp = (int16_t)s_modbusNode.getResponseBuffer(1) / 10.0f;

        readSuccess = true;
    }
    else
    {
        // Ghi log mã lỗi Modbus (0x02: Illegal Address, 0xE2: Timeout,...) và tự động kích hoạt Fail-Safe
        Serial.printf("[SENSOR] Modbus RS485 read error (0x%02X) -> Tự động chuyển Fallback sang DHT20\n", result);
    }
#endif

    // ------------------------------------------------------------------------
    // [ƯU TIÊN 2] Cơ chế phòng vệ tự động (Fail-Safe): Chuyển sang đọc cảm biến DHT20
    // Kích hoạt khi: Cảm biến RS485 chưa kết nối, mất nguồn ngoài, hoặc tuột dây cáp
    // ------------------------------------------------------------------------
    if (!readSuccess)
    {
#if USE_DHT20
        // Đọc cảm biến số DHT20 qua bus I2C nội bộ (Địa chỉ 0x38, SDA=11, SCL=12)
        int status = s_dht20.read();
        temp = s_dht20.getTemperature();
        humi = s_dht20.getHumidity();

        // Kiểm tra tính toàn vẹn tín hiệu: mã lỗi, giá trị NaN hoặc cụm [0.0, 0.0] do treo bus I2C
        if (status != 0 || isnan(temp) || isnan(humi) || (temp == 0.0f && humi == 0.0f))
        {
            g_dhtError = true;
            Serial.printf("[SENSOR] DHT20 read FAILED (status=%d) -> Giữ nguyên giá trị an toàn trước đó\n", status);

            // Tự phục hồi bus I2C (I2C Bus Recovery) nếu bị treo tín hiệu
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
        // Dự phòng cho cảm biến 1 dây DHT11 / DHT22
        temp = s_dht.readTemperature();
        humi = s_dht.readHumidity();

        if (isnan(temp) || isnan(humi))
        {
            g_dhtError = true;
            Serial.println("[SENSOR] DHT read FAILED -> Giữ nguyên giá trị an toàn trước đó");
            return;
        }
#endif
    }

    // ------------------------------------------------------------------------
    // [BƯỚC 3] Sanity Check (Kiểm tra miền vật lý hợp lệ):
    // Loại bỏ các xung nhiễu điện từ do đóng/ngắt Relay động cơ máy bơm và quạt
    // ------------------------------------------------------------------------
    if (temp < -20.0f || temp > 85.0f || humi < 0.0f || humi > 100.0f)
    {
        g_dhtError = true;
        Serial.printf("[SENSOR] Dữ liệu ngoài miền vật lý hợp lệ: T=%.1f°C, H=%.1f%% -> Bỏ qua mẫu đo\n", temp, humi);
        return;
    }

    // ------------------------------------------------------------------------
    // [BƯỚC 4] Cập nhật biến toàn cục bảo vệ bằng Khóa loại trừ tương hỗ (Mutex):
    // Ngăn chặn race condition giữa Core 1 (Scheduler/TinyML) và Core 0 (WiFi/MQTT/Web)
    // ------------------------------------------------------------------------
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

// ==================== MAP ADC -> % ĐỘ ẨM GIÁ THỂ ====================
static float mapSoilADC(int adcValue)
{
    int clamped = constrain(adcValue, SOIL_ADC_WET, SOIL_ADC_DRY);
    float moisture = ((float)(SOIL_ADC_DRY - clamped) /
                      (float)(SOIL_ADC_DRY - SOIL_ADC_WET)) *
                     100.0f;
    return constrain(moisture, 0.0f, 100.0f);
}

// ==================== ĐỌC ĐỘ ẨM GIÁ THỂ (ADC + BỘ LỌC EMA) ====================
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
