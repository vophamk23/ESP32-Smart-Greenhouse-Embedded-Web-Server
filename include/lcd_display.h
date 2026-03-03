#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include <Wire.h>
#include "LiquidCrystal_I2C.h"

// ==================== KHỞI TẠO ====================
void initLCD();
LiquidCrystal_I2C* getLCD();

// ==================== CÁC TRANG HIỂN THỊ ====================
/*
 * Trang 0: Nhiệt độ & Độ ẩm không khí
 *   Dòng 1: T:25.3C  H:65.2%
 *   Dòng 2: [PUMP:ON] [FAN:OFF]
 *
 * Trang 1: Độ ẩm đất & Ánh sáng
 *   Dòng 1: Soil:45.2%  LUX:
 *   Dòng 2: 1250 lux [LED:ON]
 *
 * Trang 2: Trạng thái thiết bị
 *   Dòng 1: PUMP:ON  FAN:OFF
 *   Dòng 2: LED:GRN  MODE:AUTO
 *
 * Trang 3: Cảnh báo / Thống kê
 *   Dòng 1: Pumps:5  Time:2min
 *   Dòng 2: ALERT: TEMP HIGH
 */
void displayPage_TempHumidity();
void displayPage_SoilLight();
void displayPage_DeviceStatus();
void displayPage_AlertStats();

// ==================== CẬP NHẬT ====================
void updateLCDPage();           // Hiển thị trang hiện tại (g_lcdPage)
void nextLCDPage();             // Chuyển sang trang tiếp theo
void setLCDPage(int page);    // Chuyển trang cụ thể (0-3), reset timer tự động chuyển trang
void showBootScreen();          // Màn hình khởi động

// ==================== HELPER ====================
void lcdPrintPadded(const char* str, int totalWidth);   // In có padding
void lcdPrintBool(bool value, const char* onStr, const char* offStr);

#endif // LCD_DISPLAY_H