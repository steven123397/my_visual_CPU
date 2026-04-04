#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

#include "../../include/platform_mmio.h"
#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/devices/clint.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/uart16550.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/exec/pipeline_core_state.h"
#include "../../src/exec/pipeline_hazards.h"
#include "../../src/exec/pipeline_sequence.h"
#include "../../src/decode.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint64_t kDataAddr = kEntry + 0x100;
constexpr uint32_t kNop = 0x00000013U;                    // addi x0, x0, 0
constexpr uint32_t kAddiX1 = 0x00100093U;                // addi x1, x0, 1
constexpr uint32_t kAddiX1Inc = 0x00108093U;             // addi x1, x1, 1
constexpr uint32_t kAddiX2FromX1Plus2 = 0x00208113U;     // addi x2, x1, 2
constexpr uint32_t kAddX3FromX2X1 = 0x001101b3U;         // add x3, x2, x1
constexpr uint32_t kAddiX1WrongPath = 0x06300093U;       // addi x1, x0, 99
constexpr uint32_t kAddiX2WrongPath = 0x06300113U;       // addi x2, x0, 99
constexpr uint32_t kAddiX2FromX0Plus7 = 0x00700113U;     // addi x2, x0, 7
constexpr uint32_t kAddiX2FromX0Plus5 = 0x00500113U;     // addi x2, x0, 5
constexpr uint32_t kAddiA7Exit = 0x05d00893U;            // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;                 // ecall
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;            // jal x0, 8
constexpr uint32_t kBeqX0Taken = 0x00000463U;            // beq x0, x0, 8
constexpr uint32_t kInvalidInsn = 0xffffffffU;
constexpr uint32_t kLwX1FromX10 = 0x00052083U;           // lw x1, 0(x10)
constexpr uint32_t kInvalidLoadFunct3X1FromX10 = 0x00057083U;
constexpr uint32_t kAddiX2FromX1Plus5 = 0x00508113U;     // addi x2, x1, 5
constexpr uint32_t kMret = 0x30200073U;
constexpr uint32_t kSbX5ToX10Plus1 = 0x005500a3U;        // sb x5, 1(x10)
constexpr uint32_t kSbX0ToX10Plus1 = 0x000500a3U;        // sb x0, 1(x10)
constexpr uint32_t kLbX6FromX10Plus1 = 0x00150303U;      // lb x6, 1(x10)
constexpr uint32_t kLbX6FromX10Plus8 = 0x00850303U;      // lb x6, 8(x10)
constexpr uint32_t kLwX6FromX20 = 0x000a2303U;           // lw x6, 0(x20)
constexpr uint32_t kSwX6ToX20 = 0x006a2023U;             // sw x6, 0(x20)
constexpr uint32_t kInvalidStoreFunct3X6ToX20 = 0x006a7023U;
constexpr uint32_t kSdX21ToX20 = 0x015a3023U;            // sd x21, 0(x20)
constexpr uint32_t kCsrrcSipX5 = 0x1442b073U;            // csrrc x0, sip, x5
constexpr uint32_t kCsrwSepcX7 = 0x14139073U;            // csrw sepc, x7
constexpr uint32_t kSret = 0x10200073U;                  // sret
constexpr uint32_t kBltX1X2Loop = 0xfe20cee3U;           // blt x1, x2, -4

inline bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

inline bool trace_contains_raw(const std::vector<RetireTraceEntry>& trace,
                               uint32_t raw) {
    for (const RetireTraceEntry& entry : trace) {
        if (entry.raw == raw) {
            return true;
        }
    }
    return false;
}

inline void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

inline bool run_until_halt(PipelineBackend& backend, CPU& cpu, int max_steps) {
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
        backend.step();
    }
    return cpu.core().halted();
}

inline int run_until_halt_and_count_redirects(PipelineBackend& backend,
                                              CPU& cpu,
                                              int max_steps) {
    int redirects = 0;
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
        backend.step();
        if (backend.debug_snapshot().pipeline.redirected) {
            ++redirects;
        }
    }
    return redirects;
}
