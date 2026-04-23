#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <vector>

#include "../../src/devices/ai_dma_engine.h"
#include "../../src/devices/ai_scratchpad.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

class DmaFailWindow final : public Device {
public:
    DmaFailWindow(uint64_t base,
                  size_t size,
                  size_t fail_after_reads,
                  size_t fail_after_writes)
        : Device(base, size),
          bytes_(size, 0),
          fail_after_reads_(fail_after_reads),
          fail_after_writes_(fail_after_writes) {}

    uint64_t load(uint64_t addr, int size) override {
        if (size != 1) {
            invalid_access(addr, size);
        }
        const size_t offset = static_cast<size_t>(addr - base());
        if (offset >= bytes_.size()) {
            invalid_access(addr, size);
        }
        if (read_count_ >= fail_after_reads_) {
            throw std::runtime_error("forced DMA read failure");
        }
        ++read_count_;
        return bytes_[offset];
    }

    void store(uint64_t addr, uint64_t value, int size) override {
        if (size != 1) {
            invalid_access(addr, size);
        }
        const size_t offset = static_cast<size_t>(addr - base());
        if (offset >= bytes_.size()) {
            invalid_access(addr, size);
        }
        if (write_count_ >= fail_after_writes_) {
            throw std::runtime_error("forced DMA write failure");
        }
        ++write_count_;
        bytes_[offset] = static_cast<uint8_t>(value & 0xffU);
    }

    PhysicalRegionInfo region_info() const override {
        return {
            .kind = PhysicalRegionKind::Ram,
            .cacheable = false,
            .dma_visible = true,
            .has_side_effect = false,
            .supports_burst = true,
            .label = "dma-fail-window",
        };
    }

    const char* debug_name() const override {
        return "dma_fail_window";
    }

    void seed(const std::vector<uint8_t>& bytes) {
        for (size_t i = 0; i < bytes.size() && i < bytes_.size(); ++i) {
            bytes_[i] = bytes[i];
        }
    }

private:
    std::vector<uint8_t> bytes_{};
    size_t fail_after_reads_{0};
    size_t fail_after_writes_{0};
    size_t read_count_{0};
    size_t write_count_{0};
};

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

template <size_t N>
bool expect_readback(AiScratchpad& scratchpad,
                     AiScratchpadSpace space,
                     uint32_t offset,
                     const std::array<uint8_t, N>& expected,
                     const char* message) {
    std::array<uint8_t, N> bytes{};
    if (!scratchpad.read(space, offset, bytes.data(), bytes.size()) || bytes != expected) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    try {
        Ram ram;
        Bus bus(ram);
        AiScratchpad scratchpad;
        scratchpad.configure(32, 16, 16);
        AiDmaEngine engine(
            scratchpad,
            AiDmaTimingConfig{
                .setup_cycles = 3,
                .bytes_per_cycle = 4,
            });

        const std::array<uint8_t, 8> source{{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88}};
        ram.write_bytes(MEM_BASE + 0x100, source.data(), source.size());

        uint32_t fault = AI_ACCEL_FAULT_NONE;
        std::string error;
        if (!expect(
                engine.start(
                    AiDmaRequest{
                        .kind = AiDmaTransferKind::Load,
                        .system_addr = MEM_BASE + 0x100,
                        .space = AiScratchpadSpace::Scratchpad,
                        .scratchpad_offset = 0,
                        .size = source.size(),
                        .initiator = "unit-ai-dma",
                    },
                    fault,
                    error),
                "expected DMA load to start")) {
            return 1;
        }
        for (int i = 0; i < 4; ++i) {
            const AiDmaTickResult step = engine.tick(bus);
            if (!expect(!step.completed, "expected DMA load to stay busy before final tick")) {
                return 1;
            }
        }
        const AiDmaTickResult load_done = engine.tick(bus);
        const AiDmaCounters counters_after_load = engine.counters();
        if (!expect(load_done.completed, "expected DMA load completion on final tick") ||
            !expect(!load_done.faulted, "expected DMA load success") ||
            !expect_readback(
                scratchpad,
                AiScratchpadSpace::Scratchpad,
                0,
                source,
                "expected scratchpad bytes after DMA load") ||
            !expect(counters_after_load.load_cycles == 5, "expected DMA load cycle accounting") ||
            !expect(counters_after_load.load_bytes == source.size(), "expected DMA load byte accounting") ||
            !expect(counters_after_load.load_transfers == 1, "expected DMA load transfer count")) {
            return 1;
        }

        const std::array<uint8_t, 8> store_pattern{{0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97}};
        if (!expect(
                scratchpad.write(
                    AiScratchpadSpace::Scratchpad,
                    8,
                    store_pattern.data(),
                    store_pattern.size()),
                "expected scratchpad seed before DMA store")) {
            return 1;
        }
        if (!expect(
                engine.start(
                    AiDmaRequest{
                        .kind = AiDmaTransferKind::Store,
                        .system_addr = MEM_BASE + 0x200,
                        .space = AiScratchpadSpace::Scratchpad,
                        .scratchpad_offset = 8,
                        .size = store_pattern.size(),
                        .initiator = "unit-ai-dma",
                    },
                    fault,
                    error),
                "expected DMA store to start")) {
            return 1;
        }
        for (int i = 0; i < 5; ++i) {
            const AiDmaTickResult step = engine.tick(bus);
            if (i < 4 && !expect(!step.completed, "expected DMA store to stay busy before final tick")) {
                return 1;
            }
        }
        std::array<uint8_t, 8> ram_readback{};
        if (!bus.dma_read(
                DmaTransaction{
                    .initiator = "unit-readback",
                    .addr = MEM_BASE + 0x200,
                    .size = ram_readback.size(),
                    .burst = true,
                    .direction = DmaDirection::Read,
                },
                ram_readback.data())
                 .ok ||
            !expect(ram_readback == store_pattern, "expected RAM bytes after DMA store") ||
            !expect(engine.counters().store_cycles == 5, "expected DMA store cycle accounting") ||
            !expect(engine.counters().store_bytes == store_pattern.size(), "expected DMA store byte accounting") ||
            !expect(engine.counters().store_transfers == 1, "expected DMA store transfer count")) {
            return 1;
        }

        if (!expect(
                !engine.start(
                    AiDmaRequest{
                        .kind = AiDmaTransferKind::Load,
                        .system_addr = MEM_BASE + 0x100,
                        .space = AiScratchpadSpace::Scratchpad,
                        .scratchpad_offset = 28,
                        .size = 8,
                        .initiator = "unit-ai-dma",
                    },
                    fault,
                    error),
                "expected scratchpad overflow rejection") ||
            !expect(fault == AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW, "expected scratchpad overflow fault code")) {
            return 1;
        }

        DmaFailWindow fail_window(0x40010000, 8, 2, static_cast<size_t>(-1));
        fail_window.seed({0xDE, 0xAD, 0xBE, 0xEF});
        bus.attach(fail_window);
        const std::array<uint8_t, 4> sentinel{{0xA0, 0xA1, 0xA2, 0xA3}};
        if (!expect(
                scratchpad.write(
                    AiScratchpadSpace::Scratchpad,
                    0,
                    sentinel.data(),
                    sentinel.size()),
                "expected sentinel scratchpad write")) {
            return 1;
        }
        if (!expect(
                engine.start(
                    AiDmaRequest{
                        .kind = AiDmaTransferKind::Load,
                        .system_addr = 0x40010000,
                        .space = AiScratchpadSpace::Scratchpad,
                        .scratchpad_offset = 0,
                        .size = sentinel.size(),
                        .initiator = "unit-ai-dma",
                    },
                    fault,
                    error),
                "expected partial-failure DMA load to start")) {
            return 1;
        }
        AiDmaTickResult failure{};
        for (int i = 0; i < 4; ++i) {
            failure = engine.tick(bus);
        }
        if (!expect(failure.completed, "expected partial-failure DMA load to finish") ||
            !expect(failure.faulted, "expected partial-failure DMA load to fault") ||
            !expect(failure.fault_code == AI_ACCEL_FAULT_DMA, "expected DMA fault code on partial load") ||
            !expect_readback(
                scratchpad,
                AiScratchpadSpace::Scratchpad,
                0,
                sentinel,
                "expected scratchpad to remain unchanged on partial DMA load")) {
            return 1;
        }

        std::puts("ai_dma_engine: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
