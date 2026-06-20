#pragma once

#include <stdint.h>

/* supervisor timer 中断最小封装：基于 CLINT 的定时调度与中断处理。 */
#define TIMER_HZ 100U

/* 安排下一次 timer 中断在 delta 个 tick 后触发。 */
void timer_schedule_delta(uint64_t delta);
/* 处理一次 timer 中断（更新调度、重排下一次）。 */
void timer_handle_interrupt(void);
