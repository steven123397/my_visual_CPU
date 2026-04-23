#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <string>

#include "../../src/debug/debug_protocol.h"
#include "../../src/devices/ai_accelerator.h"
#include "../../src/devices/ai_submission_queue.h"
#include "../../src/platform/machine.h"

namespace {

constexpr uint64_t kSubmitQueueAddr = MEM_BASE + 0x12000;
constexpr uint64_t kCompleteQueueAddr = MEM_BASE + 0x14000;
constexpr uint64_t kGraphPackageAddr = MEM_BASE + 0x16000;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool expect_contains(const std::string& text, const char* needle, const char* message) {
    if (text.find(needle) == std::string::npos) {
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

        const AiSubmissionDescriptor descriptor{
            .token = 0x7777ULL,
            .graph_package_addr = kGraphPackageAddr,
            .graph_package_bytes = 128,
            .flags = AI_ACCEL_SUBMISSION_FLAG_PROFILE,
            .input_table_addr = MEM_BASE + 0x18000,
            .output_table_addr = MEM_BASE + 0x1a000,
            .source_tag = 11,
        };
        std::array<uint8_t, kAiSubmissionDescriptorBytes> descriptor_bytes{};
        encode_ai_submission_descriptor(descriptor, descriptor_bytes);
        std::array<uint8_t, 128> package_bytes{};
        if (!store_bytes(bus, kSubmitQueueAddr, descriptor_bytes.data(), descriptor_bytes.size()) ||
            !store_bytes(bus, kGraphPackageAddr, package_bytes.data(), package_bytes.size()) ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_SUBMIT_QUEUE_TAIL, 1, "submit tail") ||
            !store_u32(bus, AI_ACCEL_BASE + AI_ACCEL_REG_DOORBELL, 1, "doorbell")) {
            return 1;
        }

        std::array<uint8_t, kAiCompletionEntryBytes> completion_bytes{};
        AiCompletionEntry completion{};
        uint64_t claimed = 0;
        if (!load_bytes(bus, kCompleteQueueAddr, completion_bytes.data(), completion_bytes.size())) {
            return 1;
        }
        decode_ai_completion_entry(completion_bytes, completion);
        if (!expect(completion.token == descriptor.token, "expected host completion token") ||
            !expect(completion.status == AI_ACCEL_COMPLETION_STATUS_SUCCESS, "expected host completion success") ||
            !expect(machine.ai_accelerator().completion_count() == 1, "expected machine AI completion count") ||
            !expect(machine.ai_accelerator().doorbell_count() == 1, "expected machine AI doorbell count") ||
            !expect(machine.plic().supervisor_has_pending(), "expected AI accelerator supervisor IRQ") ||
            !expect(bus.try_load(PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR), 4, claimed) &&
                        claimed == AI_ACCEL_PLIC_SOURCE,
                    "expected AI accelerator PLIC claim")) {
            return 1;
        }

        DebugSnapshot snapshot{};
        snapshot.devices.ai_accelerator = machine.ai_accelerator().debug_snapshot();
        const std::string json = debug_snapshot_json(snapshot);
        if (!expect_contains(json, "\"ai_accelerator\":{", "expected AI debug snapshot object") ||
            !expect_contains(json, "\"queue_depth\":0", "expected AI queue depth in debug JSON") ||
            !expect_contains(json, "\"doorbell_count\":1", "expected AI doorbell count in debug JSON") ||
            !expect_contains(json, "\"completion_count\":1", "expected AI completion count in debug JSON")) {
            return 1;
        }

        std::puts("ai_accelerator_submit_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
