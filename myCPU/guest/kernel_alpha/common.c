#include "kernel_alpha.h"

static bool kernel_alpha_storage_signature_guardrail(const uint8_t* block,
                                                     size_t block_size,
                                                     void* context);

void kernel_alpha_begin_plic_supervisor_phase(void) {
    kernel_runtime_begin_plic_supervisor_phase('P');
}

bool kernel_alpha_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                   uint64_t timeout_delta) {
    return kernel_runtime_wait_for_first_external_delivery(runtime,
                                                           timeout_delta);
}

bool kernel_alpha_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                uint64_t timer_delta,
                                                uint64_t timeout_delta) {
    return kernel_runtime_wait_for_first_timer_delivery(runtime,
                                                        timer_delta,
                                                        timeout_delta);
}

bool kernel_alpha_complete_storage_probe(void) {
    return kernel_runtime_complete_storage_probe('D');
}

bool kernel_alpha_complete_storage_signature_check(void) {
    return kernel_runtime_complete_storage_lba0_check(
        'S',
        kernel_alpha_storage_signature_guardrail,
        NULL);
}

static bool kernel_alpha_storage_signature_guardrail(const uint8_t* block,
                                                     size_t block_size,
                                                     void* context) {
    (void)context;
    return block != NULL && block_size >= 4U &&
           block[0] == 'S' && block[1] == 't' &&
           block[2] == 'o' && block[3] == 'r';
}
