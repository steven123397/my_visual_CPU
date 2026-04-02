#include <cstdio>
#include <vector>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint32_t kAddiX1 = 0x00100093U;          // addi x1, x0, 1
constexpr uint32_t kAddiX2 = 0x00200113U;          // addi x2, x0, 2
constexpr uint32_t kAddiX1WrongPath = 0x06300093U; // addi x1, x0, 99
constexpr uint32_t kAddiA7Exit = 0x05d00893U;      // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;           // ecall
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;      // jal x0, 8
constexpr uint32_t kMret = 0x30200073U;
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

bool trace_contains_raw(const std::vector<RetireTraceEntry>& trace, uint32_t raw) {
    for (const RetireTraceEntry& entry : trace) {
        if (entry.raw == raw) {
            return true;
        }
    }
    return false;
}

bool trace_sequence_is_strictly_increasing(const std::vector<RetireTraceEntry>& trace) {
    uint64_t previous = 0;
    for (const RetireTraceEntry& entry : trace) {
        if (entry.sequence_id <= previous) {
            return false;
        }
        previous = entry.sequence_id;
    }
    return true;
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

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);

        backend.step();
        const BackendDebugSnapshot after_first_fetch = backend.debug_snapshot();
        if (!expect(after_first_fetch.pipeline.last_sequence_id == 1,
                    "first fetched instruction should allocate sequence 1")) {
            return 1;
        }

        backend.step();
        const BackendDebugSnapshot after_second_fetch = backend.debug_snapshot();
        if (!expect(after_second_fetch.pipeline.last_sequence_id == 2,
                    "second fetched instruction should allocate sequence 2")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kJalX0Skip8);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "jal redirect smoke should halt")) {
            return 1;
        }

        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(snapshot.pipeline.last_sequence_id >= 3,
                    "branch redirect path should allocate sequence ids for fetched instructions")) {
            return 1;
        }
        if (!expect(trace_sequence_is_strictly_increasing(snapshot.pipeline.retire_trace),
                    "retire trace should stay in architected sequence order after redirect")) {
            return 1;
        }
        if (!expect(!trace_contains_raw(snapshot.pipeline.retire_trace, kAddiX1WrongPath),
                    "redirect should squash the younger wrong-path instruction from retire trace")) {
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
        write32(ram, kEntry + 4, kInvalidInsn);
        write32(ram, kEntry + 8, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kAddiA7Exit);
        write32(ram, kTrapVector + 4, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 48), "illegal-instruction trap path should halt from handler")) {
            return 1;
        }

        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(trace_sequence_is_strictly_increasing(snapshot.pipeline.retire_trace),
                    "trap handling should preserve architected retire order")) {
            return 1;
        }
        if (!expect(trace_contains_raw(snapshot.pipeline.retire_trace, kInvalidInsn),
                    "retire trace should include the trap boundary instruction")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.retire_trace.size() >= 3,
                    "trap path should leave multiple architected boundaries in retire trace")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.retire_trace[1].trap,
                    "illegal instruction boundary should be marked as a trap in retire trace")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MEPC, kEntry + 8, cpu.core());
        cpu.csr().write(CSR_MSTATUS,
                        static_cast<uint64_t>(PrivilegeMode::Machine) << MSTATUS_MPP_SHIFT,
                        cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "mret redirect path should halt")) {
            return 1;
        }

        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(trace_contains_raw(snapshot.pipeline.retire_trace, kMret),
                    "retire trace should include the committed mret boundary")) {
            return 1;
        }
        if (!expect(!trace_contains_raw(snapshot.pipeline.retire_trace, kAddiX1WrongPath),
                    "mret redirect should squash younger fall-through work from retire trace")) {
            return 1;
        }
    }

    return 0;
}
