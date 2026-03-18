/*
 * lcd_display.cpp - LCD 16x2 I2C 4 trang thông tin nhà kính
 */

#include "lcd_display.h"
#include "config.h"
#include "global_vars.h"
#include <Arduino.h>
#include "sensor_handler.h"

// ==================== ĐỐI TƯỢNG LCD ====================
static LiquidCrystal_I2C s_lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// ==================== CUSTOM CHARACTERS ====================
// QUAN TRỌNG: createChar chỉ có 8 slot (0-7), tối đa 8 ký tự custom
static byte iconDrop[8] = { 0b00100, 0b00100, 0b01110, 0b11111,
                             0b11111, 0b11111, 0b01110, 0b00000 };
static byte iconSun[8]  = { 0b00100, 0b10101, 0b01110, 0b11111,
                             0b01110, 0b10101, 0b00100, 0b00000 };
static byte iconLeaf[8] = { 0b00000, 0b00110, 0b01111, 0b11111,
                             0b11110, 0b11100, 0b01000, 0b00000 };
static byte iconWarn[8] = { 0b00100, 0b01110, 0b01110, 0b11111,
                             0b11111, 0b00000, 0b00100, 0b00000 };

#define ICON_DROP  0
#define ICON_SUN   1
#define ICON_LEAF  2
#define ICON_WARN  3

// ============================================================================
// KHỞI TẠO LCD
// ============================================================================
void initLCD()
{
    s_lcd.begin();
    s_lcd.backlight();
    s_lcd.createChar(ICON_DROP, iconDrop);
    s_lcd.createChar(ICON_SUN,  iconSun);
    s_lcd.createChar(ICON_LEAF, iconLeaf);
    s_lcd.createChar(ICON_WARN, iconWarn);

    showBootScreen();
    Serial.printf("[LCD] 16x2 I2C initialized (addr=0x%02X)\n", LCD_ADDRESS);
}

LiquidCrystal_I2C* getLCD() { return &s_lcd; }

// ============================================================================
// MÀN HÌNH KHỞI ĐỘNG
// ============================================================================
void showBootScreen()
{
    s_lcd.clear();
    s_lcd.setCursor(0, 0);
    s_lcd.write(ICON_LEAF);
    s_lcd.print("Greenhouse IoT ");
    s_lcd.setCursor(0, 1);
    s_lcd.print(" Initializing...");
    delay(2000);
    s_lcd.clear();
}

// ============================================================================
// HELPER: In chuỗi có padding đến đủ 16 ký tự
// Tránh ghost characters khi text ngắn hơn text cũ
// ============================================================================
static void lcdPrint16(int col, int row, const char* str)
{
    s_lcd.setCursor(col, row);
    int len = strlen(str);
    s_lcd.print(str);
    // Pad với space để xóa ký tự cũ
    for (int i = len; i < (LCD_COLS - col); i++)
        s_lcd.print(' ');
}

// ============================================================================
// TRANG 0: NHIỆT ĐỘ & ĐỘ ẨM
// Row0: "Temp:28.5C  Humi:65%  "
// Row1: "FAN:ON  PUMP:OFF" hoặc cảnh báo CRITICAL
// ============================================================================
void displayPage_TempHumidity()
{
    char buf[17];

    // Row 0
    if (g_dhtError)
    {
        lcdPrint16(0, 0, "DHT ERR! ---C   ");
    }
    else
    {
        snprintf(buf, sizeof(buf), "T: %.1fC  H: %.0f%%",
                 g_temperature, g_humidity);
        lcdPrint16(0, 0, buf);
    }

    // Row 1
    if (!g_dhtError && g_temperature >= TEMP_CRITICAL)
    {
        s_lcd.setCursor(0, 1);
        s_lcd.write(ICON_WARN);
        lcdPrint16(1, 1, "TEMP CRITICAL! ");
    }
    else
    {
        DateTime now = getRTCTime();
        snprintf(buf, sizeof(buf), "%02d/%02d/%02d  %02d:%02d",
                 now.day(), now.month(), now.year() % 100,
                 now.hour(), now.minute());
        lcdPrint16(0, 1, buf);
    }
}

// ============================================================================
// TRANG 1: ĐỘ ẨM ĐẤT & ÁNH SÁNG
// Row0: "Soil: 45%      ~" (~ = icon drop nếu đang bơm)
// Row1: "Light: BRIGHT  *" (* = icon sun)
// ============================================================================
void displayPage_SoilLight()
{
    char buf[17];

    // Row 0
    snprintf(buf, sizeof(buf), "Soil  :%.0f%%", g_soilMoisture);
    lcdPrint16(0, 0, buf);
    s_lcd.setCursor(15, 0);
    s_lcd.write(g_pumpState ? ICON_DROP : ' ');

    // Row 1
    if (g_lightError)
    {
        lcdPrint16(0, 1, "Light: ERR      ");
    }
    else
    {
        // Hiển thị "BRIGHT" hoặc "DARK" tùy ngưỡng ánh sáng
        const char* lightStr = (g_lightLux >= LIGHT_LOW_THRESHOLD) ? "BRIGHT" : "DARK  ";
        snprintf(buf, sizeof(buf), "Light :%s", lightStr);
        lcdPrint16(0, 1, buf);
        s_lcd.setCursor(15, 1);
        s_lcd.write(ICON_SUN);
    }
}

// ============================================================================
// TRANG 2: TRẠNG THÁI THIẾT BỊ
// Row0: "PUMP:ON/OF  FAN:ON/OFF" "
// Row1: "LED:GRN  AUTO   "
// ============================================================================
void displayPage_DeviceStatus()
{
    char buf[17];

    // Row 0
    snprintf(buf, sizeof(buf), "Fan:%-3s Pump:%-3s",
             g_fanState  ? "ON"  : "OFF",
             g_pumpState ? "ON"  : "OFF");
    lcdPrint16(0, 0, buf);

    // Xác định tên màu LED
    const char* ledName = "OFF";
    uint8_t r = g_currentLEDColor.r;
    uint8_t g = g_currentLEDColor.g;
    uint8_t b = g_currentLEDColor.b;

    if      (r == 0   && g == 255 && b == 0)   ledName = "Green"; // Good
    else if (r == 255 && g == 0   && b == 0)   ledName = "Red"; // Danger
    else if (r == 255 && g == 200 && b == 0)   ledName = "Yell"; // Warning
    else if (r == 0   && g == 100 && b == 255) ledName = "Blue"; // Watering
    else if (r == 255 && g == 255 && b == 255) ledName = "White"; // GrowLight
    else if (r == 0   && g == 230 && b == 255) ledName = "Cyan"; // Pump+Fan
    else if (r == 200 && g == 0   && b == 100) ledName = "Pink"; // Fan 

    snprintf(buf, sizeof(buf), "LED:%-3s %-4s",
             ledName,
             g_autoMode ? "[AUTO]" : " [MAN]"); 
    lcdPrint16(0, 1, buf);
}

// ============================================================================
// TRANG 3: THỐNG KÊ & CẢNH BÁO
// Row0: "Pumps:5  T:120s "
// Row1: Cảnh báo ưu tiên cao nhất, hoặc "All OK"
// ============================================================================
void displayPage_AlertStats()
{
    char buf[17];

    // Row 0 - thống kê bơm
    unsigned long totalSec = g_totalPumpTime / 1000;

    if (totalSec <= 9999)
    {
        // "Pumps:XX T:XXXXs" = tối đa 16 ký tự
        snprintf(buf, sizeof(buf), "Pumps:%-2lu T:%-4lus",
                 (unsigned long)g_pumpCount, totalSec);
    }
    else
    {
        // Nếu tổng thời gian bơm quá lớn, chỉ hiển thị số lần bơm để tránh tràn số
        snprintf(buf, sizeof(buf), "Pumps:%-2lu T:>9Ks",
                 (unsigned long)g_pumpCount);
    }
    lcdPrint16(0, 0, buf);

    // Row 1 - cảnh báo ưu tiên
    s_lcd.setCursor(0, 1);
    if (!g_dhtError && g_temperature >= TEMP_CRITICAL)
    {
        s_lcd.write(ICON_WARN); lcdPrint16(1, 1, " TEMP CRITICAL!");
    }
    else if (g_alertTemp)
    {
        s_lcd.write(ICON_WARN); lcdPrint16(1, 1, " TEMP HIGH!     ");
    }
    else if (g_alertSoil)
    {
        s_lcd.write(ICON_WARN); lcdPrint16(1, 1, " SOIL DRY!      ");
    }
    else if (g_alertHumidity)
    {
        s_lcd.write(ICON_WARN); lcdPrint16(1, 1, " HUMI ABNORMAL! ");
    }
    else if (g_alertLight)
    {
        s_lcd.write(ICON_WARN); lcdPrint16(1, 1, " LIGHT LOW!     ");
    }
    else if (g_dhtError)
    {
        s_lcd.write(ICON_WARN); lcdPrint16(1, 1, " DHT SENSOR ERR");
    }
    else
    {
        s_lcd.write(ICON_LEAF); lcdPrint16(1, 1, " All systems OK");
    }
}

// ============================================================================
// CẬP NHẬT TRANG HIỆN TẠI (gọi mỗi 500ms)
// Mỗi hàm display tự ghi đè toàn bộ 32 ký tự
// ============================================================================
void updateLCDPage()
{
    switch (g_lcdPage)
    {
        case 0: displayPage_TempHumidity(); break;
        case 1: displayPage_SoilLight();    break;
        case 2: displayPage_DeviceStatus(); break;
        case 3: displayPage_AlertStats();   break;
        default: g_lcdPage = 0;             break;
    }
}

// ============================================================================
// CHUYỂN TRANG KẾ TIẾP
// ============================================================================
void nextLCDPage()
{
    g_lcdPage = (g_lcdPage + 1) % LCD_PAGE_COUNT;
    g_lcdPageTimer = millis();
    s_lcd.clear();  // Clear 1 lần khi chuyển trang là OK
}

void setLCDPage(int page)
{
    if (page < 0 || page >= LCD_PAGE_COUNT) return;
    g_lcdPage = page;
    g_lcdPageTimer = millis();
    s_lcd.clear();
}