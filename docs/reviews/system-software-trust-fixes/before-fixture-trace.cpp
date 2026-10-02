// Historical diagnostic: build against code 999832c, before the U3 fixture fix.
#define main evidence_original_main
#include "../../../myCPU/tests/host/spike_differential_smoke.cpp"
#undef main

int main() {
    for (const char* name : {"delegated_user_ecall_to_supervisor", "sv39_instruction_page_fault",
                            "sv39_load_page_fault", "sv39_store_page_fault", "sv39_reserved_non_leaf_fault"}) {
        const auto scenario = find_scenario(name);
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        for (size_t i = 0; i < scenario.program.size(); ++i)
            write32(ram, kEntry + i * 4, scenario.program[i]);
        for (size_t i = 0; i < scenario.trap_program.size(); ++i)
            write32(ram, kTrapVector + i * 4, scenario.trap_program[i]);
        apply_initial_state(cpu, ram, scenario);
        FunctionalBackend backend(cpu, bus);
        for (int step = 0; step < 20; ++step) {
            backend.step();
            std::printf("%s step=%d pc=0x%llx priv=%u scause=%llu sepc=0x%llx stval=0x%llx mcause=%llu mepc=0x%llx\n",
                        name, step, (unsigned long long)cpu.core().pc(), (unsigned)cpu.core().privilege_mode(),
                        (unsigned long long)cpu.csr().read(CSR_SCAUSE, cpu.core()),
                        (unsigned long long)cpu.csr().read(CSR_SEPC, cpu.core()),
                        (unsigned long long)cpu.csr().read(CSR_STVAL, cpu.core()),
                        (unsigned long long)cpu.csr().read(CSR_MCAUSE, cpu.core()),
                        (unsigned long long)cpu.csr().read(CSR_MEPC, cpu.core()));
        }
    }
}
