#include <array>
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <utility>

#include "../../include/platform_mmio.h"
#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/exec/functional_backend.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint32_t kEcall = 0x00000073U;

constexpr uint64_t kLoadStoreSrcFull = kEntry + 0x200;
constexpr uint64_t kLoadStoreSrcSmall = kEntry + 0x220;
constexpr uint64_t kLoadStoreDstPartial = kEntry + 0x240;
constexpr uint64_t kLoadStoreDstFull = kEntry + 0x260;

constexpr uint64_t kArithSrcA = kEntry + 0x300;
constexpr uint64_t kArithSrcB = kEntry + 0x320;
constexpr uint64_t kArithOutAdd = kEntry + 0x340;
constexpr uint64_t kArithOutMul = kEntry + 0x360;
constexpr uint64_t kArithOutMax = kEntry + 0x380;
constexpr uint64_t kArithOutDot = kEntry + 0x3a0;

constexpr uint8_t kRegisterBytes = 16;

template <typename>
struct always_false : std::false_type {};

template <typename T, typename = void>
struct has_sew_field : std::false_type {};

template <typename T>
struct has_sew_field<T, std::void_t<decltype(std::declval<const T&>().sew_bytes)>> : std::true_type {};

template <typename T, typename = void>
struct has_sew_getter : std::false_type {};

template <typename T>
struct has_sew_getter<T, std::void_t<decltype(std::declval<const T&>().sew_bytes())>> : std::true_type {};

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

uint32_t encode_vsetcfg_from_code(uint8_t sew_code, uint8_t vl) {
    const uint8_t funct7 = static_cast<uint8_t>(0x40U | (sew_code & 0x7U));
    const uint8_t rs2 = static_cast<uint8_t>(vl - 1);
    return encode_rtype(0x57, 7, funct7, 0, 0, rs2);
}

uint32_t encode_vsetcfg(uint8_t sew_bytes, uint8_t vl) {
    return encode_vsetcfg_from_code(sew_code_from_bytes(sew_bytes), vl);
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

bool run_until_halt(FunctionalBackend& backend, CPU& cpu, int max_steps) {
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
        backend.step();
    }
    return cpu.core().halted();
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

bool test_vsetcfg_valid() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

    write32(ram, kEntry, encode_vsetcfg(4, 4));
    cpu_step(cpu, bus);

    if (!expect(cpu.core().pc() == kEntry + 4, "valid vsetcfg should advance pc")) {
        return false;
    }

    const auto& vector_state = cpu.core().vector();
    if (!expect(read_vector_sew(vector_state) == 4, "vsetcfg valid test should set sew_bytes=4")) {
        return false;
    }
    if (!expect(read_vector_vl(vector_state) == 4, "vsetcfg valid test should set vl=4")) {
        return false;
    }
    return true;
}

bool expect_illegal_vsetcfg(uint32_t raw, const char* tag) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);
    cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

    write32(ram, kEntry, raw);

    cpu_step(cpu, bus);

    if (!expect(cpu.core().pc() == kTrapVector, tag)) {
        return false;
    }
    if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2, "invalid vsetcfg should raise illegal-instruction cause")) {
        return false;
    }
    if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == raw, "invalid vsetcfg should preserve raw instruction in mtval")) {
        return false;
    }
    return true;
}

bool test_vle_vse() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const std::array<uint8_t, kRegisterBytes> src_full{
        0x10, 0x11, 0x12, 0x13,
        0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b,
        0x1c, 0x1d, 0x1e, 0x1f,
    };
    const std::array<uint8_t, 4> src_small{0xa1, 0xb2, 0xc3, 0xd4};
    const std::array<uint8_t, kRegisterBytes> fill_a5{
        0xa5, 0xa5, 0xa5, 0xa5,
        0xa5, 0xa5, 0xa5, 0xa5,
        0xa5, 0xa5, 0xa5, 0xa5,
        0xa5, 0xa5, 0xa5, 0xa5,
    };

    write_buffer(ram, kLoadStoreSrcFull, src_full);
    write_buffer(ram, kLoadStoreSrcSmall, src_small);
    write_buffer(ram, kLoadStoreDstPartial, fill_a5);
    write_buffer(ram, kLoadStoreDstFull, fill_a5);

    write32(ram, kEntry + 0, encode_vsetcfg(1, 16));
    write32(ram, kEntry + 4, encode_vle(3, 10));
    write32(ram, kEntry + 8, encode_vsetcfg(1, 4));
    write32(ram, kEntry + 12, encode_vle(3, 11));
    write32(ram, kEntry + 16, encode_vse(3, 12));
    write32(ram, kEntry + 20, encode_vsetcfg(1, 16));
    write32(ram, kEntry + 24, encode_vse(3, 13));
    write32(ram, kEntry + 28, kEcall);

    cpu.core().write_gpr(10, kLoadStoreSrcFull);
    cpu.core().write_gpr(11, kLoadStoreSrcSmall);
    cpu.core().write_gpr(12, kLoadStoreDstPartial);
    cpu.core().write_gpr(13, kLoadStoreDstFull);
    cpu.core().write_gpr(17, 93);

    FunctionalBackend backend(cpu, bus);
    if (!expect(run_until_halt(backend, cpu, 128), "vle/vse smoke should halt")) {
        return false;
    }

    const std::array<uint8_t, kRegisterBytes> partial = read_buffer<kRegisterBytes>(ram, kLoadStoreDstPartial);
    const std::array<uint8_t, kRegisterBytes> full = read_buffer<kRegisterBytes>(ram, kLoadStoreDstFull);

    for (size_t i = 0; i < src_small.size(); ++i) {
        if (!expect(partial[i] == src_small[i], "vse partial store should write active bytes")) {
            return false;
        }
        if (!expect(full[i] == src_small[i], "vle should copy source bytes into destination vector")) {
            return false;
        }
    }
    for (size_t i = src_small.size(); i < kRegisterBytes; ++i) {
        if (!expect(partial[i] == 0xa5, "vse should not clobber bytes outside vl*sew range")) {
            return false;
        }
        if (!expect(full[i] == 0, "vle should clear non-active vector bytes before full-width dump")) {
            return false;
        }
    }

    return true;
}

std::array<uint8_t, kRegisterBytes> pack_i16x8(const std::array<int16_t, 4>& values) {
    std::array<uint8_t, kRegisterBytes> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        const uint16_t bits = static_cast<uint16_t>(values[i]);
        out[i * 2] = static_cast<uint8_t>(bits & 0xFFU);
        out[i * 2 + 1] = static_cast<uint8_t>((bits >> 8) & 0xFFU);
    }
    return out;
}

std::array<uint8_t, kRegisterBytes> pack_dot_i64(int64_t value) {
    std::array<uint8_t, kRegisterBytes> out{};
    const uint64_t bits = static_cast<uint64_t>(value);
    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        out[i] = static_cast<uint8_t>((bits >> (i * 8)) & 0xFFU);
    }
    return out;
}

bool test_vector_alu() {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const std::array<int16_t, 4> a{1, -2, 30000, -7};
    const std::array<int16_t, 4> b{3, 5, -2, -7};

    write_buffer(ram, kArithSrcA, pack_i16x8(a));
    write_buffer(ram, kArithSrcB, pack_i16x8(b));

    write32(ram, kEntry + 0, encode_vsetcfg(2, 4));
    write32(ram, kEntry + 4, encode_vle(1, 10));
    write32(ram, kEntry + 8, encode_vle(2, 11));
    write32(ram, kEntry + 12, encode_vv(0x00, 3, 1, 2));
    write32(ram, kEntry + 16, encode_vv(0x20, 4, 1, 2));
    write32(ram, kEntry + 20, encode_vv(0x21, 5, 1, 2));
    write32(ram, kEntry + 24, encode_vv(0x22, 6, 1, 2));
    write32(ram, kEntry + 28, encode_vsetcfg(1, 16));
    write32(ram, kEntry + 32, encode_vse(3, 12));
    write32(ram, kEntry + 36, encode_vse(4, 13));
    write32(ram, kEntry + 40, encode_vse(5, 14));
    write32(ram, kEntry + 44, encode_vse(6, 15));
    write32(ram, kEntry + 48, kEcall);

    cpu.core().write_gpr(10, kArithSrcA);
    cpu.core().write_gpr(11, kArithSrcB);
    cpu.core().write_gpr(12, kArithOutAdd);
    cpu.core().write_gpr(13, kArithOutMul);
    cpu.core().write_gpr(14, kArithOutMax);
    cpu.core().write_gpr(15, kArithOutDot);
    cpu.core().write_gpr(17, 93);

    FunctionalBackend backend(cpu, bus);
    if (!expect(run_until_halt(backend, cpu, 192), "vector alu smoke should halt")) {
        return false;
    }

    std::array<int16_t, 4> add{};
    std::array<int16_t, 4> mul{};
    std::array<int16_t, 4> vmax{};
    __int128 dot_acc = 0;

    for (size_t i = 0; i < a.size(); ++i) {
        add[i] = static_cast<int16_t>(static_cast<int32_t>(a[i]) + static_cast<int32_t>(b[i]));
        mul[i] = static_cast<int16_t>(static_cast<int32_t>(a[i]) * static_cast<int32_t>(b[i]));
        vmax[i] = (a[i] >= b[i]) ? a[i] : b[i];
        dot_acc += static_cast<__int128>(a[i]) * static_cast<__int128>(b[i]);
    }

    const std::array<uint8_t, kRegisterBytes> expected_add = pack_i16x8(add);
    const std::array<uint8_t, kRegisterBytes> expected_mul = pack_i16x8(mul);
    const std::array<uint8_t, kRegisterBytes> expected_max = pack_i16x8(vmax);
    const std::array<uint8_t, kRegisterBytes> expected_dot = pack_dot_i64(static_cast<int64_t>(dot_acc));

    const std::array<uint8_t, kRegisterBytes> out_add = read_buffer<kRegisterBytes>(ram, kArithOutAdd);
    const std::array<uint8_t, kRegisterBytes> out_mul = read_buffer<kRegisterBytes>(ram, kArithOutMul);
    const std::array<uint8_t, kRegisterBytes> out_max = read_buffer<kRegisterBytes>(ram, kArithOutMax);
    const std::array<uint8_t, kRegisterBytes> out_dot = read_buffer<kRegisterBytes>(ram, kArithOutDot);

    if (!expect(out_add == expected_add, "vadd.vv should match element-wise modular add result")) {
        return false;
    }
    if (!expect(out_mul == expected_mul, "vmul.vv should match element-wise truncated multiply result")) {
        return false;
    }
    if (!expect(out_max == expected_max, "vmax.vv should match signed element-wise max result")) {
        return false;
    }
    if (!expect(out_dot == expected_dot, "vdot.vv should write low-64 reduction result and clear high bytes")) {
        return false;
    }

    return true;
}

}  // namespace

int main() {
    if (!test_vsetcfg_valid()) {
        return 1;
    }
    if (!expect_illegal_vsetcfg(encode_vsetcfg_from_code(2, 4),
                                "invalid sew encoding should trap to mtvec")) {
        return 1;
    }
    if (!expect_illegal_vsetcfg(encode_vsetcfg(8, 3),
                                "vl overflow for sew=8 should trap to mtvec")) {
        return 1;
    }
    if (!test_vle_vse()) {
        return 1;
    }
    if (!test_vector_alu()) {
        return 1;
    }
    return 0;
}
