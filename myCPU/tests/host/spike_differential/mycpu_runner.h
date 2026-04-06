#pragma once

#include <cstdint>
#include <memory>

#include "final_state.h"

#include "../../../include/platform_mmio.h"
#include "../../../src/devices/plic.h"
#include "../../../src/devices/uart16550.h"
#include "../../../src/exec/functional_backend.h"

namespace spike_differential {
namespace {

constexpr uint64_t kInterruptCauseBit = 1ULL << 63;

uint64_t trap_vector_base(uint64_t tvec, uint64_t cause) {
    if ((tvec & 3ULL) == 1ULL && (cause & kInterruptCauseBit) != 0) {
        return (tvec & ~3ULL) + 4ULL * (cause & ~kInterruptCauseBit);
    }
    return tvec & ~3ULL;
}

uint64_t tracked_csr_value(const FinalState& state, uint32_t csr) {
    const size_t index = tracked_csr_index(csr);
    if (index >= state.csrs.size()) {
        return 0;
    }
    return state.csrs[index];
}

FinalState capture_final_state(const CPU& cpu, Ram& ram, const std::vector<MemoryWatch>& watches) {
    FinalState state;
    state.pc = cpu.core().pc();
    state.instret = cpu.core().instret();
    state.halted = cpu.core().halted();
    state.privilege = cpu.core().privilege_mode();
    for (size_t i = 0; i < state.gprs.size(); ++i) {
        state.gprs[i] = cpu.core().read_gpr(static_cast<uint32_t>(i));
    }
    for (size_t i = 0; i < kTrackedCsrs.size(); ++i) {
        state.csrs[i] = cpu.csr().read(kTrackedCsrs[i], cpu.core());
    }
    state.watched_memory.reserve(watches.size());
    for (const MemoryWatch& watch : watches) {
        state.watched_memory.push_back(ram.load(watch.addr, watch.size));
    }
    return state;
}

bool capture_machine_trap_summary(const FinalState& previous,
                                  const FinalState& current,
                                  TrapSummary& summary) {
    const uint64_t current_cause = tracked_csr_value(current, CSR_MCAUSE);
    const bool cause_changed = current_cause != tracked_csr_value(previous, CSR_MCAUSE);
    const bool epc_changed =
        tracked_csr_value(current, CSR_MEPC) != tracked_csr_value(previous, CSR_MEPC);
    const bool tval_changed =
        tracked_csr_value(current, CSR_MTVAL) != tracked_csr_value(previous, CSR_MTVAL);
    if (!cause_changed && !epc_changed && !tval_changed) {
        return false;
    }
    if (current.privilege != PrivilegeMode::Machine) {
        return false;
    }
    if (current.pc != trap_vector_base(tracked_csr_value(current, CSR_MTVEC), current_cause)) {
        return false;
    }
    summary.trapped = true;
    summary.cause = current_cause;
    summary.tval = tracked_csr_value(current, CSR_MTVAL);
    summary.epc = tracked_csr_value(current, CSR_MEPC);
    summary.privilege_at_trap = PrivilegeMode::Machine;
    return true;
}

bool capture_supervisor_trap_summary(const FinalState& previous,
                                     const FinalState& current,
                                     TrapSummary& summary) {
    const uint64_t current_cause = tracked_csr_value(current, CSR_SCAUSE);
    const bool cause_changed = current_cause != tracked_csr_value(previous, CSR_SCAUSE);
    const bool epc_changed =
        tracked_csr_value(current, CSR_SEPC) != tracked_csr_value(previous, CSR_SEPC);
    const bool tval_changed =
        tracked_csr_value(current, CSR_STVAL) != tracked_csr_value(previous, CSR_STVAL);
    if (!cause_changed && !epc_changed && !tval_changed) {
        return false;
    }
    if (current.privilege != PrivilegeMode::Supervisor) {
        return false;
    }
    if (current.pc != trap_vector_base(tracked_csr_value(current, CSR_STVEC), current_cause)) {
        return false;
    }
    summary.trapped = true;
    summary.cause = current_cause;
    summary.tval = tracked_csr_value(current, CSR_STVAL);
    summary.epc = tracked_csr_value(current, CSR_SEPC);
    summary.privilege_at_trap = PrivilegeMode::Supervisor;
    return true;
}

bool capture_trap_summary(const FinalState& previous,
                          const FinalState& current,
                          TrapSummary& summary) {
    return capture_machine_trap_summary(previous, current, summary) ||
           capture_supervisor_trap_summary(previous, current, summary);
}

}  // namespace

inline FinalState run_mycpu_final_state(const Scenario& scenario) {
    Ram ram;
    Bus bus(ram);
    std::unique_ptr<Plic> plic;
    std::unique_ptr<Uart16550> uart;
    if (scenario.fixture == Scenario::PlatformFixture::UartPlic) {
        plic = std::make_unique<Plic>();
        uart = std::make_unique<Uart16550>(*plic);
        uart->set_mirror_stdout(false);
        bus.attach(*plic);
        bus.attach(*uart);
    }

    CPU cpu;
    cpu_init(cpu, kEntry);
    if (!scenario.trap_program.empty()) {
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
    }
    for (size_t i = 0; i < scenario.program.size(); ++i) {
        write32(ram, kEntry + static_cast<uint64_t>(i * 4), scenario.program[i]);
    }
    for (size_t i = 0; i < scenario.trap_program.size(); ++i) {
        write32(ram, kTrapVector + static_cast<uint64_t>(i * 4), scenario.trap_program[i]);
    }
    apply_initial_state(cpu, ram, scenario);
    if (scenario.configure) {
        scenario.configure(cpu, ram, bus);
    }

    FunctionalBackend backend(cpu, bus);
    FinalState previous = capture_final_state(cpu, ram, scenario.watches);
    TrapSummary trap_summary;
    bool trap_captured = false;

    for (int step = 0; step < scenario.max_steps; ++step) {
        backend.step();
        FinalState current = capture_final_state(cpu, ram, scenario.watches);
        if (!trap_captured) {
            trap_captured = capture_trap_summary(previous, current, trap_summary);
        }
        if (trap_captured) {
            current.trap_summary = trap_summary;
        }
        if (current.halted) {
            current.timed_out = false;
            current.exit_reason = "controlled_exit";
            return current;
        }
        previous = current;
    }

    previous.timed_out = true;
    previous.exit_reason = "step_budget_exhausted";
    if (trap_captured) {
        previous.trap_summary = trap_summary;
    }
    return previous;
}

}  // namespace spike_differential
