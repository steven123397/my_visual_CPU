#include <cstdio>

#include "../../src/cpu.h"
#include "../../src/devices/clint.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = 0x80000000ULL;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint32_t kAddiX1 = 0x00100093U;   // addi x1, x0, 1
constexpr uint32_t kNop = 0x00000013U;      // addi x0, x0, 0
constexpr uint32_t kAddiX2 = 0x00200113U;   // addi x2, x0, 2
constexpr uint32_t kInvalidInsn = 0xffffffffU;
constexpr uint32_t kAddX3 = 0x002081b3U;    // add x3, x1, x2
constexpr uint32_t kAddiX4 = 0x00518213U;   // addi x4, x3, 5
constexpr uint32_t kAddiX5FromX0Plus18 = 0x01200293U;  // addi x5, x0, 18
constexpr uint32_t kAddiX5FromX0Plus40 = 0x02800293U;  // addi x5, x0, 40
constexpr uint32_t kAddiX5FromX0Plus60 = 0x03c00293U;  // addi x5, x0, 60
constexpr uint32_t kAddiX5Minus1 = 0xfff00293U;        // addi x5, x0, -1
constexpr uint32_t kAddiA7Exit = 0x05d00893U;  // addi a7, x0, 93
constexpr uint32_t kAddiX1WrongPath = 0x06300093U;  // addi x1, x0, 99
constexpr uint32_t kAddiX2FromX1 = 0x00208113U;   // addi x2, x1, 2
constexpr uint32_t kAddiX2FromX0Plus7 = 0x00700113U;  // addi x2, x0, 7
constexpr uint32_t kAddX3FromX2X1 = 0x001101b3U;  // add x3, x2, x1
constexpr uint32_t kJalX5Skip = 0x008002efU;     // jal x5, 8
constexpr uint32_t kBeqX0Taken = 0x00000463U;    // beq x0, x0, 8
constexpr uint32_t kBeqX1X0Skip = 0x00008463U;   // beq x1, x0, 8
constexpr uint32_t kAuipcX6 = 0x00000317U;       // auipc x6, 0
constexpr uint32_t kAddiX6Target16 = 0x01030313U;  // addi x6, x6, 16
constexpr uint32_t kAddiX6Target20 = 0x01430313U;  // addi x6, x6, 20
constexpr uint32_t kAddiX6Target32 = 0x02030313U;  // addi x6, x6, 32
constexpr uint32_t kAddiX7FromX0Plus18 = 0x01200393U;  // addi x7, x0, 18
constexpr uint32_t kBneX6X7Skip = 0x00731463U;   // bne x6, x7, 8
constexpr uint32_t kJalrX7X6 = 0x000303e7U;      // jalr x7, x6, 0
constexpr uint32_t kJalrX0X0 = 0x00000067U;      // jalr x0, x0, 0
constexpr uint32_t kAddiX11FromX0Plus42 = 0x02a00593U;  // addi x11, x0, 42
constexpr uint32_t kLwX1FromX10 = 0x00052083U;   // lw x1, 0(x10)
constexpr uint32_t kLwX1FromX0Plus0 = 0x00002083U;  // lw x1, 0(x0)
constexpr uint32_t kLdX1FromX0Plus0 = 0x00003083U;  // ld x1, 0(x0)
constexpr uint32_t kLdX1FromX10 = 0x00053083U;      // ld x1, 0(x10)
constexpr uint32_t kAddiX2FromX1Plus5 = 0x00508113U;  // addi x2, x1, 5
constexpr uint32_t kSwX11ToX10 = 0x00b52023U;    // sw x11, 0(x10)
constexpr uint32_t kSwX11ToX0 = 0x00b02023U;     // sw x11, 0(x0)
constexpr uint32_t kSdX11ToX0 = 0x00b03023U;     // sd x11, 0(x0)
constexpr uint32_t kLwX12FromX10 = 0x00052603U;  // lw x12, 0(x10)
constexpr uint32_t kAddiX2WrongPath = 0x06300113U;  // addi x2, x0, 99
constexpr uint32_t kAddiX3TakenPath = 0x00700193U;  // addi x3, x0, 7
constexpr uint32_t kCsrwMscratchX1 = 0x34009073U;   // csrw mscratch, x1
constexpr uint32_t kCsrwMscratchX5 = 0x34029073U;   // csrw mscratch, x5
constexpr uint32_t kCsrwMcycleX5 = 0xB0029073U;     // csrw mcycle, x5
constexpr uint32_t kCsrwMidelegX5 = 0x30329073U;    // csrw mideleg, x5
constexpr uint32_t kCsrrX6Mcycle = 0xB0002373U;     // csrr x6, mcycle
constexpr uint32_t kCsrrX6Mideleg = 0x30302373U;    // csrr x6, mideleg
constexpr uint32_t kCsrrX6Cycle = 0xC0002373U;      // rdcycle x6
constexpr uint32_t kCsrwMinstretX5 = 0xB0229073U;   // csrw minstret, x5
constexpr uint32_t kCsrrX6Minstret = 0xB0202373U;   // csrr x6, minstret
constexpr uint32_t kCsrwSatpX5 = 0x18029073U;       // csrw satp, x5
constexpr uint32_t kCsrrX6Satp = 0x18002373U;       // csrr x6, satp
constexpr uint32_t kCsrrS1Instret = 0xC02024F3U;    // rdinstret s1
constexpr uint32_t kCsrrS2Instret = 0xC0202973U;    // rdinstret s2
constexpr uint32_t kCsrrX2Time = 0xC0102173U;       // csrr x2, time
constexpr uint32_t kCsrwSieX6 = 0x10431073U;        // csrw sie, x6
constexpr uint32_t kCsrrX7Mie = 0x304023f3U;        // csrr x7, mie
constexpr uint32_t kCsrwMtvecX6 = 0x30531073U;      // csrw mtvec, x6
constexpr uint32_t kCsrwMstatusX7 = 0x30039073U;    // csrw mstatus, x7
constexpr uint32_t kCsrwMepcX5 = 0x34129073U;       // csrw mepc, x5
constexpr uint32_t kCsrwMepcX6 = 0x34131073U;       // csrw mepc, x6
constexpr uint32_t kCsrrX6Mscratch = 0x34002373U;   // csrr x6, mscratch
constexpr uint32_t kCsrrwX5MscratchX1 = 0x340092f3U;  // csrrw x5, mscratch, x1
constexpr uint32_t kCsrrwX6MscratchX1 = 0x34009373U;  // csrrw x6, mscratch, x1
constexpr uint32_t kEcall = 0x00000073U;        // ecall
constexpr uint32_t kMret = 0x30200073U;         // mret
constexpr uint64_t kDataAddr = kEntry + 0x100;

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
        Clint clint;
        bus.attach(clint);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MIE, MIE_MTIE, cpu.core());
        cpu.csr().write(CSR_MSTATUS, MSTATUS_MIE, cpu.core());
        clint.store(CLINT_BASE + CLINT_REG_MTIMECMP, 3, 8);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().pc() == kEntry,
                "timer interrupts should stay pending until an older instruction reaches the commit boundary")) {
            return 1;
        }
        if (!expect(
                cpu.core().read_gpr(1) == 0,
                "the first addi should still be waiting for WB before the interrupt is serviced")) {
            return 1;
        }

        backend.step();
        if (!expect(
                cpu.core().read_gpr(1) == 1,
                "commit-boundary interrupts should still let the older addi retire first")) {
            return 1;
        }
        if (!expect(
                cpu.core().read_gpr(2) == 0,
                "an interrupt taken at commit should flush younger in-flight integer writes")) {
            return 1;
        }
        if (!expect(
                cpu.core().pc() == kTrapVector,
                "timer interrupts should redirect to mtvec only at the pipeline commit boundary")) {
            return 1;
        }
        if (!expect(
                cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry + 4,
                "timer interrupts should capture the post-commit architectural pc in mepc")) {
            return 1;
        }
        if (!expect(
                cpu.csr().read(CSR_MCAUSE, cpu.core()) == (1ULL << 63 | 7ULL),
                "timer interrupts should report the machine-timer interrupt cause at commit")) {
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

        for (int i = 0; i < 6; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "illegal instructions should trap before younger writes commit")) {
            return 1;
        }
        if (!expect(cpu.core().pc() == kTrapVector, "illegal instructions should redirect to mtvec through the pipeline trap path")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry, "illegal instructions should preserve the faulting pc in mepc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2, "illegal instructions should report illegal-instruction cause")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == kInvalidInsn, "illegal instructions should preserve the raw instruction in mtval")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kJalrX0X0);
        write32(ram, kEntry + 8, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 6; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().read_gpr(1) == 1,
                "older integer work should retire before a younger fetch fault is committed")) {
            return 1;
        }
        if (!expect(
                cpu.core().read_gpr(2) == 0,
                "a fetch fault should not let wrong-path instructions commit")) {
            return 1;
        }
        if (!expect(cpu.core().pc() == kTrapVector, "fetch faults should redirect to mtvec once they commit")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0, "fetch faults should report the faulting pc in mepc")) {
            return 1;
        }
        if (!expect(
                cpu.csr().read(CSR_MCAUSE, cpu.core()) == 1,
                "fetch faults should report instruction access fault cause at commit")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0, "fetch faults should preserve mtval from the faulting pc")) {
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
        if (!expect(cpu.core().pc() == kTrapVector, "mret to an unmapped fetch target should still commit an instruction fault")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "mret-following fetch faults should flush wrong-path work after the return")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0, "mret-following fetch faults should preserve the faulting pc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 1, "mret-following fetch faults should report instruction access fault")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(6, kEntry + 12);
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kCsrwMepcX6);
        write32(ram, kEntry + 4, kMret);
        write32(ram, kEntry + 8, kAddiX1WrongPath);
        write32(ram, kEntry + 12, kAddiX2FromX0Plus7);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 10; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 7, "mret should see an older just-written mepc value at commit")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "mret should still flush wrong-path work behind the return")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(5, 0);
        cpu.core().write_gpr(6, kTrapVector);
        cpu.core().write_gpr(7, 0x1800);

        write32(ram, kEntry + 0, kCsrwMscratchX5);
        write32(ram, kEntry + 4, kCsrwMtvecX6);
        write32(ram, kEntry + 8, kCsrwMstatusX7);
        write32(ram, kEntry + 12, kCsrwMepcX5);
        write32(ram, kEntry + 16, kMret);
        write32(ram, kEntry + 20, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kAddiX2FromX0Plus7);
        write32(ram, kTrapVector + 4, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 14; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().read_gpr(2) == 7,
                "an instruction fault after sequential CSR setup should land at the just-written mtvec")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "the instruction fault should still flush wrong-path work after mret")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kAuipcX6);
        write32(ram, kEntry + 4, kAddiX6Target20);
        write32(ram, kEntry + 8, kCsrwMepcX6);
        write32(ram, kEntry + 12, kMret);
        write32(ram, kEntry + 16, kAddiX1WrongPath);
        write32(ram, kEntry + 20, kAddiX2FromX0Plus7);
        write32(ram, kEntry + 24, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 12; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 7, "mret should return to a just-computed mepc target through csr writeback")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "mret should flush wrong-path work behind a computed mepc target")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, kEntry + 8, cpu.core());
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kLwX1FromX0Plus0);
        write32(ram, kEntry + 12, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "mret-following load faults should commit through the trap path")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "faulting loads should not commit their destination register")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "load faults should flush wrong-path instructions behind the fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry + 8, "load faults should preserve the faulting pc in mepc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 5, "load faults should report load access fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0, "load faults should preserve mtval from the faulting address")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, kEntry + 8, cpu.core());
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kLdX1FromX0Plus0);
        write32(ram, kEntry + 12, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "mret-following 64-bit load faults should commit through the trap path")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry + 8, "64-bit load faults should preserve the faulting pc in mepc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 5, "64-bit load faults should report load access fault")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, kEntry + 8, cpu.core());
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kAddiX11FromX0Plus42);
        write32(ram, kEntry + 12, kSwX11ToX0);
        write32(ram, kEntry + 16, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 10; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "mret-following store faults should commit through the trap path")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "store faults should flush wrong-path instructions behind the fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry + 12, "store faults should preserve the faulting pc in mepc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 7, "store faults should report store access fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0, "store faults should preserve mtval from the faulting address")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, kEntry + 8, cpu.core());
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kAddiX11FromX0Plus42);
        write32(ram, kEntry + 12, kSdX11ToX0);
        write32(ram, kEntry + 16, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 10; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "mret-following 64-bit store faults should commit through the trap path")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry + 12, "64-bit store faults should preserve the faulting pc in mepc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 7, "64-bit store faults should report store access fault")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX5FromX0Plus40);
        write32(ram, kEntry + 4, kCsrwMcycleX5);
        write32(ram, kEntry + 8, kCsrrX6Mcycle);
        write32(ram, kEntry + 12, kAddiX5FromX0Plus60);
        write32(ram, kEntry + 16, kCsrwMinstretX5);
        write32(ram, kEntry + 20, kCsrrX6Minstret);
        write32(ram, kEntry + 24, kNop);
        write32(ram, kEntry + 28, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(6) == 41, "counter reads should observe an older mcycle write after its retirement increment")) {
            return 1;
        }

        for (int i = 0; i < 6; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().read_gpr(6) == 61,
                "counter reads should observe an older minstret write through the shared instret counter view")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX5FromX0Plus40);
        write32(ram, kEntry + 4, kCsrwMcycleX5);
        write32(ram, kEntry + 8, kCsrrX6Cycle);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(6) == 41, "rdcycle should observe an older mcycle write through the shared cycle counter view")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX5FromX0Plus60);
        write32(ram, kEntry + 4, kCsrwMinstretX5);
        write32(ram, kEntry + 8, kCsrrS1Instret);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().read_gpr(9) == 61,
                "rdinstret should observe an older minstret write through the shared instret counter view")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kCsrrS1Instret);
        write32(ram, kEntry + 4, kNop);
        write32(ram, kEntry + 8, kCsrrS2Instret);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().read_gpr(18) - cpu.core().read_gpr(9) == 2,
                "instret reads should include older in-flight retirements before the younger CSR read commits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX5Minus1);
        write32(ram, kEntry + 4, kCsrwMidelegX5);
        write32(ram, kEntry + 8, kCsrrX6Mideleg);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(6) == 0x222, "CSR reads should see an older mideleg write after WARL masking")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(5, 0x222);
        cpu.core().write_gpr(6, 0x20);

        write32(ram, kEntry + 0, kCsrwMidelegX5);
        write32(ram, kEntry + 4, kCsrwSieX6);
        write32(ram, kEntry + 8, kCsrrX7Mie);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(7) == 0x20, "CSR alias reads should see an older sie write through mie visibility")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(5, 0x9000000000012345ULL);

        write32(ram, kEntry + 0, kCsrwSatpX5);
        write32(ram, kEntry + 4, kCsrrX6Satp);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(6) == 0, "CSR reads should see satp WARL masking from an older in-flight write")) {
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
        cpu.csr().bind_clint(&clint);
        cpu.core().write_gpr(10, CLINT_BASE + CLINT_REG_MTIME);

        write32(ram, kEntry + 0, kLdX1FromX10);
        write32(ram, kEntry + 4, kCsrrX2Time);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 9; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().read_gpr(2) - cpu.core().read_gpr(1) == 1,
                "time CSR reads should include the older in-flight CLINT tick ahead of a younger CSR read")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2FromX1);
        write32(ram, kEntry + 8, kAddX3FromX2X1);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 6; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 3, "forwarding should let dependent addi commit without extra decode stalls")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().read_gpr(3) == 4, "forwarding should carry chained ALU dependencies through EX")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kDataAddr);

        write32(ram, kEntry + 0, kAddiX11FromX0Plus42);
        write32(ram, kEntry + 4, kSwX11ToX10);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kDataAddr, 0);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 5; ++i) {
            backend.step();
        }
        if (!expect(ram.load(kDataAddr, 4) == 42, "store should forward rs2 data from an older ALU producer")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kJalX5Skip);
        write32(ram, kEntry + 4, kAddiX2WrongPath);
        write32(ram, kEntry + 8, kAddiX3TakenPath);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);
        write32(ram, kEntry + 20, kNop);
        write32(ram, kEntry + 24, kNop);

        PipelineBackend backend(cpu, bus);

        backend.step();
        backend.step();
        if (!expect(cpu.core().read_gpr(5) == 0, "jal should not commit its link register before reaching WB")) {
            return 1;
        }
        if (!expect(
                cpu.core().read_gpr(2) == 0, "wrong-path instruction must stay uncommitted before redirect resolves")) {
            return 1;
        }

        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        if (!expect(
                cpu.core().read_gpr(5) == kEntry + 4,
                "jal should eventually commit the return address through the pipeline")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "jal redirect should flush the skipped wrong-path addi")) {
            return 1;
        }

        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(3) == 7, "jal redirect should fetch and commit the taken-path instruction")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kBeqX0Taken);
        write32(ram, kEntry + 4, kAddiX2WrongPath);
        write32(ram, kEntry + 8, kAddiX3TakenPath);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);
        write32(ram, kEntry + 20, kNop);
        write32(ram, kEntry + 24, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "taken branch should flush the wrong-path addi")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(3) == 7, "taken branch should commit the taken-path addi")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kBeqX1X0Skip);
        write32(ram, kEntry + 8, kAddiX2FromX0Plus7);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 7, "branch compare should forward rs1 so a not-taken branch does not misredirect")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAuipcX6);
        write32(ram, kEntry + 4, kNop);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kAddiX6Target32);
        write32(ram, kEntry + 16, kNop);
        write32(ram, kEntry + 20, kNop);
        write32(ram, kEntry + 24, kJalrX7X6);
        write32(ram, kEntry + 28, kAddiX2WrongPath);
        write32(ram, kEntry + 32, kAddiX3TakenPath);
        write32(ram, kEntry + 36, kNop);
        write32(ram, kEntry + 40, kNop);
        write32(ram, kEntry + 44, kNop);
        write32(ram, kEntry + 48, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 14; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(7) == kEntry + 28, "jalr should commit the masked return address through WB")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "jalr redirect should flush the wrong-path addi")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(3) == 7, "jalr redirect should commit the taken-path addi")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAuipcX6);
        write32(ram, kEntry + 4, kAddiX6Target16);
        write32(ram, kEntry + 8, kJalrX7X6);
        write32(ram, kEntry + 12, kAddiX2WrongPath);
        write32(ram, kEntry + 16, kAddiX3TakenPath);
        write32(ram, kEntry + 20, kNop);
        write32(ram, kEntry + 24, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 10; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(7) == kEntry + 12, "jalr should forward its base register before WB")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "jalr base forwarding should still flush the wrong-path addi")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(3) == 7, "jalr base forwarding should still reach the target instruction")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(1, 0x34);
        cpu.csr().write(CSR_MSCRATCH, 0x12, cpu.core());

        write32(ram, kEntry + 0, kCsrrwX5MscratchX1);
        write32(ram, kEntry + 4, kNop);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);

        PipelineBackend backend(cpu, bus);

        backend.step();
        backend.step();
        if (!expect(cpu.core().read_gpr(5) == 0, "CSR reads should not write back before the WB stage")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MSCRATCH, cpu.core()) == 0x12, "CSR writes should not commit before the WB stage")) {
            return 1;
        }

        backend.step();
        backend.step();
        if (!expect(cpu.core().read_gpr(5) == 0, "CSR writeback should still be pending before WB")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MSCRATCH, cpu.core()) == 0x12, "CSR state should remain unchanged before WB")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().read_gpr(5) == 0x12, "CSR instructions should write rd through the pipeline commit point")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MSCRATCH, cpu.core()) == 0x34, "CSR instructions should update the CSR at WB")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(1, 0x55);
        cpu.csr().write(CSR_MSCRATCH, 0x12, cpu.core());

        write32(ram, kEntry + 0, kCsrwMscratchX1);
        write32(ram, kEntry + 4, kCsrrX6Mscratch);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 6; ++i) {
            backend.step();
        }
        if (!expect(cpu.csr().read(CSR_MSCRATCH, cpu.core()) == 0x55, "older CSR writes should still commit their new value")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(6) == 0x55, "younger CSR reads should see an older in-flight CSR write")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MSCRATCH, 0, cpu.core());

        write32(ram, kEntry + 0, kAddiX5FromX0Plus18);
        write32(ram, kEntry + 4, kCsrwMscratchX5);
        write32(ram, kEntry + 8, kCsrrX6Mscratch);
        write32(ram, kEntry + 12, kAddiX7FromX0Plus18);
        write32(ram, kEntry + 16, kBneX6X7Skip);
        write32(ram, kEntry + 20, kAddiX2FromX0Plus7);
        write32(ram, kEntry + 24, kNop);
        write32(ram, kEntry + 28, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 10; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 7, "the li-csrw-csrr-branch chain from csr_trap should stay on the fallthrough path")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(1, 0x34);
        cpu.csr().write(CSR_MSCRATCH, 0x12, cpu.core());

        write32(ram, kEntry + 0, kCsrrwX6MscratchX1);
        write32(ram, kEntry + 4, kAddiX7FromX0Plus18);
        write32(ram, kEntry + 8, kBneX6X7Skip);
        write32(ram, kEntry + 12, kAddiX2FromX0Plus7);
        write32(ram, kEntry + 16, kNop);
        write32(ram, kEntry + 20, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 7, "branch compares should see CSR rd results through forwarding")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kEcall);
        write32(ram, kEntry + 4, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        backend.step();
        backend.step();
        if (!expect(cpu.core().pc() == kEntry, "ecall should not enter trap before reaching the WB stage")) {
            return 1;
        }

        backend.step();
        backend.step();
        if (!expect(cpu.core().pc() == kEntry, "ecall should stay in flight until commit")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().pc() == kTrapVector, "ecall should update pc to mtvec only at commit")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "ecall trap should flush younger wrong-path work")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 11, "ecall from M-mode should commit the correct trap cause")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiA7Exit);
        write32(ram, kEntry + 4, kEcall);
        write32(ram, kEntry + 8, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 5; ++i) {
            backend.step();
        }
        if (!expect(!cpu.core().halted(), "exit ecall should not halt before it reaches the WB stage")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().halted(), "exit ecall should see a just-produced a7 value through forwarding")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MEPC, kEntry + 8, cpu.core());
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kAddiX2FromX0Plus7);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        backend.step();
        backend.step();
        if (!expect(cpu.core().pc() == kEntry, "mret should not redirect before reaching the WB stage")) {
            return 1;
        }

        backend.step();
        backend.step();
        if (!expect(cpu.core().pc() == kEntry, "mret should remain pending until WB")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().pc() == kEntry + 8, "mret should redirect to mepc at commit")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "mret should flush the wrong-path instruction behind it")) {
            return 1;
        }

        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 7, "mret should resume execution from mepc after commit")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kDataAddr);
        cpu.core().write_gpr(11, 42);

        write32(ram, kEntry + 0, kSwX11ToX10);
        write32(ram, kEntry + 4, kLwX12FromX10);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);
        write32(ram, kEntry + 20, kNop);
        write32(ram, kDataAddr, 0);

        PipelineBackend backend(cpu, bus);

        backend.step();
        backend.step();
        if (!expect(ram.load(kDataAddr, 4) == 0, "store should not update memory before reaching MEM")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(12) == 0, "load should not write back in the first two cycles")) {
            return 1;
        }

        backend.step();
        if (!expect(ram.load(kDataAddr, 4) == 0, "store should still be pending before the MEM stage")) {
            return 1;
        }

        backend.step();
        if (!expect(ram.load(kDataAddr, 4) == 42, "store should commit in the MEM stage")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(12) == 0, "load result should still wait for WB after MEM")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().read_gpr(12) == 0, "load should not write back until the WB stage")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().read_gpr(12) == 42, "load should eventually write back the fetched value")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kDataAddr);

        write32(ram, kEntry + 0, kLwX1FromX10);
        write32(ram, kEntry + 4, kAddiX2FromX1Plus5);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kDataAddr, 37);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 7; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 42, "load-use dependency should resolve with a single interlock and then commit")) {
            return 1;
        }
    }

    return 0;
}
