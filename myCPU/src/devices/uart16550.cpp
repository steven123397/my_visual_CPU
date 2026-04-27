#include "uart16550.h"

#include <cstdio>

#include "plic.h"

Uart16550::Uart16550(Plic& plic) : Device(UART_BASE, UART_SIZE), plic_(plic) {}

uint64_t Uart16550::load(uint64_t addr, int size) {
    if (size != 1) {
        invalid_access(addr, size);
    }

    const uint32_t offset = static_cast<uint32_t>(addr - UART_BASE);
    if (offset == UART_REG_RBR) {
        if (divisor_latch_enabled()) {
            return dll_;
        }
        if (input_.empty()) {
            return 0;
        }
        const uint8_t value = input_.front();
        input_.pop_front();
        update_interrupt_line();
        return value;
    }
    if (offset == UART_REG_IER) {
        if (divisor_latch_enabled()) {
            return dlm_;
        }
        return ier_;
    }
    if (offset == UART_REG_IIR) {
        if (rx_interrupt_pending()) {
            return UART_IIR_RDI;
        }
        if (tx_interrupt_pending_) {
            tx_interrupt_pending_ = false;
            update_interrupt_line();
            return UART_IIR_THRI;
        }
        return UART_IIR_NO_INT;
    }
    if (offset == UART_REG_LCR) {
        return lcr_;
    }
    if (offset == UART_REG_MCR) {
        return mcr_;
    }
    if (offset == UART_REG_LSR) {
        uint64_t lsr = input_.empty() ? 0U : UART_LSR_DR;
        if (tx_holding_empty_) {
            lsr |= UART_LSR_THRE;
        }
        if (tx_shift_empty_) {
            lsr |= UART_LSR_TEMT;
        }
        return lsr;
    }
    if (offset == UART_REG_MSR) {
        return UART_MSR_CTS | UART_MSR_DSR | UART_MSR_DCD;
    }

    invalid_access(addr, size);
}

void Uart16550::store(uint64_t addr, uint64_t value, int size) {
    if (size != 1) {
        invalid_access(addr, size);
    }

    const uint32_t offset = static_cast<uint32_t>(addr - UART_BASE);
    const uint8_t value8 = static_cast<uint8_t>(value);
    if (offset == UART_REG_THR) {
        if (divisor_latch_enabled()) {
            dll_ = value8;
            return;
        }
        const char ch = static_cast<char>(value & 0xFF);
        output_.push_back(ch);
        if (mirror_stdout_) {
            std::putchar(static_cast<int>(static_cast<unsigned char>(ch)));
            std::fflush(stdout);
        }
        tx_holding_empty_ = false;
        tx_shift_empty_ = false;
        tx_drain_pending_ = true;
        tx_interrupt_pending_ = false;
        update_interrupt_line();
        return;
    }
    if (offset == UART_REG_IER) {
        if (divisor_latch_enabled()) {
            dlm_ = value8;
            return;
        }
        ier_ = value8 & static_cast<uint8_t>(UART_IER_RDI | UART_IER_THRI);
        tx_interrupt_pending_ = tx_interrupt_enabled() && tx_ready();
        update_interrupt_line();
        return;
    }
    if (offset == UART_REG_FCR) {
        fcr_ = value8;
        if ((fcr_ & 0x2U) != 0) {
            input_.clear();
        }
        update_interrupt_line();
        return;
    }
    if (offset == UART_REG_LCR) {
        lcr_ = value8;
        return;
    }
    if (offset == UART_REG_MCR) {
        mcr_ = value8;
        return;
    }

    invalid_access(addr, size);
}

bool Uart16550::divisor_latch_enabled() const {
    return (lcr_ & UART_LCR_DLAB) != 0;
}

bool Uart16550::tx_interrupt_enabled() const {
    return (ier_ & UART_IER_THRI) != 0;
}

bool Uart16550::rx_interrupt_enabled() const {
    return (ier_ & UART_IER_RDI) != 0;
}

bool Uart16550::rx_interrupt_pending() const {
    return rx_interrupt_enabled() && !input_.empty();
}

bool Uart16550::tx_ready() const {
    return tx_holding_empty_ && tx_shift_empty_;
}

PlatformEvents Uart16550::tick() {
    if (tx_drain_pending_) {
        tx_holding_empty_ = true;
        tx_shift_empty_ = true;
        tx_drain_pending_ = false;
        if (tx_interrupt_enabled()) {
            tx_interrupt_pending_ = true;
        }
    }
    update_interrupt_line();
    return {};
}

void Uart16550::update_interrupt_line() {
    plic_.set_source_level(Plic::UART_SOURCE_ID, tx_interrupt_pending_ || rx_interrupt_pending());
}

uint8_t Uart16550::ier() const {
    return ier_;
}

bool Uart16550::thre_interrupt_asserted() const {
    return tx_interrupt_pending_;
}

size_t Uart16550::output_size() const {
    return output_.size();
}

const std::string& Uart16550::output() const {
    return output_;
}

uint8_t Uart16550::mcr() const {
    return mcr_;
}

void Uart16550::inject_input(std::string_view text) {
    for (const char ch : text) {
        input_.push_back(static_cast<uint8_t>(ch));
    }
    update_interrupt_line();
}

void Uart16550::set_mirror_stdout(bool enabled) {
    mirror_stdout_ = enabled;
}
