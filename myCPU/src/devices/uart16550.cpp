#include "uart16550.h"

#include <cstdio>

namespace {

constexpr uint32_t UART_RBR = 0;
constexpr uint32_t UART_LSR = 5;
constexpr uint32_t UART_LSR_THRE = 0x20;

}  // namespace

bool Uart16550::contains(uint64_t addr) const {
    return addr >= UART_BASE && addr < UART_BASE + 8;
}

uint64_t Uart16550::load(uint64_t addr, int /*size*/) {
    const uint32_t offset = static_cast<uint32_t>(addr - UART_BASE);
    if (offset == UART_LSR) {
        return UART_LSR_THRE;
    }
    return 0;
}

void Uart16550::store(uint64_t addr, uint64_t value, int /*size*/) {
    const uint32_t offset = static_cast<uint32_t>(addr - UART_BASE);
    if (offset == UART_RBR) {
        std::putchar(static_cast<int>(value & 0xFF));
        std::fflush(stdout);
    }
}
