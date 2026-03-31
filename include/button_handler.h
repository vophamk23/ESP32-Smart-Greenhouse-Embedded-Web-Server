/*
 * button_handler.h - Xử lý nút nhấn với debounce
 *
 * Sơ đồ nút:
 *   BTN_MODE -> BTN_MODE_PIN : Toggle AUTO <-> MANUAL
 *   BTN_PUMP -> BTN_PUMP_PIN : Toggle Pump  (chỉ hoạt động ở MANUAL)
 *   BTN_FAN  -> BTN_FAN_PIN  : Toggle Fan   (chỉ hoạt động ở MANUAL)
 *
 * Tất cả cấu hình pin và timing nằm trong config.h
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "lcd_display.h"

// Khai báo hàm xử lý nút nhấn
void initButtons();      // Gọi trong setup()
void Task_ScanButtons(); // Đăng ký vào scheduler, period BTN_DEBOUNCE_MS/TIMER_TICK_MS ticks

#endif // BUTTON_HANDLER_H