#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct TrapContext trap_context_t;

typedef void (*supervisor_runtime_timer_post_handler_t)(void* context);
typedef void (*supervisor_runtime_external_post_handler_t)(uint32_t source_id,
                                                           void* context);

typedef struct SupervisorRuntimeInterruptState {
    volatile uint32_t timer_interrupts;
    volatile uint32_t external_interrupts;
    uint32_t expected_external_source_id;
    supervisor_runtime_timer_post_handler_t timer_post_handler;
    void* timer_post_context;
    supervisor_runtime_external_post_handler_t external_post_handler;
    void* external_post_context;
} supervisor_runtime_interrupt_state_t;

void supervisor_runtime_interrupt_state_configure(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    void* timer_post_context,
    supervisor_runtime_external_post_handler_t external_post_handler,
    void* external_post_context);
void supervisor_runtime_interrupt_state_set_counters(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t timer_interrupts,
    uint32_t external_interrupts);
void supervisor_runtime_interrupt_state_init(
    supervisor_runtime_interrupt_state_t* state);
void supervisor_runtime_interrupt_state_bind_self_handlers(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler);
void supervisor_runtime_interrupt_state_reset_counters(
    supervisor_runtime_interrupt_state_t* state);
uint32_t supervisor_runtime_interrupt_state_timer_interrupts(
    const supervisor_runtime_interrupt_state_t* state);
uint32_t supervisor_runtime_interrupt_state_external_interrupts(
    const supervisor_runtime_interrupt_state_t* state);
uint32_t supervisor_runtime_interrupt_state_expected_external_source_id(
    const supervisor_runtime_interrupt_state_t* state);
bool supervisor_runtime_interrupt_state_timer_delivered(
    const supervisor_runtime_interrupt_state_t* state);
bool supervisor_runtime_interrupt_state_external_delivered(
    const supervisor_runtime_interrupt_state_t* state);
void supervisor_runtime_timer_counter_post_handler(uint64_t cause,
                                                   void* context);
void supervisor_runtime_external_counter_post_handler(uint64_t cause,
                                                      uint32_t source_id,
                                                      void* context);
bool supervisor_runtime_install_timer_counter_policy(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state);
bool supervisor_runtime_install_external_counter_policy(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state);
bool supervisor_runtime_install_external_counter_policy_adapter(
    trap_context_t* trap_context,
    void* context);
bool supervisor_runtime_install_interrupt_counter_policies(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state);
bool supervisor_runtime_install_interrupt_counter_policies_adapter(
    trap_context_t* trap_context,
    void* context);
bool supervisor_runtime_wait_for_first_external_delivery(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timeout_delta);
bool supervisor_runtime_wait_for_first_timer_delivery(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timer_delta,
    uint64_t timeout_delta);
bool supervisor_runtime_wait_for_counter(volatile uint32_t* counter,
                                         uint32_t target_value,
                                         uint64_t timeout_delta);
bool supervisor_runtime_wait_for_next_counter(volatile uint32_t* counter,
                                              uint64_t timeout_delta);
bool supervisor_runtime_enable_uart_thre_and_wait(
    volatile uint32_t* external_counter,
    uint64_t timeout_delta);
bool supervisor_runtime_schedule_timer_and_wait(
    volatile uint32_t* timer_counter,
    uint64_t timer_delta,
    uint64_t timeout_delta);
bool supervisor_runtime_schedule_platform_interrupts_and_wait(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timer_delta,
    uint64_t timeout_delta);
bool supervisor_runtime_wait_timeout(uint64_t timeout_delta);
void supervisor_runtime_cancel_timer_delivery(void);
