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
        if (!expect(success_completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected GEMM completion success") ||
            !expect(success_completion.retired_ops == 12, "expected GEMM retired ops") ||
            !expect(success_completion.bytes_moved == 20, "expected GEMM DMA byte accounting") ||
            !expect(almost_equal(output_tensor, 4.0f), "expected GEMM output tensor") ||
            !expect(device_cycles == 13, "expected GEMM device cycles") ||
            !expect(dma_cycles == 9, "expected GEMM DMA cycles") ||
            !expect(compute_cycles == 4, "expected GEMM compute cycles") ||
            !expect(stall_cycles == 0, "expected zero GEMM stall cycles")) {
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
            !expect(machine.ai_accelerator().completion_count() == 2, "expected GEMM completion count") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_ILLEGAL_OP, "expected last GEMM fault") ||
            !expect(machine.plic().supervisor_has_pending(), "expected GEMM IRQ pending")) {
            return 1;
        }

        std::puts("ai_accelerator_gemm_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
