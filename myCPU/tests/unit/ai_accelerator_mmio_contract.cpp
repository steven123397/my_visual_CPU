#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>

#include "../../src/devices/ai_accelerator.h"
#include "../../src/devices/ai_submission_queue.h"
#include "../../src/devices/plic.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

constexpr uint64_t kSubmitQueueAddr = MEM_BASE + 0x1000;
constexpr uint64_t kCompleteQueueAddr = MEM_BASE + 0x4000;
constexpr uint64_t kGraphPackageAddr = MEM_BASE + 0x8000;

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

        const AiSubmissionDescriptor descriptor{
            .token = 0xabcdeULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = 64,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = MEM_BASE + 0x9000,
            .output_table_addr = MEM_BASE + 0xa000,
            .source_tag = 7,
        };
        write_descriptor(ram, kSubmitQueueAddr, descriptor);
        ram.fill(kGraphPackageAddr, 0, descriptor.graph_package_bytes);

        if (!store_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "sq tail") ||
            !store_reg(bus, AI_ACCEL_REG_DOORBELL, 1, "doorbell") ||
            !load_reg(bus, AI_ACCEL_REG_SUBMIT_QUEUE_HEAD, 1, "expected submission head advance") ||
            !load_reg(bus, AI_ACCEL_REG_COMPLETE_QUEUE_TAIL, 1, "expected completion tail advance") ||
            !load_reg(bus, AI_ACCEL_REG_QUEUE_DEPTH, 0, "expected empty queue after completion") ||
            !load_reg(bus, AI_ACCEL_REG_IRQ_STATUS, AI_ACCEL_IRQ_COMPLETION, "expected completion interrupt")) {
            return 1;
        }

        const AiCompletionEntry completion = read_completion(ram, kCompleteQueueAddr);
        if (!expect(completion.token == descriptor.token, "expected completion token") ||
            !expect(completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected success completion") ||
            !expect(completion.fault_code == AI_ACCEL_FAULT_NONE, "expected no completion fault") ||
            !expect(completion.source_tag == descriptor.source_tag, "expected source tag roundtrip") ||
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
        return expect(fault_completion.status == AI_ACCEL_COMPLETION_STATUS_FAULT,
                      "expected fault completion status") &&
                       expect(fault_completion.fault_code == AI_ACCEL_FAULT_INVALID_DESCRIPTOR,
                              "expected fault completion code")
                   ? 0
                   : 1;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
