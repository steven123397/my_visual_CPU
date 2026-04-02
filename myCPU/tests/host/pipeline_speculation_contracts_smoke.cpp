#include <cstdio>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/uart16550.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/exec/pipeline_commit_boundary.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint64_t kDataAddr = kEntry + 0x100;
constexpr uint32_t kAddiA7Exit = 0x05d00893U;      // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;           // ecall
constexpr uint32_t kMret = 0x30200073U;
constexpr uint32_t kSbX5ToX10Plus1 = 0x005500a3U;  // sb x5, 1(x10)
constexpr uint32_t kCsrwMepcX5 = 0x34129073U;      // csrw mepc, x5

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

bool run_until_halt(PipelineBackend& backend, CPU& cpu, int max_steps) {
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
        backend.step();
    }
    return cpu.core().halted();
}

}  // namespace

int main() {
    CommitBoundaryInput commit_input{};
    (void)commit_input;

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, 0, cpu.core());
        cpu.csr().write(CSR_MSTATUS,
                        static_cast<uint64_t>(PrivilegeMode::Machine) << MSTATUS_MPP_SHIFT,
                        cpu.core());
        cpu.core().write_gpr(5, 0x7a);
        cpu.core().write_gpr(10, kDataAddr);

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kSbX5ToX10Plus1);
        write32(ram, kTrapVector + 0, kAddiA7Exit);
        write32(ram, kTrapVector + 4, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "mret + wrong-path RAM store contract should halt")) {
            return 1;
        }
        if (!expect(ram.load(kDataAddr + 1, 1) == 0, "squashed younger RAM store must not update memory")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 1, "trap-return path should still report precise fetch fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0, "trap-return fetch fault should preserve the faulting pc")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        Plic plic;
        Uart16550 uart(plic);
        uart.set_mirror_stdout(false);
        bus.attach(plic);
        bus.attach(uart);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, 0, cpu.core());
        cpu.csr().write(CSR_MSTATUS,
                        static_cast<uint64_t>(PrivilegeMode::Machine) << MSTATUS_MPP_SHIFT,
                        cpu.core());
        cpu.core().write_gpr(5, UART_IER_THRI);
        cpu.core().write_gpr(10, UART_BASE);

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kSbX5ToX10Plus1);
        write32(ram, kTrapVector + 0, kAddiA7Exit);
        write32(ram, kTrapVector + 4, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "mret + wrong-path MMIO store contract should halt")) {
            return 1;
        }
        if (!expect(uart.ier() == 0, "squashed younger MMIO store must not update device state")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MEPC, 0x1234, cpu.core());
        cpu.core().write_gpr(5, 0x5678);

        write32(ram, kEntry + 0, kCsrwMepcX5);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 3; ++i) {
            backend.step();
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0x1234, "CSR write must stay invisible before commit boundary")) {
            return 1;
        }

        if (!expect(run_until_halt(backend, cpu, 16), "CSR commit-boundary contract should halt")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0x5678, "CSR write should become visible after commit boundary")) {
            return 1;
        }
    }

    return 0;
}
