#include <cstdio>
#include <exception>

#include "../../src/devices/plic.h"
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

}  // namespace

int main() {
    try {
        Ram ram;
        Bus bus(ram);
        Plic plic;
        Uart16550 uart(plic);

        uart.set_mirror_stdout(false);
        bus.attach(uart);

        uint64_t lsr = 0;
        if (!bus.try_load(UART_BASE + UART_REG_LSR, 1, lsr) ||
            (lsr & UART_LSR_DR) != 0) {
            std::fprintf(stderr, "expected UART RX to start empty\n");
            return 1;
        }

        uart.inject_input("AB");

        if (!bus.try_load(UART_BASE + UART_REG_LSR, 1, lsr) ||
            (lsr & UART_LSR_DR) == 0) {
            std::fprintf(stderr, "expected UART LSR to report RX data ready\n");
            return 1;
        }

        if (!expect_store_ok(bus,
                             UART_BASE + UART_REG_IER,
                             UART_IER_RDI,
                             1,
                             "expected UART RX interrupt enable write") ||
            !plic.source_level(Plic::UART_SOURCE_ID) ||
            !expect_load(bus,
                         UART_BASE + UART_REG_IIR,
                         1,
                         UART_IIR_RDI,
                         "expected UART IIR RX interrupt identity")) {
            std::fprintf(stderr, "expected UART RX interrupt source to assert when data arrives\n");
            return 1;
        }

        if (!expect_load(bus,
                         UART_BASE + UART_REG_RBR,
                         1,
                         'A',
                         "expected first UART RX byte") ||
            !plic.source_level(Plic::UART_SOURCE_ID) ||
            !expect_load(bus,
                         UART_BASE + UART_REG_RBR,
                         1,
                         'B',
                         "expected second UART RX byte")) {
            return 1;
        }

        if (!bus.try_load(UART_BASE + UART_REG_LSR, 1, lsr) ||
            (lsr & UART_LSR_DR) != 0 ||
            plic.source_level(Plic::UART_SOURCE_ID)) {
            std::fprintf(stderr, "expected UART RX ready flag to clear after draining input\n");
            return 1;
        }

        if (!expect_store_ok(bus,
                             UART_BASE + UART_REG_THR,
                             'Z',
                             1,
                             "expected UART TX write after RX consumption") ||
            uart.output_size() != 1 ||
            uart.output() != "Z") {
            std::fprintf(stderr, "expected UART TX path to remain unchanged\n");
            return 1;
        }

        if (!bus.try_load(UART_BASE + UART_REG_LSR, 1, lsr) ||
            (lsr & (UART_LSR_THRE | UART_LSR_TEMT)) != 0) {
            std::fprintf(stderr, "expected UART TX write to clear THRE/TEMT until the next tick\n");
            return 1;
        }

        if (!expect_store_ok(bus,
                             UART_BASE + UART_REG_IER,
                             UART_IER_THRI,
                             1,
                             "expected UART TX interrupt enable write") ||
            plic.source_level(Plic::UART_SOURCE_ID)) {
            std::fprintf(stderr, "expected UART THRE interrupt to stay low while TX is still draining\n");
            return 1;
        }

        uart.tick();

        if (!bus.try_load(UART_BASE + UART_REG_LSR, 1, lsr) ||
            (lsr & (UART_LSR_THRE | UART_LSR_TEMT)) != (UART_LSR_THRE | UART_LSR_TEMT) ||
            !plic.source_level(Plic::UART_SOURCE_ID) ||
            !expect_load(bus,
                         UART_BASE + UART_REG_IIR,
                         1,
                         UART_IIR_THRI,
                         "expected UART IIR THRE interrupt identity after TX drain tick") ||
            plic.source_level(Plic::UART_SOURCE_ID)) {
            std::fprintf(stderr, "expected UART THRE interrupt to reassert after TX drain tick\n");
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
