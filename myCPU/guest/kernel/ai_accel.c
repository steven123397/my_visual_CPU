#include "ai_accel.h"

#include <stddef.h>
#include <stdint.h>

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
