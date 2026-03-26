/*
 * button_handler.cpp - Xử lý nút nhấn với debounce (FIXED)
 */

#include "button_handler.h"
#include "actuator_handler.h"
#include "lcd_display.h"
#include "global_vars.h"
#include "config.h"
#include "App_Tasks.h"

// ==================== ĐỐI TƯỢNG NÚT NHẤN ====================
struct Button_t 
{
    uint8_t       pin;
    bool          lastStableState;   // Trạng thái đã được debounce xác nhận
    bool          rawState;          // Trạng thái thô đọc từ GPIO
    unsigned long lastChangeTime;    // Thời điểm rawState thay đổi lần cuối
};

static Button_t s_btnMode = { BTN_MODE_PIN, HIGH, HIGH, 0 };
static Button_t s_btnPump = { BTN_PUMP_PIN, HIGH, HIGH, 0 };
static Button_t s_btnFan  = { BTN_FAN_PIN,  HIGH, HIGH, 0 };

// Khởi tạo chân và trạng thái nút
void initButtons()
{
    pinMode(BTN_MODE_PIN, INPUT_PULLUP);
    pinMode(BTN_PUMP_PIN, INPUT_PULLUP);
    pinMode(BTN_FAN_PIN,  INPUT_PULLUP);
    Serial.printf("[BTN] Initialized: MODE=GPIO%d, PUMP=GPIO%d, FAN=GPIO%d\n",
                  BTN_MODE_PIN, BTN_PUMP_PIN, BTN_FAN_PIN);
}

// ============================================================================
// HÀM DEBOUNCE NỘI BỘ 
// Trả về true nếu phát hiện FALLING edge (HIGH->LOW = nhấn nút) sau debounce.
// ============================================================================
static bool debounce(Button_t &btn)
{
    bool currentRaw = digitalRead(btn.pin);

    // Tín hiệu thay đổi so với lần đọc thô trước -> reset timer
    if (currentRaw != btn.rawState)
    {
        btn.rawState       = currentRaw;
        btn.lastChangeTime = millis();
        return false;
    }

    // Tín hiệu ổn định đủ lâu VÀ khác với trạng thái đã xác nhận trước đó
    if ((millis() - btn.lastChangeTime) >= BTN_DEBOUNCE_MS &&
        currentRaw != btn.lastStableState)             
    {
        bool prevStable     = btn.lastStableState;
        btn.lastStableState = currentRaw;               // Xác nhận trạng thái mới

        // FALLING edge: HIGH -> LOW = nhấn nút
        if (prevStable == HIGH && currentRaw == LOW)
            return true;
    }

    return false;
}

// ============================================================================
// TASK SCAN NÚT NHẤN (gọi mỗi BTN_DEBOUNCE_TICKS)
// ============================================================================
void Task_ScanButtons()
{
    // ---- BTN_MODE: luôn hoạt động ở cả 2 chế độ ----
    if (debounce(s_btnMode))
    {
        g_autoMode = !g_autoMode;
        if (g_autoMode)
            g_pumpManual = false; // Clear khi về AUTO để auto control hoạt động bình thường
        Serial.printf("[BTN] MODE -> %s\n", g_autoMode ? "AUTO" : "MANUAL");
        setLCDPage(2);  // Nhảy sang trang Device Status để thấy rõ mode
    }

    // ---- BTN_PUMP: chỉ hoạt động ở chế độ MANUAL ----
    if (debounce(s_btnPump))
    {
        if (!g_autoMode)
        {
            if (g_pumpState)
            {
                pumpOff();
                Serial.println("[BTN] MANUAL: Pump OFF");
            }
            else
            {
                g_pumpManual = true;    // Đánh dấu để autoControl không can thiệp
                pumpOn();
                Serial.println("[BTN] MANUAL: Pump ON");
            }
        }
        else
        {
            Serial.println("[BTN] PUMP ignored - switch to MANUAL first (BTN_MODE)");
        }
    }

    // ---- BTN_FAN: chỉ hoạt động ở chế độ MANUAL ----
    if (debounce(s_btnFan))
    {
        if (!g_autoMode)
        {
            if (g_fanState)
            {
                fanOff();
                Serial.println("[BTN] MANUAL: Fan OFF");
            }
            else
            {
                fanOn();
                Serial.println("[BTN] MANUAL: Fan ON");
            }
        }
        else
        {
            Serial.println("[BTN] FAN ignored - switch to MANUAL first (BTN_MODE)");
        }
    }
}