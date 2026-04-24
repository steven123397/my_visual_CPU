#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform_mmio.h"

enum {
    AI_ACCEL_REG_STATUS = 0x00c,
    AI_ACCEL_REG_CONTROL = 0x010,
    AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW = 0x018,
    AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH = 0x01c,
    AI_ACCEL_REG_SUBMIT_QUEUE_SIZE = 0x020,
    AI_ACCEL_REG_SUBMIT_QUEUE_HEAD = 0x024,
    AI_ACCEL_REG_SUBMIT_QUEUE_TAIL = 0x028,
    AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW = 0x02c,
    AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH = 0x030,
    AI_ACCEL_REG_COMPLETE_QUEUE_SIZE = 0x034,
    AI_ACCEL_REG_COMPLETE_QUEUE_HEAD = 0x038,
    AI_ACCEL_REG_COMPLETE_QUEUE_TAIL = 0x03c,
    AI_ACCEL_REG_DOORBELL = 0x040,
    AI_ACCEL_REG_IRQ_STATUS = 0x044,
    AI_ACCEL_REG_IRQ_MASK = 0x048,
    AI_ACCEL_REG_IRQ_ACK = 0x04c,
    AI_ACCEL_REG_LAST_FAULT = 0x050,
    AI_ACCEL_REG_DEVICE_CYCLES_LOW = 0x078,
    AI_ACCEL_REG_DEVICE_CYCLES_HIGH = 0x07c,
    AI_ACCEL_REG_DMA_CYCLES_LOW = 0x080,
    AI_ACCEL_REG_DMA_CYCLES_HIGH = 0x084,
    AI_ACCEL_REG_DMA_LOAD_BYTES_LOW = 0x098,
    AI_ACCEL_REG_DMA_LOAD_BYTES_HIGH = 0x09c,
    AI_ACCEL_REG_DMA_STORE_BYTES_LOW = 0x0a0,
    AI_ACCEL_REG_DMA_STORE_BYTES_HIGH = 0x0a4,
    AI_ACCEL_REG_COMPUTE_CYCLES_LOW = 0x0b0,
    AI_ACCEL_REG_COMPUTE_CYCLES_HIGH = 0x0b4,
    AI_ACCEL_REG_STALL_CYCLES_LOW = 0x0b8,
    AI_ACCEL_REG_STALL_CYCLES_HIGH = 0x0bc,
};

enum {
    AI_ACCEL_CONTROL_RESET = 0x1,
};

enum {
    AI_ACCEL_IRQ_COMPLETION = 0x1,
    AI_ACCEL_IRQ_FAULT = 0x2,
};

enum {
    AI_ACCEL_COMPLETION_STATUS_SUCCESS = 0,
    AI_ACCEL_COMPLETION_STATUS_FAULT = 1,
};

enum {
    AI_ACCEL_FAULT_NONE = 0,
};

enum {
    AI_ACCEL_QUEUE_MAX_ENTRIES = 1024U,
};

typedef struct AiAccelSubmissionDescriptor {
    uint64_t token;
    uint64_t graph_package_addr;
    uint32_t graph_package_bytes;
    uint32_t flags;
    uint64_t input_table_addr;
    uint64_t output_table_addr;
    uint32_t source_tag;
    uint32_t reserved0;
} ai_accel_submission_descriptor_t;

typedef struct AiAccelCompletionEntry {
    uint64_t token;
    uint32_t status;
    uint32_t fault_code;
    uint64_t retired_ops;
    uint64_t bytes_moved;
    uint32_t source_tag;
    uint32_t reserved0;
} ai_accel_completion_entry_t;

typedef struct AiAccelProfileCounters {
    uint64_t device_cycles;
    uint64_t dma_cycles;
    uint64_t compute_cycles;
    uint64_t stall_cycles;
    uint64_t dma_load_bytes;
    uint64_t dma_store_bytes;
} ai_accel_profile_counters_t;

typedef struct AiAccelQueueState {
    ai_accel_submission_descriptor_t* submit_queue;
    ai_accel_completion_entry_t* completion_queue;
    uint32_t submit_entries;
    uint32_t completion_entries;
    uint32_t submit_head;
    uint32_t submit_tail;
    uint32_t completion_head;
    uint32_t completion_tail;
} ai_accel_queue_state_t;

_Static_assert(sizeof(ai_accel_submission_descriptor_t) == 48,
               "AI accel submission descriptor ABI must stay 48 bytes");
_Static_assert(sizeof(ai_accel_completion_entry_t) == 40,
               "AI accel completion entry ABI must stay 40 bytes");

void ai_accel_reset(void);
uint32_t ai_accel_submit_head(void);
void ai_accel_configure_queues(uint64_t submit_base,
                               uint32_t submit_entries,
                               uint64_t complete_base,
                               uint32_t complete_entries);
void ai_accel_set_submit_tail(uint32_t tail);
void ai_accel_set_completion_head(uint32_t head);
uint32_t ai_accel_completion_tail(void);
void ai_accel_ring_doorbell(uint32_t budget);
void ai_accel_enable_irqs(uint32_t mask);
uint32_t ai_accel_irq_status(void);
void ai_accel_ack_irqs(uint32_t mask);
uint32_t ai_accel_last_fault(void);
void ai_accel_read_counters(ai_accel_profile_counters_t* counters);
bool ai_accel_queue_state_init(ai_accel_queue_state_t* state,
                               ai_accel_submission_descriptor_t* submit_queue,
                               uint32_t submit_entries,
                               ai_accel_completion_entry_t* completion_queue,
                               uint32_t completion_entries);
bool ai_accel_queue_enqueue_submission(ai_accel_queue_state_t* state,
                                       uint32_t submit_head,
                                       const ai_accel_submission_descriptor_t* descriptor,
                                       uint32_t* new_submit_tail);
bool ai_accel_queue_sync_completion_tail(ai_accel_queue_state_t* state,
                                         uint32_t completion_tail);
bool ai_accel_queue_dequeue_completion(ai_accel_queue_state_t* state,
                                       ai_accel_completion_entry_t* completion,
                                       uint32_t* new_completion_head);
