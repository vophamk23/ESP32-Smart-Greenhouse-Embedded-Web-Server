#ifndef ACTUATOR_HANDLER_H
#define ACTUATOR_HANDLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "global_vars.h"

// ==================== KHỞI TẠO ====================
void initActuators();

// ==================== ĐIỀU KHIỂN BƠM PHUN SƯƠNG ====================
void pumpOn();
void pumpOff();
bool isPumpOn();

// ==================== ĐIỀU KHIỂN QUẠT ====================
void fanOn();
void fanOff();
bool isFanOn();

// ==================== ĐIỀU KHIỂN LED RGB ====================
/*
 * Màu LED biểu thị trạng thái môi trường:
 *   XANH LÁ  (0,255,0)    : Tất cả điều kiện tốt
 *   XANH DƯƠNG(0,128,255) : Thiếu ánh sáng - đang bù bằng LED
 *   VÀNG     (255,200,0)  : Cảnh báo nhiệt độ cao / độ ẩm thấp
 *   ĐỎ       (255,0,0)    : Nguy hiểm - nhiệt độ cực cao
 *   TÍM      (128,0,255)  : Đang phun sương tạo ẩm
 *   TRẮNG    (255,255,255): Bổ sung ánh sáng chu kỳ sinh học
 *   TẮT      (0,0,0)      : Hệ thống off hoặc ban đêm
 */
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b);
void setLED(uint8_t ledIndex, uint8_t r, uint8_t g, uint8_t b);  // ledIndex 1-4
void setLEDStatus(RGBColor_t color);
void turnOffAllLEDs();

// ==================== MÀNG TRẠNG THÁI LED ====================
// Màu preset theo tình huống
void ledColorGood();        // Xanh lá - môi trường tốt
void ledColorWarning();     // Vàng - cảnh báo
void ledColorDanger();      // Đỏ - nguy hiểm
void ledColorWatering();    // Tím - đang phun sương
void ledColorGrowLight();   // Trắng - đèn chiếu sáng chu kỳ sinh học
void ledColorNight();       // Tắt - chế độ tối

// ==================== LOGIC ĐIỀU KHIỂN TỰ ĐỘNG ====================
void autoControlPump();     // Tự động điều khiển bơm phun sương dựa trên độ ẩm giá thể
void autoControlFan();      // Tự động điều khiển quạt đối lưu dựa trên nhiệt độ
void autoControlLED();      // Tự động điều khiển đèn dựa trên ánh sáng & trạng thái
void updateAlerts();        // Cập nhật cờ cảnh báo

#endif // ACTUATOR_HANDLER_H