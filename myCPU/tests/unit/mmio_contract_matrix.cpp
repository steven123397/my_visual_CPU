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

constexpr uint32_t kIntermediatePlicSource = PLIC_SOURCE_VIRTIO_MMIO + 1;
constexpr uint32_t kOutOfRangePlicSource = PLIC_SOURCE_UART_THRE + 1;

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

        if (!expect_load(bus, UART_BASE + UART_REG_IIR, 1, UART_IIR_NO_INT, "expected UART IIR load") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_IER, 0xFF, 1, "expected UART IER write") ||
            !expect_load(bus,
                         UART_BASE + UART_REG_IER,
                         1,
                         UART_IER_RDI | UART_IER_THRI,
                         "expected UART IER mask") ||
            !expect_load(bus,
                         UART_BASE + UART_REG_IIR,
                         1,
                         UART_IIR_THRI,
                         "expected UART THRE interrupt after enabling TX interrupt") ||
            !expect_load(bus,
                         UART_BASE + UART_REG_IIR,
                         1,
                         UART_IIR_NO_INT,
                         "expected UART IIR THRE interrupt to acknowledge on read") ||
            !expect_load(bus, UART_BASE + UART_REG_RBR, 1, 0, "expected empty UART RBR read") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_THR, 'Z', 1, "expected UART THR write") ||
            uart.output_size() != 1 ||
            uart.output() != "Z" ||
            !expect_store_ok(bus,
                             UART_BASE + UART_REG_LCR,
                             UART_LCR_DLAB,
                             1,
                             "expected UART LCR write for divisor latch access") ||
            !expect_load(bus,
                         UART_BASE + UART_REG_LCR,
                         1,
                         UART_LCR_DLAB,
                         "expected UART LCR readback") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_DLL, 0x03, 1, "expected UART DLL write") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_DLM, 0x00, 1, "expected UART DLM write") ||
            !expect_load(bus, UART_BASE + UART_REG_DLL, 1, 0x03, "expected UART DLL readback") ||
            !expect_load(bus, UART_BASE + UART_REG_DLM, 1, 0x00, "expected UART DLM readback") ||
            uart.output_size() != 1 ||
            uart.output() != "Z" ||
            !expect_store_ok(bus,
                             UART_BASE + UART_REG_LCR,
                             0x03,
                             1,
                             "expected UART LCR write to leave divisor latch mode") ||
            !expect_load(bus, UART_BASE + UART_REG_LCR, 1, 0x03, "expected UART word length readback") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_MCR, 0x0B, 1, "expected UART MCR write") ||
            !expect_load(bus, UART_BASE + UART_REG_MCR, 1, 0x0B, "expected UART MCR readback") ||
            !expect_store_ok(bus, UART_BASE + UART_REG_FCR, 0x07, 1, "expected UART FCR write") ||
            !expect_store_fail(bus, UART_BASE + UART_REG_LSR, 0, 1, "expected UART LSR write to fail") ||
            !expect_load(bus,
                         UART_BASE + UART_REG_MSR,
                         1,
                         UART_MSR_CTS | UART_MSR_DSR | UART_MSR_DCD,
                         "expected UART MSR carrier/modem ready bits") ||
            !expect_store_fail(bus, UART_BASE + UART_REG_MSR, 0, 1, "expected UART MSR write to fail") ||
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
            !expect_load(bus,
                         PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_VIRTIO_MMIO),
                         4,
                         0,
                         "expected virtio PLIC priority load") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_PRIORITY_OFFSET(kIntermediatePlicSource),
                         4,
                         0,
                         "expected intermediate PLIC priority load") ||
            !expect_store_ok(bus,
                             PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE),
                             7,
                             4,
                             "expected PLIC priority write") ||
            !expect_store_ok(bus,
                             PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_VIRTIO_MMIO),
                             3,
                             4,
                             "expected virtio PLIC priority write") ||
            !expect_store_ok(bus,
                             PLIC_BASE + PLIC_PRIORITY_OFFSET(kIntermediatePlicSource),
                             5,
                             4,
                             "expected intermediate PLIC priority write") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE),
                         4,
                         7,
                         "expected PLIC priority readback") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_VIRTIO_MMIO),
                         4,
                         3,
                         "expected virtio PLIC priority readback") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_PRIORITY_OFFSET(kIntermediatePlicSource),
                         4,
                         5,
                         "expected intermediate PLIC priority readback") ||
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
            !expect_load_fail(bus,
                              PLIC_BASE + PLIC_PRIORITY_OFFSET(kOutOfRangePlicSource),
                              4,
                              "expected out-of-range PLIC priority load to fail")) {
            return 1;
        }

        if (!expect_store_ok(bus,
                             PLIC_BASE + PLIC_ENABLE_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                             (1U << PLIC_SOURCE_UART_THRE) | (1U << PLIC_SOURCE_VIRTIO_MMIO),
                             4,
                             "expected PLIC supervisor enable write") ||
            !expect_store_ok(bus,
                             PLIC_BASE + PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_MACHINE),
                             0,
                             4,
                             "expected PLIC machine threshold write") ||
            !expect_store_ok(bus,
                             PLIC_BASE + PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                             0,
                             4,
                             "expected PLIC supervisor threshold write")) {
            return 1;
        }

        plic.set_source_level(PLIC_SOURCE_UART_THRE, true);
        if (!expect_load(bus,
                         PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                         4,
                         PLIC_SOURCE_UART_THRE,
                         "expected supervisor context claim to acquire source") ||
            !plic.source_claimed(PLIC_SOURCE_UART_THRE) ||
            plic.source_pending(PLIC_SOURCE_UART_THRE)) {
            std::fprintf(stderr, "expected claimed source to be owned and not pending\n");
            return 1;
        }

        if (!expect_store_ok(bus,
                             PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_MACHINE),
                             PLIC_SOURCE_UART_THRE,
                             4,
                             "expected machine context complete write to be accepted")) {
            return 1;
        }
        if (!plic.source_claimed(PLIC_SOURCE_UART_THRE) ||
            plic.source_pending(PLIC_SOURCE_UART_THRE) ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                         4,
                         0,
                         "expected supervisor claim to remain blocked after wrong-context complete")) {
            std::fprintf(stderr, "wrong-context complete should not release claimed source\n");
            return 1;
        }

        if (!expect_store_ok(bus,
                             PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                             PLIC_SOURCE_UART_THRE,
                             4,
                             "expected owner context complete write")) {
            return 1;
        }
        if (plic.source_claimed(PLIC_SOURCE_UART_THRE) ||
            !plic.source_pending(PLIC_SOURCE_UART_THRE) ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                         4,
                         PLIC_SOURCE_UART_THRE,
                         "expected source to become claimable again after owner complete")) {
            std::fprintf(stderr, "owner complete should release and re-pend asserted source\n");
            return 1;
        }
        if (!expect_store_ok(bus,
                             PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                             PLIC_SOURCE_UART_THRE,
                             4,
                             "expected final owner complete write")) {
            return 1;
        }

        plic.set_source_level(PLIC_SOURCE_UART_THRE, false);
        plic.set_source_level(PLIC_SOURCE_VIRTIO_MMIO, true);
        plic.set_source_level(PLIC_SOURCE_UART_THRE, true);
        if (!expect_load(bus,
                         PLIC_BASE + PLIC_PENDING_OFFSET,
                         4,
                         (1U << PLIC_SOURCE_UART_THRE) | (1U << PLIC_SOURCE_VIRTIO_MMIO),
                         "expected PLIC pending bits for uart and virtio") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                         4,
                         PLIC_SOURCE_UART_THRE,
                         "expected higher-priority UART source to claim first") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_PENDING_OFFSET,
                         4,
                         (1U << PLIC_SOURCE_VIRTIO_MMIO),
                         "expected only virtio to remain pending after UART claim")) {
            return 1;
        }
        plic.set_source_level(PLIC_SOURCE_UART_THRE, false);
        if (!expect_store_ok(bus,
                             PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                             PLIC_SOURCE_UART_THRE,
                             4,
                             "expected UART complete write") ||
            !expect_load(bus,
                         PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                         4,
                         PLIC_SOURCE_VIRTIO_MMIO,
                         "expected virtio source to remain claimable after UART completes")) {
            return 1;
        }
        if (!expect_store_ok(bus,
                             PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR),
                             PLIC_SOURCE_VIRTIO_MMIO,
                             4,
                             "expected virtio complete write")) {
            return 1;
        }
        plic.set_source_level(PLIC_SOURCE_VIRTIO_MMIO, false);
        plic.set_source_level(PLIC_SOURCE_UART_THRE, false);

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
