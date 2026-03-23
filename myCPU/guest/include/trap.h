#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*trap_interrupt_handler_t)(uint64_t cause, void* context);

void trap_init(void);
bool trap_install_interrupt_handler(uint64_t cause,
                                    trap_interrupt_handler_t handler,
                                    void* context);
void supervisor_trap_dispatch(void);
