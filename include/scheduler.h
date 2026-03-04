/*
 * scheduler.h
 * Cooperative Scheduler cho ESP32 - tái sử dụng từ Smart Clock
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <Arduino.h>

#define SCH_MAX_TASKS       20      // Số lượng task tối đa có thể đăng ký
#define NO_TASK_ID          0       // ID trả về khi thêm task thất bại
#define TIMER_TICK_MS       10      // Tick 10ms

#define ERROR_SCH_TOO_MANY_TASKS    1
#define ERROR_SCH_CANNOT_DELETE_TASK 2

#define RETURN_ERROR    0
#define RETURN_NORMAL   1

typedef struct
{
    void     (*pTask)(void);
    uint32_t Delay;
    uint32_t Period;
    uint8_t  RunMe;
    uint32_t TaskID;
} sTask;

extern sTask    SCH_tasks_G[SCH_MAX_TASKS];
extern uint8_t  Error_code_G;
extern uint8_t  MARKING[SCH_MAX_TASKS];
extern uint32_t task_count;
extern uint32_t elapsed_time;

void     SCH_Init(void);
void     SCH_Init_Timer(void);
void     SCH_Update(void);
void     SCH_Dispatch_Tasks(void);
uint32_t SCH_Add_Task(void (*pFunction)(), uint32_t DELAY, uint32_t PERIOD);
uint8_t  SCH_Delete_Task(const uint32_t TASK_INDEX);
uint32_t SCH_Get_Current_Size(void);

#endif // SCHEDULER_H