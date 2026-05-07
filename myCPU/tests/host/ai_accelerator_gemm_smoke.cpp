#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "../../src/devices/ai_accelerator.h"
#include "../../src/devices/ai_graph_package.h"
#include "../../src/devices/ai_submission_queue.h"
#include "../../src/platform/machine.h"

namespace {

constexpr uint64_t kSubmitQueueAddr = MEM_BASE + 0x32000;
constexpr uint64_t kCompleteQueueAddr = MEM_BASE + 0x34000;
constexpr uint64_t kGraphPackageAddr = MEM_BASE + 0x36000;
constexpr uint64_t kFaultGraphPackageAddr = MEM_BASE + 0x36800;
constexpr uint64_t kInputTableAddr = MEM_BASE + 0x38000;
constexpr uint64_t kOutputTableAddr = MEM_BASE + 0x38100;
constexpr uint64_t kLhsTensorAddr = MEM_BASE + 0x3a000;
constexpr uint64_t kRhsTensorAddr = MEM_BASE + 0x3a100;
constexpr uint64_t kOutputTensorAddr = MEM_BASE + 0x3b000;
constexpr uint32_t kDynamicRuntimeShapeOverlapOffset = 0x4;
constexpr uint32_t kDynamicRuntimeShapeSmallOffset = 0x200;
constexpr uint32_t kDynamicRuntimeShapeLargeOffset = 0x240;
constexpr uint32_t kDynamicRuntimeShapeUnalignedOffset = kDynamicRuntimeShapeLargeOffset + 0x41;
constexpr uint32_t kDynamicRuntimeShapeBytes = 2 * static_cast<uint32_t>(kAiRuntimeShapeEntryBytes);
constexpr uint32_t kDynamicRuntimeShapeOutOfWindowOffset =
    AI_ACCEL_MAX_GRAPH_PACKAGE_BYTES - kDynamicRuntimeShapeBytes + 4;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool almost_equal(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 1e-4f;
}

bool store_u32(Bus& bus, uint64_t addr, uint32_t value, const char* message) {
    if (!bus.try_store(addr, value, 4)) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool load_u32(Bus& bus, uint64_t addr, uint32_t expected, const char* message) {
    uint64_t value = 0;
    if (!bus.try_load(addr, 4, value) || static_cast<uint32_t>(value) != expected) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool load_counter(Bus& bus, uint64_t low_reg, uint64_t high_reg, uint64_t& value) {
    uint64_t low = 0;
    uint64_t high = 0;
    return bus.try_load(low_reg, 4, low) && bus.try_load(high_reg, 4, high) &&
           ((value = (high << 32) | static_cast<uint32_t>(low)), true);
}

bool store_bytes(Bus& bus, uint64_t addr, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        if (!bus.try_store(addr + i, bytes[i], 1)) {
            return false;
        }
    }
    return true;
}

bool load_bytes(Bus& bus, uint64_t addr, void* data, size_t size) {
    auto* bytes = static_cast<uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        uint64_t value = 0;
        if (!bus.try_load(addr + i, 1, value)) {
            return false;
        }
        bytes[i] = static_cast<uint8_t>(value & 0xffU);
    }
    return true;
}

bool expect_op_summary(const AiAcceleratorOpProfileSummary& summary,
                       uint16_t expected_index,
                       AiOpCode expected_opcode,
                       uint64_t expected_retired_ops,
                       uint64_t expected_compute_cycles,
                       uint64_t expected_stall_cycles,
                       uint64_t expected_tile_count,
                       const char* context) {
    return expect(summary.op_index == expected_index, context) &&
           expect(summary.opcode == expected_opcode, context) &&
           expect(summary.retired_ops == expected_retired_ops, context) &&
           expect(summary.compute_cycles == expected_compute_cycles, context) &&
           expect(summary.stall_cycles == expected_stall_cycles, context) &&
           expect(summary.tile_count == expected_tile_count, context);
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

bool configure_queue(Bus& bus) {
    return store_u32(bus,
                     AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW,
                     static_cast<uint32_t>(kSubmitQueueAddr),
                     "submit queue base low") &&
           store_u32(bus,
                     AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH,
                     static_cast<uint32_t>(kSubmitQueueAddr >> 32),
                     "submit queue base high") &&
           store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, 4, "submit queue size") &&
           store_u32(bus,
                     AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW,
                     static_cast<uint32_t>(kCompleteQueueAddr),
                     "complete queue base low") &&
           store_u32(bus,
                     AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH,
                     static_cast<uint32_t>(kCompleteQueueAddr >> 32),
                     "complete queue base high") &&
           store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, 4, "complete queue size");
}

bool build_gemm_graph_package(bool fault_pool,
                              std::vector<uint8_t>& bytes,
                              uint32_t& package_bytes,
                              std::string& error) {
    AiGraphPackage package{};
    package.scratchpad_budget_bytes = 48;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp16,
        .role = AiTensorRole::Input,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp16,
        .role = AiTensorRole::Weight,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp32,
        .role = AiTensorRole::Output,
        .rank = 2,
        .dims = {1, 1, 0, 0},
        .tile_dims = {1, 1, 0, 0},
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Gemm,
        .input_dtype = AiDataType::Fp16,
        .accum_dtype = AiDataType::Fp32,
        .input0 = 0,
        .input1 = 1,
        .input2 = kAiInvalidTensorIndex,
        .output = 2,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::PoolMax,
        .input_dtype = AiDataType::Fp32,
        .accum_dtype = AiDataType::Fp32,
        .input0 = 2,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 3,
        .attrs = {fault_pool ? 0 : 2, 2, 2, 2},
    });
    package.dependencies.push_back(AiDependencyEdge{.source_op = 0, .target_op = 1});
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 0,
        .system_offset = 0,
        .scratchpad_offset = 0,
        .byte_size = 8,
        .scratchpad_bytes = 8,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 1,
        .system_offset = 0,
        .scratchpad_offset = 8,
        .byte_size = 8,
        .scratchpad_bytes = 8,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 2,
        .system_offset = 0,
        .scratchpad_offset = 16,
        .byte_size = 16,
        .scratchpad_bytes = 16,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 3,
        .system_offset = 0,
        .scratchpad_offset = 32,
        .byte_size = 4,
        .scratchpad_bytes = 4,
    });
    if (!serialize_ai_graph_package(package, bytes, error)) {
        return false;
    }
    package_bytes = static_cast<uint32_t>(bytes.size());
    return true;
}

bool build_dynamic_gemm_graph_package(std::vector<uint8_t>& bytes,
                                      uint32_t& package_bytes,
                                      std::string& error) {
    AiGraphPackage package{};
    package.shape_mode = AiShapeMode::DynamicBounded;
    package.scratchpad_budget_bytes = 96;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Input,
        .rank = 2,
        .dims = {2, 8, 0, 0},
        .tile_dims = {1, 8, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Weight,
        .rank = 2,
        .dims = {8, 4, 0, 0},
        .tile_dims = {8, 4, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Output,
        .rank = 2,
        .dims = {2, 4, 0, 0},
        .tile_dims = {1, 4, 0, 0},
    });
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{
        .tensor_index = 0,
        .max_tensor_bytes = 16,
    });
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{
        .tensor_index = 2,
        .max_tensor_bytes = 32,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Gemm,
        .input_dtype = AiDataType::Int8,
        .accum_dtype = AiDataType::Int32,
        .input0 = 0,
        .input1 = 1,
        .input2 = kAiInvalidTensorIndex,
        .output = 2,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 0,
        .system_offset = 0,
        .scratchpad_offset = 0,
        .byte_size = 16,
        .scratchpad_bytes = 16,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 1,
        .system_offset = 0,
        .scratchpad_offset = 16,
        .byte_size = 32,
        .scratchpad_bytes = 32,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 2,
        .system_offset = 0,
        .scratchpad_offset = 48,
        .byte_size = 32,
        .scratchpad_bytes = 32,
    });
    if (!serialize_ai_graph_package(package, bytes, error)) {
        return false;
    }
    package_bytes = static_cast<uint32_t>(bytes.size());
    return true;
}

bool build_softmax_graph_package(std::vector<uint8_t>& bytes,
                                 uint32_t& package_bytes,
                                 std::string& error) {
    AiGraphPackage package{};
    package.scratchpad_budget_bytes = 64;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp32,
        .role = AiTensorRole::Input,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp32,
        .role = AiTensorRole::Output,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Softmax,
        .input_dtype = AiDataType::Fp32,
        .accum_dtype = AiDataType::Fp32,
        .input0 = 0,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 1,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 0,
        .system_offset = 0,
        .scratchpad_offset = 0,
        .byte_size = 16,
        .scratchpad_bytes = 16,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 1,
        .system_offset = 0,
        .scratchpad_offset = 32,
        .byte_size = 16,
        .scratchpad_bytes = 16,
    });
    if (!serialize_ai_graph_package(package, bytes, error)) {
        return false;
    }
    package_bytes = static_cast<uint32_t>(bytes.size());
    return true;
}

bool build_dynamic_runtime_shape_table(uint32_t rows,
                                       std::vector<uint8_t>& bytes,
                                       std::string& error) {
    const std::vector<AiRuntimeShapeEntry> runtime_shapes{
        AiRuntimeShapeEntry{
            .tensor_index = 0,
            .rank = 2,
            .dims = {rows, 8, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 2,
            .rank = 2,
            .dims = {rows, 4, 0, 0},
        },
    };
    return serialize_ai_runtime_shape_table(runtime_shapes, bytes, error);
}

bool tick_until_tail(Bus& bus,
                     uint32_t expected_tail,
                     uint64_t& prev_device_cycles,
                     uint64_t& prev_dma_cycles,
                     uint64_t& prev_compute_cycles,
                     uint64_t& prev_stall_cycles) {
    for (int i = 0; i < 128; ++i) {
        bus.tick();

        uint64_t device_cycles = 0;
        uint64_t dma_cycles = 0;
        uint64_t compute_cycles = 0;
        uint64_t stall_cycles = 0;
        if (!load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                          device_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_HIGH,
                          dma_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                          compute_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_HIGH,
                          stall_cycles)) {
            return false;
        }
        if (!expect(device_cycles >= prev_device_cycles, "expected monotonic device cycles") ||
            !expect(dma_cycles >= prev_dma_cycles, "expected monotonic DMA cycles") ||
            !expect(compute_cycles >= prev_compute_cycles, "expected monotonic compute cycles") ||
            !expect(stall_cycles >= prev_stall_cycles, "expected monotonic stall cycles")) {
            return false;
        }
        prev_device_cycles = device_cycles;
        prev_dma_cycles = dma_cycles;
        prev_compute_cycles = compute_cycles;
        prev_stall_cycles = stall_cycles;

        uint64_t completion_tail = 0;
        if (!bus.try_load(AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, 4, completion_tail)) {
            return false;
        }
        if (completion_tail == expected_tail) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main() {
    try {
        Machine machine;
        Bus& bus = machine.bus();

        if (!load_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_MAGIC, AI_ACCEL_MMIO_MAGIC, "expected mapped AI accelerator") ||
            !store_u32(bus,
                       PLIC_BASE + PLIC_PRIORITY_OFFSET(AI_ACCEL_PLIC_SOURCE),
                       1,
                       "AI PLIC priority") ||
            !store_u32(bus,
                       PLIC_BASE + PLIC_ENABLE_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                       (1U << AI_ACCEL_PLIC_SOURCE),
                       "AI PLIC supervisor enable") ||
            !store_u32(bus,
                       PLIC_BASE + PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                       0,
                       "AI PLIC threshold") ||
            !configure_queue(bus)) {
            return 1;
        }

        std::vector<uint8_t> graph_package_bytes{};
        std::vector<uint8_t> fault_graph_package_bytes{};
        uint32_t graph_package_size = 0;
        uint32_t fault_graph_package_size = 0;
        std::string error;
        if (!build_gemm_graph_package(false, graph_package_bytes, graph_package_size, error) ||
            !build_gemm_graph_package(true, fault_graph_package_bytes, fault_graph_package_size, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }

        const std::array<uint64_t, 4> input_table{{kLhsTensorAddr, kRhsTensorAddr, 0, 0}};
        const std::array<uint64_t, 4> output_table{{0, 0, 0, kOutputTensorAddr}};
        const std::array<uint16_t, 4> lhs_tensor{{0x3C00, 0x4000, 0x3800, 0xBC00}};
        const std::array<uint16_t, 4> rhs_tensor{{0x3C00, 0x4000, 0x3E00, 0x3800}};
        const float zero_output = 0.0f;

        const AiSubmissionDescriptor success_descriptor{
            .token = 0x47454D4DULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 29,
        };
        const AiSubmissionDescriptor fault_descriptor{
            .token = 0x4641554CULL,
            .graph_package_addr = kFaultGraphPackageAddr,
            .graph_package_bytes = fault_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 31,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> success_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> fault_descriptor_bytes{};
        encode_ai_submission_descriptor(success_descriptor, success_descriptor_bytes);
        encode_ai_submission_descriptor(fault_descriptor, fault_descriptor_bytes);

        if (!store_bytes(bus, kGraphPackageAddr, graph_package_bytes.data(), graph_package_bytes.size()) ||
            !store_bytes(bus,
                         kFaultGraphPackageAddr,
                         fault_graph_package_bytes.data(),
                         fault_graph_package_bytes.size()) ||
            !store_bytes(bus, kInputTableAddr, input_table.data(), sizeof(input_table)) ||
            !store_bytes(bus, kOutputTableAddr, output_table.data(), sizeof(output_table)) ||
            !store_bytes(bus, kLhsTensorAddr, lhs_tensor.data(), sizeof(lhs_tensor)) ||
            !store_bytes(bus, kRhsTensorAddr, rhs_tensor.data(), sizeof(rhs_tensor)) ||
            !store_bytes(bus, kOutputTensorAddr, &zero_output, sizeof(zero_output))) {
            return 1;
        }

        uint64_t prev_device_cycles = 0;
        uint64_t prev_dma_cycles = 0;
        uint64_t prev_compute_cycles = 0;
        uint64_t prev_stall_cycles = 0;

        if (!store_bytes(bus, kSubmitQueueAddr, success_descriptor_bytes.data(), success_descriptor_bytes.size()) ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "submit tail 1") ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "doorbell 1") ||
            !tick_until_tail(bus,
                             1,
                             prev_device_cycles,
                             prev_dma_cycles,
                             prev_compute_cycles,
                             prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> success_completion_bytes{};
        AiCompletionEntry success_completion{};
        float output_tensor = 0.0f;
        uint64_t device_cycles = 0;
        uint64_t dma_cycles = 0;
        uint64_t compute_cycles = 0;
        uint64_t stall_cycles = 0;
        if (!load_bytes(bus, kCompleteQueueAddr, success_completion_bytes.data(), success_completion_bytes.size()) ||
            !load_bytes(bus, kOutputTensorAddr, &output_tensor, sizeof(output_tensor)) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                          device_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_HIGH,
                          dma_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                          compute_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_HIGH,
                          stall_cycles)) {
            return 1;
        }
        decode_ai_completion_entry(success_completion_bytes, success_completion);
        const AiAcceleratorProfileSummary& success_profile = machine.ai_accelerator().profile_summary();
        if (!expect(success_completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected GEMM completion success") ||
            !expect(success_completion.retired_ops == 12, "expected GEMM retired ops") ||
            !expect(success_completion.bytes_moved == 20, "expected GEMM DMA byte accounting") ||
            !expect(almost_equal(output_tensor, 4.0f), "expected GEMM output tensor") ||
            !expect(device_cycles == 13, "expected GEMM device cycles") ||
            !expect(dma_cycles == 9, "expected GEMM DMA cycles") ||
            !expect(compute_cycles == 2, "expected GEMM compute cycles") ||
            !expect(stall_cycles == 2, "expected GEMM stall cycles") ||
            !expect_default_timing_model(success_profile, "expected default GEMM timing model") ||
            !expect_submission_timing(success_profile, 13, 9, 2, 2, 1, 1, 15,
                                      "expected GEMM submission timing summary") ||
            !expect_submission_outcome(success_profile, AI_ACCEL_FAULT_NONE, 12, 20,
                                       "expected GEMM submission outcome summary") ||
            !expect_submission_dma_breakdown(success_profile, 6, 3, 16, 4,
                                            "expected GEMM submission DMA breakdown") ||
            !expect_submission_compile_contract(success_profile,
                                               AiShapeMode::Static,
                                               0,
                                               4,
                                               4,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               1,
                                               48,
                                               2,
                                               1,
                                               1,
                                               1,
                                               2,
                                               1,
                                               1,
                                               2,
                                               1,
                                               16,
                                               4,
                                               success_descriptor.token,
                                               success_descriptor.flags,
                                               success_descriptor.graph_package_addr,
                                               success_descriptor.input_table_addr,
                                               success_descriptor.output_table_addr,
                                               kSubmitQueueAddr,
                                               kCompleteQueueAddr,
                                               graph_package_size,
                                               0,
                                               0,
                                               29,
                                               1,
                                               4,
                                               4,
                                               0,
                                               1,
                                               0,
                                               0,
                                               true,
                                               "expected GEMM submission compile contract") ||
            !expect(success_profile.tile_count == 2, "expected GEMM aggregate tile count") ||
            !expect(success_profile.scratchpad_peak_bytes == 36,
                    "expected GEMM aggregate scratchpad peak bytes") ||
            !expect(success_profile.op_summaries.size() == 2, "expected two GEMM op summaries") ||
            !expect_op_summary(success_profile.op_summaries[0], 0, AiOpCode::Gemm, 8, 1, 1, 1,
                               "expected GEMM op profile summary") ||
            !expect_op_summary(success_profile.op_summaries[1], 1, AiOpCode::PoolMax, 4, 1, 1, 1,
                               "expected pool op profile summary")) {
            return 1;
        }

        if (!store_bytes(bus,
                         kSubmitQueueAddr + kAiSubmissionDescriptorBytes,
                         fault_descriptor_bytes.data(),
                         fault_descriptor_bytes.size()) ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 2, "submit tail 2") ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "doorbell 2") ||
            !tick_until_tail(bus,
                             2,
                             prev_device_cycles,
                             prev_dma_cycles,
                             prev_compute_cycles,
                             prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> fault_completion_bytes{};
        AiCompletionEntry fault_completion{};
        uint64_t dma_load_bytes = 0;
        uint64_t dma_store_bytes = 0;
        if (!load_bytes(bus,
                        kCompleteQueueAddr + kAiCompletionEntryBytes,
                        fault_completion_bytes.data(),
                        fault_completion_bytes.size()) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                          device_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_HIGH,
                          dma_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                          compute_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_HIGH,
                          stall_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_LOAD_BYTES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_LOAD_BYTES_HIGH,
                          dma_load_bytes) ||
            !load_counter(bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_STORE_BYTES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_DMA_STORE_BYTES_HIGH,
                          dma_store_bytes)) {
            return 1;
        }
        decode_ai_completion_entry(fault_completion_bytes, fault_completion);
        const AiAcceleratorProfileSummary& fault_profile = machine.ai_accelerator().profile_summary();
        if (!expect(fault_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT, "expected GEMM fault completion") ||
            !expect(fault_completion.fault_code == AI_ACCEL_FAULT_ILLEGAL_OP, "expected GEMM fault code") ||
            !expect(fault_completion.retired_ops == 0, "expected zero retired ops on fault") ||
            !expect(fault_completion.bytes_moved == 16, "expected fault DMA byte accounting") ||
            !expect(device_cycles == 19, "expected cumulative GEMM device cycles") ||
            !expect(dma_cycles == 15, "expected cumulative GEMM DMA cycles") ||
            !expect(compute_cycles == 2, "expected cumulative GEMM compute cycles") ||
            !expect(stall_cycles == 2, "expected cumulative GEMM stall cycles") ||
            !expect(dma_load_bytes == 32, "expected cumulative GEMM DMA load bytes") ||
            !expect(dma_store_bytes == 4, "expected cumulative GEMM DMA store bytes") ||
            !expect_default_timing_model(fault_profile, "expected stable GEMM timing model after fault") ||
            !expect_submission_timing(fault_profile, 6, 6, 0, 0, 1, 1, 8,
                                      "expected fault GEMM submission timing summary") ||
            !expect_submission_outcome(fault_profile, AI_ACCEL_FAULT_ILLEGAL_OP, 0, 16,
                                       "expected fault GEMM submission outcome summary") ||
            !expect_submission_dma_breakdown(fault_profile, 6, 0, 16, 0,
                                            "expected fault GEMM submission DMA breakdown") ||
            !expect_submission_compile_contract(fault_profile,
                                               AiShapeMode::Static,
                                               0,
                                               4,
                                               4,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               1,
                                               48,
                                               2,
                                               1,
                                               1,
                                               1,
                                               2,
                                               1,
                                               1,
                                               2,
                                               1,
                                               16,
                                               4,
                                               success_descriptor.token,
                                               success_descriptor.flags,
                                               success_descriptor.graph_package_addr,
                                               success_descriptor.input_table_addr,
                                               success_descriptor.output_table_addr,
                                               kSubmitQueueAddr,
                                               kCompleteQueueAddr,
                                               graph_package_size,
                                               0,
                                               0,
                                               29,
                                               1,
                                               4,
                                               4,
                                               0,
                                               1,
                                               0,
                                               0,
                                               true,
                                               "expected stable GEMM compile contract after fault") ||
            !expect(fault_profile.tile_count == 2, "expected GEMM tile profile to remain stable on fault") ||
            !expect(fault_profile.scratchpad_peak_bytes == 36,
                    "expected GEMM scratchpad peak to remain stable on fault") ||
            !expect(fault_profile.op_summaries.size() == 2,
                    "expected GEMM op summaries to remain stable on fault") ||
            !expect_op_summary(fault_profile.op_summaries[0], 0, AiOpCode::Gemm, 8, 1, 1, 1,
                               "expected stable GEMM op profile after fault") ||
            !expect_op_summary(fault_profile.op_summaries[1], 1, AiOpCode::PoolMax, 4, 1, 1, 1,
                               "expected stable pool op profile after fault") ||
            !expect(machine.ai_accelerator().completion_count() == 2, "expected GEMM completion count") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_ILLEGAL_OP, "expected last GEMM fault") ||
            !expect(machine.plic().supervisor_has_pending(), "expected GEMM IRQ pending")) {
            return 1;
        }

        Machine softmax_machine;
        Bus& softmax_bus = softmax_machine.bus();
        if (!load_u32(softmax_bus,
                      AI_ACCEL_BASE + AI_ACCEL_REG_MAGIC,
                      AI_ACCEL_MMIO_MAGIC,
                      "expected mapped softmax AI accelerator") ||
            !configure_queue(softmax_bus)) {
            return 1;
        }

        std::vector<uint8_t> softmax_graph_package_bytes{};
        uint32_t softmax_graph_package_size = 0;
        if (!build_softmax_graph_package(softmax_graph_package_bytes,
                                         softmax_graph_package_size,
                                         error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }

        const std::array<uint64_t, 2> softmax_input_table{{kLhsTensorAddr, 0}};
        const std::array<uint64_t, 2> softmax_output_table{{0, kOutputTensorAddr}};
        const std::array<float, 4> softmax_input{{0.0f, 0.0f, 7.0f, 7.0f}};
        const std::array<float, 4> softmax_zero_output{{0.0f, 0.0f, 0.0f, 0.0f}};
        const AiSubmissionDescriptor softmax_descriptor{
            .token = 0x534F4654ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = softmax_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 59,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> softmax_descriptor_bytes{};
        encode_ai_submission_descriptor(softmax_descriptor, softmax_descriptor_bytes);

        uint64_t softmax_prev_device_cycles = 0;
        uint64_t softmax_prev_dma_cycles = 0;
        uint64_t softmax_prev_compute_cycles = 0;
        uint64_t softmax_prev_stall_cycles = 0;
        if (!store_bytes(softmax_bus,
                         kGraphPackageAddr,
                         softmax_graph_package_bytes.data(),
                         softmax_graph_package_bytes.size()) ||
            !store_bytes(softmax_bus, kInputTableAddr, softmax_input_table.data(), sizeof(softmax_input_table)) ||
            !store_bytes(softmax_bus, kOutputTableAddr, softmax_output_table.data(), sizeof(softmax_output_table)) ||
            !store_bytes(softmax_bus, kLhsTensorAddr, softmax_input.data(), sizeof(softmax_input)) ||
            !store_bytes(softmax_bus, kOutputTensorAddr, softmax_zero_output.data(), sizeof(softmax_zero_output)) ||
            !store_bytes(softmax_bus,
                         kSubmitQueueAddr,
                         softmax_descriptor_bytes.data(),
                         softmax_descriptor_bytes.size()) ||
            !store_u32(softmax_bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "softmax submit tail") ||
            !store_u32(softmax_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "softmax doorbell") ||
            !tick_until_tail(softmax_bus,
                             1,
                             softmax_prev_device_cycles,
                             softmax_prev_dma_cycles,
                             softmax_prev_compute_cycles,
                             softmax_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> softmax_completion_bytes{};
        AiCompletionEntry softmax_completion{};
        std::array<float, 4> softmax_output{};
        uint64_t softmax_compute_cycles = 0;
        uint64_t softmax_stall_cycles = 0;
        if (!load_bytes(softmax_bus,
                        kCompleteQueueAddr,
                        softmax_completion_bytes.data(),
                        softmax_completion_bytes.size()) ||
            !load_bytes(softmax_bus, kOutputTensorAddr, softmax_output.data(), sizeof(softmax_output)) ||
            !load_counter(softmax_bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_COMPUTE_CYCLES_HIGH,
                          softmax_compute_cycles) ||
            !load_counter(softmax_bus,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_LOW,
                          AI_ACCEL_BASE + AI_ACCEL_REG_STALL_CYCLES_HIGH,
                          softmax_stall_cycles)) {
            return 1;
        }
        decode_ai_completion_entry(softmax_completion_bytes, softmax_completion);
        const AiAcceleratorProfileSummary& softmax_profile =
            softmax_machine.ai_accelerator().profile_summary();
        if (!expect(softmax_completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS,
                    "expected Softmax completion success") ||
            !expect(softmax_completion.retired_ops == 4,
                    "expected Softmax retired ops") ||
            !expect(softmax_completion.bytes_moved == 32,
                    "expected Softmax DMA byte accounting") ||
            !expect(almost_equal(softmax_output[0], 0.5f) &&
                        almost_equal(softmax_output[1], 0.5f) &&
                        almost_equal(softmax_output[2], 0.5f) &&
                        almost_equal(softmax_output[3], 0.5f),
                    "expected Softmax output tensor") ||
            !expect(softmax_compute_cycles == 1,
                    "expected Softmax compute cycles") ||
            !expect(softmax_stall_cycles == 1,
                    "expected Softmax stall cycles") ||
            !expect_default_timing_model(softmax_profile, "expected default Softmax timing model") ||
            !expect_submission_timing(softmax_profile, 8, 6, 1, 1, 1, 1, 10,
                                      "expected Softmax submission timing summary") ||
            !expect_submission_outcome(softmax_profile, AI_ACCEL_FAULT_NONE, 4, 32,
                                       "expected Softmax submission outcome summary") ||
            !expect_submission_dma_breakdown(softmax_profile, 3, 3, 16, 16,
                                            "expected Softmax submission DMA breakdown") ||
            !expect_submission_compile_contract(softmax_profile,
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
                                               64,
                                               1,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               1,
                                               1,
                                               16,
                                               16,
                                               softmax_descriptor.token,
                                               softmax_descriptor.flags,
                                               softmax_descriptor.graph_package_addr,
                                               softmax_descriptor.input_table_addr,
                                               softmax_descriptor.output_table_addr,
                                               kSubmitQueueAddr,
                                               kCompleteQueueAddr,
                                               softmax_graph_package_size,
                                               0,
                                               0,
                                               59,
                                               1,
                                               4,
                                               4,
                                               0,
                                               1,
                                               0,
                                               0,
                                               true,
                                               "expected Softmax submission compile contract") ||
            !expect(softmax_profile.tile_count == 1,
                    "expected Softmax aggregate tile count") ||
            !expect(softmax_profile.scratchpad_peak_bytes == 48,
                    "expected Softmax scratchpad peak bytes") ||
            !expect(softmax_profile.op_summaries.size() == 1,
                    "expected one Softmax op summary") ||
            !expect_op_summary(softmax_profile.op_summaries[0], 0, AiOpCode::Softmax, 4, 1, 1, 1,
                               "expected Softmax op profile summary")) {
            return 1;
        }

        Machine dynamic_machine;
        Bus& dynamic_bus = dynamic_machine.bus();
        if (!load_u32(dynamic_bus,
                      AI_ACCEL_BASE + AI_ACCEL_REG_MAGIC,
                      AI_ACCEL_MMIO_MAGIC,
                      "expected mapped dynamic AI accelerator") ||
            !configure_queue(dynamic_bus)) {
            return 1;
        }

        std::vector<uint8_t> dynamic_graph_package_bytes{};
        std::vector<uint8_t> small_runtime_shape_bytes{};
        std::vector<uint8_t> large_runtime_shape_bytes{};
        uint32_t dynamic_graph_package_size = 0;
        if (!build_dynamic_gemm_graph_package(dynamic_graph_package_bytes,
                                              dynamic_graph_package_size,
                                              error) ||
            !build_dynamic_runtime_shape_table(1, small_runtime_shape_bytes, error) ||
            !build_dynamic_runtime_shape_table(2, large_runtime_shape_bytes, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }

        const std::array<uint64_t, 3> dynamic_input_table{{kLhsTensorAddr, kRhsTensorAddr, 0}};
        const std::array<uint64_t, 3> dynamic_output_table{{0, 0, kOutputTensorAddr}};
        const std::array<int8_t, 16> dynamic_lhs_tensor{{1, 2, 3, 4, 5, 6, 7, 8, -1, 0, 1, 2, 3, 4, 5, 6}};
        const std::array<int8_t, 32> dynamic_rhs_tensor{{
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 1,
        }};
        const std::array<int32_t, 8> expected_small_output{{1, 2, 3, 8, 0, 0, 0, 0}};
        const std::array<int32_t, 8> expected_large_output{{1, 2, 3, 8, -1, 0, 1, 6}};
        const std::array<int32_t, 8> zero_dynamic_output{{0, 0, 0, 0, 0, 0, 0, 0}};

        const AiSubmissionDescriptor dynamic_small_descriptor{
            .token = 0x44594E31ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 41,
            .runtime_shape_table_offset = kDynamicRuntimeShapeSmallOffset,
        };
        const AiSubmissionDescriptor dynamic_large_descriptor{
            .token = 0x44594E32ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 43,
            .runtime_shape_table_offset = kDynamicRuntimeShapeLargeOffset,
        };
        const AiSubmissionDescriptor dynamic_missing_shape_descriptor{
            .token = 0x44594E46ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 47,
        };
        const AiSubmissionDescriptor dynamic_unaligned_shape_descriptor{
            .token = 0x44594E55ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 49,
            .runtime_shape_table_offset = kDynamicRuntimeShapeUnalignedOffset,
        };
        const AiSubmissionDescriptor dynamic_overlap_shape_descriptor{
            .token = 0x44594E4FULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 51,
            .runtime_shape_table_offset = kDynamicRuntimeShapeOverlapOffset,
        };
        const AiSubmissionDescriptor dynamic_out_of_window_shape_descriptor{
            .token = 0x44594E57ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 53,
            .runtime_shape_table_offset = kDynamicRuntimeShapeOutOfWindowOffset,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_small_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_large_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_missing_shape_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_unaligned_shape_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_overlap_shape_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_out_of_window_shape_descriptor_bytes{};
        encode_ai_submission_descriptor(dynamic_small_descriptor, dynamic_small_descriptor_bytes);
        encode_ai_submission_descriptor(dynamic_large_descriptor, dynamic_large_descriptor_bytes);
        encode_ai_submission_descriptor(dynamic_missing_shape_descriptor, dynamic_missing_shape_descriptor_bytes);
        encode_ai_submission_descriptor(dynamic_unaligned_shape_descriptor,
                                        dynamic_unaligned_shape_descriptor_bytes);
        encode_ai_submission_descriptor(dynamic_overlap_shape_descriptor,
                                        dynamic_overlap_shape_descriptor_bytes);
        encode_ai_submission_descriptor(dynamic_out_of_window_shape_descriptor,
                                        dynamic_out_of_window_shape_descriptor_bytes);

        if (!store_bytes(dynamic_bus,
                         kGraphPackageAddr,
                         dynamic_graph_package_bytes.data(),
                         dynamic_graph_package_bytes.size()) ||
            !store_bytes(dynamic_bus,
                         kGraphPackageAddr + kDynamicRuntimeShapeSmallOffset,
                         small_runtime_shape_bytes.data(),
                         small_runtime_shape_bytes.size()) ||
            !store_bytes(dynamic_bus,
                         kGraphPackageAddr + kDynamicRuntimeShapeLargeOffset,
                         large_runtime_shape_bytes.data(),
                         large_runtime_shape_bytes.size()) ||
            !store_bytes(dynamic_bus,
                         kGraphPackageAddr + kDynamicRuntimeShapeUnalignedOffset,
                         large_runtime_shape_bytes.data(),
                         large_runtime_shape_bytes.size()) ||
            !store_bytes(dynamic_bus, kInputTableAddr, dynamic_input_table.data(), sizeof(dynamic_input_table)) ||
            !store_bytes(dynamic_bus, kOutputTableAddr, dynamic_output_table.data(), sizeof(dynamic_output_table)) ||
            !store_bytes(dynamic_bus, kLhsTensorAddr, dynamic_lhs_tensor.data(), sizeof(dynamic_lhs_tensor)) ||
            !store_bytes(dynamic_bus, kRhsTensorAddr, dynamic_rhs_tensor.data(), sizeof(dynamic_rhs_tensor)) ||
            !store_bytes(dynamic_bus,
                         kOutputTensorAddr,
                         zero_dynamic_output.data(),
                         sizeof(zero_dynamic_output))) {
            return 1;
        }

        uint64_t dynamic_prev_device_cycles = 0;
        uint64_t dynamic_prev_dma_cycles = 0;
        uint64_t dynamic_prev_compute_cycles = 0;
        uint64_t dynamic_prev_stall_cycles = 0;

        if (!store_bytes(dynamic_bus,
                         kSubmitQueueAddr,
                         dynamic_small_descriptor_bytes.data(),
                         dynamic_small_descriptor_bytes.size()) ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "dynamic submit tail 1") ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "dynamic doorbell 1") ||
            !tick_until_tail(dynamic_bus,
                             1,
                             dynamic_prev_device_cycles,
                             dynamic_prev_dma_cycles,
                             dynamic_prev_compute_cycles,
                             dynamic_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> dynamic_small_completion_bytes{};
        AiCompletionEntry dynamic_small_completion{};
        std::array<int32_t, 8> dynamic_small_output{};
        if (!load_bytes(dynamic_bus,
                        kCompleteQueueAddr,
                        dynamic_small_completion_bytes.data(),
                        dynamic_small_completion_bytes.size()) ||
            !load_bytes(dynamic_bus,
                        kOutputTensorAddr,
                        dynamic_small_output.data(),
                        sizeof(dynamic_small_output))) {
            return 1;
        }
        decode_ai_completion_entry(dynamic_small_completion_bytes, dynamic_small_completion);
        const AiAcceleratorProfileSummary& dynamic_small_profile =
            dynamic_machine.ai_accelerator().profile_summary();
        if (!expect(dynamic_small_completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS,
                    "expected dynamic GEMM small completion success") ||
            !expect(dynamic_small_completion.retired_ops == 32,
                    "expected dynamic GEMM small retired ops") ||
            !expect(dynamic_small_completion.bytes_moved == 56,
                    "expected dynamic GEMM small DMA byte accounting") ||
            !expect(dynamic_small_output == expected_small_output,
                    "expected dynamic GEMM small output tensor") ||
            !expect_default_timing_model(dynamic_small_profile,
                                         "expected default dynamic GEMM small timing model") ||
            !expect_submission_timing(dynamic_small_profile, 12, 10, 1, 1, 1, 1, 14,
                                      "expected dynamic GEMM small timing summary") ||
            !expect_submission_outcome(dynamic_small_profile, AI_ACCEL_FAULT_NONE, 32, 56,
                                       "expected dynamic GEMM small outcome summary") ||
            !expect_submission_dma_breakdown(dynamic_small_profile, 7, 3, 40, 16,
                                            "expected dynamic GEMM small DMA breakdown") ||
            !expect_submission_compile_contract(dynamic_small_profile,
                                               AiShapeMode::DynamicBounded,
                                               2,
                                               3,
                                               3,
                                               2,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               96,
                                               1,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               2,
                                               1,
                                               48,
                                               32,
                                               dynamic_small_descriptor.token,
                                               dynamic_small_descriptor.flags,
                                               dynamic_small_descriptor.graph_package_addr,
                                               dynamic_small_descriptor.input_table_addr,
                                               dynamic_small_descriptor.output_table_addr,
                                               kSubmitQueueAddr,
                                               kCompleteQueueAddr,
                                               dynamic_graph_package_size,
                                               kDynamicRuntimeShapeSmallOffset,
                                               kGraphPackageAddr + kDynamicRuntimeShapeSmallOffset,
                                               41,
                                               1,
                                               4,
                                               4,
                                               0,
                                               1,
                                               0,
                                               0,
                                               true,
                                               "expected dynamic GEMM small compile contract") ||
            !expect(dynamic_small_profile.tile_count == 1,
                    "expected dynamic GEMM small aggregate tile count") ||
            !expect(dynamic_small_profile.scratchpad_peak_bytes == 64,
                    "expected dynamic GEMM small scratchpad peak bytes") ||
            !expect(dynamic_small_profile.op_summaries.size() == 1,
                    "expected one dynamic GEMM small op summary") ||
            !expect_op_summary(dynamic_small_profile.op_summaries[0], 0, AiOpCode::Gemm, 32, 1, 1, 1,
                               "expected dynamic GEMM small op profile summary")) {
            return 1;
        }

        if (!store_bytes(dynamic_bus,
                         kSubmitQueueAddr + kAiSubmissionDescriptorBytes,
                         dynamic_large_descriptor_bytes.data(),
                         dynamic_large_descriptor_bytes.size()) ||
            !store_bytes(dynamic_bus,
                         kOutputTensorAddr,
                         zero_dynamic_output.data(),
                         sizeof(zero_dynamic_output)) ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 2, "dynamic submit tail 2") ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "dynamic doorbell 2") ||
            !tick_until_tail(dynamic_bus,
                             2,
                             dynamic_prev_device_cycles,
                             dynamic_prev_dma_cycles,
                             dynamic_prev_compute_cycles,
                             dynamic_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> dynamic_large_completion_bytes{};
        AiCompletionEntry dynamic_large_completion{};
        std::array<int32_t, 8> dynamic_large_output{};
        if (!load_bytes(dynamic_bus,
                        kCompleteQueueAddr + kAiCompletionEntryBytes,
                        dynamic_large_completion_bytes.data(),
                        dynamic_large_completion_bytes.size()) ||
            !load_bytes(dynamic_bus,
                        kOutputTensorAddr,
                        dynamic_large_output.data(),
                        sizeof(dynamic_large_output))) {
            return 1;
        }
        decode_ai_completion_entry(dynamic_large_completion_bytes, dynamic_large_completion);
        const AiAcceleratorProfileSummary& dynamic_large_profile =
            dynamic_machine.ai_accelerator().profile_summary();
        if (!expect(dynamic_large_completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS,
                    "expected dynamic GEMM large completion success") ||
            !expect(dynamic_large_completion.retired_ops == 64,
                    "expected dynamic GEMM large retired ops") ||
            !expect(dynamic_large_completion.bytes_moved == 80,
                    "expected dynamic GEMM large DMA byte accounting") ||
            !expect(dynamic_large_output == expected_large_output,
                    "expected dynamic GEMM large output tensor") ||
            !expect_default_timing_model(dynamic_large_profile,
                                         "expected default dynamic GEMM large timing model") ||
            !expect_submission_timing(dynamic_large_profile, 15, 11, 2, 2, 1, 1, 17,
                                      "expected dynamic GEMM large timing summary") ||
            !expect_submission_outcome(dynamic_large_profile, AI_ACCEL_FAULT_NONE, 64, 80,
                                       "expected dynamic GEMM large outcome summary") ||
            !expect_submission_dma_breakdown(dynamic_large_profile, 7, 4, 48, 32,
                                            "expected dynamic GEMM large DMA breakdown") ||
            !expect_submission_compile_contract(dynamic_large_profile,
                                               AiShapeMode::DynamicBounded,
                                               2,
                                               3,
                                               3,
                                               2,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               96,
                                               1,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               2,
                                               1,
                                               48,
                                               32,
                                               dynamic_large_descriptor.token,
                                               dynamic_large_descriptor.flags,
                                               dynamic_large_descriptor.graph_package_addr,
                                               dynamic_large_descriptor.input_table_addr,
                                               dynamic_large_descriptor.output_table_addr,
                                               kSubmitQueueAddr,
                                               kCompleteQueueAddr,
                                               dynamic_graph_package_size,
                                               kDynamicRuntimeShapeLargeOffset,
                                               kGraphPackageAddr + kDynamicRuntimeShapeLargeOffset,
                                               43,
                                               1,
                                               4,
                                               4,
                                               1,
                                               2,
                                               0,
                                               1,
                                               true,
                                               "expected dynamic GEMM large compile contract") ||
            !expect(dynamic_large_profile.tile_count == 2,
                    "expected dynamic GEMM large aggregate tile count") ||
            !expect(dynamic_large_profile.scratchpad_peak_bytes == 80,
                    "expected dynamic GEMM large scratchpad peak bytes") ||
            !expect(dynamic_large_profile.op_summaries.size() == 1,
                    "expected one dynamic GEMM large op summary") ||
            !expect_op_summary(dynamic_large_profile.op_summaries[0], 0, AiOpCode::Gemm, 64, 2, 2, 2,
                               "expected dynamic GEMM large op profile summary")) {
            return 1;
        }

        if (!store_bytes(dynamic_bus,
                         kSubmitQueueAddr + (2 * kAiSubmissionDescriptorBytes),
                         dynamic_unaligned_shape_descriptor_bytes.data(),
                         dynamic_unaligned_shape_descriptor_bytes.size()) ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 3, "dynamic submit tail 3") ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "dynamic doorbell 3") ||
            !tick_until_tail(dynamic_bus,
                             3,
                             dynamic_prev_device_cycles,
                             dynamic_prev_dma_cycles,
                             dynamic_prev_compute_cycles,
                             dynamic_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> dynamic_unaligned_completion_bytes{};
        AiCompletionEntry dynamic_unaligned_completion{};
        std::array<int32_t, 8> dynamic_unaligned_output{};
        if (!load_bytes(dynamic_bus,
                        kCompleteQueueAddr + (2 * kAiCompletionEntryBytes),
                        dynamic_unaligned_completion_bytes.data(),
                        dynamic_unaligned_completion_bytes.size()) ||
            !load_bytes(dynamic_bus,
                        kOutputTensorAddr,
                        dynamic_unaligned_output.data(),
                        sizeof(dynamic_unaligned_output))) {
            return 1;
        }
        decode_ai_completion_entry(dynamic_unaligned_completion_bytes, dynamic_unaligned_completion);
        const AiAcceleratorProfileSummary& dynamic_unaligned_profile =
            dynamic_machine.ai_accelerator().profile_summary();
        if (!expect(dynamic_unaligned_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                    "expected dynamic GEMM unaligned-shape completion fault") ||
            !expect(dynamic_unaligned_completion.fault_code == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                    "expected dynamic GEMM unaligned-shape fault code") ||
            !expect(dynamic_unaligned_completion.retired_ops == 0,
                    "expected zero dynamic GEMM retired ops on unaligned-shape fault") ||
            !expect(dynamic_unaligned_completion.bytes_moved == 0,
                    "expected zero dynamic GEMM bytes moved on unaligned-shape fault") ||
            !expect(dynamic_unaligned_output == expected_large_output,
                    "expected dynamic GEMM output tensor stability after unaligned fault") ||
            !expect_default_timing_model(dynamic_unaligned_profile,
                                         "expected stable dynamic GEMM timing model after unaligned fault") ||
            !expect_submission_timing(dynamic_unaligned_profile, 15, 11, 2, 2, 1, 1, 17,
                                      "expected stable dynamic GEMM timing after unaligned fault") ||
            !expect_submission_outcome(dynamic_unaligned_profile, AI_ACCEL_FAULT_NONE, 64, 80,
                                       "expected stable dynamic GEMM outcome after unaligned fault") ||
            !expect_submission_dma_breakdown(dynamic_unaligned_profile, 7, 4, 48, 32,
                                            "expected stable dynamic GEMM DMA breakdown after unaligned fault") ||
            !expect_submission_compile_contract(dynamic_unaligned_profile,
                                               AiShapeMode::DynamicBounded,
                                               2,
                                               3,
                                               3,
                                               2,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               96,
                                               1,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               2,
                                               1,
                                               48,
                                               32,
                                               dynamic_large_descriptor.token,
                                               dynamic_large_descriptor.flags,
                                               dynamic_large_descriptor.graph_package_addr,
                                               dynamic_large_descriptor.input_table_addr,
                                               dynamic_large_descriptor.output_table_addr,
                                               kSubmitQueueAddr,
                                               kCompleteQueueAddr,
                                               dynamic_graph_package_size,
                                               kDynamicRuntimeShapeLargeOffset,
                                               kGraphPackageAddr + kDynamicRuntimeShapeLargeOffset,
                                               43,
                                               1,
                                               4,
                                               4,
                                               1,
                                               2,
                                               0,
                                               1,
                                               true,
                                               "expected stable dynamic GEMM compile contract after unaligned fault") ||
            !expect(dynamic_unaligned_profile.tile_count == 2,
                    "expected dynamic GEMM profile stability after unaligned-shape fault") ||
            !expect(dynamic_unaligned_profile.scratchpad_peak_bytes == 80,
                    "expected dynamic GEMM scratchpad peak stability after unaligned fault") ||
            !expect(dynamic_unaligned_profile.op_summaries.size() == 1,
                    "expected dynamic GEMM op summary stability after unaligned fault") ||
            !expect_op_summary(dynamic_unaligned_profile.op_summaries[0], 0, AiOpCode::Gemm, 64, 2, 2, 2,
                               "expected dynamic GEMM op profile stability after unaligned fault")) {
            return 1;
        }

        if (!store_bytes(dynamic_bus,
                         kSubmitQueueAddr + (3 * kAiSubmissionDescriptorBytes),
                         dynamic_missing_shape_descriptor_bytes.data(),
                         dynamic_missing_shape_descriptor_bytes.size()) ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 4, "dynamic submit tail 4") ||
            !store_u32(dynamic_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "dynamic doorbell 4") ||
            !tick_until_tail(dynamic_bus,
                             4,
                             dynamic_prev_device_cycles,
                             dynamic_prev_dma_cycles,
                             dynamic_prev_compute_cycles,
                             dynamic_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> dynamic_fault_completion_bytes{};
        AiCompletionEntry dynamic_fault_completion{};
        std::array<int32_t, 8> dynamic_fault_output{};
        if (!load_bytes(dynamic_bus,
                        kCompleteQueueAddr + (3 * kAiCompletionEntryBytes),
                        dynamic_fault_completion_bytes.data(),
                        dynamic_fault_completion_bytes.size()) ||
            !load_bytes(dynamic_bus,
                        kOutputTensorAddr,
                        dynamic_fault_output.data(),
                        sizeof(dynamic_fault_output))) {
            return 1;
        }
        decode_ai_completion_entry(dynamic_fault_completion_bytes, dynamic_fault_completion);
        const AiAcceleratorProfileSummary& dynamic_fault_profile =
            dynamic_machine.ai_accelerator().profile_summary();
        if (!expect(dynamic_fault_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                    "expected dynamic GEMM missing-shape completion fault") ||
            !expect(dynamic_fault_completion.fault_code == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                    "expected dynamic GEMM missing-shape fault code") ||
            !expect(dynamic_fault_completion.retired_ops == 0,
                    "expected zero dynamic GEMM retired ops on missing-shape fault") ||
            !expect(dynamic_fault_completion.bytes_moved == 0,
                    "expected zero dynamic GEMM bytes moved on missing-shape fault") ||
            !expect(dynamic_fault_output == expected_large_output,
                    "expected dynamic GEMM output tensor stability after missing-shape fault") ||
            !expect_default_timing_model(dynamic_fault_profile,
                                         "expected stable dynamic GEMM timing model after missing-shape fault") ||
            !expect_submission_timing(dynamic_fault_profile, 15, 11, 2, 2, 1, 1, 17,
                                      "expected stable dynamic GEMM timing after missing-shape fault") ||
            !expect_submission_outcome(dynamic_fault_profile, AI_ACCEL_FAULT_NONE, 64, 80,
                                       "expected stable dynamic GEMM outcome after missing-shape fault") ||
            !expect_submission_dma_breakdown(dynamic_fault_profile, 7, 4, 48, 32,
                                            "expected stable dynamic GEMM DMA breakdown after missing-shape fault") ||
            !expect_submission_compile_contract(dynamic_fault_profile,
                                               AiShapeMode::DynamicBounded,
                                               2,
                                               3,
                                               3,
                                               2,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               96,
                                               1,
                                               0,
                                               1,
                                               1,
                                               1,
                                               0,
                                               0,
                                               2,
                                               1,
                                               48,
                                               32,
                                               dynamic_large_descriptor.token,
                                               dynamic_large_descriptor.flags,
                                               dynamic_large_descriptor.graph_package_addr,
                                               dynamic_large_descriptor.input_table_addr,
                                               dynamic_large_descriptor.output_table_addr,
                                               kSubmitQueueAddr,
                                               kCompleteQueueAddr,
                                               dynamic_graph_package_size,
                                               kDynamicRuntimeShapeLargeOffset,
                                               kGraphPackageAddr + kDynamicRuntimeShapeLargeOffset,
                                               43,
                                               1,
                                               4,
                                               4,
                                               1,
                                               2,
                                               0,
                                               1,
                                               true,
                                               "expected stable dynamic GEMM compile contract after missing-shape fault") ||
            !expect(dynamic_fault_profile.tile_count == 2,
                    "expected dynamic GEMM profile stability after missing-shape fault") ||
            !expect(dynamic_fault_profile.scratchpad_peak_bytes == 80,
                    "expected dynamic GEMM scratchpad peak stability after fault") ||
            !expect(dynamic_fault_profile.op_summaries.size() == 1,
                    "expected dynamic GEMM op summary stability after fault") ||
            !expect_op_summary(dynamic_fault_profile.op_summaries[0], 0, AiOpCode::Gemm, 64, 2, 2, 2,
                               "expected dynamic GEMM op profile stability after fault") ||
            !expect(dynamic_machine.ai_accelerator().completion_count() == 4,
                    "expected dynamic GEMM completion count") ||
            !expect(dynamic_machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                    "expected dynamic GEMM last fault")) {
            return 1;
        }

        Machine runtime_fault_machine;
        Bus& runtime_fault_bus = runtime_fault_machine.bus();
        if (!load_u32(runtime_fault_bus,
                      AI_ACCEL_BASE + AI_ACCEL_REG_MAGIC,
                      AI_ACCEL_MMIO_MAGIC,
                      "expected mapped runtime-fault AI accelerator") ||
            !configure_queue(runtime_fault_bus)) {
            return 1;
        }

        if (!store_bytes(runtime_fault_bus,
                         kGraphPackageAddr,
                         dynamic_graph_package_bytes.data(),
                         dynamic_graph_package_bytes.size()) ||
            !store_bytes(runtime_fault_bus,
                         kGraphPackageAddr + kDynamicRuntimeShapeLargeOffset,
                         large_runtime_shape_bytes.data(),
                         large_runtime_shape_bytes.size()) ||
            !store_bytes(runtime_fault_bus,
                         kInputTableAddr,
                         dynamic_input_table.data(),
                         sizeof(dynamic_input_table)) ||
            !store_bytes(runtime_fault_bus,
                         kOutputTableAddr,
                         dynamic_output_table.data(),
                         sizeof(dynamic_output_table)) ||
            !store_bytes(runtime_fault_bus,
                         kLhsTensorAddr,
                         dynamic_lhs_tensor.data(),
                         sizeof(dynamic_lhs_tensor)) ||
            !store_bytes(runtime_fault_bus,
                         kRhsTensorAddr,
                         dynamic_rhs_tensor.data(),
                         sizeof(dynamic_rhs_tensor)) ||
            !store_bytes(runtime_fault_bus,
                         kOutputTensorAddr,
                         zero_dynamic_output.data(),
                         sizeof(zero_dynamic_output))) {
            return 1;
        }

        uint64_t runtime_fault_prev_device_cycles = 0;
        uint64_t runtime_fault_prev_dma_cycles = 0;
        uint64_t runtime_fault_prev_compute_cycles = 0;
        uint64_t runtime_fault_prev_stall_cycles = 0;

        if (!store_bytes(runtime_fault_bus,
                         kSubmitQueueAddr,
                         dynamic_large_descriptor_bytes.data(),
                         dynamic_large_descriptor_bytes.size()) ||
            !store_u32(runtime_fault_bus,
                       AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL,
                       1,
                       "runtime-fault submit tail 1") ||
            !store_u32(runtime_fault_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "runtime-fault doorbell 1") ||
            !tick_until_tail(runtime_fault_bus,
                             1,
                             runtime_fault_prev_device_cycles,
                             runtime_fault_prev_dma_cycles,
                             runtime_fault_prev_compute_cycles,
                             runtime_fault_prev_stall_cycles)) {
            return 1;
        }

        std::array<int32_t, 8> runtime_fault_success_output{};
        if (!load_bytes(runtime_fault_bus,
                        kOutputTensorAddr,
                        runtime_fault_success_output.data(),
                        sizeof(runtime_fault_success_output)) ||
            !expect(runtime_fault_success_output == expected_large_output,
                    "expected runtime-fault setup dynamic GEMM output tensor")) {
            return 1;
        }

        if (!store_u32(runtime_fault_bus, AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_HEAD, 1, "runtime-fault completion head 1")) {
            return 1;
        }

        if (!store_bytes(runtime_fault_bus,
                         kSubmitQueueAddr + kAiSubmissionDescriptorBytes,
                         dynamic_overlap_shape_descriptor_bytes.data(),
                         dynamic_overlap_shape_descriptor_bytes.size()) ||
            !store_u32(runtime_fault_bus,
                       AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL,
                       2,
                       "runtime-fault submit tail 2") ||
            !store_u32(runtime_fault_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "runtime-fault doorbell 2") ||
            !tick_until_tail(runtime_fault_bus,
                             2,
                             runtime_fault_prev_device_cycles,
                             runtime_fault_prev_dma_cycles,
                             runtime_fault_prev_compute_cycles,
                             runtime_fault_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> runtime_fault_overlap_completion_bytes{};
        AiCompletionEntry runtime_fault_overlap_completion{};
        std::array<int32_t, 8> runtime_fault_overlap_output{};
        if (!load_bytes(runtime_fault_bus,
                        kCompleteQueueAddr + kAiCompletionEntryBytes,
                        runtime_fault_overlap_completion_bytes.data(),
                        runtime_fault_overlap_completion_bytes.size()) ||
            !load_bytes(runtime_fault_bus,
                        kOutputTensorAddr,
                        runtime_fault_overlap_output.data(),
                        sizeof(runtime_fault_overlap_output))) {
            return 1;
        }
        decode_ai_completion_entry(runtime_fault_overlap_completion_bytes, runtime_fault_overlap_completion);
        const AiAcceleratorProfileSummary& runtime_fault_overlap_profile =
            runtime_fault_machine.ai_accelerator().profile_summary();
        if (!expect(runtime_fault_overlap_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                    "expected dynamic GEMM overlap completion fault") ||
            !expect(runtime_fault_overlap_completion.fault_code == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                    "expected dynamic GEMM overlap fault code") ||
            !expect(runtime_fault_overlap_completion.bytes_moved == 0,
                    "expected zero dynamic GEMM bytes moved on overlap fault") ||
            !expect(runtime_fault_overlap_output == expected_large_output,
                    "expected dynamic GEMM output tensor stability after overlap fault") ||
            !expect_default_timing_model(runtime_fault_overlap_profile,
                                         "expected stable dynamic GEMM timing model after overlap fault") ||
            !expect_submission_timing(runtime_fault_overlap_profile, 15, 11, 2, 2, 1, 1, 17,
                                      "expected stable dynamic GEMM timing after overlap fault") ||
            !expect_submission_outcome(runtime_fault_overlap_profile, AI_ACCEL_FAULT_NONE, 64, 80,
                                       "expected stable dynamic GEMM outcome after overlap fault") ||
            !expect_submission_dma_breakdown(runtime_fault_overlap_profile, 7, 4, 48, 32,
                                            "expected stable dynamic GEMM DMA breakdown after overlap fault") ||
            !expect(runtime_fault_overlap_profile.tile_count == 2,
                    "expected dynamic GEMM profile stability after overlap fault") ||
            !expect(runtime_fault_overlap_profile.scratchpad_peak_bytes == 80,
                    "expected dynamic GEMM scratchpad stability after overlap fault") ||
            !expect(runtime_fault_overlap_profile.op_summaries.size() == 1,
                    "expected dynamic GEMM op summary stability after overlap fault") ||
            !expect_op_summary(runtime_fault_overlap_profile.op_summaries[0], 0, AiOpCode::Gemm, 64, 2, 2, 2,
                               "expected dynamic GEMM op profile stability after overlap fault")) {
            return 1;
        }

        if (!store_u32(runtime_fault_bus, AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_HEAD, 2, "runtime-fault completion head 2")) {
            return 1;
        }

        if (!store_bytes(runtime_fault_bus,
                         kSubmitQueueAddr + (2 * kAiSubmissionDescriptorBytes),
                         dynamic_out_of_window_shape_descriptor_bytes.data(),
                         dynamic_out_of_window_shape_descriptor_bytes.size()) ||
            !store_u32(runtime_fault_bus,
                       AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL,
                       3,
                       "runtime-fault submit tail 3") ||
            !store_u32(runtime_fault_bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "runtime-fault doorbell 3") ||
            !tick_until_tail(runtime_fault_bus,
                             3,
                             runtime_fault_prev_device_cycles,
                             runtime_fault_prev_dma_cycles,
                             runtime_fault_prev_compute_cycles,
                             runtime_fault_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> runtime_fault_out_of_window_completion_bytes{};
        AiCompletionEntry runtime_fault_out_of_window_completion{};
        std::array<int32_t, 8> runtime_fault_out_of_window_output{};
        if (!load_bytes(runtime_fault_bus,
                        kCompleteQueueAddr + (2 * kAiCompletionEntryBytes),
                        runtime_fault_out_of_window_completion_bytes.data(),
                        runtime_fault_out_of_window_completion_bytes.size()) ||
            !load_bytes(runtime_fault_bus,
                        kOutputTensorAddr,
                        runtime_fault_out_of_window_output.data(),
                        sizeof(runtime_fault_out_of_window_output))) {
            return 1;
        }
        decode_ai_completion_entry(runtime_fault_out_of_window_completion_bytes,
                                   runtime_fault_out_of_window_completion);
        const AiAcceleratorProfileSummary& runtime_fault_out_of_window_profile =
            runtime_fault_machine.ai_accelerator().profile_summary();
        if (!expect(runtime_fault_out_of_window_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                    "expected dynamic GEMM out-of-window completion fault") ||
            !expect(runtime_fault_out_of_window_completion.fault_code == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                    "expected dynamic GEMM out-of-window fault code") ||
            !expect(runtime_fault_out_of_window_completion.bytes_moved == 0,
                    "expected zero dynamic GEMM bytes moved on out-of-window fault") ||
            !expect(runtime_fault_out_of_window_output == expected_large_output,
                    "expected dynamic GEMM output tensor stability after out-of-window fault") ||
            !expect_default_timing_model(runtime_fault_out_of_window_profile,
                                         "expected stable dynamic GEMM timing model after out-of-window fault") ||
            !expect_submission_timing(runtime_fault_out_of_window_profile, 15, 11, 2, 2, 1, 1, 17,
                                      "expected stable dynamic GEMM timing after out-of-window fault") ||
            !expect_submission_outcome(runtime_fault_out_of_window_profile, AI_ACCEL_FAULT_NONE, 64, 80,
                                       "expected stable dynamic GEMM outcome after out-of-window fault") ||
            !expect_submission_dma_breakdown(runtime_fault_out_of_window_profile, 7, 4, 48, 32,
                                            "expected stable dynamic GEMM DMA breakdown after out-of-window fault") ||
            !expect(runtime_fault_out_of_window_profile.tile_count == 2,
                    "expected dynamic GEMM profile stability after out-of-window fault") ||
            !expect(runtime_fault_out_of_window_profile.scratchpad_peak_bytes == 80,
                    "expected dynamic GEMM scratchpad stability after out-of-window fault") ||
            !expect(runtime_fault_out_of_window_profile.op_summaries.size() == 1,
                    "expected dynamic GEMM op summary stability after out-of-window fault") ||
            !expect_op_summary(runtime_fault_out_of_window_profile.op_summaries[0],
                               0,
                               AiOpCode::Gemm,
                               64,
                               2,
                               2,
                               2,
                               "expected dynamic GEMM op profile stability after out-of-window fault")) {
            return 1;
        }

        Machine runtime_dma_fault_machine;
        Bus& runtime_dma_fault_bus = runtime_dma_fault_machine.bus();
        if (!load_u32(runtime_dma_fault_bus,
                      AI_ACCEL_BASE + AI_ACCEL_REG_MAGIC,
                      AI_ACCEL_MMIO_MAGIC,
                      "expected mapped runtime-DMA-fault AI accelerator") ||
            !configure_queue(runtime_dma_fault_bus)) {
            return 1;
        }

        const uint32_t dynamic_dma_fault_runtime_shape_offset =
            (dynamic_graph_package_size + 3U) & ~3U;
        const uint64_t dynamic_dma_fault_graph_package_addr =
            MEM_BASE + MEM_SIZE - dynamic_graph_package_size - (large_runtime_shape_bytes.size() / 2U);
        const AiSubmissionDescriptor dynamic_dma_fault_shape_descriptor{
            .token = 0x44594E44ULL,
            .graph_package_addr = dynamic_dma_fault_graph_package_addr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 55,
            .runtime_shape_table_offset = dynamic_dma_fault_runtime_shape_offset,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_dma_fault_shape_descriptor_bytes{};
        encode_ai_submission_descriptor(dynamic_dma_fault_shape_descriptor,
                                        dynamic_dma_fault_shape_descriptor_bytes);

        if (!store_bytes(runtime_dma_fault_bus,
                         dynamic_dma_fault_graph_package_addr,
                         dynamic_graph_package_bytes.data(),
                         dynamic_graph_package_bytes.size()) ||
            !store_bytes(runtime_dma_fault_bus,
                         kInputTableAddr,
                         dynamic_input_table.data(),
                         sizeof(dynamic_input_table)) ||
            !store_bytes(runtime_dma_fault_bus,
                         kOutputTableAddr,
                         dynamic_output_table.data(),
                         sizeof(dynamic_output_table)) ||
            !store_bytes(runtime_dma_fault_bus,
                         kLhsTensorAddr,
                         dynamic_lhs_tensor.data(),
                         sizeof(dynamic_lhs_tensor)) ||
            !store_bytes(runtime_dma_fault_bus,
                         kRhsTensorAddr,
                         dynamic_rhs_tensor.data(),
                         sizeof(dynamic_rhs_tensor)) ||
            !store_bytes(runtime_dma_fault_bus,
                         kOutputTensorAddr,
                         expected_large_output.data(),
                         sizeof(expected_large_output)) ||
            !store_bytes(runtime_dma_fault_bus,
                         kSubmitQueueAddr,
                         dynamic_dma_fault_shape_descriptor_bytes.data(),
                         dynamic_dma_fault_shape_descriptor_bytes.size()) ||
            !store_u32(runtime_dma_fault_bus,
                       AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL,
                       1,
                       "runtime-DMA-fault submit tail 1") ||
            !store_u32(runtime_dma_fault_bus,
                       AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL,
                       1,
                       "runtime-DMA-fault doorbell 1")) {
            return 1;
        }

        uint64_t runtime_dma_fault_prev_device_cycles = 0;
        uint64_t runtime_dma_fault_prev_dma_cycles = 0;
        uint64_t runtime_dma_fault_prev_compute_cycles = 0;
        uint64_t runtime_dma_fault_prev_stall_cycles = 0;
        if (!tick_until_tail(runtime_dma_fault_bus,
                             1,
                             runtime_dma_fault_prev_device_cycles,
                             runtime_dma_fault_prev_dma_cycles,
                             runtime_dma_fault_prev_compute_cycles,
                             runtime_dma_fault_prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> runtime_dma_fault_completion_bytes{};
        AiCompletionEntry runtime_dma_fault_completion{};
        std::array<int32_t, 8> runtime_dma_fault_output{};
        if (!load_bytes(runtime_dma_fault_bus,
                        kCompleteQueueAddr,
                        runtime_dma_fault_completion_bytes.data(),
                        runtime_dma_fault_completion_bytes.size()) ||
            !load_bytes(runtime_dma_fault_bus,
                        kOutputTensorAddr,
                        runtime_dma_fault_output.data(),
                        sizeof(runtime_dma_fault_output))) {
            return 1;
        }
        decode_ai_completion_entry(runtime_dma_fault_completion_bytes, runtime_dma_fault_completion);
        const AiAcceleratorProfileSummary& runtime_dma_fault_profile =
            runtime_dma_fault_machine.ai_accelerator().profile_summary();
        if (!expect(runtime_dma_fault_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                    "expected dynamic GEMM DMA-fault completion fault") ||
            !expect(runtime_dma_fault_completion.fault_code == AI_ACCEL_FAULT_DMA,
                    "expected dynamic GEMM DMA-fault code") ||
            !expect(runtime_dma_fault_completion.bytes_moved == 0,
                    "expected zero dynamic GEMM bytes moved on DMA fault") ||
            !expect(runtime_dma_fault_output == expected_large_output,
                    "expected dynamic GEMM output tensor stability after DMA fault") ||
            !expect_default_timing_model(runtime_dma_fault_profile,
                                         "expected default dynamic GEMM timing model after DMA fault") ||
            !expect_submission_timing(runtime_dma_fault_profile, 0, 0, 0, 0, 0, 0, 0,
                                      "expected empty dynamic GEMM timing after DMA fault without prior success") ||
            !expect_submission_outcome(runtime_dma_fault_profile, AI_ACCEL_FAULT_NONE, 0, 0,
                                       "expected empty dynamic GEMM outcome after DMA fault without prior success") ||
            !expect_submission_dma_breakdown(runtime_dma_fault_profile, 0, 0, 0, 0,
                                            "expected empty dynamic GEMM DMA breakdown after DMA fault") ||
            !expect(runtime_dma_fault_profile.tile_count == 0,
                    "expected empty dynamic GEMM profile after DMA fault without prior success") ||
            !expect(runtime_dma_fault_profile.scratchpad_peak_bytes == 0,
                    "expected zero dynamic GEMM scratchpad peak after DMA fault without prior success") ||
            !expect(runtime_dma_fault_profile.op_summaries.empty(),
                    "expected empty dynamic GEMM op summaries after DMA fault without prior success") ||
            !expect(runtime_dma_fault_machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_DMA,
                    "expected dynamic GEMM DMA last fault")) {
            return 1;
        }

        std::puts("ai_accelerator_gemm_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
