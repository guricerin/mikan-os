#include "task.hpp"

#include "asmfunc.h"
#include "timer.hpp"

alignas(16) TaskContext g_task_b_ctx, g_task_a_ctx;

namespace {
    TaskContext* g_current_task;
}

void SwitchTask() {
    TaskContext* old_current_task = g_current_task;
    if (g_current_task == &g_task_a_ctx) {
        g_current_task = &g_task_b_ctx;
    } else {
        g_current_task = &g_task_a_ctx;
    }
    SwitchContext(g_current_task, old_current_task);
}

void InitializeTask() {
    g_current_task = &g_task_a_ctx;

    // タスク切換え用
    __asm__("cli");
    g_timer_manager->AddTimer(Timer{g_timer_manager->CurrentTick() + kTaskTimerPeriod, kTaskTimerValue});
    __asm__("sti");
}