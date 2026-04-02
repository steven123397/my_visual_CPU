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
constexpr uint32_t kAddiA7Exit = 0x05d00893U;          // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;               // ecall
constexpr uint32_t kInvalidInsn = 0xffffffffU;

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

        write32(ram, kEntry + 0, kAddiX1FromX0Plus1);
        write32(ram, kEntry + 4, kAddiX1Inc);
        write32(ram, kEntry + 8, kAddiX2FromX1Plus2);
        write32(ram, kEntry + 12, kAddiA7Exit);
        write32(ram, kEntry + 16, kEcall);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 3; ++i) {
            backend.step();
        }

        const PipelineCoreState& state = backend.testing_state();
        const uint32_t first_phys = state.ex_mem.slot.rd_phys;
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

        for (int i = 0; i < 2; ++i) {
            backend.step();
        }

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

        if (!expect(run_until_halt(backend, cpu, 16), "rename/commit smoke should halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 2 && cpu.core().read_gpr(2) == 4,
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
