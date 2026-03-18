#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>

/*
 * Thư viện sử dụng (local lib/):
 *   - DHT (Adafruit) - đã có sẵn trong lib/DHT
 *   - Adafruit_Sensor - đã có sẵn trong lib/Adafruit_Sensor
 *   - LDR module DO: không cần thư viện
 *   - Cảm biến đất ADC: không cần thư viện
 */
#include "DHT.h"    // dùng thư viện local lib/DHT
#include <RTClib.h>


// ==================== KHỞI TẠO ====================
void initSensors();

// ==================== ĐỌC CẢM BIẾN ====================
void readDHT();             // Đọc nhiệt độ & độ ẩm -> g_temperature, g_humidity
void readLightSensor();     // Đọc LDR DO -> g_lightLux (0 hoặc 1000)
void readSoilMoisture();    // Đọc độ ẩm đất (ADC) -> g_soilMoisture

void initRTC();         // Khởi tạo DS3231

DateTime getRTCTime();  // Trả về thời gian hiện tại từ DS3231
// ==================== GETTER ====================
DHT* getDHT();

// ==================== UTILITY ====================
// float mapSoilADC(int adcValue);
void  printSensorData();

#endif // SENSOR_HANDLER_H