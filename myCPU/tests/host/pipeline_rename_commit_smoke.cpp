#include <cstdio>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint32_t kAddiX1FromX0Plus1 = 0x00100093U;   // addi x1, x0, 1
constexpr uint32_t kAddiX1Inc = 0x00108093U;           // addi x1, x1, 1
constexpr uint32_t kAddiX2FromX1Plus2 = 0x00208113U;   // addi x2, x1, 2
constexpr uint32_t kAddiX2FromX0Plus5 = 0x00500113U;   // addi x2, x0, 5
constexpr uint32_t kAddiX3FromX2Plus7 = 0x00710193U;   // addi x3, x2, 7
constexpr uint32_t kAddiX3FromX0Plus3 = 0x00300193U;   // addi x3, x0, 3
constexpr uint32_t kAddiA7Exit = 0x05d00893U;          // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;               // ecall
constexpr uint32_t kInvalidInsn = 0xffffffffU;
constexpr uint32_t kLwX1FromX10 = 0x00052083U;         // lw x1, 0(x10)

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
    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kEntry + 0x100);
        ram.store(kEntry + 0x100, 0x11223344U, 4);

        write32(ram, kEntry + 0, kLwX1FromX10);
        write32(ram, kEntry + 4, kAddiX2FromX0Plus5);
        write32(ram, kEntry + 8, kAddiX3FromX2Plus7);
        write32(ram, kEntry + 12, kAddiA7Exit);
        write32(ram, kEntry + 16, kEcall);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 4; ++i) {
            backend.step();
        }

        const PipelineCoreState& state = backend.testing_state();
        const auto rob_head = state.rob().peek_head();
        const uint32_t x2_phys = state.rename_map().map_source(2);

        if (!expect(rob_head.has_value() && rob_head->sequence_id == 1 && !rob_head->ready,
                    "older load should still be the not-ready ROB head while OoO execute is in flight")) {
            return 1;
        }
        if (!expect(x2_phys != 0 && state.phys_regs().is_ready(x2_phys) && state.phys_regs().read(x2_phys) == 5,
                    "younger independent ALU should finish into the physical register file before the older memory op completes")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0,
                    "younger ALU completion must stay invisible to architected state before the older ROB head retires")) {
            return 1;
        }

        if (!expect(run_until_halt(backend, cpu, 24), "minimal OoO execute smoke should halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0x11223344ULL && cpu.core().read_gpr(2) == 5 && cpu.core().read_gpr(3) == 12,
                    "final architected state should still retire in program order after OoO completion")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX1FromX0Plus1);
        write32(ram, kEntry + 4, kAddiX1Inc);
        write32(ram, kEntry + 8, kAddiX2FromX1Plus2);
        write32(ram, kEntry + 12, kAddiX3FromX0Plus3);
        write32(ram, kEntry + 16, kAddiA7Exit);
        write32(ram, kEntry + 20, kEcall);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 3; ++i) {
            backend.step();
        }

        const PipelineCoreState& state = backend.testing_state();
        const uint32_t first_phys = state.mem_wb.slot.rd_phys;
        const uint32_t second_phys = state.id_ex.slot.rd_phys;

        if (!expect(first_phys != 0 && first_phys != 1,
                    "older destination should already be renamed to a non-architectural physical register")) {
            return 1;
        }
        if (!expect(state.id_ex.slot.rs1_phys == first_phys,
                    "younger instruction should consume the older speculative destination through the renamed source tag")) {
            return 1;
        }
        if (!expect(state.rob().size() == 2,
                    "decode/rename should allocate ROB entries before architected commit")) {
            return 1;
        }
        if (!expect(state.phys_regs().is_ready(first_phys) && state.phys_regs().read(first_phys) == 1,
                    "execute should publish the speculative result into the physical register file")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0,
                    "architected GPR state must stay unchanged before the ROB head commits")) {
            return 1;
        }

        backend.step();
        if (!expect(cpu.core().read_gpr(1) == 1,
                    "ROB head commit should make the first renamed result architecturally visible")) {
            return 1;
        }
        if (!expect(state.rename_map().architectural_source(1) == first_phys,
                    "commit should advance the architectural mapping to the retired physical register")) {
            return 1;
        }
        if (!expect(state.rename_map().map_source(1) == second_phys,
                    "committing an older write must not clobber a younger speculative rename of the same architectural register")) {
            return 1;
        }
        backend.step();
        if (!expect(state.id_ex.slot.raw == kAddiX3FromX0Plus3 && state.id_ex.slot.rd_phys == first_phys,
                    "a rename that happens after ROB head commit should be able to recycle the stale committed phys tag")) {
            return 1;
        }

        if (!expect(run_until_halt(backend, cpu, 16), "rename/commit smoke should halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 2 && cpu.core().read_gpr(2) == 4 && cpu.core().read_gpr(3) == 3,
                    "final architected state should still match the renamed dependency chain")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kAddiX1FromX0Plus1);
        write32(ram, kEntry + 4, kInvalidInsn);
        write32(ram, kEntry + 8, kAddiX1Inc);
        write32(ram, kEntry + 12, kAddiA7Exit);
        write32(ram, kEntry + 16, kEcall);
        write32(ram, kTrapVector + 0, kAddiA7Exit);
        write32(ram, kTrapVector + 4, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "trap flush rename smoke should halt")) {
            return 1;
        }

        const PipelineCoreState& state = backend.testing_state();
        if (!expect(cpu.core().read_gpr(1) == 1,
                    "flush after an older trap must not leak the younger renamed destination into architected state")) {
            return 1;
        }
        if (!expect(state.rob().size() == 0,
                    "trap flush should discard younger speculative ROB entries")) {
            return 1;
        }
        if (!expect(state.rename_map().map_source(1) == state.rename_map().architectural_source(1),
                    "trap flush should roll speculative rename state back to the committed architectural baseline")) {
            return 1;
        }
    }

    return 0;
}
