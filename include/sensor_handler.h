#ifndef SENSOR_HANDLER_H
#define SENSOR_HANDLER_H

#include <Arduino.h>
#include <RTClib.h>
#include "config.h"

#if USE_MODBUS_RS485
  #include <ModbusMaster.h>
#endif

#if USE_DHT20
  #include "DHT20.h"
#else
  #include "DHT.h"
#endif

// ==================== KHỞI TẠO ====================
void initSensors();
void initRTC();

// ==================== ĐỌC CẢM BIẾN ====================
void readDHT();             // Đọc nhiệt độ & độ ẩm -> g_temperature, g_humidity (tích hợp RS485 & fallback DHT20)
void readLightSensor();     // Đọc quang trở ADC -> g_lightLux
void readSoilMoisture();    // Đọc độ ẩm giá thể (ADC + EMA Filter) -> g_soilMoisture

DateTime getRTCTime();      // Trả về thời gian hiện tại từ DS3231 hoặc giả lập

void printSensorData();

#endif // SENSOR_HANDLER_H
