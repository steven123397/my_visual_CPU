#include <stdbool.h>
#include <stdint.h>

#include "ai_accel.h"
#include "console.h"
#include "kernel_runtime.h"
#include "panic.h"
#include "platform.h"

enum {
    AI_ACCEL_DEMO_MMIO_MASK = KERNEL_BRINGUP_MMIO_UART |
                              KERNEL_BRINGUP_MMIO_CLINT |
                              KERNEL_BRINGUP_MMIO_PLIC |
                              KERNEL_BRINGUP_MMIO_AI_ACCEL,
    AI_ACCEL_DEMO_QUEUE_ENTRIES = 1,
    AI_ACCEL_DEMO_SOURCE_TAG = 0x33,
    AI_ACCEL_DEMO_TIMEOUT_DELTA = 4096U,
};

static const uint8_t k_ai_accel_demo_graph_package[] = {
    0x41, 0x50, 0x47, 0x31, 0x01, 0x00, 0x28, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x28, 0x00, 0x00, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x8c, 0x00, 0x00, 0x00,
    0xb4, 0x00, 0x00, 0x00, 0x03, 0x01, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x05, 0x03, 0x03, 0x00, 0x00, 0x00, 0xff, 0xff,
    0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
};

static const int32_t k_ai_accel_demo_input_tensor[3] = {1, 2, 3};

static ai_accel_submission_descriptor_t g_submit_queue[AI_ACCEL_DEMO_QUEUE_ENTRIES]
    __attribute__((aligned(64)));
static ai_accel_completion_entry_t g_complete_queue[AI_ACCEL_DEMO_QUEUE_ENTRIES]
    __attribute__((aligned(64)));
static uint64_t g_input_table[2] __attribute__((aligned(16)));
static uint64_t g_output_table[2] __attribute__((aligned(16)));
static int32_t g_output_tensor[1] __attribute__((aligned(16)));

static void ai_accel_demo_zero_buffers(void) {
    g_output_tensor[0] = 0;
    g_submit_queue[0].token = 0;
    g_submit_queue[0].graph_package_addr = 0;
    g_submit_queue[0].graph_package_bytes = 0;
    g_submit_queue[0].flags = 0;
    g_submit_queue[0].input_table_addr = 0;
    g_submit_queue[0].output_table_addr = 0;
    g_submit_queue[0].source_tag = 0;
    g_submit_queue[0].reserved0 = 0;

    g_complete_queue[0].token = 0;
    g_complete_queue[0].status = 0;
    g_complete_queue[0].fault_code = 0;
    g_complete_queue[0].retired_ops = 0;
    g_complete_queue[0].bytes_moved = 0;
    g_complete_queue[0].source_tag = 0;
    g_complete_queue[0].reserved0 = 0;

    g_input_table[0] = (uint64_t)(uintptr_t)k_ai_accel_demo_input_tensor;
    g_input_table[1] = 0;
    g_output_table[0] = 0;
    g_output_table[1] = (uint64_t)(uintptr_t)g_output_tensor;
}

static bool ai_accel_demo_submit(kernel_runtime_t* runtime) {
    ai_accel_profile_counters_t counters;
    const supervisor_runtime_interrupt_state_t* interrupts = NULL;
    const uint64_t token = UINT64_C(0x41494343454c0101);
    uint32_t irq_status = 0;

    if (runtime == NULL) {
        return false;
    }

    counters.device_cycles = 0;
    counters.dma_cycles = 0;
    counters.compute_cycles = 0;
    counters.stall_cycles = 0;
    counters.dma_load_bytes = 0;
    counters.dma_store_bytes = 0;
    interrupts = kernel_runtime_interrupt_state_const(runtime);
    if (interrupts == NULL) {
        return false;
    }

    ai_accel_demo_zero_buffers();
    supervisor_runtime_interrupt_state_reset_counters(
        kernel_runtime_interrupt_state(runtime));
    g_submit_queue[0].token = token;
    g_submit_queue[0].graph_package_addr =
        (uint64_t)(uintptr_t)k_ai_accel_demo_graph_package;
    g_submit_queue[0].graph_package_bytes =
        (uint32_t)sizeof(k_ai_accel_demo_graph_package);
    g_submit_queue[0].flags = 0;
    g_submit_queue[0].input_table_addr = (uint64_t)(uintptr_t)g_input_table;
    g_submit_queue[0].output_table_addr = (uint64_t)(uintptr_t)g_output_table;
    g_submit_queue[0].source_tag = AI_ACCEL_DEMO_SOURCE_TAG;

    ai_accel_reset();
    ai_accel_enable_irqs(AI_ACCEL_IRQ_COMPLETION | AI_ACCEL_IRQ_FAULT);
    ai_accel_configure_queues((uint64_t)(uintptr_t)g_submit_queue,
                              AI_ACCEL_DEMO_QUEUE_ENTRIES,
                              (uint64_t)(uintptr_t)g_complete_queue,
                              AI_ACCEL_DEMO_QUEUE_ENTRIES);
    ai_accel_set_submit_tail(1);
    ai_accel_ring_doorbell(1);
    console_putc('A');

    if (!supervisor_runtime_interrupt_state_external_delivered(interrupts) &&
        !kernel_runtime_wait_for_next_external_delivery(runtime,
                                                        AI_ACCEL_DEMO_TIMEOUT_DELTA)) {
        return false;
    }

    irq_status = ai_accel_irq_status();
    ai_accel_ack_irqs(irq_status);
    if ((irq_status & AI_ACCEL_IRQ_COMPLETION) == 0 ||
        (irq_status & AI_ACCEL_IRQ_FAULT) != 0 ||
        ai_accel_last_fault() != AI_ACCEL_FAULT_NONE ||
        ai_accel_completion_tail() != 1) {
        return false;
    }

    if (g_complete_queue[0].token != token ||
        g_complete_queue[0].status != AI_ACCEL_COMPLETION_STATUS_SUCCESS ||
        g_complete_queue[0].fault_code != AI_ACCEL_FAULT_NONE ||
        g_complete_queue[0].retired_ops != 3 ||
        g_complete_queue[0].bytes_moved != 16 ||
        g_complete_queue[0].source_tag != AI_ACCEL_DEMO_SOURCE_TAG ||
        g_output_tensor[0] != 6) {
        return false;
    }

    ai_accel_set_completion_head(1);
    ai_accel_read_counters(&counters);
    if (counters.device_cycles != 8 ||
        counters.dma_cycles != 6 ||
        counters.compute_cycles != 2 ||
        counters.stall_cycles != 0 ||
        counters.dma_load_bytes != 12 ||
        counters.dma_store_bytes != 4) {
        return false;
    }

    console_putc('I');
    return true;
}

static void ai_accel_demo_external_post_handler(uint32_t source_id, void* context) {
    if (context == NULL || source_id != PLIC_SOURCE_AI_ACCEL) {
        panic_shutdown();
    }

    ai_accel_enable_irqs(0);
}

void kernel_main(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_bind_self_interrupt_handlers(&runtime,
                                                     PLIC_SOURCE_AI_ACCEL,
                                                     NULL,
                                                     ai_accel_demo_external_post_handler)) {
        panic_shutdown();
    }
    if (!kernel_runtime_run_bringup(&runtime,
                                    AI_ACCEL_DEMO_MMIO_MASK,
                                    UINT64_C(0x41494343454c4450),
                                    kernel_runtime_install_external_counter_policy_adapter)) {
        panic_shutdown();
    }

    kernel_runtime_begin_plic_supervisor_phase('\0');
    platform_plic_supervisor_enable_source(PLIC_SOURCE_AI_ACCEL);
    if (!ai_accel_demo_submit(&runtime)) {
        panic_shutdown();
    }

    platform_shutdown(0);
}
