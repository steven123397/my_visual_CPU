#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct TrapContext trap_context_t;

typedef void (*supervisor_runtime_timer_post_handler_t)(void* context);
typedef void (*supervisor_runtime_external_post_handler_t)(uint32_t source_id,
                                                           void* context);

/* supervisor bring-up 共享中断状态与等待原语：计数 timer/external delivery，
   并提供 deadline 等待、UART THRE 使能、platform 中断调度等可复用 helper。 */
typedef struct SupervisorRuntimeInterruptState {
    volatile uint32_t timer_interrupts;
    volatile uint32_t external_interrupts;
    uint32_t expected_external_source_id;
    supervisor_runtime_timer_post_handler_t timer_post_handler;
    void* timer_post_context;
    supervisor_runtime_external_post_handler_t external_post_handler;
    void* external_post_context;
} supervisor_runtime_interrupt_state_t;

/* 配置中断状态：期望 external 源 + timer/external post-handler。 */
void supervisor_runtime_interrupt_state_configure(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    void* timer_post_context,
    supervisor_runtime_external_post_handler_t external_post_handler,
    void* external_post_context);
/* 直接设置 timer/external 计数器值。 */
void supervisor_runtime_interrupt_state_set_counters(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t timer_interrupts,
    uint32_t external_interrupts);
/* 初始化中断状态为干净未配置。 */
void supervisor_runtime_interrupt_state_init(
    supervisor_runtime_interrupt_state_t* state);
/* 绑定 self handler（与 configure 等价但面向 self 上下文）。 */
void supervisor_runtime_interrupt_state_bind_self_handlers(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler);
/* 复位计数器为 0（保留配置）。 */
void supervisor_runtime_interrupt_state_reset_counters(
    supervisor_runtime_interrupt_state_t* state);
/* 取 timer delivery 计数。 */
uint32_t supervisor_runtime_interrupt_state_timer_interrupts(
    const supervisor_runtime_interrupt_state_t* state);
/* 取 external delivery 计数。 */
uint32_t supervisor_runtime_interrupt_state_external_interrupts(
    const supervisor_runtime_interrupt_state_t* state);
/* 取期望 external 源 id。 */
uint32_t supervisor_runtime_interrupt_state_expected_external_source_id(
    const supervisor_runtime_interrupt_state_t* state);
/* 是否已发生 timer delivery。 */
bool supervisor_runtime_interrupt_state_timer_delivered(
    const supervisor_runtime_interrupt_state_t* state);
/* 是否已发生 external delivery。 */
bool supervisor_runtime_interrupt_state_external_delivered(
    const supervisor_runtime_interrupt_state_t* state);
/* timer 中断 post-handler：累加 timer 计数并回调 post_handler。 */
void supervisor_runtime_timer_counter_post_handler(uint64_t cause,
                                                   void* context);
/* external 中断 post-handler：累加 external 计数并回调 post_handler。 */
void supervisor_runtime_external_counter_post_handler(uint64_t cause,
                                                      uint32_t source_id,
                                                      void* context);
/* 安装 timer counter policy（把 post-handler 接到 trap_context）。 */
bool supervisor_runtime_install_timer_counter_policy(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state);
/* 安装 external counter policy。 */
bool supervisor_runtime_install_external_counter_policy(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state);
/* adapter：context 当 state，装 external counter policy。 */
bool supervisor_runtime_install_external_counter_policy_adapter(
    trap_context_t* trap_context,
    void* context);
/* 一次性安装 timer + external counter policy。 */
bool supervisor_runtime_install_interrupt_counter_policies(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state);
/* adapter：context 当 state，装 timer + external counter policy。 */
bool supervisor_runtime_install_interrupt_counter_policies_adapter(
    trap_context_t* trap_context,
    void* context);
/* 等待首次 external delivery，超时失败。 */
bool supervisor_runtime_wait_for_first_external_delivery(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timeout_delta);
/* 等待首次 timer delivery，超时失败。 */
bool supervisor_runtime_wait_for_first_timer_delivery(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timer_delta,
    uint64_t timeout_delta);
/* 等待计数器到达 target_value，超时失败。 */
bool supervisor_runtime_wait_for_counter(volatile uint32_t* counter,
                                         uint32_t target_value,
                                         uint64_t timeout_delta);
/* 等待计数器相比基线再增长一次。 */
bool supervisor_runtime_wait_for_next_counter(volatile uint32_t* counter,
                                              uint64_t timeout_delta);
/* 使能 UART THRE 并等待首次 external delivery。 */
bool supervisor_runtime_enable_uart_thre_and_wait(
    volatile uint32_t* external_counter,
    uint64_t timeout_delta);
/* 安排下次 timer 并等待首次 timer delivery。 */
bool supervisor_runtime_schedule_timer_and_wait(
    volatile uint32_t* timer_counter,
    uint64_t timer_delta,
    uint64_t timeout_delta);
/* 安排 timer + 使能 UART THRE 并等待两类中断都到达。 */
bool supervisor_runtime_schedule_platform_interrupts_and_wait(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timer_delta,
    uint64_t timeout_delta);
/* 简单忙等 timeout_delta 个 tick（无计数器依赖）。 */
bool supervisor_runtime_wait_timeout(uint64_t timeout_delta);
/* 取消尚未投递的 timer 安排。 */
void supervisor_runtime_cancel_timer_delivery(void);
