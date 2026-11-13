/*
 * config.h - Cấu hình toàn bộ hệ thống Cleanroom Bio-Chamber IoT cho Yolo:UNO (ESP32-S3)
 * Tự động tương thích với cả Kit Yolo:UNO (ESP32-S3) và ESP32 thường.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// LỰA CHỌN CẢM BIẾN NHIỆT ĐỘ - ĐỘ ẨM
// ============================================================================
#define USE_DHT20           1         // 1: Cảm biến I2C DHT20, 0: Cảm biến DHT11/DHT22

// Cấu hình Cảm biến Công nghiệp Modbus RS485
// Nếu đặt 1: Hệ thống tự động ưu tiên đọc nhiệt độ, độ ẩm qua Modbus RS485 (Holding Registers 0 & 1)
// Nếu mất tín hiệu RS485: Hệ thống tự động kích hoạt cơ chế Fail-Safe chuyển về đọc DHT20
#define USE_MODBUS_RS485    1         // 1: Bật Modbus RS485, 0: Tắt (chỉ DHT20/DHT)
#define RS485_RX_PIN        44        // Chân RX của ESP32 (nối TXD/RO của module MAX485)
#define RS485_TX_PIN        43        // Chân TX của ESP32 (nối RXD/DI của module MAX485)
#define RS485_BAUDRATE      9600      // Tốc độ Baud mặc định (9600 bps)
#define RS485_SLAVE_ID      4         // Địa chỉ Slave ID của cảm biến (4)
#define RS485_REG_HUMI      0         // Địa chỉ thanh ghi Độ ẩm (Function code 03)
#define RS485_REG_TEMP      1         // Địa chỉ thanh ghi Nhiệt độ (Function code 03)

// ============================================================================
// ĐỊNH NGHĨA CHÂN NGOẠI VI (PINOUT)
// ============================================================================
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_YOLO_UNO)
  // ── CẤU HÌNH DÀNH CHO KIT YOLO:UNO (ESP32-S3) ──
  // I2C Cổng ĐỎ Grove: Dùng chung cho DHT20 (0x38), LCD 1602 (0x21 / 0x27)
  #define SDA_PIN             11          // Dây trắng trên các cổng Đỏ I2C1..I2C4
  #define SCL_PIN             12          // Dây vàng trên các cổng Đỏ I2C1..I2C4

  #define NEO_PIN             45          // LED RGB NeoPixel on-board Yolo:UNO
  #define NEO_COUNT           1           // 1 hạt LED RGB tích hợp trên board
  #define LED_PIN             48          // LED đơn on-board Yolo:UNO (GPIO 48 / D13)

  // Thiết bị chấp hành: Khớp đúng cổng Trắng Grove và cọc chân D trên Yolo:UNO
  #define RELAY_PUMP_PIN      7           // Rơ-le Bơm phun sương: Chân D4 trên cổng Trắng D4-D3 (GPIO 7)
  #define FAN_S1_PIN          8           // Quạt đối lưu phòng sạch: Chân D5 trên cổng Trắng D6-D5 (GPIO 8)

  // Cảm biến Analog: Khớp đúng cổng Vàng Grove A1-A0 (ADC1 hoạt động tốt khi có WiFi)
  #define SOIL_MOISTURE_PIN   1           // Cảm biến Độ ẩm phôi/giá thể: Chân A0 (GPIO 1 - ADC1_CH0)
  #define LIGHT_SENSOR_PIN    2           // Cảm biến Quang trở LDR chu kỳ sáng: Chân A1 (GPIO 2 - ADC1_CH1)

  // Nút bấm vật lý (Pullup nội bộ)
  #define BTN_MODE_PIN        17          // Nút Mode: Chân D8 trên cổng Trắng D8-D7 (GPIO 17)
  #define BTN_PUMP_PIN        18          // Nút Phun sương: Chân D9 trên cổng Trắng D10-D9 (GPIO 18)
  #define BTN_FAN_PIN         21          // Nút Quạt: Chân D10 trên cổng Trắng D10-D9 (GPIO 21)

  #define LCD_ADDRESS         0x21        // Địa chỉ I2C màn hình LCD trên Yolo:UNO
  #define LCD_ADDRESS_ALT     0x27        // Địa chỉ I2C dự phòng nếu dùng module rời
#else
  // ── CẤU HÌNH DỰ PHÒNG CHO ESP32 THƯỜNG (ESP32DEV) ──
  #define SDA_PIN             21
  #define SCL_PIN             22

  #define NEO_PIN             26
  #define NEO_COUNT           4
  #define LED_PIN             2

  #define RELAY_PUMP_PIN      17
  #define FAN_S1_PIN          25

  #define LIGHT_SENSOR_PIN    34
  #define SOIL_MOISTURE_PIN   33

  #define BTN_MODE_PIN        19
  #define BTN_PUMP_PIN        18
  #define BTN_FAN_PIN         5

  #define LCD_ADDRESS         0x27
  #define LCD_ADDRESS_ALT     0x3F
#endif

// ============================================================================
// LCD 16x2 I2C
// ============================================================================
#define LCD_COLS              16
#define LCD_ROWS              2
#define LCD_PAGE_COUNT        4

// ============================================================================
// RTC DS3231 I2C (Địa chỉ 0x68)
// ============================================================================
#define RTC_I2C_ADDRESS       0x68

// ============================================================================
// CẤU HÌNH CẢM BIẾN & HIỆU CHUẨN ADC
// ============================================================================
#define DHT_PIN               SDA_PIN     // Dùng khi USE_DHT20 = 0
#define DHT_TYPE              11          // 11 = DHT11, 22 = DHT22

#define SOIL_ADC_DRY          3900        // Giá thể khô hoàn toàn
#define SOIL_ADC_WET          2200        // Bão hòa ẩm
#define SOIL_ADC_SAMPLES      5

#define LIGHT_ADC_BRIGHT      2000
#define LIGHT_ADC_DARK        50
#define LIGHT_ADC_SAMPLES     5
#define LIGHT_LUX_BRIGHT      1000.0f
#define LIGHT_LUX_DARK        0.0f

// ============================================================================
// LOGIC RƠ-LE
// ============================================================================
#define RELAY_ON              LOW         // Rơ-le active-LOW
#define RELAY_OFF             HIGH

// ============================================================================
// NGƯỠNG ĐIỀU KHIỂN TỰ ĐỘNG (DEFAULT KHI CHƯA CÓ LITTLEFS)
// ============================================================================
#define DEFAULT_TEMP_CRITICAL 36.0f       // Nhiệt độ cực đoan -> LED Đỏ
#define DEFAULT_TEMP_HIGH     32.0f       // Bật quạt
#define DEFAULT_TEMP_LOW      31.0f       // Tắt quạt (vùng trễ Hysteresis 1°C)

#define HUMIDITY_HIGH_THRESHOLD 85.0f
#define HUMIDITY_LOW_THRESHOLD  40.0f

#define DEFAULT_SOIL_DRY      30.0f       // Độ ẩm giá thể dưới 30% -> Bơm phun sương ON
#define DEFAULT_SOIL_WET      65.0f       // Độ ẩm giá thể trên 65% -> Bơm phun sương OFF

#define LIGHT_LOW_THRESHOLD   500.0f
#define LIGHT_HIGH_THRESHOLD  999.0f

// Bảo vệ máy bơm phun sương
#define DEFAULT_PUMP_MAX_MS   (2UL * 60UL * 1000UL) // Tối đa 2 phút mỗi lần phun sương
#define DEFAULT_PUMP_COOLDOWN_MS (30UL * 1000UL)   // Nghỉ 30 giây giữa 2 chu kỳ phun sương

// Độ sáng LED NeoPixel
#define NEO_BRIGHTNESS        100

#endif // CONFIG_H
