/*
 * actuator_handler.cpp - Điều khiển Relay bơm/quạt và NeoPixel LED
 */

#include "actuator_handler.h"
#include "config.h"
#include "global_vars.h"

// ==================== ĐỐI TƯỢNG NEOPIXEL ====================
static Adafruit_NeoPixel s_neoPixel(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================================
// KHỞI TẠO
// ============================================================================
void initActuators()
{
    // ---- Relay bơm ----
    pinMode(RELAY_PUMP_PIN, OUTPUT);
    digitalWrite(RELAY_PUMP_PIN, RELAY_OFF);
 // ---- Fan Yolo: chỉ S1 ----
    pinMode(FAN_S1_PIN, OUTPUT);
    digitalWrite(FAN_S1_PIN, LOW);  // Dừng khi khởi động
    Serial.printf("[ACT] Fan Yolo init: S1=GPIO%d only\n", FAN_S1_PIN);

    // ---- NeoPixel ----
    s_neoPixel.begin();
    s_neoPixel.setBrightness(NEO_BRIGHTNESS);
    s_neoPixel.clear();
    s_neoPixel.show();
    Serial.printf("[ACT] NeoPixel init: %d LEDs, GPIO%d, brightness=%d\n",
                  NEO_COUNT, NEO_PIN, NEO_BRIGHTNESS);
}

// ============================================================================
// BƠM NƯỚC
// ============================================================================
// Bật bơm nếu chưa bật, kiểm tra cooldown và cập nhật trạng thái
void pumpOn()
{
    if (g_pumpState) return;

    // Cooldown: bỏ qua nếu MANUAL hoặc do LỊCH TƯỚI kích hoạt
    if (!g_pumpManual && !g_scheduleTriggered && g_pumpCooldown)
    {
        unsigned long elapsed = millis() - g_pumpLastOffTime;
        if (elapsed < PUMP_COOLDOWN_MS)
        {
            Serial.printf("[PUMP] Cooldown active (%lus/%lus) - skip\n",
                          elapsed / 1000,
                          (unsigned long)(PUMP_COOLDOWN_MS / 1000));
            return;
        }
        g_pumpCooldown = false;
    }

    digitalWrite(RELAY_PUMP_PIN, RELAY_ON); 
    g_pumpState     = true;
    g_pumpStartTime = millis();
    g_pumpCount++;
    Serial.printf("[PUMP] ON - count=%lu, manual=%s\n",
                  (unsigned long)g_pumpCount,
                  g_pumpManual ? "YES" : "NO");
}

// Tắt bơm, cập nhật thời gian bơm và cooldown nếu cần
void pumpOff()
{
    if (!g_pumpState) return;

    unsigned long duration = millis() - g_pumpStartTime;
    g_totalPumpTime   += duration;
    g_pumpLastOffTime  = millis();

    // Chỉ bật cooldown khi ở chế độ AUTO
    // Manual không cần cooldown vì người dùng tự kiểm soát
    if (!g_pumpManual)
        g_pumpCooldown = true;
    else
        g_pumpCooldown = false;  // Manual → không cooldown

    g_pumpManual = false;
    digitalWrite(RELAY_PUMP_PIN, RELAY_OFF);
    g_pumpState = false;
    Serial.printf("[PUMP] OFF - ran %lus, total=%lus, cooldown=%s\n",
                  duration / 1000, g_totalPumpTime / 1000,
                  g_pumpCooldown ? "YES" : "NO");
}

bool isPumpOn() { return g_pumpState; }

// ============================================================================
// QUẠT
// ============================================================================
void fanOn()
{
    if (g_fanState) return;
    digitalWrite(FAN_S1_PIN, HIGH); // S1=HIGH → quay thuận
    g_fanState = true;
    Serial.println("[FAN] ON");
}

void fanOff()
{
    if (!g_fanState) return;
    digitalWrite(FAN_S1_PIN, LOW);  // S1=LOW → dừng
    g_fanState = false;
    Serial.println("[FAN] OFF");
}

bool isFanOn() { return g_fanState; }

// ============================================================================
// NEOPIXEL
// ============================================================================
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b)
{
    // Chỉ update nếu màu thực sự thay đổi
    if (g_currentLEDColor.r == r &&
        g_currentLEDColor.g == g &&
        g_currentLEDColor.b == b)
        return;

    for (int i = 0; i < NEO_COUNT; i++)
        s_neoPixel.setPixelColor(i, s_neoPixel.Color(r, g, b));
    s_neoPixel.show();
    g_currentLEDColor = {r, g, b};
}

void setLED(uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b)
{
    if (ledIndex < 1 || ledIndex > NEO_COUNT) return;
    s_neoPixel.setPixelColor(ledIndex - 1, s_neoPixel.Color(r, g, b));
    s_neoPixel.show();
}

void setLEDStatus(RGBColor_t color)
{
    setAllLEDs(color.r, color.g, color.b);
}

void turnOffAllLEDs()
{
    if (g_currentLEDColor.r == 0 &&
        g_currentLEDColor.g == 0 &&
        g_currentLEDColor.b == 0)
        return; // Đã tắt rồi

    s_neoPixel.clear();
    s_neoPixel.show();
    g_currentLEDColor = {0, 0, 0};
}

// Các màu trạng thái chuẩn (có thể tùy chỉnh thêm nếu muốn)
void ledColorGood()      { setAllLEDs(0,   255, 0);   }   // Xanh lá
void ledColorWarning()   { setAllLEDs(255, 200, 0);   }   // Vàng cam
void ledColorDanger()    { setAllLEDs(255, 0,   0);   }   // Đỏ
void ledColorWatering()  { setAllLEDs(0,   100, 255); }   // Xanh dương (tưới nước)
void ledColorFanOn()     { setAllLEDs(200, 0, 100); }   // Hồng 
void ledColorGrowLight() { setAllLEDs(255, 255, 255); }   // Trắng (grow light)

void ledColorPumpFan()   { setAllLEDs(0, 230, 255); }   // Cyan (pump + fan)
void ledColorNight()     { turnOffAllLEDs(); }

// ============================================================================
// CẬP NHẬT CẢNH BÁO
// ============================================================================
void updateAlerts()
{
    // Chỉ cảnh báo nhiệt độ khi DHT hoạt động bình thường
    g_alertTemp     = (!g_dhtError && g_temperature >= TEMP_HIGH_THRESHOLD);
    g_alertHumidity = (!g_dhtError &&
                       (g_humidity < HUMIDITY_LOW_THRESHOLD ||
                        g_humidity > HUMIDITY_HIGH_THRESHOLD));
    g_alertSoil     = (g_soilMoisture < SOIL_DRY_THRESHOLD);
    // Cảnh báo ánh sáng chỉ khi không đang bật grow light
    g_alertLight    = (g_lightLux < LIGHT_LOW_THRESHOLD && !g_ledGrowState);
}

// ============================================================================
// TỰ ĐỘNG ĐIỀU KHIỂN BƠM
// ============================================================================
void autoControlPump()
{
    if (!g_autoMode) return;
    if (g_pumpManual) return;   // Bơm đang do lệnh manual, không can thiệp

    if (!g_pumpState)
    {
        // Điều kiện bật bơm: đất quá khô
        if (g_soilMoisture < SOIL_DRY_THRESHOLD)
            pumpOn();
    }
    else
    {
        // Điều kiện tắt bơm: đất đủ ẩm HOẶC đã bơm quá lâu
        if (g_soilMoisture >= SOIL_WET_THRESHOLD)
        {
            Serial.println("[AUTO] Soil wet enough -> pump OFF");
            pumpOff();
        }
        else if (millis() - g_pumpStartTime >= PUMP_MAX_ON_TIME_MS)
        {
            Serial.println("[AUTO] Pump max time exceeded -> force OFF");
            pumpOff();
        }
    }
}

// ============================================================================
// TỰ ĐỘNG ĐIỀU KHIỂN QUẠT (hysteresis tránh flicker)
// ============================================================================
void autoControlFan()
{
    if (!g_autoMode) return;
    if (g_dhtError) return;     // Không điều khiển nếu cảm biến lỗi

    if (!g_fanState && g_temperature >= TEMP_HIGH_THRESHOLD)
    {
        fanOn();
    }
    else if (g_fanState && g_temperature < TEMP_LOW_THRESHOLD)
    {
        fanOff();
    }
}

// ============================================================================
// TỰ ĐỘNG ĐIỀU KHIỂN LED
// Ưu tiên: 1. Cảnh báo nhiệt độ > 35°C (đỏ) - ưu tiên cao nhất
//           2. Pump + Fan cùng bật → cyan
//           3. Chỉ pump → xanh dương
//           4. Chỉ fan bật thủ công → tím
//           5. Thiếu sáng → grow light trắng
//           6. Cảnh báo nhẹ (nhiệt độ cao, ẩm thấp, đất khô) → vàng cam
//           7. Bình thường → xanh lá
// ============================================================================
void autoControlLED()
{
    // 1. CRITICAL - ưu tiên tuyệt đối dù AUTO hay MANUAL
    if (!g_dhtError && g_temperature >= TEMP_CRITICAL)
    {
        ledColorDanger();
        return;
    }

    // 2. Pump + Fan cùng bật → cyan
    if (g_pumpState && g_fanState)
    {
        ledColorPumpFan();
        return;
    }

    // 3. Chỉ pump → xanh dương
    if (g_pumpState)
    {
        ledColorWatering();
        return;
    }

    // 4. Chỉ fan bật thủ công → tím
    if (g_fanState && !g_autoMode)
    {
        ledColorFanOn();
        return;
    }

    // 5. Thiếu sáng → grow light
    if (g_lightLux < LIGHT_LOW_THRESHOLD)
    {
        g_ledGrowState = true;
        ledColorGrowLight();
        return;
    }

    // 6. Đủ sáng → tắt grow light flag
    g_ledGrowState = false;

    // 7. Cảnh báo nhẹ
    if (g_alertTemp || g_alertHumidity || g_alertSoil)
    {
        ledColorWarning();
        return;
    }

    // 8. Bình thường
    ledColorGood();
}