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

constexpr uint64_t kSubmitQueueAddr = MEM_BASE + 0x12000;
constexpr uint64_t kCompleteQueueAddr = MEM_BASE + 0x14000;
constexpr uint64_t kGraphPackageAddr = MEM_BASE + 0x16000;
constexpr uint64_t kInputTableAddr = MEM_BASE + 0x18000;
constexpr uint64_t kOutputTableAddr = MEM_BASE + 0x18100;
constexpr uint64_t kInputTensorAddr = MEM_BASE + 0x1a000;
constexpr uint64_t kOutputTensorAddr = MEM_BASE + 0x1b000;

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
           store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, 2, "submit queue size") &&
           store_u32(bus,
                     AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW,
                     static_cast<uint32_t>(kCompleteQueueAddr),
                     "complete queue base low") &&
           store_u32(bus,
                     AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH,
                     static_cast<uint32_t>(kCompleteQueueAddr >> 32),
                     "complete queue base high") &&
           store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, 2, "complete queue size");
}

bool build_identity_graph_package(std::vector<uint8_t>& bytes, uint32_t& package_bytes, std::string& error) {
    AiGraphPackage package{};
    package.scratchpad_budget_bytes = 8;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Input,
        .rank = 1,
        .dims = {8, 0, 0, 0},
        .tile_dims = {8, 0, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Output,
        .rank = 1,
        .dims = {8, 0, 0, 0},
        .tile_dims = {8, 0, 0, 0},
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::EltwiseRelu,
        .input_dtype = AiDataType::Int8,
        .accum_dtype = AiDataType::Int32,
        .input0 = 0,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 1,
    });
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
        .scratchpad_offset = 0,
        .byte_size = 8,
        .scratchpad_bytes = 8,
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
        if (!build_identity_graph_package(graph_package_bytes, graph_package_size, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        const std::array<uint64_t, 2> input_table{{kInputTensorAddr, 0}};
        const std::array<uint64_t, 2> output_table{{0, kOutputTensorAddr}};
        const std::array<uint8_t, 8> input_tensor{{1, 2, 3, 4, 5, 6, 7, 8}};
        const std::array<uint8_t, 8> zero_tensor{{0, 0, 0, 0, 0, 0, 0, 0}};
        const AiSubmissionDescriptor descriptor{
            .token = 0x12345678ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 19,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> descriptor_bytes{};
        encode_ai_submission_descriptor(descriptor, descriptor_bytes);

        if (!store_bytes(bus, kSubmitQueueAddr, descriptor_bytes.data(), descriptor_bytes.size()) ||
            !store_bytes(bus, kGraphPackageAddr, graph_package_bytes.data(), graph_package_bytes.size()) ||
            !store_bytes(bus, kInputTableAddr, input_table.data(), sizeof(input_table)) ||
            !store_bytes(bus, kOutputTableAddr, output_table.data(), sizeof(output_table)) ||
            !store_bytes(bus, kInputTensorAddr, input_tensor.data(), input_tensor.size()) ||
            !store_bytes(bus, kOutputTensorAddr, zero_tensor.data(), zero_tensor.size()) ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "submit tail") ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "doorbell")) {
            return 1;
        }

        if (!load_u32(bus,
                      AI_ACCEL_BASE + AI_ACCEL_REG_STATUS,
                      AI_ACCEL_STATUS_READY | AI_ACCEL_STATUS_BUSY,
                      "expected AI accelerator busy after doorbell") ||
            !load_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_HEAD, 0, "expected async submit head") ||
            !load_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, 0, "expected async completion tail")) {
            return 1;
        }

        uint64_t prev_device_cycles = 0;
        uint64_t prev_dma_cycles = 0;
        uint64_t prev_dma_load_cycles = 0;
        uint64_t prev_dma_store_cycles = 0;
        uint64_t prev_dma_load_bytes = 0;
        uint64_t prev_dma_store_bytes = 0;
        bool completed = false;
        for (int i = 0; i < 64; ++i) {
            bus.tick();

            uint64_t device_cycles = 0;
            uint64_t dma_cycles = 0;
            uint64_t dma_load_cycles = 0;
            uint64_t dma_store_cycles = 0;
            uint64_t dma_load_bytes = 0;
            uint64_t dma_store_bytes = 0;
            if (!load_counter(bus,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_LOW,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DEVICE_CYCLES_HIGH,
                              device_cycles) ||
                !load_counter(bus,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_LOW,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DMA_CYCLES_HIGH,
                              dma_cycles) ||
                !load_counter(bus,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DMA_LOAD_CYCLES_LOW,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DMA_LOAD_CYCLES_HIGH,
                              dma_load_cycles) ||
                !load_counter(bus,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DMA_STORE_CYCLES_LOW,
                              AI_ACCEL_BASE + AI_ACCEL_REG_DMA_STORE_CYCLES_HIGH,
                              dma_store_cycles) ||
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
            if (!expect(device_cycles >= prev_device_cycles, "expected monotonic device cycles") ||
                !expect(dma_cycles >= prev_dma_cycles, "expected monotonic DMA cycles") ||
                !expect(dma_load_cycles >= prev_dma_load_cycles, "expected monotonic DMA load cycles") ||
                !expect(dma_store_cycles >= prev_dma_store_cycles, "expected monotonic DMA store cycles") ||
                !expect(dma_load_bytes >= prev_dma_load_bytes, "expected monotonic DMA load bytes") ||
                !expect(dma_store_bytes >= prev_dma_store_bytes, "expected monotonic DMA store bytes")) {
                return 1;
            }
            prev_device_cycles = device_cycles;
            prev_dma_cycles = dma_cycles;
            prev_dma_load_cycles = dma_load_cycles;
            prev_dma_store_cycles = dma_store_cycles;
            prev_dma_load_bytes = dma_load_bytes;
            prev_dma_store_bytes = dma_store_bytes;

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
        std::array<uint8_t, 8> output_tensor{};
        if (!expect(completed, "expected AI accelerator DMA smoke to complete") ||
            !load_bytes(bus, kCompleteQueueAddr, completion_bytes.data(), completion_bytes.size()) ||
            !load_bytes(bus, kOutputTensorAddr, output_tensor.data(), output_tensor.size())) {
            return 1;
        }
        decode_ai_completion_entry(completion_bytes, completion);
        if (!expect(completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected successful DMA completion") ||
            !expect(completion.bytes_moved == 16, "expected DMA completion byte accounting") ||
            !expect(output_tensor == input_tensor, "expected DMA roundtrip output bytes") ||
            !expect(machine.ai_accelerator().completion_count() == 1, "expected completion count") ||
            !expect(machine.ai_accelerator().doorbell_count() == 1, "expected doorbell count") ||
            !expect(machine.plic().supervisor_has_pending(), "expected DMA completion IRQ")) {
            return 1;
        }

        std::puts("ai_accelerator_dma_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
