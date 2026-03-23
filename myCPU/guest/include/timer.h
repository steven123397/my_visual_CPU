#pragma once

#include <stdint.h>

void timer_schedule_delta(uint64_t delta);
void timer_handle_interrupt(void);
