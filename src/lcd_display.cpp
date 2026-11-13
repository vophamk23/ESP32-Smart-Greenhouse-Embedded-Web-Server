/*
 * lcd_display.cpp - LCD 16x2 I2C 4 trang thông tin Cleanroom Bio-Chamber & Edge AI
 */

#include "lcd_display.h"
#include "config.h"
#include "global_vars.h"
#include <Arduino.h>
#include "sensor_handler.h"
#include <Wire.h>

// ==================== ĐỐI TƯỢNG LCD ====================
static LiquidCrystal_I2C s_lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// ==================== CUSTOM CHARACTERS ====================
static byte iconDrop[8] = {0b00100, 0b00100, 0b01110, 0b11111,
                           0b11111, 0b11111, 0b01110, 0b00000};
static byte iconSun[8]  = {0b00100, 0b10101, 0b01110, 0b11111,
                           0b01110, 0b10101, 0b00100, 0b00000};
static byte iconLeaf[8] = {0b00000, 0b00110, 0b01111, 0b11111,
                           0b11110, 0b11100, 0b01000, 0b00000};
static byte iconWarn[8] = {0b00100, 0b01110, 0b01110, 0b11111,
                           0b11111, 0b00000, 0b00100, 0b00000};

#define ICON_DROP 0
#define ICON_SUN  1
#define ICON_LEAF 2
#define ICON_WARN 3

// ============================================================================
// KHỞI TẠO LCD
// ============================================================================
void initLCD()
{
    Wire.begin(SDA_PIN, SCL_PIN);

    // Tự động quét địa chỉ I2C của màn hình LCD (0x21 hoặc 0x27)
    uint8_t targetAddr = LCD_ADDRESS;
    Wire.beginTransmission(LCD_ADDRESS);
    if (Wire.endTransmission() != 0)
    {
        Wire.beginTransmission(LCD_ADDRESS_ALT);
        if (Wire.endTransmission() == 0)
        {
            targetAddr = LCD_ADDRESS_ALT;
        }
    }

    s_lcd = LiquidCrystal_I2C(targetAddr, LCD_COLS, LCD_ROWS);
    s_lcd.begin();
    s_lcd.backlight();
    s_lcd.createChar(ICON_DROP, iconDrop);
    s_lcd.createChar(ICON_SUN, iconSun);
    s_lcd.createChar(ICON_LEAF, iconLeaf);
    s_lcd.createChar(ICON_WARN, iconWarn);

    showBootScreen();
    Serial.printf("[LCD] 16x2 I2C initialized on addr=0x%02X (SDA=%d, SCL=%d)\n",
                  targetAddr, SDA_PIN, SCL_PIN);
}

LiquidCrystal_I2C *getLCD() { return &s_lcd; }

// ============================================================================
// MÀN HÌNH KHỞI ĐỘNG
// ============================================================================
void showBootScreen()
{
    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.write(ICON_LEAF);
    s_lcd.print(" Yolo:UNO S3 ");
    s_lcd.setCursor(0, 1);
    s_lcd.print("IoT & Edge AI...");
    delay(1500);
    s_lcd.clear();
}

// ============================================================================
// HELPER: In chuỗi có padding đến đủ 16 ký tự
// ============================================================================
static void lcdPrint16(int col, int row, const char *str)
{
    s_lcd.setCursor(col, row);
    int len = strlen(str);
    s_lcd.print(str);
    for (int i = len; i < (LCD_COLS - col); i++)
        s_lcd.print(' ');
}

// ============================================================================
// TRANG 0: NHIỆT ĐỘ & ĐỘ ẨM
// ============================================================================
void displayPage_TempHumidity()
{
    char buf[17];

    // Row 0
    if (g_dhtError)
    {
        lcdPrint16(0, 0, "DHT20 ERR! ---C ");
    }
    else
    {
        snprintf(buf, sizeof(buf), "T: %.1fC  H: %.0f%%", g_temperature, g_humidity);
        lcdPrint16(0, 0, buf);
    }

    // Row 1
    if (!g_dhtError && g_temperature >= DEFAULT_TEMP_CRITICAL)
    {
        s_lcd.write(ICON_WARN);
        lcdPrint16(1, 1, " TEMP CRITICAL!");
    }
    else
    {
        snprintf(buf, sizeof(buf), "FAN:%-3s MIST:%-3s",
                 g_fanState ? "ON" : "OFF",
                 g_pumpState ? "ON" : "OFF");
        lcdPrint16(0, 1, buf);
    }
}

// ============================================================================
// TRANG 1: ĐỘ ẨM GIÁ THỂ & ÁNH SÁNG CHU KỲ SINH HỌC
// ============================================================================
void displayPage_SoilLight()
{
    char buf[17];

    // Row 0: Độ ẩm giá thể / phôi nuôi cấy
    s_lcd.setCursor(0, 0);
    s_lcd.write(ICON_DROP);
    snprintf(buf, sizeof(buf), " Sub : %3.0f%%   ", g_soilMoisture);
    lcdPrint16(1, 0, buf);

    // Row 1: Ánh sáng
    s_lcd.setCursor(0, 1);
    s_lcd.write(ICON_SUN);
    snprintf(buf, sizeof(buf), " Lux: %4.0flx  ", g_lightLux);
    lcdPrint16(1, 1, buf);
}

// ============================================================================
// TRANG 2: TRẠNG THÁI THIẾT BỊ & CHẾ ĐỘ
// ============================================================================
void displayPage_DeviceStatus()
{
    char buf[32];

    snprintf(buf, sizeof(buf), "Fan:%-3s Mist:%-3s",
             g_fanState ? "ON" : "OFF",
             g_pumpState ? "ON" : "OFF");
    lcdPrint16(0, 0, buf);

    snprintf(buf, sizeof(buf), "Mode:%-4s   [OK]",
             g_autoMode ? "AUTO" : "MAN");
    lcdPrint16(0, 1, buf);
}

// ============================================================================
// TRANG 3: TRÍ TUỆ NHÂN TẠO TINYML & ĐÁNH GIÁ VI KHÍ HẬU
// ============================================================================
void displayPage_AlertStats()
{
    char buf[32];

    // Row 0 - Trạng thái AI
    snprintf(buf, sizeof(buf), "AI:%-13.13s", g_aiStateStr);
    lcdPrint16(0, 0, buf);

    // Row 1 - Độ tin cậy & Độ trễ suy luận
    snprintf(buf, sizeof(buf), "C:%.0f%% Lat:%3luu",
             g_aiConfidence, (unsigned long)g_aiInferenceTimeUs);
    lcdPrint16(0, 1, buf);
}

// ============================================================================
// CẬP NHẬT TRANG HIỆN TẠI (gọi mỗi 500ms)
// ============================================================================
void updateLCDPage()
{
    switch (g_lcdPage)
    {
    case 0:
        displayPage_TempHumidity();
        break;
    case 1:
        displayPage_SoilLight();
        break;
    case 2:
        displayPage_DeviceStatus();
        break;
    case 3:
        displayPage_AlertStats();
        break;
    default:
        g_lcdPage = 0;
        break;
    }
}

// ============================================================================
// CHUYỂN TRANG
// ============================================================================
void nextLCDPage()
{
    g_lcdPage = (g_lcdPage + 1) % LCD_PAGE_COUNT;
    g_lcdPageTimer = millis();
    s_lcd.clear();
}

void setLCDPage(int page)
{
    if (page < 0 || page >= LCD_PAGE_COUNT)
        return;
    g_lcdPage = page;
    g_lcdPageTimer = millis();
    s_lcd.clear();
}
