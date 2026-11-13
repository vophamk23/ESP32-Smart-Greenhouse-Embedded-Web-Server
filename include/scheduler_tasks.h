/*
 * scheduler_tasks.h - Khai báo các tác vụ cho Cooperative Scheduler & Edge AI
 */
#ifndef SCHEDULER_TASKS_H
#define SCHEDULER_TASKS_H

#include <Arduino.h>
#include "scheduler.h"
#include "config.h"

#define TIMER_TICK_MS 10 // Scheduler tick mỗi 10ms

// ==================== TIMING (MS) ====================
#define TASK_SENSOR_READ_DELAY_MS      500       // Đọc cảm biến mỗi 0.5 giây
#define TASK_SOIL_READ_DELAY_MS        1000      // Đọc độ ẩm giá thể mỗi 1 giây
#define TASK_LCD_UPDATE_DELAY_MS       500       // Cập nhật LCD mỗi 500ms
#define TASK_LCD_PAGE_SWITCH_DELAY_MS  5000      // Tự chuyển trang LCD mỗi 5 giây
#define TASK_AUTO_CONTROL_DELAY_MS     1000      // Chạy logic điều khiển mỗi 1 giây
#define TASK_LED_UPDATE_DELAY_MS       500       // Cập nhật màu LED mỗi 500ms
#define TASK_ALERT_CHECK_DELAY_MS      1000      // Kiểm tra cảnh báo mỗi 1 giây
#define TASK_HEARTBEAT_DELAY_MS        15000     // Heartbeat Serial in bảng trạng thái mỗi 15 giây
#define TASK_PUMP_WATCHDOG_DELAY_MS    1000      // Giám sát bơm mỗi 1 giây
#define BTN_DEBOUNCE_MS                50        // Debounce mỗi 50ms

#define TASK_TELEMETRY_DELAY_MS        3000      // Gửi telemetry lên CoreIOT mỗi 3 giây
#define TASK_RPC_POLL_DELAY_MS         200       // Poll lệnh RPC từ cloud mỗi 200ms
#define TASK_SCHEDULE_CHECK_DELAY_MS   5000      // Kiểm tra lịch phun sương mỗi 5 giây
#define TASK_AI_INFERENCE_DELAY_MS     3000      // Chạy suy luận AI TinyML mỗi 3 giây

// ==================== TIMING IN TICKS ====================
#define TASK_SENSOR_READ_TICKS     (TASK_SENSOR_READ_DELAY_MS / TIMER_TICK_MS)
#define TASK_SOIL_READ_TICKS       (TASK_SOIL_READ_DELAY_MS / TIMER_TICK_MS)
#define TASK_LCD_UPDATE_TICKS      (TASK_LCD_UPDATE_DELAY_MS / TIMER_TICK_MS)
#define TASK_LCD_PAGE_SWITCH_TICKS (TASK_LCD_PAGE_SWITCH_DELAY_MS / TIMER_TICK_MS)
#define TASK_AUTO_CONTROL_TICKS    (TASK_AUTO_CONTROL_DELAY_MS / TIMER_TICK_MS)
#define TASK_LED_UPDATE_TICKS      (TASK_LED_UPDATE_DELAY_MS / TIMER_TICK_MS)
#define TASK_ALERT_CHECK_TICKS     (TASK_ALERT_CHECK_DELAY_MS / TIMER_TICK_MS)
#define TASK_HEARTBEAT_TICKS       (TASK_HEARTBEAT_DELAY_MS / TIMER_TICK_MS)
#define TASK_PUMP_WATCHDOG_TICKS   (TASK_PUMP_WATCHDOG_DELAY_MS / TIMER_TICK_MS)
#define BTN_DEBOUNCE_TICKS         (BTN_DEBOUNCE_MS / TIMER_TICK_MS)
#define TASK_TELEMETRY_TICKS       (TASK_TELEMETRY_DELAY_MS / TIMER_TICK_MS)
#define TASK_RPC_POLL_TICKS        (TASK_RPC_POLL_DELAY_MS / TIMER_TICK_MS)
#define TASK_SCHEDULE_CHECK_TICKS  (TASK_SCHEDULE_CHECK_DELAY_MS / TIMER_TICK_MS)
#define TASK_AI_INFERENCE_TICKS    (TASK_AI_INFERENCE_DELAY_MS / TIMER_TICK_MS)

// ==================== TASK PROTOTYPES ====================
void Task_ReadAirSensors(void);    // Đọc nhiệt độ, độ ẩm (Modbus RS485 / DHT20 fallback) & LDR
void Task_ReadSoilSensor(void);    // Đọc độ ẩm giá thể/phôi
void Task_UpdateLCD(void);         // Cập nhật hiển thị LCD
void Task_AutoSwitchLCDPage(void); // Tự động chuyển trang LCD
void Task_AutoControl(void);       // Logic điều khiển tự động (phun sương + quạt đối lưu)
void Task_UpdateLEDStatus(void);   // Cập nhật màu LED theo trạng thái & AI
void Task_CheckAlerts(void);       // Kiểm tra và cập nhật cảnh báo
void Task_Heartbeat(void);         // In trạng thái hệ thống ra Serial
void Task_PumpWatchdog(void);      // Bảo vệ bơm phun sương không chạy quá lâu

void Task_SendTelemetry(void);     // Đóng gói và gửi data vào IoT queue
void Task_ProcessRpc(void);        // Thực thi lệnh điều khiển từ cloud
void Task_CheckSchedule(void);     // Kiểm tra lịch phun sương chu kỳ
void Task_TinyML_Inference(void);  // Suy luận AI TinyML & Ra quyết định thông minh

#endif // SCHEDULER_TASKS_H
