#include <cstdio>
#include <cstring>
#include <exception>
#include <vector>

#include "../../src/devices/clint.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/dma_transaction.h"
#include "../../src/mem/ram.h"

namespace {

class DmaWindowDevice final : public Device {
public:
    DmaWindowDevice(uint64_t base,
                    size_t size,
                    PhysicalRegionInfo region,
                    size_t fail_after_reads,
                    size_t fail_after_writes)
        : Device(base, size),
          bytes_(size, 0),
          region_(region),
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
        bytes_[offset] = static_cast<uint8_t>(value & 0xFFU);
    }

    PhysicalRegionInfo region_info() const override {
        return region_;
    }

    const char* debug_name() const override {
        return region_.label;
    }

    void seed(const std::vector<uint8_t>& bytes) {
        if (bytes.size() > bytes_.size()) {
            throw std::runtime_error("seed data too large");
        }
        std::copy(bytes.begin(), bytes.end(), bytes_.begin());
    }

private:
    std::vector<uint8_t> bytes_;
    PhysicalRegionInfo region_{};
    size_t fail_after_reads_{0};
    size_t fail_after_writes_{0};
    size_t read_count_{0};
    size_t write_count_{0};
};

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

bool expect_fault(const DmaTransferResult& result,
                 DmaFault fault,
                 size_t transferred,
                 DmaDirection direction,
                 const char* initiator) {
    return result.ok == (fault == DmaFault::None) && result.fault == fault &&
           result.transferred_bytes == transferred && result.direction == direction &&
           std::strcmp(result.initiator, initiator) == 0;
}

}  // namespace

int main() {
    try {
        Ram ram;
        Bus bus(ram);

        const uint8_t source[] = {0x11, 0x22, 0x33, 0x44};
        ram.write_bytes(MEM_BASE + 0x100, source, sizeof(source));

        uint8_t readback[sizeof(source)] = {};
        DmaTransaction read_tx{
            .initiator = "unit-read",
            .addr = MEM_BASE + 0x100,
            .size = sizeof(readback),
            .burst = true,
        };
        const DmaTransferResult read_result = bus.dma_read(read_tx, readback);
        if (!expect_fault(
                read_result, DmaFault::None, sizeof(readback), DmaDirection::Read, "unit-read") ||
            std::memcmp(readback, source, sizeof(source)) != 0) {
            return fail("expected RAM DMA read to succeed");
        }

        const uint8_t write_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
        DmaTransaction write_tx{
            .initiator = "unit-write",
            .addr = MEM_BASE + 0x200,
            .size = sizeof(write_data),
            .burst = true,
        };
        const DmaTransferResult write_result = bus.dma_write(write_tx, write_data);
        uint64_t observed = 0;
        if (!expect_fault(
                write_result, DmaFault::None, sizeof(write_data), DmaDirection::Write, "unit-write") ||
            !bus.try_load(MEM_BASE + 0x200, 4, observed) || observed != 0xDDCCBBAAU) {
            return fail("expected RAM DMA write to succeed");
        }

        Clint clint;
        bus.attach(clint);
        uint8_t mmio_bytes[8] = {};
        const DmaTransferResult mmio_result = bus.dma_read(
            DmaTransaction{
                .initiator = "unit-mmio",
                .addr = CLINT_BASE + CLINT_REG_MTIME,
                .size = sizeof(mmio_bytes),
                .burst = true,
            },
            mmio_bytes);
        if (!expect_fault(mmio_result, DmaFault::SideEffectRegion, 0, DmaDirection::Read, "unit-mmio")) {
            return fail("expected side-effect MMIO DMA read to fail closed");
        }

        uint8_t unmapped_bytes[4] = {};
        const DmaTransferResult unmapped_result = bus.dma_read(
            DmaTransaction{
                .initiator = "unit-unmapped",
                .addr = 0x40000000,
                .size = sizeof(unmapped_bytes),
                .burst = true,
            },
            unmapped_bytes);
        if (!expect_fault(unmapped_result, DmaFault::Unmapped, 0, DmaDirection::Read, "unit-unmapped")) {
            return fail("expected unmapped DMA read to fault");
        }

        uint8_t boundary_bytes[8];
        std::memset(boundary_bytes, 0x5A, sizeof(boundary_bytes));
        const DmaTransferResult boundary_result = bus.dma_read(
            DmaTransaction{
                .initiator = "unit-boundary",
                .addr = MEM_BASE + MEM_SIZE - 4,
                .size = sizeof(boundary_bytes),
                .burst = true,
            },
            boundary_bytes);
        if (!expect_fault(
                boundary_result, DmaFault::SpanCrossesRegionBoundary, 0, DmaDirection::Read, "unit-boundary") ||
            boundary_bytes[0] != 0x5A || boundary_bytes[7] != 0x5A) {
            return fail("expected boundary-crossing DMA read to fail before any transfer");
        }

        DmaWindowDevice partial_device(
            0x40010000,
            8,
            PhysicalRegionInfo{
                .kind = PhysicalRegionKind::Mmio,
                .cacheable = false,
                .dma_visible = true,
                .has_side_effect = false,
                .supports_burst = true,
                .label = "dma-window",
            },
            3,
            static_cast<size_t>(-1));
        partial_device.seed({0x10, 0x20, 0x30, 0x40, 0x50});
        bus.attach(partial_device);

        uint8_t partial_bytes[5];
        std::memset(partial_bytes, 0xEE, sizeof(partial_bytes));
        const DmaTransferResult partial_result = bus.dma_read(
            DmaTransaction{
                .initiator = "unit-partial",
                .addr = 0x40010000,
                .size = sizeof(partial_bytes),
                .burst = true,
            },
            partial_bytes);
        if (!expect_fault(partial_result, DmaFault::DeviceFault, 3, DmaDirection::Read, "unit-partial") ||
            partial_bytes[0] != 0x10 || partial_bytes[1] != 0x20 || partial_bytes[2] != 0x30 ||
            partial_bytes[3] != 0xEE || partial_bytes[4] != 0xEE) {
            return fail("expected partial DMA read to report bytes transferred");
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
