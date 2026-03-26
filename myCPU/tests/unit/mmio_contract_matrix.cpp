#include <cstdio>
#include <exception>

#include "../../src/devices/clint.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/simple_storage.h"
#include "../../src/devices/uart16550.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../src/platform/address_map.h"

namespace {

bool expect_load(Bus& bus,
                 uint64_t addr,
                 int size,
                 uint64_t expected,
                 const char* message) {
    uint64_t value = 0;
    if (!bus.try_load(addr, size, value) || value != expected) {
        std::fprintf(stderr,
                     "%s at 0x%llx size %d\n",
                     message,
                     static_cast<unsigned long long>(addr),
                     size);
        return false;
    }
    return true;
}

bool expect_load_fail(Bus& bus, uint64_t addr, int size, const char* message) {
    uint64_t value = 0;
    if (bus.try_load(addr, size, value)) {
        std::fprintf(stderr,
                     "%s unexpectedly succeeded at 0x%llx size %d\n",
                     message,
                     static_cast<unsigned long long>(addr),
                     size);
        return false;
    }
    return true;
}

bool expect_store_ok(Bus& bus,
                     uint64_t addr,
                     uint64_t value,
                     int size,
                     const char* message) {
    if (!bus.try_store(addr, value, size)) {
        std::fprintf(stderr,
                     "%s failed at 0x%llx size %d\n",
                     message,
                     static_cast<unsigned long long>(addr),
                     size);
        return false;
    }
    return true;
}

bool expect_store_fail(Bus& bus,
                       uint64_t addr,
                       uint64_t value,
                       int size,
                       const char* message) {
    if (bus.try_store(addr, value, size)) {
        std::fprintf(stderr,
                     "%s unexpectedly succeeded at 0x%llx size %d\n",
                     message,
                     static_cast<unsigned long long>(addr),
                     size);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    try {
        Ram ram;
        Bus bus(ram);
        Clint clint;
        Plic plic;
        Uart16550 uart(plic);
        SimpleStorage storage;

        uart.set_mirror_stdout(false);
        bus.attach(uart);
        bus.attach(clint);
        bus.attach(plic);
        bus.attach(storage);

        if (!expect_load(bus, UART_BASE + UART_REG_IIR, 1, 0x01, "expected UART IIR load") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_IER, 0xFF, 1, "expected UART IER write") ||
            !expect_load(bus, UART_BASE + UART_REG_IER, 1, UART_IER_THRI, "expected UART IER mask") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_THR, 'Z', 1, "expected UART THR write") ||
            uart.output_size() != 1 ||
            !expect_load_fail(bus, UART_BASE + UART_REG_THR, 1, "expected UART THR read to fail") ||
            !expect_store_fail(bus, UART_BASE + UART_REG_LSR, 0, 1, "expected UART LSR write to fail") ||
            !expect_load_fail(bus, UART_BASE + UART_REG_IER, 2, "expected UART wide load to fail") ||
            !expect_store_fail(bus, UART_BASE + UART_SIZE - 1, 0, 2, "expected UART boundary crossing write to fail")) {
            return 1;
        }

        if (!expect_store_ok(bus,
                             CLINT_BASE + CLINT_REG_MTIME,
                             UINT64_C(0x1122334455667788),
                             8,
                             "expected CLINT mtime write") ||
            !expect_load(bus,
                         CLINT_BASE + CLINT_REG_MTIME,
                         8,
                         UINT64_C(0x1122334455667788),
                         "expected CLINT mtime load") ||
            !expect_load(bus,
                         CLINT_BASE + CLINT_REG_MTIME + 4,
                         4,
                         UINT64_C(0x11223344),
                         "expected CLINT high-half load") ||
            !expect_store_fail(bus,
                               CLINT_BASE + CLINT_REG_MTIMECMP,
                               0,
                               3,
                               "expected CLINT invalid width write to fail") ||
            !expect_load_fail(bus,
                              CLINT_BASE + CLINT_REG_MTIME + 7,
                              2,
                              "expected CLINT cross-window load to fail") ||
            !expect_load_fail(bus, CLINT_BASE, 1, "expected CLINT base offset load to fail")) {
            return 1;
        }

        if (!expect_load(bus,
                         PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE),
                         4,
                         0,
                         "expected PLIC priority load") ||
            !expect_store_ok(bus,
                             PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE),
                             7,
                             4,
                             "expected PLIC priority write") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE),
                         4,
                         7,
                         "expected PLIC priority readback") ||
            !expect_store_ok(bus,
                             PLIC_BASE + PLIC_ENABLE_OFFSET(PLIC_CONTEXT_MACHINE),
                             (1U << PLIC_SOURCE_UART_THRE),
                             4,
                             "expected PLIC enable write") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_ENABLE_OFFSET(PLIC_CONTEXT_MACHINE),
                         4,
                         (1U << PLIC_SOURCE_UART_THRE),
                         "expected PLIC enable readback") ||
            !expect_load_fail(bus,
                              PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE),
                              1,
                              "expected PLIC byte load to fail") ||
            !expect_store_fail(bus,
                               PLIC_BASE + PLIC_PENDING_OFFSET,
                               0,
                               4,
                               "expected PLIC pending write to fail") ||
            !expect_load_fail(bus, PLIC_BASE + 0x8, 4, "expected invalid PLIC offset load to fail")) {
            return 1;
        }

        if (!expect_load(bus, STORAGE_BASE + STORAGE_REG_MAGIC, 8, STORAGE_MMIO_MAGIC, "expected storage magic load") ||
            !expect_store_ok(bus, STORAGE_BASE + STORAGE_REG_LBA, 3, 8, "expected storage LBA write") ||
            !expect_load(bus, STORAGE_BASE + STORAGE_REG_LBA, 8, 3, "expected storage LBA readback") ||
            !expect_store_ok(bus,
                             STORAGE_BASE + STORAGE_DATA_WINDOW_OFFSET + STORAGE_DATA_WINDOW_SIZE - 8,
                             UINT64_C(0x8877665544332211),
                             8,
                             "expected storage edge window write") ||
            !expect_load(bus,
                         STORAGE_BASE + STORAGE_DATA_WINDOW_OFFSET + STORAGE_DATA_WINDOW_SIZE - 8,
                         8,
                         UINT64_C(0x8877665544332211),
                         "expected storage edge window readback") ||
            !expect_load_fail(bus,
                              STORAGE_BASE + STORAGE_REG_COMMAND,
                              8,
                              "expected storage COMMAND read to fail") ||
            !expect_store_fail(bus,
                               STORAGE_BASE + STORAGE_REG_MAGIC,
                               STORAGE_MMIO_MAGIC,
                               8,
                               "expected storage MAGIC write to fail") ||
            !expect_store_fail(bus,
                               STORAGE_BASE + STORAGE_REG_COMMAND,
                               STORAGE_CMD_READ,
                               4,
                               "expected storage narrow COMMAND write to fail") ||
            !expect_load_fail(bus,
                              STORAGE_BASE + STORAGE_DATA_WINDOW_OFFSET + STORAGE_DATA_WINDOW_SIZE - 4,
                              8,
                              "expected storage window crossing load to fail")) {
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
