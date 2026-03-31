/*
 * config.h - Cấu hình toàn bộ hệ thống Greenhouse IoT
 * Chỉnh sửa file này để thay đổi thông số phần cứng và ngưỡng cảm biến.
 */

// ============================================================================
// CHÂN GPIO (ESP32) - Nguồn 3.3V
// ============================================================================
#define DHT_PIN 27           // DHT11/22 data pin
#define LIGHT_SENSOR_PIN 34  // LDR module (DO digital)
#define SOIL_MOISTURE_PIN 33 // Capacitive soil sensor ADC
#define RELAY_PUMP_PIN 17    // Relay bơm nước (IN1)
#define NEO_PIN 26           // NeoPixel data pin
#define NEO_COUNT 4          // Số LED NeoPixel
#define FAN_S1_PIN 25        // Fan Yolo S1 (quay thuận)
#define BTN_MODE_PIN 19      // Toggle AUTO <-> MANUAL
#define BTN_PUMP_PIN 18      // Toggle Pump  (chỉ MANUAL)
#define BTN_FAN_PIN 5        // Toggle Fan   (chỉ MANUAL)

// ============================================================================
// LCD 16x2 I2C (Nguồn 5V, SDA=GPIO21, SCL=GPIO22)
// ============================================================================
#define LCD_ADDRESS 0x27 // Thử 0x3F nếu không hiển thị
#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_PAGE_COUNT 4 // Tổng số trang màn hình
#define SDA_PIN 21
#define SCL_PIN 22

// ============================================================================
// RTC DS3231 I2C (dùng chung bus SDA=21, SCL=22 với LCD)
// DS3231 địa chỉ mặc định 0x68 — không xung đột với LCD 0x27
// ============================================================================
#define RTC_I2C_ADDRESS 0x68 // Địa chỉ cố định của DS3231

// ============================================================================
// CẤU HÌNH CẢM BIẾN
// ============================================================================
#define DHT_TYPE DHT11 // DHT11 hoặc DHT22

// Calibration ADC đất (capacitive): đo thực tế trước khi dùng
// Cắm sensor vào không khí hoàn toàn khô -> ghi ADC_DRY
// Nhúng sensor vào nước  -> ghi ADC_WET
#define SOIL_ADC_DRY 3900  // ADC khi đất khô hoàn toàn (~80% 4095)
#define SOIL_ADC_WET 2200  // ADC khi ngập nước (~20% 4095)
#define SOIL_ADC_SAMPLES 5 // Số mẫu lấy trung bình (giảm nhiễu)

// Calibration lux từ ADC analog (đo thực tế)
#define LIGHT_ADC_BRIGHT 2000 // ADC khi có ánh sáng
#define LIGHT_ADC_DARK 50     // ADC khi che tối
#define LIGHT_ADC_SAMPLES 5   // Số mẫu trung bình

// DO=LOW (đủ sáng) -> 1000 lux, DO=HIGH (tối) -> 0 lux
#define LIGHT_LUX_BRIGHT 1000.0f
#define LIGHT_LUX_DARK 0.0f

// ============================================================================
// RELAY LOGIC
// ============================================================================
// Relay active-LOW: IN=LOW -> relay bật, IN=HIGH -> relay tắt
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// ============================================================================
// NGƯỠNG ĐIỀU KHIỂN TỰ ĐỘNG
// ============================================================================
// Nhiệt độ (°C)
#define TEMP_CRITICAL 35.0f       // Nguy hiểm cực cao -> LED đỏ
#define TEMP_HIGH_THRESHOLD 32.0f // Bật quạt
#define TEMP_LOW_THRESHOLD 31.0f  // Tắt quạt (hysteresis)

// Độ ẩm không khí (%)
#define HUMIDITY_HIGH_THRESHOLD 85.0f
#define HUMIDITY_LOW_THRESHOLD 40.0f

// Độ ẩm đất (%)
#define SOIL_DRY_THRESHOLD 30.0f // Dưới -> bơm ON
#define SOIL_WET_THRESHOLD 65.0f // Trên  -> bơm OFF

// Ánh sáng (lux giả lập)
#define LIGHT_LOW_THRESHOLD 500.0f  // Dưới -> bật đèn grow
#define LIGHT_HIGH_THRESHOLD 999.0f // Trên -> tắt đèn grow

// ============================================================================
// BẢO VỆ BƠM NƯỚC
// ============================================================================
/*
Đất khô → pumpOn()
    │
    │ Bơm chạy...
    │
    ├── Nếu đất đủ ẩm trước 5 phút → pumpOff() bình thường
    │
    └── Nếu sau 5 phút đất VẪN khô → force pumpOff()
        (bảo vệ bơm không cháy)*/
// Thời gian tối đa bơm được phép chạy liên tục trong 1 lần bật)
#define PUMP_MAX_ON_TIME_MS (2UL * 60UL * 1000UL) // 2 phút tối đa mỗi lần

// Thời gian chờ bắt buộc sau khi bơm tắt, trước khi được phép bơm lần tiếp theo
#define PUMP_COOLDOWN_MS (2UL * 60UL * 1000UL) // 2 phút chờ giữa các lần bơm

// ============================================================================
// NEOPIXEL BRIGHTNESS (0-255)
// ============================================================================
#define NEO_BRIGHTNESS 150 // 0-255 (150 ~ 60%)
