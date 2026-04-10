#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "../../include/platform_mmio.h"
#include "../../src/cpu.h"
#include "../../src/exec/backend.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint32_t kEcall = 0x00000073U;

constexpr uint64_t kDotA = kEntry + 0x200;
constexpr uint64_t kDotB = kEntry + 0x210;
constexpr uint64_t kGemmRow0 = kEntry + 0x240;
constexpr uint64_t kGemmRow1 = kEntry + 0x250;
constexpr uint64_t kGemmCol0 = kEntry + 0x260;
constexpr uint64_t kGemmCol1 = kEntry + 0x270;
constexpr uint64_t kConvInput = kEntry + 0x2a0;
constexpr uint64_t kConvKernel = kEntry + 0x2b0;
constexpr uint64_t kReluInput = kEntry + 0x2c0;

constexpr uint64_t kOutputBase = kEntry + 0x380;
constexpr size_t kGuardBytes = 16;
constexpr size_t kDotOutBytes = 8;
constexpr size_t kGemmOutBytes = 32;
constexpr size_t kConvOutBytes = 24;
constexpr size_t kReluOutBytes = 16;
constexpr size_t kPayloadBytes = kDotOutBytes + kGemmOutBytes + kConvOutBytes + kReluOutBytes;
constexpr size_t kWatchBytes = kGuardBytes + kPayloadBytes + kGuardBytes;
constexpr uint8_t kGuardPattern = 0x5a;
constexpr uint8_t kRegisterBytes = 16;

constexpr int32_t kDotOutOffset = 0;
constexpr int32_t kGemmOutOffset = kDotOutOffset + static_cast<int32_t>(kDotOutBytes);
constexpr int32_t kConvOutOffset = kGemmOutOffset + static_cast<int32_t>(kGemmOutBytes);
constexpr int32_t kReluOutOffset = kConvOutOffset + static_cast<int32_t>(kConvOutBytes);

enum class BackendKind : uint8_t {
    Functional,
    Pipeline,
};

struct FinalState {
    bool halted{false};
    uint64_t pc{0};
    uint64_t instret{0};
    uint8_t sew_bytes{0};
    uint8_t vl{0};
    std::array<std::array<uint8_t, kRegisterBytes>, 32> vector_regs{};
    std::array<uint8_t, kWatchBytes> watched{};
};

template <typename>
struct always_false : std::false_type {};

template <typename T, typename = void>
struct has_sew_field : std::false_type {};

template <typename T>
struct has_sew_field<T, std::void_t<decltype(std::declval<const T&>().sew_bytes)>> : std::true_type {};

template <typename T, typename = void>
struct has_sew_getter : std::false_type {};

template <typename T>
struct has_sew_getter<T, std::void_t<decltype(std::declval<const T&>().sew_bytes())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_vl_field : std::false_type {};

template <typename T>
struct has_vl_field<T, std::void_t<decltype(std::declval<const T&>().vl)>> : std::true_type {};

template <typename T, typename = void>
struct has_vl_getter : std::false_type {};

template <typename T>
struct has_vl_getter<T, std::void_t<decltype(std::declval<const T&>().vl())>> : std::true_type {};

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

uint32_t encode_rtype(uint8_t opcode,
                      uint8_t funct3,
                      uint8_t funct7,
                      uint8_t rd,
                      uint8_t rs1,
                      uint8_t rs2) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t encode_itype(uint8_t opcode,
                      uint8_t funct3,
                      uint8_t rd,
                      uint8_t rs1,
                      int32_t imm12) {
    const uint32_t imm = static_cast<uint32_t>(imm12) & 0xFFFU;
    return (imm << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t encode_stype(uint8_t opcode,
                      uint8_t funct3,
                      uint8_t rs1,
                      uint8_t rs2,
                      int32_t imm12) {
    const uint32_t imm = static_cast<uint32_t>(imm12) & 0xFFFU;
    const uint32_t imm_hi = (imm >> 5) & 0x7FU;
    const uint32_t imm_lo = imm & 0x1FU;
    return (imm_hi << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (imm_lo << 7) |
           static_cast<uint32_t>(opcode);
}

uint8_t sew_code_from_bytes(uint8_t sew_bytes) {
    switch (sew_bytes) {
    case 1:
        return 0;
    case 2:
        return 1;
    case 4:
        return 3;
    case 8:
        return 7;
    default:
        return 2;
    }
}

uint32_t encode_vsetcfg(uint8_t sew_bytes, uint8_t vl) {
    const uint8_t funct7 = static_cast<uint8_t>(0x40U | sew_code_from_bytes(sew_bytes));
    return encode_rtype(0x57, 7, funct7, 0, 0, static_cast<uint8_t>(vl - 1));
}

uint32_t encode_vv(uint8_t funct7, uint8_t vd, uint8_t vs1, uint8_t vs2) {
    return encode_rtype(0x57, 0, funct7, vd, vs1, vs2);
}

uint32_t encode_vle(uint8_t vd, uint8_t base, int32_t imm12 = 0) {
    return encode_itype(0x07, 0, vd, base, imm12);
}

uint32_t encode_vse(uint8_t vs, uint8_t base, int32_t imm12 = 0) {
    return encode_stype(0x27, 0, base, vs, imm12);
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

template <size_t N>
void write_buffer(Ram& ram, uint64_t addr, const std::array<uint8_t, N>& bytes) {
    ram.write_bytes(addr, bytes.data(), bytes.size());
}

template <size_t N>
std::array<uint8_t, N> read_buffer(Ram& ram, uint64_t addr) {
    std::array<uint8_t, N> out{};
    for (size_t i = 0; i < N; ++i) {
        out[i] = static_cast<uint8_t>(ram.load(addr + i, 1));
    }
    return out;
}

std::unique_ptr<ExecutionBackend> make_backend(BackendKind kind, CPU& cpu, Bus& bus) {
    switch (kind) {
    case BackendKind::Functional:
        return std::make_unique<FunctionalBackend>(cpu, bus);
    case BackendKind::Pipeline:
        return std::make_unique<PipelineBackend>(cpu, bus);
    }
    return nullptr;
}

template <typename VectorStateLike>
uint8_t read_vector_sew(const VectorStateLike& vector_state) {
    if constexpr (has_sew_field<VectorStateLike>::value) {
        return static_cast<uint8_t>(vector_state.sew_bytes);
    } else if constexpr (has_sew_getter<VectorStateLike>::value) {
        return static_cast<uint8_t>(vector_state.sew_bytes());
    } else {
        static_assert(always_false<VectorStateLike>::value,
                      "VectorState must expose sew_bytes field or getter");
    }
}

template <typename VectorStateLike>
uint8_t read_vector_vl(const VectorStateLike& vector_state) {
    if constexpr (has_vl_field<VectorStateLike>::value) {
        return static_cast<uint8_t>(vector_state.vl);
    } else if constexpr (has_vl_getter<VectorStateLike>::value) {
        return static_cast<uint8_t>(vector_state.vl());
    } else {
        static_assert(always_false<VectorStateLike>::value,
                      "VectorState must expose vl field or getter");
    }
}

template <typename VectorStateLike>
std::array<std::array<uint8_t, kRegisterBytes>, 32> capture_vector_regs(
    const VectorStateLike& vector_state) {
    std::array<std::array<uint8_t, kRegisterBytes>, 32> regs{};
    for (size_t i = 0; i < regs.size(); ++i) {
        regs[i] = vector_state.read_reg(static_cast<uint32_t>(i));
    }
    return regs;
}

std::array<uint8_t, 8> pack_i64(int64_t value) {
    std::array<uint8_t, 8> out{};
    const uint64_t bits = static_cast<uint64_t>(value);
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<uint8_t>((bits >> (i * 8U)) & 0xFFU);
    }
    return out;
}

std::array<uint8_t, 16> pack_i32x4(const std::array<int32_t, 4>& values) {
    std::array<uint8_t, 16> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        const uint32_t bits = static_cast<uint32_t>(values[i]);
        for (size_t b = 0; b < sizeof(uint32_t); ++b) {
            out[i * sizeof(uint32_t) + b] =
                static_cast<uint8_t>((bits >> (b * 8U)) & 0xFFU);
        }
    }
    return out;
}

std::array<uint8_t, 16> pack_i16x8(const std::array<int16_t, 8>& values) {
    std::array<uint8_t, 16> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        const uint16_t bits = static_cast<uint16_t>(values[i]);
        out[i * sizeof(uint16_t)] = static_cast<uint8_t>(bits & 0xFFU);
        out[i * sizeof(uint16_t) + 1] = static_cast<uint8_t>((bits >> 8U) & 0xFFU);
    }
    return out;
}

std::array<uint8_t, 8> pack_i8x8(const std::array<int8_t, 8>& values) {
    std::array<uint8_t, 8> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        out[i] = static_cast<uint8_t>(values[i]);
    }
    return out;
}

std::array<uint8_t, 4> pack_i8x4(const std::array<int8_t, 4>& values) {
    std::array<uint8_t, 4> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        out[i] = static_cast<uint8_t>(values[i]);
    }
    return out;
}

std::array<uint8_t, 6> pack_i8x6(const std::array<int8_t, 6>& values) {
    std::array<uint8_t, 6> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        out[i] = static_cast<uint8_t>(values[i]);
    }
    return out;
}

std::array<uint8_t, kWatchBytes> expected_watch() {
    const std::array<int8_t, 8> dot_a{1, -2, 3, -4, 5, -6, 7, -8};
    const std::array<int8_t, 8> dot_b{-1, 2, 3, -4, 5, 6, -7, 8};
    int64_t dot = 0;
    for (size_t i = 0; i < dot_a.size(); ++i) {
        dot += static_cast<int64_t>(dot_a[i]) * static_cast<int64_t>(dot_b[i]);
    }

    const std::array<int32_t, 4> gemm_row0{1, 2, -3, 4};
    const std::array<int32_t, 4> gemm_row1{-5, 6, 7, -8};
    const std::array<int32_t, 4> gemm_col0{1, -2, 3, 4};
    const std::array<int32_t, 4> gemm_col1{-1, 2, 5, -6};
    std::array<int64_t, 4> gemm{};
    const std::array<std::array<int32_t, 4>, 2> rows{gemm_row0, gemm_row1};
    const std::array<std::array<int32_t, 4>, 2> cols{gemm_col0, gemm_col1};
    for (size_t r = 0; r < rows.size(); ++r) {
        for (size_t c = 0; c < cols.size(); ++c) {
            int64_t acc = 0;
            for (size_t k = 0; k < rows[r].size(); ++k) {
                acc += static_cast<int64_t>(rows[r][k]) * static_cast<int64_t>(cols[c][k]);
            }
            gemm[r * cols.size() + c] = acc;
        }
    }

    const std::array<int8_t, 6> conv_input{2, -1, 3, 4, -2, 1};
    const std::array<int8_t, 4> conv_kernel{1, 0, -1, 2};
    std::array<int64_t, 3> conv{};
    for (size_t i = 0; i < conv.size(); ++i) {
        int64_t acc = 0;
        for (size_t k = 0; k < conv_kernel.size(); ++k) {
            acc += static_cast<int64_t>(conv_input[i + k]) *
                   static_cast<int64_t>(conv_kernel[k]);
        }
        conv[i] = acc;
    }

    const std::array<int16_t, 8> relu_input{-5, 0, 7, -2, 123, -327, 42, -1};
    std::array<int16_t, 8> relu{};
    for (size_t i = 0; i < relu.size(); ++i) {
        relu[i] = relu_input[i] > 0 ? relu_input[i] : 0;
    }

    std::array<uint8_t, kWatchBytes> out{};
    out.fill(kGuardPattern);
    size_t cursor = kGuardBytes;

    const std::array<uint8_t, 8> dot_bytes = pack_i64(dot);
    for (uint8_t value : dot_bytes) {
        out[cursor++] = value;
    }
    for (int64_t value : gemm) {
        const std::array<uint8_t, 8> bytes = pack_i64(value);
        for (uint8_t byte : bytes) {
            out[cursor++] = byte;
        }
    }
    for (int64_t value : conv) {
        const std::array<uint8_t, 8> bytes = pack_i64(value);
        for (uint8_t byte : bytes) {
            out[cursor++] = byte;
        }
    }
    const std::array<uint8_t, 16> relu_bytes = pack_i16x8(relu);
    for (uint8_t value : relu_bytes) {
        out[cursor++] = value;
    }

    return out;
}

void load_program(Ram& ram) {
    std::vector<uint32_t> program;
    program.reserve(48);

    // int8 dot-product
    program.push_back(encode_vsetcfg(1, 8));
    program.push_back(encode_vle(1, 10));
    program.push_back(encode_vle(2, 11));
    program.push_back(encode_vv(0x22, 3, 1, 2));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(3, 12, kDotOutOffset));

    // int32 2x2 GEMM (4 independent dots)
    program.push_back(encode_vsetcfg(4, 4));
    program.push_back(encode_vle(4, 13));
    program.push_back(encode_vle(5, 15));
    program.push_back(encode_vv(0x22, 6, 4, 5));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(6, 12, kGemmOutOffset + 0));

    program.push_back(encode_vsetcfg(4, 4));
    program.push_back(encode_vle(4, 13));
    program.push_back(encode_vle(5, 16));
    program.push_back(encode_vv(0x22, 6, 4, 5));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(6, 12, kGemmOutOffset + 8));

    program.push_back(encode_vsetcfg(4, 4));
    program.push_back(encode_vle(4, 14));
    program.push_back(encode_vle(5, 15));
    program.push_back(encode_vv(0x22, 6, 4, 5));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(6, 12, kGemmOutOffset + 16));

    program.push_back(encode_vsetcfg(4, 4));
    program.push_back(encode_vle(4, 14));
    program.push_back(encode_vle(5, 16));
    program.push_back(encode_vv(0x22, 6, 4, 5));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(6, 12, kGemmOutOffset + 24));

    // int8 1D conv (3 sliding windows)
    program.push_back(encode_vsetcfg(1, 4));
    program.push_back(encode_vle(7, 18, 0));
    program.push_back(encode_vle(8, 19, 0));
    program.push_back(encode_vv(0x22, 9, 7, 8));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(9, 12, kConvOutOffset + 0));

    program.push_back(encode_vsetcfg(1, 4));
    program.push_back(encode_vle(7, 18, 1));
    program.push_back(encode_vle(8, 19, 0));
    program.push_back(encode_vv(0x22, 9, 7, 8));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(9, 12, kConvOutOffset + 8));

    program.push_back(encode_vsetcfg(1, 4));
    program.push_back(encode_vle(7, 18, 2));
    program.push_back(encode_vle(8, 19, 0));
    program.push_back(encode_vv(0x22, 9, 7, 8));
    program.push_back(encode_vsetcfg(8, 1));
    program.push_back(encode_vse(9, 12, kConvOutOffset + 16));

    // int16 ReLU via vmax(x, 0)
    program.push_back(encode_vsetcfg(2, 8));
    program.push_back(encode_vle(15, 20));
    program.push_back(encode_vv(0x21, 16, 15, 0));
    program.push_back(encode_vse(16, 12, kReluOutOffset));

    program.push_back(kEcall);

    uint64_t pc = kEntry;
    for (uint32_t raw : program) {
        write32(ram, pc, raw);
        pc += 4;
    }
}

void seed_memory(Ram& ram) {
    const std::array<int8_t, 8> dot_a{1, -2, 3, -4, 5, -6, 7, -8};
    const std::array<int8_t, 8> dot_b{-1, 2, 3, -4, 5, 6, -7, 8};
    const std::array<int32_t, 4> gemm_row0{1, 2, -3, 4};
    const std::array<int32_t, 4> gemm_row1{-5, 6, 7, -8};
    const std::array<int32_t, 4> gemm_col0{1, -2, 3, 4};
    const std::array<int32_t, 4> gemm_col1{-1, 2, 5, -6};
    const std::array<int8_t, 6> conv_input{2, -1, 3, 4, -2, 1};
    const std::array<int8_t, 4> conv_kernel{1, 0, -1, 2};
    const std::array<int16_t, 8> relu_input{-5, 0, 7, -2, 123, -327, 42, -1};
    std::array<uint8_t, kWatchBytes> watch_init{};
    watch_init.fill(kGuardPattern);

    write_buffer(ram, kDotA, pack_i8x8(dot_a));
    write_buffer(ram, kDotB, pack_i8x8(dot_b));
    write_buffer(ram, kGemmRow0, pack_i32x4(gemm_row0));
    write_buffer(ram, kGemmRow1, pack_i32x4(gemm_row1));
    write_buffer(ram, kGemmCol0, pack_i32x4(gemm_col0));
    write_buffer(ram, kGemmCol1, pack_i32x4(gemm_col1));
    write_buffer(ram, kConvInput, pack_i8x6(conv_input));
    write_buffer(ram, kConvKernel, pack_i8x4(conv_kernel));
    write_buffer(ram, kReluInput, pack_i16x8(relu_input));
    write_buffer(ram, kOutputBase, watch_init);
}

FinalState run_case(BackendKind kind) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    load_program(ram);
    seed_memory(ram);

    cpu.core().write_gpr(10, kDotA);
    cpu.core().write_gpr(11, kDotB);
    cpu.core().write_gpr(12, kOutputBase + kGuardBytes);
    cpu.core().write_gpr(13, kGemmRow0);
    cpu.core().write_gpr(14, kGemmRow1);
    cpu.core().write_gpr(15, kGemmCol0);
    cpu.core().write_gpr(16, kGemmCol1);
    cpu.core().write_gpr(17, 93);
    cpu.core().write_gpr(18, kConvInput);
    cpu.core().write_gpr(19, kConvKernel);
    cpu.core().write_gpr(20, kReluInput);

    std::unique_ptr<ExecutionBackend> backend = make_backend(kind, cpu, bus);
    for (int step = 0; step < 512 && !cpu.core().halted(); ++step) {
        backend->step();
    }

    FinalState out;
    out.halted = cpu.core().halted();
    out.pc = cpu.core().pc();
    out.instret = cpu.core().instret();
    const auto& vector_state = cpu.core().vector();
    out.sew_bytes = read_vector_sew(vector_state);
    out.vl = read_vector_vl(vector_state);
    out.vector_regs = capture_vector_regs(vector_state);
    out.watched = read_buffer<kWatchBytes>(ram, kOutputBase);
    return out;
}

bool test_vector_operator_workload() {
    const FinalState functional = run_case(BackendKind::Functional);
    const FinalState pipeline = run_case(BackendKind::Pipeline);
    const std::array<uint8_t, kWatchBytes> expected = expected_watch();

    if (!expect(functional.halted, "functional vector operator workload should halt")) {
        return false;
    }
    if (!expect(pipeline.halted, "pipeline vector operator workload should halt")) {
        return false;
    }
    if (!expect(functional.pc == pipeline.pc,
                "functional/pipeline final pc should match for vector operator workload")) {
        return false;
    }
    if (!expect(functional.instret == pipeline.instret,
                "functional/pipeline instret should match for vector operator workload")) {
        return false;
    }
    if (!expect(functional.sew_bytes == pipeline.sew_bytes && functional.vl == pipeline.vl,
                "functional/pipeline final vector config should match for vector operator workload")) {
        return false;
    }
    if (!expect(functional.vector_regs == pipeline.vector_regs,
                "functional/pipeline final vector regs should match for vector operator workload")) {
        return false;
    }
    if (!expect(functional.watched == pipeline.watched,
                "functional/pipeline watched memory should match for vector operator workload")) {
        return false;
    }
    if (!expect(functional.watched == expected,
                "functional watched memory should match scalar reference for vector operator workload")) {
        return false;
    }
    if (!expect(pipeline.watched == expected,
                "pipeline watched memory should match scalar reference for vector operator workload")) {
        return false;
    }
    if (!expect(functional.sew_bytes == 2 && functional.vl == 8,
                "final vector config should remain at sew_bytes=2, vl=8 after relu store")) {
        return false;
    }

    return true;
}

}  // namespace

int main() {
    if (!test_vector_operator_workload()) {
        return 1;
    }
    std::puts("vector_operator_smoke: PASS");
    return 0;
}
