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

bool load_counter(Bus& bus, uint32_t low_reg, uint32_t high_reg, uint64_t& value) {
    uint64_t low = 0;
    uint64_t high = 0;
    return bus.try_load(AI_ACCEL_BASE + low_reg, 4, low) &&
           bus.try_load(AI_ACCEL_BASE + high_reg, 4, high) &&
           ((value = (high << 32) | static_cast<uint32_t>(low)), true);
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
                    "guest AI accel demo should write one completion entry")) {
            return 1;
        }

        Bus& bus = machine.bus();
        uint64_t device_cycles = 0;
        uint64_t dma_cycles = 0;
        uint64_t compute_cycles = 0;
        uint64_t stall_cycles = 0;
        uint64_t dma_load_bytes = 0;
        uint64_t dma_store_bytes = 0;
        if (!load_counter(bus,
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

        if (!expect(device_cycles == 8, "guest AI accel demo should lock device_cycles=8") ||
            !expect(dma_cycles == 6, "guest AI accel demo should lock dma_cycles=6") ||
            !expect(compute_cycles == 1, "guest AI accel demo should lock compute_cycles=1") ||
            !expect(stall_cycles == 1, "guest AI accel demo should lock stall_cycles=1") ||
            !expect(dma_load_bytes == 12, "guest AI accel demo should lock dma_load_bytes=12") ||
            !expect(dma_store_bytes == 4, "guest AI accel demo should lock dma_store_bytes=4")) {
            return 1;
        }

        std::puts("ai_accel_guest_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
