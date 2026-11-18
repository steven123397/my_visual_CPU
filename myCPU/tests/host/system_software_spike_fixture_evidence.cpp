#define main spike_fixture_original_main
#include "spike_differential_smoke.cpp"
#undef main

namespace {

void print_state(const char* label, int step, uint32_t raw, const FinalState& state) {
    std::printf(
        "{\"case\":\"%s\",\"step\":%d,\"raw\":%u,\"pc\":%llu,"
        "\"privilege\":%u,\"a7\":%llu,\"instret\":%llu,\"halted\":%s,"
        "\"timed_out\":%s,\"scause\":%llu,\"sepc\":%llu,\"mcause\":%llu,"
        "\"mepc\":%llu,\"first_trap_cause\":%llu,\"exit_reason\":\"%s\"}\n",
        label, step, raw, static_cast<unsigned long long>(state.pc),
        static_cast<unsigned>(state.privilege),
        static_cast<unsigned long long>(state.gprs[17]),
        static_cast<unsigned long long>(state.instret), state.halted ? "true" : "false",
        state.timed_out ? "true" : "false",
        static_cast<unsigned long long>(tracked_csr_value(state, CSR_SCAUSE)),
        static_cast<unsigned long long>(tracked_csr_value(state, CSR_SEPC)),
        static_cast<unsigned long long>(tracked_csr_value(state, CSR_MCAUSE)),
        static_cast<unsigned long long>(tracked_csr_value(state, CSR_MEPC)),
        static_cast<unsigned long long>(state.first_trap_summary.cause),
        state.exit_reason.c_str());
}

void trace_scenario(const char* label, const Scenario& scenario) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
    for (size_t i = 0; i < scenario.program.size(); ++i) {
        write32(ram, kEntry + i * 4, scenario.program[i]);
    }
    for (size_t i = 0; i < scenario.trap_program.size(); ++i) {
        write32(ram, kTrapVector + i * 4, scenario.trap_program[i]);
    }
    apply_initial_state(cpu, ram, scenario);
    if (scenario.configure) {
        scenario.configure(cpu, ram, bus);
    }
    FunctionalBackend backend(cpu, bus);
    for (int step = 0; step < 20 && !cpu.core().halted(); ++step) {
        const uint32_t raw = static_cast<uint32_t>(ram.load(cpu.core().pc(), 4));
        backend.step();
        print_state(label, step, raw, capture_final_state(cpu, ram, scenario.watches));
    }
}

}  // namespace

int main() {
    const Scenario original = find_scenario("delegated_user_ecall_to_supervisor");
    trace_scenario("original_trace", original);
    const FinalState baseline = run_mycpu_final_state(original);
    print_state("original_final", original.max_steps, 0, baseline);

    // A distinct M-mode handler isolates the exit convention from delegated U ecall handling.
    Scenario control = original;
    control.configure = [](CPU& cpu, Ram& ram, Bus&) {
        constexpr uint64_t exit_vector = kEntry + 0xc0;
        cpu.csr().write(CSR_MTVEC, exit_vector, cpu.core());
        write32(ram, exit_vector, kAddiA7Exit);
        write32(ram, exit_vector + 4, kEcall);
    };
    trace_scenario("machine_exit_control_trace", control);
    const FinalState controlled = run_mycpu_final_state(control);
    print_state("machine_exit_control_final", -1, 0, controlled);

    const bool baseline_observed = !baseline.halted && baseline.timed_out &&
        baseline.first_trap_summary.trapped && baseline.first_trap_summary.cause == 8 &&
        tracked_csr_value(baseline, CSR_MCAUSE) == 9;
    const bool control_observed = controlled.halted && !controlled.timed_out &&
        controlled.first_trap_summary.trapped && controlled.first_trap_summary.cause == 8 &&
        controlled.privilege == PrivilegeMode::Machine;
    return baseline_observed && control_observed ? 0 : 1;
}
