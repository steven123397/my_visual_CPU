#pragma once

#include <stdbool.h>

void trap_reset_state(void);
bool trap_timer_irq_seen(void);
bool trap_external_irq_seen(void);
void supervisor_trap_dispatch(void);
