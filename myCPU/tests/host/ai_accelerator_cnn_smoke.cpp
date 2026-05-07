#include <array>
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

constexpr uint64_t kSubmitQueueAddr = MEM_BASE + 0x22000;
constexpr uint64_t kCompleteQueueAddr = MEM_BASE + 0x24000;
constexpr uint64_t kGraphPackageAddr = MEM_BASE + 0x26000;
constexpr uint64_t kInputTableAddr = MEM_BASE + 0x28000;
constexpr uint64_t kOutputTableAddr = MEM_BASE + 0x28100;
constexpr uint64_t kInputTensorAddr = MEM_BASE + 0x2a000;
constexpr uint64_t kKernelTensorAddr = MEM_BASE + 0x2a100;
constexpr uint64_t kOutputTensorAddr = MEM_BASE + 0x2b000;
constexpr uint32_t kDynamicRuntimeShapeOffset = 0x400;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
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

bool build_cnn_graph_package(std::vector<uint8_t>& bytes, uint32_t& package_bytes, std::string& error) {
    AiGraphPackage package{};
    package.scratchpad_budget_bytes = 192;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Input,
        .rank = 2,
        .dims = {4, 4, 0, 0},
        .tile_dims = {4, 4, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Weight,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {3, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {3, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {3, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Output,
        .rank = 1,
        .dims = {3, 0, 0, 0},
        .tile_dims = {3, 0, 0, 0},
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Conv2d,
        .input_dtype = AiDataType::Int8,
        .accum_dtype = AiDataType::Int32,
        .input0 = 0,
        .input1 = 1,
        .input2 = kAiInvalidTensorIndex,
        .output = 2,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::EltwiseRelu,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 2,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 3,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::LayoutTranspose,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 3,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 4,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::ReduceSum,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 4,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 5,
    });
    package.dependencies.push_back(AiDependencyEdge{.source_op = 0, .target_op = 1});
    package.dependencies.push_back(AiDependencyEdge{.source_op = 1, .target_op = 2});
    package.dependencies.push_back(AiDependencyEdge{.source_op = 2, .target_op = 3});
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
        .byte_size = 4,
        .scratchpad_bytes = 4,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 2,
        .system_offset = 0,
        .scratchpad_offset = 32,
        .byte_size = 36,
        .scratchpad_bytes = 36,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 3,
        .system_offset = 0,
        .scratchpad_offset = 80,
        .byte_size = 36,
        .scratchpad_bytes = 36,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 4,
        .system_offset = 0,
        .scratchpad_offset = 128,
        .byte_size = 36,
        .scratchpad_bytes = 36,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 5,
        .system_offset = 0,
        .scratchpad_offset = 176,
        .byte_size = 12,
        .scratchpad_bytes = 12,
    });
    if (!serialize_ai_graph_package(package, bytes, error)) {
        return false;
    }
    package_bytes = static_cast<uint32_t>(bytes.size());
    return true;
}

bool build_dynamic_cnn_graph_package(bool overflow_scratchpad,
                                     std::vector<uint8_t>& bytes,
                                     uint32_t& package_bytes,
                                     std::string& error) {
    AiGraphPackage package{};
    package.shape_mode = AiShapeMode::DynamicBounded;
    package.scratchpad_budget_bytes = overflow_scratchpad ? 16 : 192;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Input,
        .rank = 2,
        .dims = {4, 4, 0, 0},
        .tile_dims = {2, 4, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Weight,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {2, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {2, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {3, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Output,
        .rank = 1,
        .dims = {3, 0, 0, 0},
        .tile_dims = {2, 0, 0, 0},
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Conv2d,
        .input_dtype = AiDataType::Int8,
        .accum_dtype = AiDataType::Int32,
        .input0 = 0,
        .input1 = 1,
        .input2 = kAiInvalidTensorIndex,
        .output = 2,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::EltwiseRelu,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 2,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 3,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::LayoutTranspose,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 3,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 4,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::ReduceSum,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 4,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 5,
    });
    package.dependencies.push_back(AiDependencyEdge{.source_op = 0, .target_op = 1});
    package.dependencies.push_back(AiDependencyEdge{.source_op = 1, .target_op = 2});
    package.dependencies.push_back(AiDependencyEdge{.source_op = 2, .target_op = 3});

    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 0,
        .system_offset = 0,
        .scratchpad_offset = overflow_scratchpad ? 12U : 0U,
        .byte_size = 16,
        .scratchpad_bytes = overflow_scratchpad ? 4U : 16U,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 1,
        .system_offset = 0,
        .scratchpad_offset = overflow_scratchpad ? 0U : 16U,
        .byte_size = 4,
        .scratchpad_bytes = 4,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 2,
        .system_offset = 0,
        .scratchpad_offset = overflow_scratchpad ? 0U : 32U,
        .byte_size = 36,
        .scratchpad_bytes = overflow_scratchpad ? 4U : 36U,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 3,
        .system_offset = 0,
        .scratchpad_offset = overflow_scratchpad ? 0U : 80U,
        .byte_size = 36,
        .scratchpad_bytes = overflow_scratchpad ? 4U : 36U,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 4,
        .system_offset = 0,
        .scratchpad_offset = overflow_scratchpad ? 0U : 128U,
        .byte_size = 36,
        .scratchpad_bytes = overflow_scratchpad ? 4U : 36U,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 5,
        .system_offset = 0,
        .scratchpad_offset = overflow_scratchpad ? 0U : 176U,
        .byte_size = 12,
        .scratchpad_bytes = overflow_scratchpad ? 4U : 12U,
    });
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 0, .max_tensor_bytes = 16});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 2, .max_tensor_bytes = 36});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 3, .max_tensor_bytes = 36});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 4, .max_tensor_bytes = 36});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 5, .max_tensor_bytes = 12});
    if (!serialize_ai_graph_package(package, bytes, error)) {
        return false;
    }
    package_bytes = static_cast<uint32_t>(bytes.size());
    return true;
}

bool build_dynamic_cnn_runtime_shape_table(bool max_shape,
                                           std::vector<uint8_t>& bytes,
                                           std::string& error) {
    const std::vector<AiRuntimeShapeEntry> runtime_shapes{
        AiRuntimeShapeEntry{
            .tensor_index = 0,
            .rank = 2,
            .dims = max_shape ? std::array<uint32_t, kAiMaxTensorRank>{4, 4, 0, 0}
                              : std::array<uint32_t, kAiMaxTensorRank>{3, 3, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 2,
            .rank = 2,
            .dims = max_shape ? std::array<uint32_t, kAiMaxTensorRank>{3, 3, 0, 0}
                              : std::array<uint32_t, kAiMaxTensorRank>{2, 2, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 3,
            .rank = 2,
            .dims = max_shape ? std::array<uint32_t, kAiMaxTensorRank>{3, 3, 0, 0}
                              : std::array<uint32_t, kAiMaxTensorRank>{2, 2, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 4,
            .rank = 2,
            .dims = max_shape ? std::array<uint32_t, kAiMaxTensorRank>{3, 3, 0, 0}
                              : std::array<uint32_t, kAiMaxTensorRank>{2, 2, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 5,
            .rank = 1,
            .dims = max_shape ? std::array<uint32_t, kAiMaxTensorRank>{3, 0, 0, 0}
                              : std::array<uint32_t, kAiMaxTensorRank>{2, 0, 0, 0},
        },
    };
    return serialize_ai_runtime_shape_table(runtime_shapes, bytes, error);
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
        uint32_t graph_package_size = 0;
        std::string error;
        if (!build_cnn_graph_package(graph_package_bytes, graph_package_size, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }

        const std::array<uint64_t, 6> input_table{{kInputTensorAddr, kKernelTensorAddr, 0, 0, 0, 0}};
        const std::array<uint64_t, 6> output_table{{0, 0, 0, 0, 0, kOutputTensorAddr}};
        const std::array<int8_t, 16> input_tensor{{
            1, -2, 3, -4,
            5, -6, 7, -8,
            9, -10, 11, -12,
            13, -14, 15, -16,
        }};
        const std::array<int8_t, 4> kernel_tensor{{1, 0, -1, 2}};
        const std::array<int32_t, 3> zero_output{{0, 0, 0}};
        const AiSubmissionDescriptor descriptor{
            .token = 0x434E4E31ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 23,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> descriptor_bytes{};
        encode_ai_submission_descriptor(descriptor, descriptor_bytes);

        if (!store_bytes(bus, kSubmitQueueAddr, descriptor_bytes.data(), descriptor_bytes.size()) ||
            !store_bytes(bus, kGraphPackageAddr, graph_package_bytes.data(), graph_package_bytes.size()) ||
            !store_bytes(bus, kInputTableAddr, input_table.data(), sizeof(input_table)) ||
            !store_bytes(bus, kOutputTableAddr, output_table.data(), sizeof(output_table)) ||
            !store_bytes(bus, kInputTensorAddr, input_tensor.data(), sizeof(input_tensor)) ||
            !store_bytes(bus, kKernelTensorAddr, kernel_tensor.data(), sizeof(kernel_tensor)) ||
            !store_bytes(bus, kOutputTensorAddr, zero_output.data(), sizeof(zero_output)) ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "submit tail") ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "doorbell")) {
            return 1;
        }

        uint64_t prev_device_cycles = 0;
        uint64_t prev_dma_cycles = 0;
        uint64_t prev_compute_cycles = 0;
        uint64_t prev_stall_cycles = 0;
        bool completed = false;
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
                return 1;
            }
            if (!expect(device_cycles >= prev_device_cycles, "expected monotonic device cycles") ||
                !expect(dma_cycles >= prev_dma_cycles, "expected monotonic DMA cycles") ||
                !expect(compute_cycles >= prev_compute_cycles, "expected monotonic compute cycles") ||
                !expect(stall_cycles >= prev_stall_cycles, "expected monotonic stall cycles")) {
                return 1;
            }
            prev_device_cycles = device_cycles;
            prev_dma_cycles = dma_cycles;
            prev_compute_cycles = compute_cycles;
            prev_stall_cycles = stall_cycles;

            uint64_t completion_tail = 0;
            if (!bus.try_load(AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, 4, completion_tail)) {
                return 1;
            }
            if (completion_tail == 1) {
                completed = true;
                break;
            }
        }

        std::array<uint8_t, kAiCompletionEntryBytes> completion_bytes{};
        AiCompletionEntry completion{};
        std::array<int32_t, 3> output_tensor{};
        uint64_t device_cycles = 0;
        uint64_t dma_cycles = 0;
        uint64_t compute_cycles = 0;
        uint64_t stall_cycles = 0;
        uint64_t dma_load_bytes = 0;
        uint64_t dma_store_bytes = 0;
        if (!expect(completed, "expected AI accelerator CNN smoke to complete") ||
            !load_bytes(bus, kCompleteQueueAddr, completion_bytes.data(), completion_bytes.size()) ||
            !load_bytes(bus, kOutputTensorAddr, output_tensor.data(), sizeof(output_tensor)) ||
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

        decode_ai_completion_entry(completion_bytes, completion);
        const AiAcceleratorProfileSummary& profile_summary = machine.ai_accelerator().profile_summary();
        if (!expect(completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected successful CNN completion") ||
            !expect(completion.retired_ops == 63, "expected CNN retired ops") ||
            !expect(completion.bytes_moved == 32, "expected CNN DMA byte accounting") ||
            !expect(output_tensor == std::array<int32_t, 3>{{0, 78, 0}}, "expected CNN output tensor") ||
            !expect(device_cycles == 18, "expected CNN device cycles") ||
            !expect(dma_cycles == 9, "expected CNN DMA cycles") ||
            !expect(compute_cycles == 5, "expected CNN compute cycles") ||
            !expect(stall_cycles == 4, "expected CNN stall cycles") ||
            !expect(dma_load_bytes == 20, "expected CNN DMA load bytes") ||
            !expect(dma_store_bytes == 12, "expected CNN DMA store bytes") ||
            !expect_default_timing_model(profile_summary, "expected default CNN timing model") ||
            !expect_submission_timing(profile_summary, 18, 9, 5, 4, 1, 1, 20,
                                      "expected CNN submission timing summary") ||
            !expect_submission_outcome(profile_summary, AI_ACCEL_FAULT_NONE, 63, 32,
                                       "expected CNN submission outcome summary") ||
            !expect_submission_dma_breakdown(profile_summary, 6, 3, 20, 12,
                                            "expected CNN submission DMA breakdown") ||
            !expect(profile_summary.tile_count == 4, "expected CNN aggregate tile count") ||
            !expect(profile_summary.scratchpad_peak_bytes == 188,
                    "expected CNN aggregate scratchpad peak bytes") ||
            !expect(profile_summary.op_summaries.size() == 4, "expected four CNN op summaries") ||
            !expect_op_summary(profile_summary.op_summaries[0], 0, AiOpCode::Conv2d, 36, 2, 1, 1,
                               "expected CNN conv profile summary") ||
            !expect_op_summary(profile_summary.op_summaries[1],
                               1,
                               AiOpCode::EltwiseRelu,
                               9,
                               1,
                               1,
                               1,
                               "expected CNN relu profile summary") ||
            !expect_op_summary(profile_summary.op_summaries[2],
                               2,
                               AiOpCode::LayoutTranspose,
                               9,
                               1,
                               1,
                               1,
                               "expected CNN transpose profile summary") ||
            !expect_op_summary(profile_summary.op_summaries[3],
                               3,
                               AiOpCode::ReduceSum,
                               9,
                               1,
                               1,
                               1,
                               "expected CNN reduce profile summary") ||
            !expect(machine.ai_accelerator().completion_count() == 1, "expected CNN completion count") ||
            !expect(machine.ai_accelerator().doorbell_count() == 1, "expected CNN doorbell count") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE, "expected no CNN fault") ||
            !expect(machine.plic().supervisor_has_pending(), "expected CNN completion IRQ")) {
            return 1;
        }

        std::vector<uint8_t> dynamic_graph_package_bytes{};
        std::vector<uint8_t> dynamic_overflow_graph_package_bytes{};
        std::vector<uint8_t> dynamic_runtime_shape_bytes{};
        std::vector<uint8_t> dynamic_overflow_runtime_shape_bytes{};
        uint32_t dynamic_graph_package_size = 0;
        uint32_t dynamic_overflow_graph_package_size = 0;
        if (!build_dynamic_cnn_graph_package(false,
                                             dynamic_graph_package_bytes,
                                             dynamic_graph_package_size,
                                             error) ||
            !build_dynamic_cnn_graph_package(true,
                                             dynamic_overflow_graph_package_bytes,
                                             dynamic_overflow_graph_package_size,
                                             error) ||
            !build_dynamic_cnn_runtime_shape_table(false, dynamic_runtime_shape_bytes, error) ||
            !build_dynamic_cnn_runtime_shape_table(true, dynamic_overflow_runtime_shape_bytes, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }

        const AiSubmissionDescriptor dynamic_descriptor{
            .token = 0x434E4E32ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 25,
            .runtime_shape_table_offset = kDynamicRuntimeShapeOffset,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_descriptor_bytes{};
        encode_ai_submission_descriptor(dynamic_descriptor, dynamic_descriptor_bytes);
        const std::array<int32_t, 3> zero_dynamic_output{{0, 0, 0}};
        prev_device_cycles = device_cycles;
        prev_dma_cycles = dma_cycles;
        prev_compute_cycles = compute_cycles;
        prev_stall_cycles = stall_cycles;
        if (!store_bytes(bus,
                         kSubmitQueueAddr + kAiSubmissionDescriptorBytes,
                         dynamic_descriptor_bytes.data(),
                         dynamic_descriptor_bytes.size()) ||
            !store_bytes(bus, kGraphPackageAddr, dynamic_graph_package_bytes.data(), dynamic_graph_package_bytes.size()) ||
            !store_bytes(bus,
                         kGraphPackageAddr + kDynamicRuntimeShapeOffset,
                         dynamic_runtime_shape_bytes.data(),
                         dynamic_runtime_shape_bytes.size()) ||
            !store_bytes(bus, kInputTensorAddr, input_tensor.data(), sizeof(input_tensor)) ||
            !store_bytes(bus, kKernelTensorAddr, kernel_tensor.data(), sizeof(kernel_tensor)) ||
            !store_bytes(bus, kOutputTensorAddr, zero_dynamic_output.data(), sizeof(zero_dynamic_output)) ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 2, "dynamic submit tail") ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "dynamic doorbell") ||
            !tick_until_tail(bus,
                             2,
                             prev_device_cycles,
                             prev_dma_cycles,
                             prev_compute_cycles,
                             prev_stall_cycles)) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> dynamic_completion_bytes{};
        AiCompletionEntry dynamic_completion{};
        std::array<int32_t, 3> dynamic_output_tensor{};
        if (!load_bytes(bus,
                        kCompleteQueueAddr + kAiCompletionEntryBytes,
                        dynamic_completion_bytes.data(),
                        dynamic_completion_bytes.size()) ||
            !load_bytes(bus, kOutputTensorAddr, dynamic_output_tensor.data(), sizeof(dynamic_output_tensor))) {
            return 1;
        }
        decode_ai_completion_entry(dynamic_completion_bytes, dynamic_completion);
        const AiAcceleratorProfileSummary& dynamic_profile_summary =
            machine.ai_accelerator().profile_summary();
        if (!expect(dynamic_completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS,
                    "expected dynamic CNN completion success") ||
            !expect(dynamic_completion.retired_ops == 28, "expected dynamic CNN retired ops") ||
            !expect(dynamic_completion.bytes_moved == 21, "expected dynamic CNN bytes moved") ||
            !expect(dynamic_output_tensor == std::array<int32_t, 3>{{15, 31, 0}},
                    "expected dynamic CNN output tensor") ||
            !expect_default_timing_model(dynamic_profile_summary,
                                         "expected default dynamic CNN timing model") ||
            !expect_submission_timing(dynamic_profile_summary, 17, 9, 4, 4, 1, 1, 19,
                                      "expected dynamic CNN submission timing summary") ||
            !expect_submission_outcome(dynamic_profile_summary, AI_ACCEL_FAULT_NONE, 28, 21,
                                       "expected dynamic CNN submission outcome summary") ||
            !expect_submission_dma_breakdown(dynamic_profile_summary, 6, 3, 13, 8,
                                            "expected dynamic CNN submission DMA breakdown") ||
            !expect(dynamic_profile_summary.tile_count == 4,
                    "expected dynamic CNN aggregate tile count") ||
            !expect(dynamic_profile_summary.scratchpad_peak_bytes == 184,
                    "expected dynamic CNN scratchpad peak bytes") ||
            !expect(dynamic_profile_summary.op_summaries.size() == 4,
                    "expected four dynamic CNN op summaries") ||
            !expect_op_summary(dynamic_profile_summary.op_summaries[0],
                               0,
                               AiOpCode::Conv2d,
                               16,
                               1,
                               1,
                               1,
                               "expected dynamic CNN conv profile summary") ||
            !expect_op_summary(dynamic_profile_summary.op_summaries[1],
                               1,
                               AiOpCode::EltwiseRelu,
                               4,
                               1,
                               1,
                               1,
                               "expected dynamic CNN relu profile summary") ||
            !expect_op_summary(dynamic_profile_summary.op_summaries[2],
                               2,
                               AiOpCode::LayoutTranspose,
                               4,
                               1,
                               1,
                               1,
                               "expected dynamic CNN transpose profile summary") ||
            !expect_op_summary(dynamic_profile_summary.op_summaries[3],
                               3,
                               AiOpCode::ReduceSum,
                               4,
                               1,
                               1,
                               1,
                               "expected dynamic CNN reduce profile summary")) {
            return 1;
        }

        const AiSubmissionDescriptor dynamic_overflow_descriptor{
            .token = 0x434E4E33ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = dynamic_overflow_graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 27,
            .runtime_shape_table_offset = kDynamicRuntimeShapeOffset,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_overflow_descriptor_bytes{};
        encode_ai_submission_descriptor(dynamic_overflow_descriptor, dynamic_overflow_descriptor_bytes);
        if (!store_bytes(bus,
                         kSubmitQueueAddr + 2 * kAiSubmissionDescriptorBytes,
                         dynamic_overflow_descriptor_bytes.data(),
                         dynamic_overflow_descriptor_bytes.size()) ||
            !store_bytes(bus,
                         kGraphPackageAddr,
                         dynamic_overflow_graph_package_bytes.data(),
                         dynamic_overflow_graph_package_bytes.size()) ||
            !store_bytes(bus,
                         kGraphPackageAddr + kDynamicRuntimeShapeOffset,
                         dynamic_overflow_runtime_shape_bytes.data(),
                         dynamic_overflow_runtime_shape_bytes.size()) ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 3, "dynamic overflow submit tail") ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "dynamic overflow doorbell") ||
            !tick_until_tail(bus,
                             3,
                             prev_device_cycles,
                             prev_dma_cycles,
                             prev_compute_cycles,
                             prev_stall_cycles)) {
            return 1;
        }
        std::array<uint8_t, kAiCompletionEntryBytes> dynamic_overflow_completion_bytes{};
        AiCompletionEntry dynamic_overflow_completion{};
        std::array<int32_t, 3> dynamic_overflow_output{};
        if (!load_bytes(bus,
                        kCompleteQueueAddr + 2 * kAiCompletionEntryBytes,
                        dynamic_overflow_completion_bytes.data(),
                        dynamic_overflow_completion_bytes.size()) ||
            !load_bytes(bus, kOutputTensorAddr, dynamic_overflow_output.data(), sizeof(dynamic_overflow_output))) {
            return 1;
        }
        decode_ai_completion_entry(dynamic_overflow_completion_bytes, dynamic_overflow_completion);
        const AiAcceleratorProfileSummary& dynamic_overflow_profile =
            machine.ai_accelerator().profile_summary();
        if (!expect(dynamic_overflow_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                    "expected dynamic CNN overflow completion fault") ||
            !expect(dynamic_overflow_completion.fault_code == AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW,
                    "expected dynamic CNN overflow fault code") ||
            !expect(dynamic_overflow_completion.retired_ops == 0,
                    "expected zero retired ops on dynamic CNN overflow") ||
            !expect(dynamic_overflow_completion.bytes_moved == 0,
                    "expected zero bytes moved on dynamic CNN overflow") ||
            !expect(dynamic_overflow_output == dynamic_output_tensor,
                    "expected dynamic CNN output stability after overflow fault") ||
            !expect_default_timing_model(dynamic_overflow_profile,
                                         "expected stable dynamic CNN timing model after overflow fault") ||
            !expect_submission_timing(dynamic_overflow_profile, 17, 9, 4, 4, 1, 1, 19,
                                      "expected stable dynamic CNN timing after overflow fault") ||
            !expect_submission_outcome(dynamic_overflow_profile, AI_ACCEL_FAULT_NONE, 28, 21,
                                       "expected stable dynamic CNN outcome after overflow fault") ||
            !expect_submission_dma_breakdown(dynamic_overflow_profile, 6, 3, 13, 8,
                                            "expected stable dynamic CNN DMA breakdown after overflow fault") ||
            !expect(dynamic_overflow_profile.tile_count == 4,
                    "expected dynamic CNN profile stability after overflow fault") ||
            !expect(dynamic_overflow_profile.scratchpad_peak_bytes == 184,
                    "expected dynamic CNN scratchpad profile stability after overflow fault") ||
            !expect(dynamic_overflow_profile.op_summaries.size() == 4,
                    "expected dynamic CNN op summaries stable after overflow fault") ||
            !expect(machine.ai_accelerator().completion_count() == 3,
                    "expected dynamic CNN completion count") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW,
                    "expected dynamic CNN last overflow fault")) {
            return 1;
        }

        if (!store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_CONTROL, AI_ACCEL_CONTROL_RESET, "cnn reset")) {
            return 1;
        }
        const AiAcceleratorProfileSummary& reset_profile_summary = machine.ai_accelerator().profile_summary();
        if (!expect_default_timing_model(reset_profile_summary, "expected default CNN timing model after reset") ||
            !expect_submission_timing(reset_profile_summary, 0, 0, 0, 0, 0, 0, 0,
                                      "expected empty CNN submission timing after reset") ||
            !expect_submission_outcome(reset_profile_summary, AI_ACCEL_FAULT_NONE, 0, 0,
                                       "expected empty CNN submission outcome after reset") ||
            !expect_submission_dma_breakdown(reset_profile_summary, 0, 0, 0, 0,
                                            "expected empty CNN DMA breakdown after reset") ||
            !expect(reset_profile_summary.tile_count == 0, "expected CNN profile tile count reset") ||
            !expect(reset_profile_summary.scratchpad_peak_bytes == 0,
                    "expected CNN scratchpad peak bytes reset") ||
            !expect(reset_profile_summary.op_summaries.empty(), "expected CNN op summaries reset") ||
            !expect(machine.ai_accelerator().completion_count() == 0, "expected CNN completion count reset") ||
            !expect(machine.ai_accelerator().doorbell_count() == 0, "expected CNN doorbell count reset") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE,
                    "expected CNN last fault cleared on reset")) {
            return 1;
        }

        std::puts("ai_accelerator_cnn_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
