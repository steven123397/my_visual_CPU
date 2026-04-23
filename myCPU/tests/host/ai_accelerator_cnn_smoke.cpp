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
        if (!expect(completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected successful CNN completion") ||
            !expect(completion.retired_ops == 63, "expected CNN retired ops") ||
            !expect(completion.bytes_moved == 32, "expected CNN DMA byte accounting") ||
            !expect(output_tensor == std::array<int32_t, 3>{{0, 78, 0}}, "expected CNN output tensor") ||
            !expect(device_cycles == 18, "expected CNN device cycles") ||
            !expect(dma_cycles == 9, "expected CNN DMA cycles") ||
            !expect(compute_cycles == 9, "expected CNN compute cycles") ||
            !expect(stall_cycles == 0, "expected zero CNN stall cycles") ||
            !expect(dma_load_bytes == 20, "expected CNN DMA load bytes") ||
            !expect(dma_store_bytes == 12, "expected CNN DMA store bytes") ||
            !expect(machine.ai_accelerator().completion_count() == 1, "expected CNN completion count") ||
            !expect(machine.ai_accelerator().doorbell_count() == 1, "expected CNN doorbell count") ||
            !expect(machine.ai_accelerator().last_fault() == AI_ACCEL_FAULT_NONE, "expected no CNN fault") ||
            !expect(machine.plic().supervisor_has_pending(), "expected CNN completion IRQ")) {
            return 1;
        }

        std::puts("ai_accelerator_cnn_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
