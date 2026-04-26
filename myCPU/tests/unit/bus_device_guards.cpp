#include <cstdio>
#include <exception>
#include <stdexcept>

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

class DummyDevice : public Device {
public:
    DummyDevice(uint64_t base, uint64_t size)
        : Device(base, size) {}

    uint64_t load(uint64_t, int) override {
        return 0;
    }

    void store(uint64_t, uint64_t, int) override {}
};

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

}  // namespace

int main() {
    try {
        {
            Ram ram;
            Bus bus(ram);
            DummyDevice first(0x1000, 0x10);
            DummyDevice second(0x1008, 0x10);

            bus.attach(first);

            bool threw = false;
            try {
                bus.attach(second);
            } catch (const std::runtime_error&) {
                threw = true;
            }
            if (!threw) {
                return fail("expected overlapping attach to fail");
            }
        }

	        {
	            Ram ram;
	            Bus bus(ram);
	            Clint clint;
	            Plic plic;
	            Uart16550 uart(plic);
	            SimpleStorage storage;
	            uint64_t value = 0;

	            bus.attach(uart);
	            bus.attach(clint);
	            bus.attach(plic);
	            bus.attach(storage);

            if (!bus.try_load(UART_BASE + UART_REG_LSR, 1, value) || value != 0x60) {
	                return fail("expected valid UART byte load to succeed");
            }
            if (bus.try_store(UART_BASE + UART_REG_THR, 'A', 4)) {
                return fail("expected invalid UART width to fail");
            }
            const DebugBusAccess& failed_uart_store = bus.last_access();
            if (!failed_uart_store.valid ||
                failed_uart_store.success ||
                failed_uart_store.addr != UART_BASE + UART_REG_THR ||
                failed_uart_store.size != 4 ||
                failed_uart_store.device != "uart" ||
                failed_uart_store.detail.empty()) {
                return fail("expected failed UART store to remain observable via last_access");
            }

            const uint64_t unmapped_load_addr = 0x40000000;
            if (bus.try_load(unmapped_load_addr, 4, value)) {
                return fail("expected unmapped load to fail");
            }
            const DebugBusAccess& unmapped_load = bus.last_access();
            if (!unmapped_load.valid ||
                unmapped_load.success ||
                unmapped_load.write ||
                unmapped_load.addr != unmapped_load_addr ||
                unmapped_load.size != 4 ||
                unmapped_load.device != "<unmapped>") {
                return fail("expected unmapped load to update last_access");
            }

            const uint64_t unmapped_store_addr = 0x50000000;
            if (bus.try_store(unmapped_store_addr, 0xAB, 1)) {
                return fail("expected unmapped store to fail");
            }
            const DebugBusAccess& unmapped_store = bus.last_access();
            if (!unmapped_store.valid ||
                unmapped_store.success ||
                !unmapped_store.write ||
                unmapped_store.addr != unmapped_store_addr ||
                unmapped_store.size != 1 ||
                unmapped_store.value != 0xAB ||
                unmapped_store.device != "<unmapped>") {
                return fail("expected unmapped store to update last_access");
            }

            if (!bus.try_store(UART_BASE + UART_REG_MCR, 0x0B, 1)) {
                return fail("expected valid UART MCR write to succeed");
            }
            if (!bus.try_load(UART_BASE + UART_REG_MCR, 1, value) || value != 0x0B) {
                return fail("expected valid UART MCR load to succeed");
            }
            if (!bus.try_load(CLINT_BASE + CLINT_REG_MTIME, 8, value) || value != 0) {
                return fail("expected valid CLINT register load to succeed");
            }
            if (bus.try_load(CLINT_BASE + CLINT_REG_MTIME + 7, 2, value)) {
                return fail("expected CLINT access crossing register boundary to fail");
            }
            if (!bus.try_store(PLIC_BASE + PLIC_PRIORITY_OFFSET(kIntermediatePlicSource), 1, 4)) {
                return fail("expected intermediate PLIC priority write to succeed");
            }
            if (!bus.try_load(PLIC_BASE + PLIC_PRIORITY_OFFSET(kIntermediatePlicSource), 4, value) || value != 1) {
                return fail("expected intermediate PLIC priority readback to succeed");
            }
            if (bus.try_store(PLIC_BASE + PLIC_PRIORITY_OFFSET(kOutOfRangePlicSource), 1, 4)) {
                return fail("expected out-of-range PLIC priority write to fail");
            }
            if (bus.try_load(PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE), 8, value)) {
                return fail("expected invalid PLIC width to fail");
            }
            if (!bus.try_load(STORAGE_BASE + STORAGE_REG_MAGIC, 8, value) || value != STORAGE_MMIO_MAGIC) {
                return fail("expected valid storage register load to succeed");
            }
            if (bus.try_store(STORAGE_BASE + STORAGE_REG_COMMAND, STORAGE_CMD_READ, 4)) {
                return fail("expected invalid storage register width to fail");
            }
            if (!bus.try_store(STORAGE_BASE + STORAGE_DATA_WINDOW_OFFSET + 510, 0xABCD, 2)) {
                return fail("expected valid storage window edge write to succeed");
            }
            if (bus.try_store(STORAGE_BASE + STORAGE_DATA_WINDOW_OFFSET + 511, 0xABCD, 2)) {
                return fail("expected storage window write crossing boundary to fail");
            }
            if (bus.try_load(STORAGE_BASE + 0x48, 8, value)) {
                return fail("expected invalid storage register offset to fail");
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
