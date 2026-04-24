#include "ai_accel.h"

#include <stddef.h>
#include <stdint.h>

static bool ai_accel_queue_args_valid(const ai_accel_queue_state_t* state,
                                      const void* submit_queue,
                                      uint32_t submit_entries,
                                      const void* completion_queue,
                                      uint32_t completion_entries) {
    return state != NULL && submit_queue != NULL && completion_queue != NULL &&
           submit_entries != 0U && completion_entries != 0U &&
           submit_entries <= AI_ACCEL_QUEUE_MAX_ENTRIES &&
           completion_entries <= AI_ACCEL_QUEUE_MAX_ENTRIES;
}

static volatile uint32_t* ai_accel_reg_ptr(uint32_t offset) {
    return (volatile uint32_t*)(uintptr_t)(AI_ACCEL_BASE + offset);
}

static uint32_t ai_accel_read_u32(uint32_t offset) {
    return *ai_accel_reg_ptr(offset);
}

static void ai_accel_write_u32(uint32_t offset, uint32_t value) {
    *ai_accel_reg_ptr(offset) = value;
}

static void ai_accel_write_u64(uint32_t low_offset, uint64_t value) {
    ai_accel_write_u32(low_offset, (uint32_t)value);
    ai_accel_write_u32(low_offset + 4U, (uint32_t)(value >> 32));
}

static uint64_t ai_accel_read_u64(uint32_t low_offset) {
    const uint64_t low = ai_accel_read_u32(low_offset);
    const uint64_t high = ai_accel_read_u32(low_offset + 4U);
    return (high << 32) | low;
}

void ai_accel_reset(void) {
    ai_accel_write_u32(AI_ACCEL_REG_CONTROL, AI_ACCEL_CONTROL_RESET);
}

uint32_t ai_accel_submit_head(void) {
    return ai_accel_read_u32(AI_ACCEL_REG_SUBMIT_QUEUE_HEAD);
}

void ai_accel_configure_queues(uint64_t submit_base,
                               uint32_t submit_entries,
                               uint64_t complete_base,
                               uint32_t complete_entries) {
    ai_accel_write_u64(AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW, submit_base);
    ai_accel_write_u32(AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, submit_entries);
    ai_accel_write_u32(AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 0);

    ai_accel_write_u64(AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW, complete_base);
    ai_accel_write_u32(AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, complete_entries);
    ai_accel_write_u32(AI_ACCEL_REG_COMPLETE_QUEUE_HEAD, 0);
}

void ai_accel_set_submit_tail(uint32_t tail) {
    ai_accel_write_u32(AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, tail);
}

void ai_accel_set_completion_head(uint32_t head) {
    ai_accel_write_u32(AI_ACCEL_REG_COMPLETE_QUEUE_HEAD, head);
}

uint32_t ai_accel_completion_tail(void) {
    return ai_accel_read_u32(AI_ACCEL_REG_COMPLETE_QUEUE_TAIL);
}

void ai_accel_ring_doorbell(uint32_t budget) {
    ai_accel_write_u32(AI_ACCEL_REG_DOORBELL, budget);
}

void ai_accel_enable_irqs(uint32_t mask) {
    ai_accel_write_u32(AI_ACCEL_REG_IRQ_MASK, mask);
}

uint32_t ai_accel_irq_status(void) {
    return ai_accel_read_u32(AI_ACCEL_REG_IRQ_STATUS);
}

void ai_accel_ack_irqs(uint32_t mask) {
    ai_accel_write_u32(AI_ACCEL_REG_IRQ_ACK, mask);
}

uint32_t ai_accel_last_fault(void) {
    return ai_accel_read_u32(AI_ACCEL_REG_LAST_FAULT);
}

void ai_accel_read_counters(ai_accel_profile_counters_t* counters) {
    if (counters == NULL) {
        return;
    }

    counters->device_cycles = ai_accel_read_u64(AI_ACCEL_REG_DEVICE_CYCLES_LOW);
    counters->dma_cycles = ai_accel_read_u64(AI_ACCEL_REG_DMA_CYCLES_LOW);
    counters->compute_cycles = ai_accel_read_u64(AI_ACCEL_REG_COMPUTE_CYCLES_LOW);
    counters->stall_cycles = ai_accel_read_u64(AI_ACCEL_REG_STALL_CYCLES_LOW);
    counters->dma_load_bytes = ai_accel_read_u64(AI_ACCEL_REG_DMA_LOAD_BYTES_LOW);
    counters->dma_store_bytes = ai_accel_read_u64(AI_ACCEL_REG_DMA_STORE_BYTES_LOW);
}

bool ai_accel_queue_state_init(ai_accel_queue_state_t* state,
                               ai_accel_submission_descriptor_t* submit_queue,
                               uint32_t submit_entries,
                               ai_accel_completion_entry_t* completion_queue,
                               uint32_t completion_entries) {
    if (!ai_accel_queue_args_valid(state,
                                   submit_queue,
                                   submit_entries,
                                   completion_queue,
                                   completion_entries)) {
        return false;
    }

    state->submit_queue = submit_queue;
    state->completion_queue = completion_queue;
    state->submit_entries = submit_entries;
    state->completion_entries = completion_entries;
    state->submit_head = 0U;
    state->submit_tail = 0U;
    state->completion_head = 0U;
    state->completion_tail = 0U;
    return true;
}

bool ai_accel_queue_enqueue_submission(ai_accel_queue_state_t* state,
                                       uint32_t submit_head,
                                       const ai_accel_submission_descriptor_t* descriptor,
                                       uint32_t* new_submit_tail) {
    if (state == NULL || descriptor == NULL || state->submit_queue == NULL ||
        state->submit_entries == 0U) {
        return false;
    }
    if (submit_head > state->submit_tail) {
        return false;
    }
    if ((state->submit_tail - submit_head) >= state->submit_entries) {
        return false;
    }

    state->submit_head = submit_head;
    state->submit_queue[state->submit_tail % state->submit_entries] = *descriptor;
    state->submit_tail += 1U;
    if (new_submit_tail != NULL) {
        *new_submit_tail = state->submit_tail;
    }
    return true;
}

bool ai_accel_queue_sync_completion_tail(ai_accel_queue_state_t* state,
                                         uint32_t completion_tail) {
    if (state == NULL || state->completion_queue == NULL ||
        state->completion_entries == 0U) {
        return false;
    }
    if (completion_tail < state->completion_tail ||
        completion_tail < state->completion_head) {
        return false;
    }
    if ((completion_tail - state->completion_head) > state->completion_entries) {
        return false;
    }

    state->completion_tail = completion_tail;
    return true;
}

bool ai_accel_queue_dequeue_completion(ai_accel_queue_state_t* state,
                                       ai_accel_completion_entry_t* completion,
                                       uint32_t* new_completion_head) {
    if (state == NULL || completion == NULL || state->completion_queue == NULL ||
        state->completion_entries == 0U ||
        state->completion_head >= state->completion_tail) {
        return false;
    }

    *completion =
        state->completion_queue[state->completion_head % state->completion_entries];
    state->completion_head += 1U;
    if (new_completion_head != NULL) {
        *new_completion_head = state->completion_head;
    }
    return true;
}
