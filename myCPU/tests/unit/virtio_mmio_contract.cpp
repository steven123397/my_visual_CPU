#include <cstdio>
#include <exception>

#include "../../src/devices/plic.h"
#include "../../src/devices/virtio_blk.h"
#include "../../src/devices/virtio_mmio.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

bool expect_load(Bus& bus, uint64_t addr, uint64_t expected, const char* message) {
    uint64_t value = 0;
    if (!bus.try_load(addr, 4, value) || value != expected) {
        std::fprintf(stderr,
                     "%s at 0x%llx expected 0x%llx got 0x%llx\n",
                     message,
                     static_cast<unsigned long long>(addr),
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(value));
        return false;
    }
    return true;
}

bool expect_store(Bus& bus, uint64_t addr, uint64_t value, const char* message) {
    if (!bus.try_store(addr, value, 4)) {
        std::fprintf(stderr,
                     "%s at 0x%llx value 0x%llx\n",
                     message,
                     static_cast<unsigned long long>(addr),
                     static_cast<unsigned long long>(value));
        return false;
    }
    return true;
}

}  // namespace

int main() {
    try {
        Ram ram;
        Bus bus(ram);
        Plic plic;
        VirtioBlk blk;
        VirtioMmio mmio(plic, VIRTIO_MMIO_PLIC_SOURCE, blk);
        mmio.bind_bus(bus);

        bus.attach(mmio);
        bus.attach(plic);

        if (!expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_MAGIC_VALUE,
                         VIRTIO_MMIO_MAGIC_VALUE,
                         "expected virtio magic value") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_VERSION,
                         VIRTIO_MMIO_VERSION_VALUE,
                         "expected virtio version") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DEVICE_ID,
                         VIRTIO_DEVICE_ID_BLOCK,
                         "expected block device id") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_VENDOR_ID,
                         VIRTIO_MMIO_VENDOR_ID,
                         "expected vendor id") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DEVICE_FEATURES,
                         0,
                         "expected legacy feature word to stay empty") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DEVICE_FEATURES_SEL,
                          1,
                          "expected device features select write for high word") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DEVICE_FEATURES,
                         0x1,
                         "expected modern feature word to expose VIRTIO_F_VERSION_1") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DEVICE_FEATURES_SEL,
                          0,
                          "expected device features select reset") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_SEL,
                          0,
                          "expected queue select write") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DRIVER_FEATURES_SEL,
                          0,
                          "expected driver features select write") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DRIVER_FEATURES,
                          0x12345678,
                          "expected driver features write") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DRIVER_FEATURES,
                         0x12345678,
                         "expected driver features readback") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DRIVER_FEATURES_SEL,
                          1,
                          "expected high-word driver features select write") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DRIVER_FEATURES,
                          0x1,
                          "expected high-word driver features write") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DRIVER_FEATURES,
                         0x1,
                         "expected high-word driver features readback") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_DRIVER_FEATURES_SEL,
                          0,
                          "expected driver features select reset") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_NUM_MAX,
                         8,
                         "expected queue num max") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_NUM,
                          8,
                          "expected queue num write") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_DESC_LOW,
                          MEM_BASE + 0x1000,
                          "expected queue desc write") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_DRIVER_LOW,
                          MEM_BASE + 0x2000,
                          "expected queue driver write") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_DEVICE_LOW,
                          MEM_BASE + 0x3000,
                          "expected queue device write") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_READY,
                          1,
                          "expected queue ready write") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_READY,
                         1,
                         "expected queue ready readback") ||
            !expect_store(bus,
                          VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_STATUS,
                          VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER,
                          "expected status write") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_STATUS,
                         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER,
                         "expected status readback") ||
            !expect_load(bus,
                         VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_INTERRUPT_STATUS,
                         0,
                         "expected clear interrupt status")) {
            return 1;
        }

        uint64_t value = 0;
        if (bus.try_load(VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_QUEUE_NOTIFY, 4, value)) {
            return fail("expected queue notify register load to fail");
        }
        if (bus.try_store(VIRTIO_MMIO_BASE + VIRTIO_MMIO_REG_STATUS, 1, 8)) {
            return fail("expected wide status store to fail");
        }
        if (bus.try_load(VIRTIO_MMIO_BASE + 0x74, 4, value)) {
            return fail("expected invalid register offset to fail");
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
