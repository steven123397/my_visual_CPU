#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>

#include "../../src/devices/ai_accelerator.h"
#include "../../src/platform/machine.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool expect_default_timing_model(const AiAcceleratorProfileSummary& summary, const char* context) {
    return expect(summary.timing_model == AiAcceleratorTimingModel::TimedSimpleNoOverlap, context) &&
           expect(summary.scheduler_ops_per_cycle == 32, context) &&
           expect(summary.scheduler_tile_setup_cycles == 1, context) &&
           expect(!summary.allow_dma_compute_overlap, context) &&
           expect(summary.dma_setup_cycles == 2, context) &&
           expect(summary.dma_bytes_per_cycle == 16, context);
}

bool expect_submission_timing(const AiAcceleratorProfileSummary& summary,
                              uint64_t expected_device_cycles,
                              uint64_t expected_dma_cycles,
                              uint64_t expected_compute_cycles,
                              uint64_t expected_stall_cycles,
                              uint64_t expected_queue_cycles,
                              uint64_t expected_completion_cycles,
                              uint64_t expected_busy_cycles,
                              const char* context) {
    return expect(summary.last_submission_device_cycles == expected_device_cycles, context) &&
           expect(summary.last_submission_dma_cycles == expected_dma_cycles, context) &&
           expect(summary.last_submission_compute_cycles == expected_compute_cycles, context) &&
           expect(summary.last_submission_stall_cycles == expected_stall_cycles, context) &&
           expect(summary.last_submission_queue_cycles == expected_queue_cycles, context) &&
           expect(summary.last_submission_completion_cycles == expected_completion_cycles, context) &&
           expect(summary.last_submission_busy_cycles == expected_busy_cycles, context);
}

bool expect_submission_outcome(const AiAcceleratorProfileSummary& summary,
                               uint32_t expected_fault,
                               uint64_t expected_retired_ops,
                               uint64_t expected_bytes_moved,
                               const char* context) {
    return expect(summary.last_submission_fault == expected_fault, context) &&
           expect(summary.last_submission_retired_ops == expected_retired_ops, context) &&
           expect(summary.last_submission_bytes_moved == expected_bytes_moved, context);
}

bool expect_submission_dma_breakdown(const AiAcceleratorProfileSummary& summary,
                                     uint64_t expected_load_cycles,
                                     uint64_t expected_store_cycles,
                                     uint64_t expected_load_bytes,
                                     uint64_t expected_store_bytes,
                                     const char* context) {
    return expect(summary.last_submission_dma_load_cycles == expected_load_cycles, context) &&
           expect(summary.last_submission_dma_store_cycles == expected_store_cycles, context) &&
           expect(summary.last_submission_dma_load_bytes == expected_load_bytes, context) &&
           expect(summary.last_submission_dma_store_bytes == expected_store_bytes, context);
}

bool expect_submission_compile_contract(const AiAcceleratorProfileSummary& summary,
                                        AiShapeMode expected_shape_mode,
                                        uint32_t expected_runtime_shape_count,
                                        uint32_t expected_tensor_count,
                                        uint32_t expected_memory_plan_entries,
                                        uint32_t expected_dynamic_tensor_count,
                                        uint32_t expected_input_tensor_count,
                                        uint32_t expected_output_tensor_count,
                                        uint32_t expected_weight_tensor_count,
                                        uint32_t expected_constant_tensor_count,
                                        uint32_t expected_intermediate_tensor_count,
                                        uint32_t expected_scratchpad_budget_bytes,
                                        uint32_t expected_op_count,
                                        uint32_t expected_dependency_count,
                                        uint32_t expected_root_op_count,
                                        uint32_t expected_leaf_op_count,
                                        uint32_t expected_dependency_depth,
                                        uint32_t expected_max_fanin,
                                        uint32_t expected_max_fanout,
                                        uint32_t expected_load_entry_count,
                                        uint32_t expected_store_entry_count,
                                        uint32_t expected_load_plan_bytes,
                                        uint32_t expected_store_plan_bytes,
                                        uint64_t expected_token,
                                        uint32_t expected_flags,
                                        uint64_t expected_graph_package_addr,
                                        uint64_t expected_input_table_addr,
                                        uint64_t expected_output_table_addr,
                                        uint64_t expected_submission_base_snapshot,
                                        uint64_t expected_completion_base_snapshot,
                                        uint32_t expected_graph_package_bytes,
                                        uint32_t expected_runtime_shape_table_offset,
                                        uint64_t expected_runtime_shape_table_addr,
                                        uint32_t expected_source_tag,
                                        uint32_t expected_queue_depth_snapshot,
                                        uint32_t expected_submission_queue_size_snapshot,
                                        uint32_t expected_completion_queue_size_snapshot,
                                        uint32_t expected_submission_head_snapshot,
                                        uint32_t expected_submission_tail_snapshot,
                                        uint32_t expected_completion_head_snapshot,
                                        uint32_t expected_completion_tail_snapshot,
                                        bool expected_queue_configured_snapshot,
                                        const char* context) {
    return expect(summary.last_submission_shape_mode == expected_shape_mode, context) &&
           expect(summary.last_submission_runtime_shape_count == expected_runtime_shape_count, context) &&
           expect(summary.last_submission_tensor_count == expected_tensor_count, context) &&
           expect(summary.last_submission_memory_plan_entries == expected_memory_plan_entries, context) &&
           expect(summary.last_submission_dynamic_tensor_count == expected_dynamic_tensor_count, context) &&
           expect(summary.last_submission_input_tensor_count == expected_input_tensor_count, context) &&
           expect(summary.last_submission_output_tensor_count == expected_output_tensor_count, context) &&
           expect(summary.last_submission_weight_tensor_count == expected_weight_tensor_count, context) &&
           expect(summary.last_submission_constant_tensor_count == expected_constant_tensor_count, context) &&
           expect(summary.last_submission_intermediate_tensor_count ==
                      expected_intermediate_tensor_count,
                  context) &&
           expect(summary.last_submission_scratchpad_budget_bytes ==
                      expected_scratchpad_budget_bytes,
                  context) &&
           expect(summary.last_submission_op_count == expected_op_count, context) &&
           expect(summary.last_submission_dependency_count == expected_dependency_count, context) &&
           expect(summary.last_submission_root_op_count == expected_root_op_count, context) &&
           expect(summary.last_submission_leaf_op_count == expected_leaf_op_count, context) &&
           expect(summary.last_submission_dependency_depth == expected_dependency_depth, context) &&
           expect(summary.last_submission_max_fanin == expected_max_fanin, context) &&
           expect(summary.last_submission_max_fanout == expected_max_fanout, context) &&
           expect(summary.last_submission_load_entry_count == expected_load_entry_count, context) &&
           expect(summary.last_submission_store_entry_count == expected_store_entry_count, context) &&
           expect(summary.last_submission_load_plan_bytes == expected_load_plan_bytes, context) &&
           expect(summary.last_submission_store_plan_bytes == expected_store_plan_bytes, context) &&
           expect(summary.last_submission_token == expected_token, context) &&
           expect(summary.last_submission_flags == expected_flags, context) &&
           expect(summary.last_submission_graph_package_addr == expected_graph_package_addr, context) &&
           expect(summary.last_submission_input_table_addr == expected_input_table_addr, context) &&
           expect(summary.last_submission_output_table_addr == expected_output_table_addr, context) &&
           expect(summary.submission_base_snapshot == expected_submission_base_snapshot, context) &&
           expect(summary.completion_base_snapshot == expected_completion_base_snapshot, context) &&
           expect(summary.last_submission_graph_package_bytes == expected_graph_package_bytes, context) &&
           expect(summary.last_submission_runtime_shape_table_offset ==
                      expected_runtime_shape_table_offset,
                  context) &&
           expect(summary.last_submission_runtime_shape_table_addr ==
                      expected_runtime_shape_table_addr,
                  context) &&
           expect(summary.last_submission_source_tag == expected_source_tag, context) &&
           expect(summary.queue_depth_snapshot == expected_queue_depth_snapshot, context) &&
           expect(summary.submission_queue_size_snapshot ==
                      expected_submission_queue_size_snapshot,
                  context) &&
           expect(summary.completion_queue_size_snapshot ==
                      expected_completion_queue_size_snapshot,
                  context) &&
           expect(summary.submission_head_snapshot == expected_submission_head_snapshot, context) &&
           expect(summary.submission_tail_snapshot == expected_submission_tail_snapshot, context) &&
           expect(summary.completion_head_snapshot == expected_completion_head_snapshot, context) &&
           expect(summary.completion_tail_snapshot == expected_completion_tail_snapshot, context) &&
           expect(summary.queue_configured_snapshot == expected_queue_configured_snapshot, context);
}

bool load_counter(Bus& bus, uint32_t low_reg, uint32_t high_reg, uint64_t& value) {
    uint64_t low = 0;
    uint64_t high = 0;
    return bus.try_load(AI_ACCEL_BASE + low_reg, 4, low) &&
           bus.try_load(AI_ACCEL_BASE + high_reg, 4, high) &&
           ((value = (high << 32) | static_cast<uint32_t>(low)), true);
}

bool load_u32(Bus& bus, uint32_t reg, uint32_t& value) {
    uint64_t raw = 0;
    if (!bus.try_load(AI_ACCEL_BASE + reg, 4, raw)) {
        return false;
    }
    value = static_cast<uint32_t>(raw);
    return true;
}

bool store_u32(Bus& bus, uint64_t addr, uint32_t value, const char* context) {
    if (!bus.try_store(addr, value, 4)) {
        std::fprintf(stderr, "%s\n", context);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    try {
        const std::filesystem::path guest_demo = "guest/ai_accel_demo.elf";
        if (!expect(std::filesystem::exists(guest_demo),
                    "ai accel guest smoke expects guest/ai_accel_demo.elf")) {
            return 1;
        }

        Machine machine;
        machine.load_elf(guest_demo.string());
        Bus& bus = machine.bus();

        const DebugAiAcceleratorSnapshot initial_snapshot = machine.ai_accelerator().debug_snapshot();
        if (!expect(initial_snapshot.present, "guest AI accel demo expects mapped AI accelerator before run") ||
            !expect(initial_snapshot.queue_depth == 0,
                    "guest AI accel demo should start with empty queue depth") ||
            !expect(initial_snapshot.doorbell_count == 0,
                    "guest AI accel demo should start with zero doorbell count") ||
            !expect(initial_snapshot.last_fault == AI_ACCEL_FAULT_NONE,
                    "guest AI accel demo should start without AI faults") ||
            !expect(initial_snapshot.completion_count == 0,
                    "guest AI accel demo should start with zero completion count") ||
            !expect(!initial_snapshot.engine_busy,
                    "guest AI accel demo should start with the AI engine idle") ||
            !expect(initial_snapshot.scratchpad_occupancy_bytes == 0,
                    "guest AI accel demo should start with zero scratchpad occupancy") ||
            !expect(initial_snapshot.dma_load_bytes == 0,
                    "guest AI accel demo should start with zero debug DMA load bytes") ||
            !expect(initial_snapshot.dma_store_bytes == 0,
                    "guest AI accel demo should start with zero debug DMA store bytes") ||
            !expect(initial_snapshot.device_cycles == 0,
                    "guest AI accel demo should start with zero debug device cycles") ||
            !expect(initial_snapshot.dma_cycles == 0,
                    "guest AI accel demo should start with zero debug DMA cycles") ||
            !expect(initial_snapshot.compute_cycles == 0,
                    "guest AI accel demo should start with zero debug compute cycles") ||
            !expect(initial_snapshot.stall_cycles == 0,
                    "guest AI accel demo should start with zero debug stall cycles") ||
            !expect(initial_snapshot.busy_cycles == 0,
                    "guest AI accel demo should start with zero debug busy cycles") ||
            !expect(initial_snapshot.queue_cycles == 0,
                    "guest AI accel demo should start with zero debug queue cycles") ||
            !expect(initial_snapshot.completion_cycles == 0,
                    "guest AI accel demo should start with zero debug completion cycles") ||
            !expect(initial_snapshot.effective_ops_per_cycle == 0,
                    "guest AI accel demo should start with zero debug effective ops") ||
            !expect(initial_snapshot.utilization == 0,
                    "guest AI accel demo should start with zero debug utilization")) {
            return 1;
        }

        const AiAcceleratorProfileSummary& initial_summary = machine.ai_accelerator().profile_summary();
        if (!expect_default_timing_model(initial_summary,
                                         "guest AI accel demo should preserve timing metadata before run") ||
            !expect_submission_timing(initial_summary,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      "guest AI accel demo should start with zero submission timing") ||
            !expect_submission_outcome(initial_summary,
                                       AI_ACCEL_FAULT_NONE,
                                       0,
                                       0,
                                       "guest AI accel demo should start with zero submission outcome") ||
            !expect_submission_dma_breakdown(initial_summary,
                                             0,
                                             0,
                                             0,
                                             0,
                                             "guest AI accel demo should start with zero DMA breakdown") ||
            !expect_submission_compile_contract(initial_summary,
                                               AiShapeMode::Static,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               false,
                                               "guest AI accel demo should start with zero compile contract") ||
            !expect(initial_summary.tile_count == 0,
                    "guest AI accel demo should start with zero tile count") ||
            !expect(initial_summary.scratchpad_peak_bytes == 0,
                    "guest AI accel demo should start with zero scratchpad peak bytes") ||
            !expect(initial_summary.op_summaries.empty(),
                    "guest AI accel demo should start with empty op summaries")) {
            return 1;
        }

        uint32_t initial_status = 0;
        uint32_t initial_queue_depth = 0;
        uint64_t initial_submit_queue_base = 1;
        uint64_t initial_complete_queue_base = 1;
        uint32_t initial_submit_queue_size = 1;
        uint32_t initial_submit_queue_head = 1;
        uint32_t initial_submit_queue_tail = 1;
        uint32_t initial_complete_queue_size = 1;
        uint32_t initial_complete_queue_head = 1;
        uint32_t initial_complete_queue_tail = 1;
        uint32_t initial_irq_status = 0;
        uint32_t initial_irq_mask = 0;
        uint32_t initial_last_fault = AI_ACCEL_FAULT_EXECUTION;
        uint32_t initial_fault_detail = 1;
        if (!load_u32(bus, AI_ACCEL_REG_STATUS, initial_status) ||
            !load_u32(bus, AI_ACCEL_REG_QUEUE_DEPTH, initial_queue_depth) ||
            !load_counter(bus,
                          AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW,
                          AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH,
                          initial_submit_queue_base) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW,
                          AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH,
                          initial_complete_queue_base) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, initial_submit_queue_size) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_HEAD, initial_submit_queue_head) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, initial_submit_queue_tail) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, initial_complete_queue_size) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_HEAD, initial_complete_queue_head) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, initial_complete_queue_tail) ||
            !load_u32(bus, AI_ACCEL_REG_IRQ_STATUS, initial_irq_status) ||
            !load_u32(bus, AI_ACCEL_REG_IRQ_MASK, initial_irq_mask) ||
            !load_u32(bus, AI_ACCEL_REG_LAST_FAULT, initial_last_fault) ||
            !load_u32(bus, AI_ACCEL_REG_FAULT_DETAIL, initial_fault_detail)) {
            std::fprintf(stderr, "guest AI accel demo could not read initial AI control registers\n");
            return 1;
        }

        if (!expect(initial_status == AI_ACCEL_STATUS_READY,
                    "guest AI accel demo should start with READY status only") ||
            !expect(initial_queue_depth == 0,
                    "guest AI accel demo should start with zero MMIO queue depth") ||
            !expect(initial_submit_queue_base == 0,
                    "guest AI accel demo should start with zero submit queue base") ||
            !expect(initial_complete_queue_base == 0,
                    "guest AI accel demo should start with zero completion queue base") ||
            !expect(initial_submit_queue_size == 0,
                    "guest AI accel demo should start with zero submit queue size") ||
            !expect(initial_submit_queue_head == 0,
                    "guest AI accel demo should start with zero submit queue head") ||
            !expect(initial_submit_queue_tail == 0,
                    "guest AI accel demo should start with zero submit queue tail") ||
            !expect(initial_complete_queue_size == 0,
                    "guest AI accel demo should start with zero completion queue size") ||
            !expect(initial_complete_queue_head == 0,
                    "guest AI accel demo should start with zero completion queue head") ||
            !expect(initial_complete_queue_tail == 0,
                    "guest AI accel demo should start with zero completion queue tail") ||
            !expect(initial_irq_status == 0,
                    "guest AI accel demo should start with zero IRQ status") ||
            !expect(initial_irq_mask == AI_ACCEL_IRQ_ALL,
                    "guest AI accel demo should start with default IRQ mask") ||
            !expect(initial_last_fault == AI_ACCEL_FAULT_NONE,
                    "guest AI accel demo should start with zero MMIO last fault") ||
            !expect(initial_fault_detail == 0,
                    "guest AI accel demo should start with zero fault detail")) {
            return 1;
        }

        uint64_t initial_doorbell_count = 1;
        uint64_t initial_completion_count = 1;
        uint64_t initial_submission_count = 1;
        uint64_t initial_fault_count = 1;
        uint64_t initial_device_cycles = 1;
        uint64_t initial_dma_cycles = 1;
        uint64_t initial_compute_cycles = 1;
        uint64_t initial_stall_cycles = 1;
        uint64_t initial_dma_load_bytes = 1;
        uint64_t initial_dma_store_bytes = 1;
        if (!load_counter(bus,
                          AI_ACCEL_REG_DOORBELL_COUNT_LOW,
                          AI_ACCEL_REG_DOORBELL_COUNT_HIGH,
                          initial_doorbell_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPLETION_COUNT_LOW,
                          AI_ACCEL_REG_COMPLETION_COUNT_HIGH,
                          initial_completion_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_SUBMISSION_COUNT_LOW,
                          AI_ACCEL_REG_SUBMISSION_COUNT_HIGH,
                          initial_submission_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_FAULT_COUNT_LOW,
                          AI_ACCEL_REG_FAULT_COUNT_HIGH,
                          initial_fault_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DEVICE_CYCLES_LOW,
                          AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                          initial_device_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_CYCLES_LOW,
                          AI_ACCEL_REG_DMA_CYCLES_HIGH,
                          initial_dma_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPUTE_CYCLES_LOW,
                          AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                          initial_compute_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_STALL_CYCLES_LOW,
                          AI_ACCEL_REG_STALL_CYCLES_HIGH,
                          initial_stall_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_LOAD_BYTES_LOW,
                          AI_ACCEL_REG_DMA_LOAD_BYTES_HIGH,
                          initial_dma_load_bytes) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_STORE_BYTES_LOW,
                          AI_ACCEL_REG_DMA_STORE_BYTES_HIGH,
                          initial_dma_store_bytes)) {
            std::fprintf(stderr, "guest AI accel demo could not read initial AI counters\n");
            return 1;
        }

        if (!expect(initial_doorbell_count == 0,
                    "guest AI accel demo should start with zero MMIO doorbell count") ||
            !expect(initial_completion_count == 0,
                    "guest AI accel demo should start with zero MMIO completion count") ||
            !expect(initial_submission_count == 0,
                    "guest AI accel demo should start with zero MMIO submission count") ||
            !expect(initial_fault_count == 0,
                    "guest AI accel demo should start with zero MMIO fault count") ||
            !expect(initial_device_cycles == 0,
                    "guest AI accel demo should start with zero MMIO device cycles") ||
            !expect(initial_dma_cycles == 0,
                    "guest AI accel demo should start with zero MMIO DMA cycles") ||
            !expect(initial_compute_cycles == 0,
                    "guest AI accel demo should start with zero MMIO compute cycles") ||
            !expect(initial_stall_cycles == 0,
                    "guest AI accel demo should start with zero MMIO stall cycles") ||
            !expect(initial_dma_load_bytes == 0,
                    "guest AI accel demo should start with zero MMIO DMA load bytes") ||
            !expect(initial_dma_store_bytes == 0,
                    "guest AI accel demo should start with zero MMIO DMA store bytes")) {
            return 1;
        }

        machine.run();

        if (!expect(machine.cpu().core().halted(), "guest AI accel demo should halt") ||
            !expect(machine.uart().output() == "KMVAI",
                    "guest AI accel demo should emit KMVAI on success")) {
            return 1;
        }

        const DebugAiAcceleratorSnapshot snapshot = machine.ai_accelerator().debug_snapshot();
        if (!expect(snapshot.present, "guest AI accel demo expects mapped AI accelerator") ||
            !expect(snapshot.queue_depth == 0, "guest AI accel demo should retire the queue entry") ||
            !expect(snapshot.doorbell_count == 1, "guest AI accel demo should ring one doorbell") ||
            !expect(snapshot.last_fault == AI_ACCEL_FAULT_NONE,
                    "guest AI accel demo should complete without AI faults") ||
            !expect(snapshot.completion_count == 1,
                    "guest AI accel demo should write one completion entry") ||
            !expect(!snapshot.engine_busy,
                    "guest AI accel demo should leave the AI engine idle") ||
            !expect(snapshot.scratchpad_occupancy_bytes == 0,
                    "guest AI accel demo should leave scratchpad occupancy at zero")) {
            return 1;
        }

        uint32_t success_status = 0;
        uint32_t success_queue_depth = 1;
        uint64_t success_submit_queue_base = 0;
        uint64_t success_complete_queue_base = 0;
        uint32_t success_submit_queue_size = 0;
        uint32_t success_submit_queue_head = 0;
        uint32_t success_submit_queue_tail = 0;
        uint32_t success_complete_queue_size = 0;
        uint32_t success_complete_queue_head = 0;
        uint32_t success_complete_queue_tail = 0;
        uint32_t success_irq_status = 0;
        uint32_t success_irq_mask = 0;
        uint32_t success_last_fault = AI_ACCEL_FAULT_EXECUTION;
        uint32_t success_fault_detail = 1;
        if (!load_u32(bus, AI_ACCEL_REG_STATUS, success_status) ||
            !load_u32(bus, AI_ACCEL_REG_QUEUE_DEPTH, success_queue_depth) ||
            !load_counter(bus,
                          AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW,
                          AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH,
                          success_submit_queue_base) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW,
                          AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH,
                          success_complete_queue_base) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, success_submit_queue_size) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_HEAD, success_submit_queue_head) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, success_submit_queue_tail) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, success_complete_queue_size) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_HEAD, success_complete_queue_head) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, success_complete_queue_tail) ||
            !load_u32(bus, AI_ACCEL_REG_IRQ_STATUS, success_irq_status) ||
            !load_u32(bus, AI_ACCEL_REG_IRQ_MASK, success_irq_mask) ||
            !load_u32(bus, AI_ACCEL_REG_LAST_FAULT, success_last_fault) ||
            !load_u32(bus, AI_ACCEL_REG_FAULT_DETAIL, success_fault_detail)) {
            std::fprintf(stderr, "guest AI accel demo could not read success AI control registers\n");
            return 1;
        }

        if (!expect(success_status == AI_ACCEL_STATUS_READY,
                    "guest AI accel demo should return to READY-only status after guest IRQ ack") ||
            !expect(success_queue_depth == 0,
                    "guest AI accel demo should keep MMIO queue depth empty after completion") ||
            !expect(success_submit_queue_base != 0,
                    "guest AI accel demo should program a non-zero submit queue base") ||
            !expect((success_submit_queue_base % 64) == 0,
                    "guest AI accel demo should keep submit queue base 64B aligned") ||
            !expect(success_complete_queue_base != 0,
                    "guest AI accel demo should program a non-zero completion queue base") ||
            !expect((success_complete_queue_base % 64) == 0,
                    "guest AI accel demo should keep completion queue base 64B aligned") ||
            !expect(success_submit_queue_base != success_complete_queue_base,
                    "guest AI accel demo should use distinct submit and completion queue bases") ||
            !expect(success_submit_queue_size == 1,
                    "guest AI accel demo should configure one submit queue entry") ||
            !expect(success_submit_queue_head == 1,
                    "guest AI accel demo should advance submit queue head after completion") ||
            !expect(success_submit_queue_tail == 1,
                    "guest AI accel demo should leave submit queue tail at one entry") ||
            !expect(success_complete_queue_size == 1,
                    "guest AI accel demo should configure one completion queue entry") ||
            !expect(success_complete_queue_head == 1,
                    "guest AI accel demo should advance completion queue head after guest consume") ||
            !expect(success_complete_queue_tail == 1,
                    "guest AI accel demo should advance completion queue tail after device writeback") ||
            !expect(success_irq_status == 0,
                    "guest AI accel demo should clear completion IRQ status after guest ack") ||
            !expect(success_irq_mask == 0,
                    "guest AI accel demo should disable IRQ mask in guest post handler") ||
            !expect(success_last_fault == AI_ACCEL_FAULT_NONE,
                    "guest AI accel demo should keep MMIO last fault clear after successful completion") ||
            !expect(success_fault_detail == 0,
                    "guest AI accel demo should keep fault detail clear after successful completion")) {
            return 1;
        }

        uint64_t doorbell_count = 0;
        uint64_t completion_count = 0;
        uint64_t submission_count = 0;
        uint64_t fault_count = 0;
        uint64_t device_cycles = 0;
        uint64_t dma_cycles = 0;
        uint64_t compute_cycles = 0;
        uint64_t stall_cycles = 0;
        uint64_t dma_load_bytes = 0;
        uint64_t dma_store_bytes = 0;
        if (!load_counter(bus,
                          AI_ACCEL_REG_DOORBELL_COUNT_LOW,
                          AI_ACCEL_REG_DOORBELL_COUNT_HIGH,
                          doorbell_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPLETION_COUNT_LOW,
                          AI_ACCEL_REG_COMPLETION_COUNT_HIGH,
                          completion_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_SUBMISSION_COUNT_LOW,
                          AI_ACCEL_REG_SUBMISSION_COUNT_HIGH,
                          submission_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_FAULT_COUNT_LOW,
                          AI_ACCEL_REG_FAULT_COUNT_HIGH,
                          fault_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DEVICE_CYCLES_LOW,
                          AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                          device_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_CYCLES_LOW,
                          AI_ACCEL_REG_DMA_CYCLES_HIGH,
                          dma_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPUTE_CYCLES_LOW,
                          AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                          compute_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_STALL_CYCLES_LOW,
                          AI_ACCEL_REG_STALL_CYCLES_HIGH,
                          stall_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_LOAD_BYTES_LOW,
                          AI_ACCEL_REG_DMA_LOAD_BYTES_HIGH,
                          dma_load_bytes) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_STORE_BYTES_LOW,
                          AI_ACCEL_REG_DMA_STORE_BYTES_HIGH,
                          dma_store_bytes)) {
            std::fprintf(stderr, "guest AI accel demo could not read AI counters\n");
            return 1;
        }

        if (!expect(doorbell_count == 1, "guest AI accel demo should lock MMIO doorbell_count=1") ||
            !expect(completion_count == 1, "guest AI accel demo should lock MMIO completion_count=1") ||
            !expect(submission_count == 1, "guest AI accel demo should lock MMIO submission_count=1") ||
            !expect(fault_count == 0, "guest AI accel demo should lock MMIO fault_count=0") ||
            !expect(device_cycles == 8, "guest AI accel demo should lock device_cycles=8") ||
            !expect(dma_cycles == 6, "guest AI accel demo should lock dma_cycles=6") ||
            !expect(compute_cycles == 1, "guest AI accel demo should lock compute_cycles=1") ||
            !expect(stall_cycles == 1, "guest AI accel demo should lock stall_cycles=1") ||
            !expect(dma_load_bytes == 12, "guest AI accel demo should lock dma_load_bytes=12") ||
            !expect(dma_store_bytes == 4, "guest AI accel demo should lock dma_store_bytes=4")) {
            return 1;
        }

        const AiAcceleratorProfileSummary& summary = machine.ai_accelerator().profile_summary();
        if (!expect_default_timing_model(summary,
                                         "guest AI accel demo should preserve default timing model metadata") ||
            !expect_submission_timing(summary,
                                      8,
                                      6,
                                      1,
                                      1,
                                      1,
                                      1,
                                      10,
                                      "guest AI accel demo should publish submission timing summary") ||
            !expect_submission_outcome(summary,
                                       AI_ACCEL_FAULT_NONE,
                                       3,
                                       16,
                                       "guest AI accel demo should publish submission outcome summary") ||
            !expect_submission_dma_breakdown(summary,
                                             3,
                                             3,
                                             12,
                                             4,
                                             "guest AI accel demo should publish submission DMA breakdown") ||
            !expect_submission_compile_contract(summary,
                                               AiShapeMode::Static,
                                               0,
                                               2,
                                               2,
                                               0,
                                               1,
                                               1,
                                               0,
                                               0,
                                               0,
                                               16,
                                               1,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               1,
                                               1,
                                               12,
                                               4,
                                               UINT64_C(0x41494343454c0101),
                                               0,
                                               summary.last_submission_graph_package_addr,
                                               summary.last_submission_input_table_addr,
                                               summary.last_submission_output_table_addr,
                                               success_submit_queue_base,
                                               success_complete_queue_base,
                                               180,
                                               0,
                                               0,
                                               0x33,
                                               1,
                                               1,
                                               1,
                                               0,
                                               1,
                                               0,
                                               0,
                                               true,
                                               "guest AI accel demo should publish submission compile contract") ||
            !expect(summary.tile_count == 1,
                    "guest AI accel demo should publish one tile in profile summary") ||
            !expect(summary.scratchpad_peak_bytes == 16,
                    "guest AI accel demo should publish scratchpad peak bytes in profile summary") ||
            !expect(summary.op_summaries.size() == 1,
                    "guest AI accel demo should publish one op summary") ||
            !expect(summary.op_summaries[0].op_index == 0,
                    "guest AI accel demo should publish op index 0") ||
            !expect(summary.op_summaries[0].opcode == AiOpCode::ReduceSum,
                    "guest AI accel demo should publish reduce_sum op summary") ||
            !expect(summary.op_summaries[0].retired_ops == 3,
                    "guest AI accel demo should publish retired ops in op summary") ||
            !expect(summary.op_summaries[0].compute_cycles == 1,
                    "guest AI accel demo should publish compute cycles in op summary") ||
            !expect(summary.op_summaries[0].stall_cycles == 1,
                    "guest AI accel demo should publish stall cycles in op summary") ||
            !expect(summary.op_summaries[0].tile_count == 1,
                    "guest AI accel demo should publish tile count in op summary") ||
            !expect(summary.last_submission_graph_package_addr != 0,
                    "guest AI accel demo should publish non-zero graph package addr") ||
            !expect(summary.last_submission_input_table_addr != 0,
                    "guest AI accel demo should publish non-zero input table addr") ||
            !expect(summary.last_submission_output_table_addr != 0,
                    "guest AI accel demo should publish non-zero output table addr") ||
            !expect((summary.last_submission_input_table_addr % 8) == 0,
                    "guest AI accel demo should publish 8B-aligned input table addr") ||
            !expect((summary.last_submission_output_table_addr % 8) == 0,
                    "guest AI accel demo should publish 8B-aligned output table addr")) {
            return 1;
        }

        if (!store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_CONTROL, AI_ACCEL_CONTROL_RESET,
                       "guest AI accel demo should accept MMIO reset")) {
            return 1;
        }

        const AiAcceleratorProfileSummary& reset_summary = machine.ai_accelerator().profile_summary();
        if (!expect_default_timing_model(reset_summary,
                                         "guest AI accel demo should preserve timing metadata after reset") ||
            !expect_submission_timing(reset_summary,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      "guest AI accel demo should clear submission timing after reset") ||
            !expect_submission_outcome(reset_summary,
                                       AI_ACCEL_FAULT_NONE,
                                       0,
                                       0,
                                       "guest AI accel demo should clear submission outcome after reset") ||
            !expect_submission_dma_breakdown(reset_summary,
                                             0,
                                             0,
                                             0,
                                             0,
                                             "guest AI accel demo should clear DMA breakdown after reset") ||
            !expect_submission_compile_contract(reset_summary,
                                               AiShapeMode::Static,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               0,
                                               false,
                                               "guest AI accel demo should clear compile contract after reset") ||
            !expect(reset_summary.tile_count == 0,
                    "guest AI accel demo should clear tile count after reset") ||
            !expect(reset_summary.scratchpad_peak_bytes == 0,
                    "guest AI accel demo should clear scratchpad peak bytes after reset") ||
            !expect(reset_summary.op_summaries.empty(),
                    "guest AI accel demo should clear op summaries after reset") ||
            !expect(machine.ai_accelerator().completion_count() == 0,
                    "guest AI accel demo should clear completion count after reset") ||
            !expect(machine.ai_accelerator().doorbell_count() == 0,
                    "guest AI accel demo should clear doorbell count after reset") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE,
                    "guest AI accel demo should clear last fault after reset")) {
            return 1;
        }

        uint64_t reset_device_cycles = 1;
        uint64_t reset_dma_cycles = 1;
        uint64_t reset_compute_cycles = 1;
        uint64_t reset_stall_cycles = 1;
        uint64_t reset_submission_count = 1;
        uint64_t reset_fault_count = 1;
        uint64_t reset_dma_load_bytes = 1;
        uint64_t reset_dma_store_bytes = 1;
        if (!load_counter(bus,
                          AI_ACCEL_REG_DEVICE_CYCLES_LOW,
                          AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                          reset_device_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_CYCLES_LOW,
                          AI_ACCEL_REG_DMA_CYCLES_HIGH,
                          reset_dma_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPUTE_CYCLES_LOW,
                          AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                          reset_compute_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_STALL_CYCLES_LOW,
                          AI_ACCEL_REG_STALL_CYCLES_HIGH,
                          reset_stall_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_SUBMISSION_COUNT_LOW,
                          AI_ACCEL_REG_SUBMISSION_COUNT_HIGH,
                          reset_submission_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_FAULT_COUNT_LOW,
                          AI_ACCEL_REG_FAULT_COUNT_HIGH,
                          reset_fault_count) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_LOAD_BYTES_LOW,
                          AI_ACCEL_REG_DMA_LOAD_BYTES_HIGH,
                          reset_dma_load_bytes) ||
            !load_counter(bus,
                          AI_ACCEL_REG_DMA_STORE_BYTES_LOW,
                          AI_ACCEL_REG_DMA_STORE_BYTES_HIGH,
                          reset_dma_store_bytes)) {
            std::fprintf(stderr, "guest AI accel demo could not read AI counters after reset\n");
            return 1;
        }

        if (!expect(reset_device_cycles == 0, "guest AI accel demo should clear device_cycles after reset") ||
            !expect(reset_dma_cycles == 0, "guest AI accel demo should clear dma_cycles after reset") ||
            !expect(reset_compute_cycles == 0, "guest AI accel demo should clear compute_cycles after reset") ||
            !expect(reset_stall_cycles == 0, "guest AI accel demo should clear stall_cycles after reset") ||
            !expect(reset_submission_count == 0,
                    "guest AI accel demo should clear submission_count after reset") ||
            !expect(reset_fault_count == 0,
                    "guest AI accel demo should clear fault_count after reset") ||
            !expect(reset_dma_load_bytes == 0,
                    "guest AI accel demo should clear dma_load_bytes after reset") ||
            !expect(reset_dma_store_bytes == 0,
                    "guest AI accel demo should clear dma_store_bytes after reset")) {
            return 1;
        }

        const DebugAiAcceleratorSnapshot reset_snapshot = machine.ai_accelerator().debug_snapshot();
        if (!expect(reset_snapshot.present, "guest AI accel demo expects mapped AI accelerator after reset") ||
            !expect(reset_snapshot.queue_depth == 0, "guest AI accel demo should clear queue depth after reset") ||
            !expect(reset_snapshot.doorbell_count == 0,
                    "guest AI accel demo should clear doorbell count in debug snapshot after reset") ||
            !expect(reset_snapshot.last_fault == AI_ACCEL_FAULT_NONE,
                    "guest AI accel demo should clear last fault in debug snapshot after reset") ||
            !expect(reset_snapshot.completion_count == 0,
                    "guest AI accel demo should clear completion count in debug snapshot after reset") ||
            !expect(!reset_snapshot.engine_busy,
                    "guest AI accel demo should leave the AI engine idle after reset") ||
            !expect(reset_snapshot.scratchpad_occupancy_bytes == 0,
                    "guest AI accel demo should clear scratchpad occupancy after reset") ||
            !expect(reset_snapshot.dma_load_bytes == 0,
                    "guest AI accel demo should clear debug DMA load bytes after reset") ||
            !expect(reset_snapshot.dma_store_bytes == 0,
                    "guest AI accel demo should clear debug DMA store bytes after reset") ||
            !expect(reset_snapshot.device_cycles == 0,
                    "guest AI accel demo should clear debug device cycles after reset") ||
            !expect(reset_snapshot.dma_cycles == 0,
                    "guest AI accel demo should clear debug dma cycles after reset") ||
            !expect(reset_snapshot.compute_cycles == 0,
                    "guest AI accel demo should clear debug compute cycles after reset") ||
            !expect(reset_snapshot.stall_cycles == 0,
                    "guest AI accel demo should clear debug stall cycles after reset") ||
            !expect(reset_snapshot.busy_cycles == 0,
                    "guest AI accel demo should clear debug busy cycles after reset") ||
            !expect(reset_snapshot.queue_cycles == 0,
                    "guest AI accel demo should clear debug queue cycles after reset") ||
            !expect(reset_snapshot.completion_cycles == 0,
                    "guest AI accel demo should clear debug completion cycles after reset") ||
            !expect(reset_snapshot.effective_ops_per_cycle == 0,
                    "guest AI accel demo should clear debug effective ops after reset") ||
            !expect(reset_snapshot.utilization == 0,
                    "guest AI accel demo should clear debug utilization after reset")) {
            return 1;
        }

        uint32_t reset_status = 0;
        uint32_t reset_queue_depth = 1;
        uint64_t reset_submit_queue_base = 1;
        uint64_t reset_complete_queue_base = 1;
        uint32_t reset_submit_queue_size = 1;
        uint32_t reset_submit_queue_head = 1;
        uint32_t reset_submit_queue_tail = 1;
        uint32_t reset_complete_queue_size = 1;
        uint32_t reset_complete_queue_head = 1;
        uint32_t reset_complete_queue_tail = 1;
        uint32_t reset_irq_status = 1;
        uint32_t reset_irq_mask = 0;
        uint32_t reset_last_fault = AI_ACCEL_FAULT_EXECUTION;
        uint32_t reset_fault_detail = 1;
        if (!load_u32(bus, AI_ACCEL_REG_STATUS, reset_status) ||
            !load_u32(bus, AI_ACCEL_REG_QUEUE_DEPTH, reset_queue_depth) ||
            !load_counter(bus,
                          AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW,
                          AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH,
                          reset_submit_queue_base) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW,
                          AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH,
                          reset_complete_queue_base) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, reset_submit_queue_size) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_HEAD, reset_submit_queue_head) ||
            !load_u32(bus, AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, reset_submit_queue_tail) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, reset_complete_queue_size) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_HEAD, reset_complete_queue_head) ||
            !load_u32(bus, AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, reset_complete_queue_tail) ||
            !load_u32(bus, AI_ACCEL_REG_IRQ_STATUS, reset_irq_status) ||
            !load_u32(bus, AI_ACCEL_REG_IRQ_MASK, reset_irq_mask) ||
            !load_u32(bus, AI_ACCEL_REG_LAST_FAULT, reset_last_fault) ||
            !load_u32(bus, AI_ACCEL_REG_FAULT_DETAIL, reset_fault_detail)) {
            std::fprintf(stderr, "guest AI accel demo could not read AI control registers after reset\n");
            return 1;
        }

        if (!expect(reset_status == AI_ACCEL_STATUS_READY,
                    "guest AI accel demo should restore READY status after reset") ||
            !expect(reset_queue_depth == 0,
                    "guest AI accel demo should clear MMIO queue depth after reset") ||
            !expect(reset_submit_queue_base == 0,
                    "guest AI accel demo should clear submit queue base after reset") ||
            !expect(reset_complete_queue_base == 0,
                    "guest AI accel demo should clear completion queue base after reset") ||
            !expect(reset_submit_queue_size == 0,
                    "guest AI accel demo should clear submit queue size after reset") ||
            !expect(reset_submit_queue_head == 0,
                    "guest AI accel demo should clear submit queue head after reset") ||
            !expect(reset_submit_queue_tail == 0,
                    "guest AI accel demo should clear submit queue tail after reset") ||
            !expect(reset_complete_queue_size == 0,
                    "guest AI accel demo should clear completion queue size after reset") ||
            !expect(reset_complete_queue_head == 0,
                    "guest AI accel demo should clear completion queue head after reset") ||
            !expect(reset_complete_queue_tail == 0,
                    "guest AI accel demo should clear completion queue tail after reset") ||
            !expect(reset_irq_status == 0,
                    "guest AI accel demo should clear IRQ status after reset") ||
            !expect(reset_irq_mask == AI_ACCEL_IRQ_ALL,
                    "guest AI accel demo should restore default IRQ mask after reset") ||
            !expect(reset_last_fault == AI_ACCEL_FAULT_NONE,
                    "guest AI accel demo should clear MMIO last fault after reset") ||
            !expect(reset_fault_detail == 0,
                    "guest AI accel demo should clear fault detail after reset")) {
            return 1;
        }

        std::puts("ai_accel_guest_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
