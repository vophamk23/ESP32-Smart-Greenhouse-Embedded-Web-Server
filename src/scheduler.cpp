/*
 * scheduler.cpp - ESP32 Hardware Timer Implementation
 * Tái sử dụng từ dự án Smart Clock
 */
#include "scheduler.h"

sTask   SCH_tasks_G[SCH_MAX_TASKS];
uint8_t Error_code_G = 0;
uint8_t MARKING[SCH_MAX_TASKS];
uint32_t task_count  = 0;
uint32_t elapsed_time = 0;

hw_timer_t *timer    = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static void SCH_Update_Marking(void);

// Timer ISR: cập nhật scheduler mỗi 10ms
void IRAM_ATTR onTimer()
{
    portENTER_CRITICAL_ISR(&timerMux);
    SCH_Update();
    portEXIT_CRITICAL_ISR(&timerMux);
}

// Khởi tạo timer để gọi onTimer() mỗi 10ms
void SCH_Init_Timer(void)
{
    timer = timerBegin(0, 80, true);           // 80MHz / 80 = 1MHz
   timerAttachInterrupt(timer, &onTimer, false);
    timerAlarmWrite(timer, TIMER_TICK_MS * 1000, true); // 10ms
    timerAlarmEnable(timer);
    Serial.println("[SCH] Timer 10ms started");
}

// Khởi tạo scheduler: xóa tất cả task, reset biến đếm
void SCH_Init(void)
{
    for (uint32_t i = 0; i < SCH_MAX_TASKS; i++)
    {
        SCH_tasks_G[i].pTask  = 0x0000;
        SCH_tasks_G[i].Delay  = 0;
        SCH_tasks_G[i].Period = 0;
        SCH_tasks_G[i].RunMe  = 0;
        SCH_tasks_G[i].TaskID = 0;
        MARKING[i] = 0;
    }
    task_count   = 0;
    elapsed_time = 0;
    Error_code_G = 0;
}

// Thêm task mới vào scheduler, trả về TaskID hoặc NO_TASK_ID nếu thất bại
uint32_t SCH_Add_Task(void (*pFunction)(), uint32_t DELAY, uint32_t PERIOD)
{
    if (task_count == 0)
    {
        SCH_tasks_G[0].pTask  = pFunction;
        SCH_tasks_G[0].Delay  = DELAY;
        SCH_tasks_G[0].Period = PERIOD;
        SCH_tasks_G[0].RunMe  = 0;
        SCH_tasks_G[0].TaskID = 0;
        MARKING[0] = 1;
        elapsed_time = 0;
        task_count++;
        return 0;
    }
    if (task_count >= SCH_MAX_TASKS)
    {
        Error_code_G = ERROR_SCH_TOO_MANY_TASKS;
        return SCH_MAX_TASKS;
    }
    uint32_t insert_index = 0;
    for (uint32_t i = 0; i < task_count; i++)
    {
        if (DELAY >= SCH_tasks_G[i].Delay) insert_index = i + 1;
        else break;
    }
    for (int32_t j = task_count; j > (int32_t)insert_index; j--)
        SCH_tasks_G[j] = SCH_tasks_G[j - 1];

    SCH_tasks_G[insert_index].pTask  = pFunction;
    SCH_tasks_G[insert_index].Delay  = DELAY;
    SCH_tasks_G[insert_index].Period = PERIOD;
    SCH_tasks_G[insert_index].RunMe  = 0;
    SCH_tasks_G[insert_index].TaskID = insert_index;
    task_count++;
    SCH_Update_Marking();
    return insert_index;
}

// Xóa task tại TASK_INDEX, trả về RETURN_NORMAL hoặc RETURN_ERROR nếu thất bại
uint8_t SCH_Delete_Task(const uint32_t TASK_INDEX)
{
    if (TASK_INDEX >= task_count || task_count == 0)
    {
        Error_code_G = ERROR_SCH_CANNOT_DELETE_TASK;
        return RETURN_ERROR;
    }
    if (task_count == 1)
    {
        SCH_tasks_G[0] = {0x0000, 0, 0, 0, 0};
        MARKING[0] = 0;
        task_count = 0;
    }
    else
    {
        for (uint32_t k = TASK_INDEX; k < task_count - 1; k++)
            SCH_tasks_G[k] = SCH_tasks_G[k + 1];
        uint32_t last = task_count - 1;
        SCH_tasks_G[last] = {0x0000, 0, 0, 0, 0};
        MARKING[last] = 0;
        task_count--;
        if (task_count > 0) SCH_Update_Marking();
    }
    return RETURN_NORMAL;
}


// Cập nhật MARKING array: đánh dấu task nào có Delay bằng Delay của task đầu tiên
static void SCH_Update_Marking(void)
{
    if (task_count == 0) return;
    uint32_t first_delay = SCH_tasks_G[0].Delay;
    for (uint32_t n = 0; n < task_count; n++)
        MARKING[n] = (SCH_tasks_G[n].Delay == first_delay) ? 1 : 0;
}

// Cập nhật scheduler: giảm Delay của tất cả task, đánh dấu RunMe nếu Delay về 0
void SCH_Update(void)
{
    if (task_count > 0)
    {
        if (SCH_tasks_G[0].Delay > 0) SCH_tasks_G[0].Delay--;
        elapsed_time++;
        if (SCH_tasks_G[0].Delay == 0) SCH_tasks_G[0].RunMe++;
    }
}

// Dispatch các task có RunMe > 0, chạy task, xóa nếu không lặp lại hoặc reset Delay nếu có Period
// Chạy ngoài critical section để tránh block ISR, nhưng vẫn đảm bảo an toàn khi thao tác task list
void SCH_Dispatch_Tasks(void)
{
    portENTER_CRITICAL(&timerMux);                          
    if (task_count == 0 || SCH_tasks_G[0].RunMe == 0)
    {
        portEXIT_CRITICAL(&timerMux);
        return;
    }
    uint32_t snapshotElapsed = elapsed_time;
    elapsed_time = 0;  // Reset đếm thời gian đã trôi qua kể từ lần cập nhật cuối cùng                                     

    // Cập nhật Delay của tất cả task dựa trên thời gian đã trôi qua
    for (uint32_t m = 0; m < task_count; m++)
    {
        if (MARKING[m] == 0)
        {
            if (SCH_tasks_G[m].Delay >= snapshotElapsed)
                SCH_tasks_G[m].Delay -= snapshotElapsed;
            else
                SCH_tasks_G[m].Delay = 0;
        }
        else
        {
            SCH_tasks_G[m].Delay = 0;
            SCH_tasks_G[m].RunMe = 1;
        }
    }
    portEXIT_CRITICAL(&timerMux);                           // ← nhả lock trước khi gọi task

    // Dispatch: gọi task function NGOÀI critical section
    // (task function có thể chạy lâu, không được block ISR)
    while (task_count > 0 && SCH_tasks_G[0].RunMe > 0)
    {
        if (SCH_tasks_G[0].pTask != 0x0000)
            (*SCH_tasks_G[0].pTask)();

        portENTER_CRITICAL(&timerMux);                      // ← lock lại khi thao tác task list
        SCH_tasks_G[0].RunMe--;
        if (SCH_tasks_G[0].Period == 0)
        {
            portEXIT_CRITICAL(&timerMux);
            SCH_Delete_Task(0);
        }
        else
        {
            void (*tmp)(void) = SCH_tasks_G[0].pTask;
            uint32_t period   = SCH_tasks_G[0].Period;
            portEXIT_CRITICAL(&timerMux);
            SCH_Delete_Task(0);
            SCH_Add_Task(tmp, period, period);
        }
    }
}
// Trả về số lượng task hiện tại trong scheduler
uint32_t SCH_Get_Current_Size(void) { return task_count; }