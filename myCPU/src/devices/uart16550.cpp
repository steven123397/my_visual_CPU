#include "uart16550.h"

#include <cstdio>

#include "plic.h"

namespace {

constexpr uint32_t UART_IIR_NO_INT = 0x01;
constexpr uint32_t UART_IIR_THRI = 0x02;
constexpr uint32_t UART_LSR_THRE = 0x20;
constexpr uint32_t UART_LSR_TEMT = 0x40;

}  // namespace

Uart16550::Uart16550(Plic& plic) : Device(UART_BASE, UART_SIZE), plic_(plic) {}

uint64_t Uart16550::load(uint64_t addr, int size) {
    if (size != 1) {
        invalid_access(addr, size);
    }

    const uint32_t offset = static_cast<uint32_t>(addr - UART_BASE);
    if (offset == UART_REG_RBR) {
        if (input_.empty()) {
            return 0;
        }
        const uint8_t value = input_.front();
        input_.pop_front();
        return value;
    }
    if (offset == UART_REG_IER) {
        return ier_;
    }
    if (offset == UART_REG_IIR) {
        return (ier_ & UART_IER_THRI) ? UART_IIR_THRI : UART_IIR_NO_INT;
    }
    if (offset == UART_REG_LSR) {
        return (input_.empty() ? 0U : UART_LSR_DR) | UART_LSR_THRE | UART_LSR_TEMT;
    }

    invalid_access(addr, size);
}

void Uart16550::store(uint64_t addr, uint64_t value, int size) {
    if (size != 1) {
        invalid_access(addr, size);
    }

    const uint32_t offset = static_cast<uint32_t>(addr - UART_BASE);
    if (offset == UART_REG_THR) {
        const char ch = static_cast<char>(value & 0xFF);
        output_.push_back(ch);
        if (mirror_stdout_) {
            std::putchar(static_cast<int>(static_cast<unsigned char>(ch)));
            std::fflush(stdout);
        }
        return;
    }
    if (offset == UART_REG_IER) {
        ier_ = static_cast<uint8_t>(value) & UART_IER_THRI;
        update_interrupt_line();
        return;
    }

    invalid_access(addr, size);
}

void Uart16550::update_interrupt_line() {
    plic_.set_source_level(Plic::UART_SOURCE_ID, (ier_ & UART_IER_THRI) != 0);
}

uint8_t Uart16550::ier() const {
    return ier_;
}

bool Uart16550::thre_interrupt_asserted() const {
    return (ier_ & UART_IER_THRI) != 0;
}

size_t Uart16550::output_size() const {
    return output_.size();
}

const std::string& Uart16550::output() const {
    return output_;
}

void Uart16550::inject_input(std::string_view text) {
    for (const char ch : text) {
        input_.push_back(static_cast<uint8_t>(ch));
    }
}

void Uart16550::set_mirror_stdout(bool enabled) {
    mirror_stdout_ = enabled;
}
