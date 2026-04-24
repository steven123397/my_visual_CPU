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
constexpr uint32_t kDynamicRuntimeShapeSmallOffset = 0x200;
constexpr uint32_t kDynamicRuntimeShapeLargeOffset = 0x240;

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
                       uint64_t expected_tile_count,
                       const char* context) {
    return expect(summary.op_index == expected_index, context) &&
           expect(summary.opcode == expected_opcode, context) &&
           expect(summary.retired_ops == expected_retired_ops, context) &&
           expect(summary.compute_cycles == expected_compute_cycles, context) &&
           expect(summary.stall_cycles == 0, context) &&
           expect(summary.tile_count == expected_tile_count, context);
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
            !expect(compute_cycles == 4, "expected GEMM compute cycles") ||
            !expect(stall_cycles == 0, "expected zero GEMM stall cycles") ||
            !expect(success_profile.tile_count == 2, "expected GEMM aggregate tile count") ||
            !expect(success_profile.scratchpad_peak_bytes == 36,
                    "expected GEMM aggregate scratchpad peak bytes") ||
            !expect(success_profile.op_summaries.size() == 2, "expected two GEMM op summaries") ||
            !expect_op_summary(success_profile.op_summaries[0], 0, AiOpCode::Gemm, 8, 2, 1,
                               "expected GEMM op profile summary") ||
            !expect_op_summary(success_profile.op_summaries[1], 1, AiOpCode::PoolMax, 4, 2, 1,
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
            !expect(compute_cycles == 4, "expected cumulative GEMM compute cycles") ||
            !expect(stall_cycles == 0, "expected cumulative GEMM stall cycles") ||
            !expect(dma_load_bytes == 32, "expected cumulative GEMM DMA load bytes") ||
            !expect(dma_store_bytes == 4, "expected cumulative GEMM DMA store bytes") ||
            !expect(fault_profile.tile_count == 2, "expected GEMM tile profile to remain stable on fault") ||
            !expect(fault_profile.scratchpad_peak_bytes == 36,
                    "expected GEMM scratchpad peak to remain stable on fault") ||
            !expect(fault_profile.op_summaries.size() == 2,
                    "expected GEMM op summaries to remain stable on fault") ||
            !expect_op_summary(fault_profile.op_summaries[0], 0, AiOpCode::Gemm, 8, 2, 1,
                               "expected stable GEMM op profile after fault") ||
            !expect_op_summary(fault_profile.op_summaries[1], 1, AiOpCode::PoolMax, 4, 2, 1,
                               "expected stable pool op profile after fault") ||
            !expect(machine.ai_accelerator().completion_count() == 2, "expected GEMM completion count") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_ILLEGAL_OP, "expected last GEMM fault") ||
            !expect(machine.plic().supervisor_has_pending(), "expected GEMM IRQ pending")) {
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
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_small_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_large_descriptor_bytes{};
        std::array<uint8_t, kAiSubmissionDescriptorBytes> dynamic_missing_shape_descriptor_bytes{};
        encode_ai_submission_descriptor(dynamic_small_descriptor, dynamic_small_descriptor_bytes);
        encode_ai_submission_descriptor(dynamic_large_descriptor, dynamic_large_descriptor_bytes);
        encode_ai_submission_descriptor(dynamic_missing_shape_descriptor, dynamic_missing_shape_descriptor_bytes);

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
            !expect(dynamic_small_profile.tile_count == 1,
                    "expected dynamic GEMM small aggregate tile count") ||
            !expect(dynamic_small_profile.scratchpad_peak_bytes == 64,
                    "expected dynamic GEMM small scratchpad peak bytes") ||
            !expect(dynamic_small_profile.op_summaries.size() == 1,
                    "expected one dynamic GEMM small op summary") ||
            !expect_op_summary(dynamic_small_profile.op_summaries[0], 0, AiOpCode::Gemm, 32, 2, 1,
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
            !expect(dynamic_large_profile.tile_count == 2,
                    "expected dynamic GEMM large aggregate tile count") ||
            !expect(dynamic_large_profile.scratchpad_peak_bytes == 80,
                    "expected dynamic GEMM large scratchpad peak bytes") ||
            !expect(dynamic_large_profile.op_summaries.size() == 1,
                    "expected one dynamic GEMM large op summary") ||
            !expect_op_summary(dynamic_large_profile.op_summaries[0], 0, AiOpCode::Gemm, 64, 4, 2,
                               "expected dynamic GEMM large op profile summary")) {
            return 1;
        }

        if (!store_bytes(dynamic_bus,
                         kSubmitQueueAddr + (2 * kAiSubmissionDescriptorBytes),
                         dynamic_missing_shape_descriptor_bytes.data(),
                         dynamic_missing_shape_descriptor_bytes.size()) ||
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

        std::array<uint8_t, kAiCompletionEntryBytes> dynamic_fault_completion_bytes{};
        AiCompletionEntry dynamic_fault_completion{};
        if (!load_bytes(dynamic_bus,
                        kCompleteQueueAddr + (2 * kAiCompletionEntryBytes),
                        dynamic_fault_completion_bytes.data(),
                        dynamic_fault_completion_bytes.size())) {
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
            !expect(dynamic_fault_profile.tile_count == 2,
                    "expected dynamic GEMM profile stability after missing-shape fault") ||
            !expect(dynamic_fault_profile.scratchpad_peak_bytes == 80,
                    "expected dynamic GEMM scratchpad peak stability after fault") ||
            !expect(dynamic_fault_profile.op_summaries.size() == 1,
                    "expected dynamic GEMM op summary stability after fault") ||
            !expect_op_summary(dynamic_fault_profile.op_summaries[0], 0, AiOpCode::Gemm, 64, 4, 2,
                               "expected dynamic GEMM op profile stability after fault") ||
            !expect(dynamic_machine.ai_accelerator().completion_count() == 3,
                    "expected dynamic GEMM completion count") ||
            !expect(dynamic_machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                    "expected dynamic GEMM last fault")) {
            return 1;
        }

        std::puts("ai_accelerator_gemm_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
