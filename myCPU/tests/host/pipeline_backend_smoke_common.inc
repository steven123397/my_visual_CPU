#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/devices/clint.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/uart16550.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/exec/pipeline_core_state.h"
#include "../../src/exec/pipeline_hazards.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../include/platform_mmio.h"

namespace {

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
constexpr uint32_t kFldF5FromX10 = 0x00053287U;          // fld f5, 0(x10)
constexpr uint32_t kFaddDF6F5F10 = 0x02a28353U;          // fadd.d ft6, ft5, fa0
constexpr uint32_t kFaddSF9F9F10 = 0x00a4f4d3U;          // fadd.s fs1, fs1, fa0
constexpr uint32_t kFsdF5ToX10Plus8 = 0x00553427U;       // fsd f5, 8(x10)
constexpr uint32_t kFmvDXF10FromX0 = 0xf2000553U;        // fmv.d.x fa0, x0
constexpr uint32_t kFmvXDA0FromF10 = 0xe2050553U;        // fmv.x.d a0, fa0
constexpr uint32_t kFmvWXF10FromX10 = 0xf0050553U;       // fmv.w.x fa0, a0
constexpr uint32_t kFmvXWA0FromF10 = 0xe0050553U;        // fmv.x.w a0, fa0
constexpr uint32_t kFaddDF9F9F10 = 0x02a4f4d3U;          // fadd.d fs1, fs1, fa0
constexpr uint32_t kFsubSF10F18F10 = 0x08a97553U;        // fsub.s fa0, fs2, fa0
constexpr uint32_t kFmaddDF9F10F11F12 = 0x62b504c3U;     // fmadd.d fs1, fa0, fa1, fa2
constexpr uint32_t kFmsubDF9F10F11F12 = 0x62b504c7U;     // fmsub.d fs1, fa0, fa1, fa2
constexpr uint32_t kFnmsubDF9F10F11F12 = 0x62b504cbU;    // fnmsub.d fs1, fa0, fa1, fa2
constexpr uint32_t kFnmaddDF9F10F11F12 = 0x62b504cfU;    // fnmadd.d fs1, fa0, fa1, fa2
constexpr uint32_t kFmaddSF8F10F11F12 = 0x60b50443U;     // fmadd.s fs0, fa0, fa1, fa2
constexpr uint32_t kFmsubSF8F10F11F12 = 0x60b50447U;     // fmsub.s fs0, fa0, fa1, fa2
constexpr uint32_t kFnmsubSF8F10F11F12 = 0x60b5044bU;    // fnmsub.s fs0, fa0, fa1, fa2
constexpr uint32_t kFnmaddSF8F10F11F12 = 0x60b5044fU;    // fnmadd.s fs0, fa0, fa1, fa2
constexpr uint32_t kFcvtLDX13FromF10 = 0xc22516d3U;      // fcvt.l.d a3, fa0, rtz
constexpr uint32_t kFcvtWUDX10FromF10 = 0xc2151553U;     // fcvt.wu.d a0, fa0, rtz
constexpr uint32_t kFcvtWSX10FromF10 = 0xc0051553U;      // fcvt.w.s a0, fa0, rtz
constexpr uint32_t kFcvtLSX10FromF10 = 0xc0251553U;      // fcvt.l.s a0, fa0, rtz
constexpr uint32_t kFcvtWUSX10FromF10 = 0xc0151553U;     // fcvt.wu.s a0, fa0, rtz
constexpr uint32_t kFcvtLUSX10FromF10 = 0xc0351553U;     // fcvt.lu.s a0, fa0, rtz
constexpr uint32_t kFcvtDWUDF15FromX13 = 0xd216f7d3U;    // fcvt.d.wu fa5, a3
constexpr uint32_t kFcvtDLUDF15FromX13 = 0xd236f7d3U;    // fcvt.d.lu fa5, a3
constexpr uint32_t kFltDX10F15F10 = 0xa2a79553U;         // flt.d a0, fa5, fa0
constexpr uint32_t kFeqSA0F11F12 = 0xa0c5a553U;          // feq.s a0, fa1, fa2
constexpr uint32_t kFltSA1F10F11 = 0xa0b515d3U;          // flt.s a1, fa0, fa1
constexpr uint32_t kFleSA2F11F12 = 0xa0c58653U;          // fle.s a2, fa1, fa2
constexpr uint32_t kFmulDF10F10F15 = 0x12f57553U;        // fmul.d fa0, fa0, fa5
constexpr uint32_t kFmulSF10F10F15 = 0x10f57553U;        // fmul.s fa0, fa0, fa5
constexpr uint32_t kFsgnjDF9F10F11 = 0x22b504d3U;        // fsgnj.d fs1, fa0, fa1
constexpr uint32_t kFsgnjnDF9F10F11 = 0x22b514d3U;       // fsgnjn.d fs1, fa0, fa1
constexpr uint32_t kFabsDF9F10 = 0x22a524d3U;            // fabs.d fs1, fa0
constexpr uint32_t kFsgnjSF9F10F11 = 0x20b504d3U;        // fsgnj.s fs1, fa0, fa1
constexpr uint32_t kFsgnjnSF9F10F11 = 0x20b514d3U;       // fsgnjn.s fs1, fa0, fa1
constexpr uint32_t kFsgnjxSF9F10F11 = 0x20b524d3U;       // fsgnjx.s fs1, fa0, fa1
constexpr uint32_t kFnegSF9F10 = 0x20a514d3U;            // fneg.s fs1, fa0
constexpr uint32_t kFnegSF9F9 = 0x209494d3U;             // fneg.s fs1, fs1
constexpr uint32_t kFabsSF9F10 = 0x20a524d3U;            // fabs.s fs1, fa0
constexpr uint32_t kFclassSA1F15 = 0xe00795d3U;          // fclass.s a1, fa5
constexpr uint32_t kFclassDA1F15 = 0xe20795d3U;          // fclass.d a1, fa5
constexpr uint32_t kFmaxSF9F10F11 = 0x28b514d3U;         // fmax.s fs1, fa0, fa1
constexpr uint32_t kFminSF9F10F11 = 0x28b504d3U;         // fmin.s fs1, fa0, fa1
constexpr uint32_t kFmaxDF9F10F11 = 0x2ab514d3U;         // fmax.d fs1, fa0, fa1
constexpr uint32_t kFminDF9F10F11 = 0x2ab504d3U;         // fmin.d fs1, fa0, fa1
constexpr uint32_t kFdivSF9F9F10 = 0x18a4f4d3U;          // fdiv.s fs1, fs1, fa0
constexpr uint32_t kFcvtDSF15FromF10 = 0x421507d3U;      // fcvt.d.s fa5, fa0
constexpr uint32_t kFcvtSDF15FromF10 = 0x401507d3U;      // fcvt.s.d fa5, fa0
constexpr uint32_t kFcvtSWF15FromX10 = 0xd00507d3U;      // fcvt.s.w fa5, a0, rne
constexpr uint32_t kFcvtSWUF15FromX10 = 0xd01507d3U;     // fcvt.s.wu fa5, a0, rne
constexpr uint32_t kFcvtSLF15FromX10 = 0xd02507d3U;      // fcvt.s.l fa5, a0, rne
constexpr uint32_t kFcvtSLUF15FromX10 = 0xd03507d3U;     // fcvt.s.lu fa5, a0, rne
constexpr uint32_t kFsqrtSF15FromF10 = 0x580507d3U;      // fsqrt.s fa5, fa0
constexpr uint32_t kCsrrcSipX5 = 0x1442b073U;            // csrrc x0, sip, x5
constexpr uint32_t kCsrwSepcX7 = 0x14139073U;            // csrw sepc, x7
constexpr uint32_t kSret = 0x10200073U;                  // sret
constexpr uint32_t kBltX1X2Loop = 0xfe20cee3U;           // blt x1, x2, -4
constexpr uint64_t kMstatusFsDirty = 0x3ULL << 12;
constexpr uint64_t kMstatusSd = 1ULL << 63;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

[[maybe_unused]] uint64_t double_to_raw_bits(double value) {
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

[[maybe_unused]] double raw_bits_to_double(uint64_t bits) {
    double value = 0.0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&value, &bits, sizeof(bits));
    return value;
}

[[maybe_unused]] uint64_t float_to_fpr_raw_bits(float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return 0xffffffff00000000ULL | static_cast<uint64_t>(bits);
}

[[maybe_unused]] float raw_bits_to_float(uint64_t bits) {
    const uint32_t single_bits = static_cast<uint32_t>(bits & 0xffffffffULL);
    float value = 0.0f;
    static_assert(sizeof(single_bits) == sizeof(value));
    std::memcpy(&value, &single_bits, sizeof(single_bits));
    return value;
}

[[maybe_unused]] bool trace_contains_raw(const std::vector<RetireTraceEntry>& trace, uint32_t raw) {
    for (const RetireTraceEntry& entry : trace) {
        if (entry.raw == raw) {
            return true;
        }
    }
    return false;
}

[[maybe_unused]] void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

[[maybe_unused]] int run_until_halt_and_count_redirects(PipelineBackend& backend, CPU& cpu, int max_steps) {
    int redirects = 0;
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
        backend.step();
        if (backend.debug_snapshot().pipeline.redirected) {
            ++redirects;
        }
    }
    return redirects;
}

}  // namespace
