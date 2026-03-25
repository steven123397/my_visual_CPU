#include <cstdio>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/devices/clint.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../include/platform_mmio.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint64_t kDataAddr = kEntry + 0x100;
constexpr uint32_t kNop = 0x00000013U;                    // addi x0, x0, 0
constexpr uint32_t kAddiX1 = 0x00100093U;                // addi x1, x0, 1
constexpr uint32_t kAddiX2FromX1Plus2 = 0x00208113U;     // addi x2, x1, 2
constexpr uint32_t kAddX3FromX2X1 = 0x001101b3U;         // add x3, x2, x1
constexpr uint32_t kAddiX1WrongPath = 0x06300093U;       // addi x1, x0, 99
constexpr uint32_t kAddiX2WrongPath = 0x06300113U;       // addi x2, x0, 99
constexpr uint32_t kAddiX2FromX0Plus7 = 0x00700113U;     // addi x2, x0, 7
constexpr uint32_t kInvalidInsn = 0xffffffffU;
constexpr uint32_t kLwX1FromX10 = 0x00052083U;           // lw x1, 0(x10)
constexpr uint32_t kAddiX2FromX1Plus5 = 0x00508113U;     // addi x2, x1, 5
constexpr uint32_t kMret = 0x30200073U;

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

}  // namespace

int main() {
    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2FromX1Plus2);
        write32(ram, kEntry + 8, kAddX3FromX2X1);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 7; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(3) == 4, "pipeline should forward ALU results across dependent instructions")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kDataAddr);
        ram.store(kDataAddr, 37, 4);

        write32(ram, kEntry + 0, kLwX1FromX10);
        write32(ram, kEntry + 4, kAddiX2FromX1Plus5);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 7; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 42, "pipeline should resolve load-use hazards with a single interlock")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        Clint clint;
        bus.attach(clint);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MIE, MIE_MTIE, cpu.core());
        cpu.csr().write(CSR_MSTATUS, MSTATUS_MIE, cpu.core());
        clint.store(CLINT_BASE + CLINT_REG_MTIMECMP, 3, 8);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kEntry, "timer interrupt should not redirect before the first commit boundary")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().pc() == kTrapVector, "timer interrupt should redirect at the commit boundary")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 1, "older committed work should be preserved before taking the timer interrupt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "timer interrupt should flush younger in-flight work")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == ((1ULL << 63) | 7ULL), "timer interrupt should report machine timer cause")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry + 4, "timer interrupt should capture the post-commit architectural pc")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kInvalidInsn);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 5; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "illegal instruction should trap only when it reaches commit")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "illegal instruction should flush younger writes")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2, "illegal instruction should report illegal-instruction cause")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, 0, cpu.core());
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 6; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "fetch fault after mret should eventually trap to mtvec")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 1, "fetch fault after mret should report instruction access fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0, "fetch fault after mret should preserve the faulting pc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0, "fetch fault after mret should preserve mtval")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "fetch fault after mret should flush wrong-path work after the return")) {
            return 1;
        }
    }

    return 0;
}
