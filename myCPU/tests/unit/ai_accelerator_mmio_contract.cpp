#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "../../src/devices/ai_accelerator.h"
#include "../../src/devices/ai_graph_package.h"
#include "../../src/devices/ai_submission_queue.h"
#include "../../src/devices/plic.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

constexpr uint64_t kSubmitQueueAddr = MEM_BASE + 0x1000;
constexpr uint64_t kCompleteQueueAddr = MEM_BASE + 0x4000;
constexpr uint64_t kGraphPackageAddr = MEM_BASE + 0x8000;
constexpr uint64_t kInputTableAddr = MEM_BASE + 0x9000;
constexpr uint64_t kOutputTableAddr = MEM_BASE + 0x9100;
constexpr uint64_t kInputTensorAddr = MEM_BASE + 0xA000;
constexpr uint64_t kOutputTensorAddr = MEM_BASE + 0xA100;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool load_reg(Bus& bus, uint32_t reg, uint64_t expected, const char* message) {
    uint64_t value = 0;
    if (!bus.try_load(AI_ACCEL_BASE + reg, 4, value) || value != expected) {
        std::fprintf(stderr,
                     "%s expected 0x%llx got 0x%llx\n",
                     message,
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(value));
        return false;
    }
    return true;
}

bool load_counter(Bus& bus, uint32_t low_reg, uint32_t high_reg, uint64_t& value) {
    uint64_t low = 0;
    uint64_t high = 0;
    return bus.try_load(AI_ACCEL_BASE + low_reg, 4, low) &&
           bus.try_load(AI_ACCEL_BASE + high_reg, 4, high) &&
           ((value = (high << 32) | static_cast<uint32_t>(low)), true);
}

bool store_reg(Bus& bus, uint32_t reg, uint64_t value, const char* message) {
    if (!bus.try_store(AI_ACCEL_BASE + reg, value, 4)) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write_descriptor(Ram& ram, uint64_t addr, const AiSubmissionDescriptor& descriptor) {
    std::array<uint8_t, kAiSubmissionDescriptorBytes> bytes{};
    encode_ai_submission_descriptor(descriptor, bytes);
    ram.write_bytes(addr, bytes.data(), bytes.size());
}

AiCompletionEntry read_completion(Ram& ram, uint64_t addr) {
    std::array<uint8_t, kAiCompletionEntryBytes> bytes{};
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(ram.load(addr + i, 1) & 0xffU);
    }
    AiCompletionEntry completion{};
    decode_ai_completion_entry(bytes, completion);
    return completion;
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

bool tick_until_completion(Bus& bus, uint32_t max_ticks) {
    for (uint32_t i = 0; i < max_ticks; ++i) {
        bus.tick();
        uint64_t completion_tail = 0;
        if (!bus.try_load(AI_ACCEL_BASE + AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, 4, completion_tail)) {
            return false;
        }
        if (completion_tail == 1) {
            return true;
        }
    }
    return false;
}

bool program_queues(Bus& bus) {
    return store_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_BASE_LOW, static_cast<uint32_t>(kSubmitQueueAddr), "sq base low") &&
           store_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_BASE_HIGH, static_cast<uint32_t>(kSubmitQueueAddr >> 32), "sq base high") &&
           store_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_SIZE, 4, "sq size") &&
           store_reg(bus, AI_ACCEL_REG_COMPLETE_QUEUE_BASE_LOW, static_cast<uint32_t>(kCompleteQueueAddr), "cq base low") &&
           store_reg(bus, AI_ACCEL_REG_COMPLETE_QUEUE_BASE_HIGH, static_cast<uint32_t>(kCompleteQueueAddr >> 32), "cq base high") &&
           store_reg(bus, AI_ACCEL_REG_COMPLETE_QUEUE_SIZE, 4, "cq size");
}

}  // namespace

int main() {
    try {
        const AiSubmissionDescriptor descriptor_roundtrip{
            .token = 0x1234ULL,
            .graph_package_addr = 0x2000ULL,
            .graph_package_bytes = 256,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = 0x3000ULL,
            .output_table_addr = 0x4000ULL,
            .source_tag = 9,
            .runtime_shape_table_offset = 0x120U,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> descriptor_bytes{};
        encode_ai_submission_descriptor(descriptor_roundtrip, descriptor_bytes);
        AiSubmissionDescriptor decoded_descriptor{};
        if (!expect(decode_ai_submission_descriptor(descriptor_bytes, decoded_descriptor),
                    "expected submission descriptor decode") ||
            !expect(decoded_descriptor.runtime_shape_table_offset ==
                        descriptor_roundtrip.runtime_shape_table_offset,
                    "expected runtime shape table offset roundtrip")) {
            return 1;
        }

        Ram ram;
        Bus bus(ram);
        Plic plic;
        AiAccelerator accelerator(plic, AI_ACCEL_PLIC_SOURCE);
        accelerator.bind_bus(bus);

        bus.attach(accelerator);
        bus.attach(plic);

        if (!load_reg(bus, AI_ACCEL_REG_MAGIC, AI_ACCEL_MMIO_MAGIC, "expected AI accelerator magic") ||
            !load_reg(bus, AI_ACCEL_REG_VERSION, AI_ACCEL_MMIO_VERSION, "expected AI accelerator version") ||
            !load_reg(bus, AI_ACCEL_REG_STATUS, AI_ACCEL_STATUS_READY, "expected ready status") ||
            !expect(!bus.try_store(AI_ACCEL_BASE + AI_ACCEL_REG_STATUS, 0, 8),
                    "expected invalid AI MMIO width to fail")) {
            return 1;
        }

        if (!program_queues(bus)) {
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
        const AiSubmissionDescriptor descriptor{
            .token = 0xabcdeULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = graph_package_size,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = kInputTableAddr,
            .output_table_addr = kOutputTableAddr,
            .source_tag = 7,
        };
        write_descriptor(ram, kSubmitQueueAddr, descriptor);
        ram.write_bytes(kGraphPackageAddr, graph_package_bytes.data(), graph_package_bytes.size());
        ram.write_bytes(kInputTableAddr, input_table.data(), sizeof(input_table));
        ram.write_bytes(kOutputTableAddr, output_table.data(), sizeof(output_table));
        ram.write_bytes(kInputTensorAddr, input_tensor.data(), input_tensor.size());
        ram.fill(kOutputTensorAddr, 0, input_tensor.size());

        if (!store_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "sq tail") ||
            !store_reg(bus, AI_ACCEL_REG_DOORBELL, 1, "doorbell") ||
            !load_reg(bus, AI_ACCEL_REG_STATUS, AI_ACCEL_STATUS_READY | AI_ACCEL_STATUS_BUSY, "expected async busy status") ||
            !load_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_HEAD, 0, "expected async submit head before ticks") ||
            !load_reg(bus, AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, 0, "expected async completion tail before ticks") ||
            !tick_until_completion(bus, 64) ||
            !load_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_HEAD, 1, "expected submission head advance") ||
            !load_reg(bus, AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, 1, "expected completion tail advance") ||
            !load_reg(bus, AI_ACCEL_REG_QUEUE_DEPTH, 0, "expected empty queue after completion") ||
            !load_reg(bus, AI_ACCEL_REG_IRQ_STATUS, AI_ACCEL_IRQ_COMPLETION, "expected completion interrupt")) {
            return 1;
        }

        const AiCompletionEntry completion = read_completion(ram, kCompleteQueueAddr);
        uint64_t device_cycles = 0;
        uint64_t dma_cycles = 0;
        uint64_t compute_cycles = 0;
        uint64_t stall_cycles = 0;
        uint64_t busy_cycles = 0;
        uint64_t queue_cycles = 0;
        uint64_t completion_cycles = 0;
        if (!expect(completion.token == descriptor.token, "expected completion token") ||
            !expect(completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected success completion") ||
            !expect(completion.fault_code == AI_ACCEL_FAULT_NONE, "expected no completion fault") ||
            !expect(completion.retired_ops == 8, "expected relu retired ops") ||
            !expect(completion.bytes_moved == 16, "expected DMA byte accounting") ||
            !expect(completion.source_tag == descriptor.source_tag, "expected source tag roundtrip") ||
            !load_counter(bus, AI_ACCEL_REG_DEVICE_CYCLES_LOW, AI_ACCEL_REG_DEVICE_CYCLES_HIGH, device_cycles) ||
            !load_counter(bus, AI_ACCEL_REG_DMA_CYCLES_LOW, AI_ACCEL_REG_DMA_CYCLES_HIGH, dma_cycles) ||
            !load_counter(bus, AI_ACCEL_REG_COMPUTE_CYCLES_LOW, AI_ACCEL_REG_COMPUTE_CYCLES_HIGH, compute_cycles) ||
            !load_counter(bus, AI_ACCEL_REG_STALL_CYCLES_LOW, AI_ACCEL_REG_STALL_CYCLES_HIGH, stall_cycles) ||
            !load_counter(bus, AI_ACCEL_REG_BUSY_CYCLES_LOW, AI_ACCEL_REG_BUSY_CYCLES_HIGH, busy_cycles) ||
            !load_counter(bus, AI_ACCEL_REG_QUEUE_CYCLES_LOW, AI_ACCEL_REG_QUEUE_CYCLES_HIGH, queue_cycles) ||
            !load_counter(bus,
                          AI_ACCEL_REG_COMPLETION_CYCLES_LOW,
                          AI_ACCEL_REG_COMPLETION_CYCLES_HIGH,
                          completion_cycles) ||
            !expect(device_cycles == 8, "expected total device cycles") ||
            !expect(dma_cycles == 6, "expected total DMA cycles") ||
            !expect(compute_cycles == 1, "expected total compute cycles") ||
            !expect(stall_cycles == 1, "expected total stall cycles") ||
            !expect(busy_cycles == 10, "expected busy cycles to include queue and completion attribution") ||
            !expect(queue_cycles == 1, "expected one queue/control cycle") ||
            !expect(completion_cycles == 1, "expected one completion cycle") ||
            !load_reg(bus,
                      AI_ACCEL_REG_EFFECTIVE_OPS_PER_CYCLE,
                      8,
                      "expected effective ops per compute cycle") ||
            !load_reg(bus, AI_ACCEL_REG_UTILIZATION, 10, "expected compute utilization percent") ||
            !expect(plic.source_level(AI_ACCEL_PLIC_SOURCE), "expected AI accelerator IRQ line asserted")) {
            return 1;
        }

        if (!store_reg(bus, AI_ACCEL_REG_IRQ_ACK, AI_ACCEL_IRQ_COMPLETION, "irq ack") ||
            !load_reg(bus, AI_ACCEL_REG_IRQ_STATUS, 0, "expected IRQ status clear") ||
            !expect(!plic.source_level(AI_ACCEL_PLIC_SOURCE), "expected AI accelerator IRQ line cleared")) {
            return 1;
        }

        if (!store_reg(bus, AI_ACCEL_REG_CONTROL, AI_ACCEL_CONTROL_RESET, "control reset") ||
            !load_reg(bus, AI_ACCEL_REG_DOORBELL_COUNT_LOW, 0, "expected reset doorbell count") ||
            !load_reg(bus, AI_ACCEL_REG_COMPLETION_COUNT_LOW, 0, "expected reset completion count") ||
            !load_reg(bus, AI_ACCEL_REG_BUSY_CYCLES_LOW, 0, "expected reset busy cycles") ||
            !load_reg(bus, AI_ACCEL_REG_QUEUE_CYCLES_LOW, 0, "expected reset queue cycles") ||
            !load_reg(bus, AI_ACCEL_REG_COMPLETION_CYCLES_LOW, 0, "expected reset completion cycles") ||
            !load_reg(bus,
                      AI_ACCEL_REG_EFFECTIVE_OPS_PER_CYCLE,
                      0,
                      "expected reset effective ops per cycle") ||
            !load_reg(bus, AI_ACCEL_REG_UTILIZATION, 0, "expected reset utilization") ||
            !load_reg(bus, AI_ACCEL_REG_LAST_FAULT, AI_ACCEL_FAULT_NONE, "expected reset fault")) {
            return 1;
        }

        if (!program_queues(bus)) {
            return 1;
        }
        const AiSubmissionDescriptor invalid_descriptor{
            .token = 0x55ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = 0,
            .flags = 0,
            .input_table_addr = 0,
            .output_table_addr = 0,
            .source_tag = 3,
        };
        write_descriptor(ram, kSubmitQueueAddr, invalid_descriptor);

        if (!store_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "invalid sq tail") ||
            !store_reg(bus, AI_ACCEL_REG_DOORBELL, 1, "invalid doorbell") ||
            !load_reg(bus, AI_ACCEL_REG_LAST_FAULT, AI_ACCEL_FAULT_INVALID_DESCRIPTOR, "expected invalid descriptor fault")) {
            return 1;
        }

        const AiCompletionEntry fault_completion = read_completion(ram, kCompleteQueueAddr);
        uint64_t fault_busy_cycles = 0;
        uint64_t fault_queue_cycles = 0;
        uint64_t fault_completion_cycles = 0;
        return expect(fault_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                      "expected fault completion status") &&
                       expect(fault_completion.fault_code == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                              "expected fault completion code") &&
                       load_counter(bus,
                                    AI_ACCEL_REG_BUSY_CYCLES_LOW,
                                    AI_ACCEL_REG_BUSY_CYCLES_HIGH,
                                    fault_busy_cycles) &&
                       load_counter(bus,
                                    AI_ACCEL_REG_QUEUE_CYCLES_LOW,
                                    AI_ACCEL_REG_QUEUE_CYCLES_HIGH,
                                    fault_queue_cycles) &&
                       load_counter(bus,
                                    AI_ACCEL_REG_COMPLETION_CYCLES_LOW,
                                    AI_ACCEL_REG_COMPLETION_CYCLES_HIGH,
                                    fault_completion_cycles) &&
                       expect(fault_busy_cycles == 2, "expected fault completion busy attribution") &&
                       expect(fault_queue_cycles == 1, "expected fault path queue attribution") &&
                       expect(fault_completion_cycles == 1, "expected fault completion attribution")
                   ? 0
                   : 1;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
