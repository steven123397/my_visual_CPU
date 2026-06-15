#pragma once

#include <stdint.h>

#define TIMER_HZ 100U

void timer_schedule_delta(uint64_t delta);
void timer_handle_interrupt(void);
