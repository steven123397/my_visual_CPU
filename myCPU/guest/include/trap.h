#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*trap_interrupt_handler_t)(uint64_t cause, void* context);
typedef void (*trap_exception_handler_t)(uint64_t cause,
                                         uint64_t epc,
                                         uint64_t tval,
                                         void* context);
typedef enum TrapPageFaultAction {
    TRAP_PAGE_FAULT_ACTION_UNHANDLED = 0,
    TRAP_PAGE_FAULT_ACTION_RETRY,
    TRAP_PAGE_FAULT_ACTION_SKIP_INSTRUCTION,
    TRAP_PAGE_FAULT_ACTION_RESUME_AT,
} trap_page_fault_action_t;

typedef struct TrapPageFaultResult {
    trap_page_fault_action_t action;
    uintptr_t resume_pc;
} trap_page_fault_result_t;

typedef trap_page_fault_result_t (*trap_page_fault_handler_t)(uint64_t cause,
                                                              uint64_t epc,
                                                              uint64_t tval,
                                                              void* context);

#define TRAP_PAGE_FAULT_RESULT_UNHANDLED \
    ((trap_page_fault_result_t){TRAP_PAGE_FAULT_ACTION_UNHANDLED, 0})
#define TRAP_PAGE_FAULT_RESULT_RETRY \
    ((trap_page_fault_result_t){TRAP_PAGE_FAULT_ACTION_RETRY, 0})
#define TRAP_PAGE_FAULT_RESULT_SKIP_INSTRUCTION \
    ((trap_page_fault_result_t){TRAP_PAGE_FAULT_ACTION_SKIP_INSTRUCTION, 0})
#define TRAP_PAGE_FAULT_RESULT_RESUME_AT(pc_value) \
    ((trap_page_fault_result_t){TRAP_PAGE_FAULT_ACTION_RESUME_AT, (uintptr_t)(pc_value)})

void trap_init(void);
bool trap_install_interrupt_handler(uint64_t cause,
                                    trap_interrupt_handler_t handler,
                                    void* context);
bool trap_install_page_fault_handler(trap_page_fault_handler_t handler,
                                     void* context);
bool trap_install_exception_handler(uint64_t cause,
                                    trap_exception_handler_t handler,
                                    void* context);
void supervisor_trap_dispatch(void);
