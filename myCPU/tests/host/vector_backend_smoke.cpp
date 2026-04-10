#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <type_traits>
#include <utility>

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

constexpr uint64_t kInputA = kEntry + 0x300;
constexpr uint64_t kInputB = kEntry + 0x320;
constexpr uint64_t kOutputBase = kEntry + 0x380;

constexpr uint8_t kRegisterBytes = 16;
constexpr uint8_t kDumpRegisters = 6;
constexpr uint8_t kGuardBytes = 16;
constexpr size_t kDumpBytes = static_cast<size_t>(kRegisterBytes) * kDumpRegisters;
constexpr size_t kTotalBytes = static_cast<size_t>(kGuardBytes) + kDumpBytes + static_cast<size_t>(kGuardBytes);
constexpr uint8_t kGuardPattern = 0xcc;

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
    std::array<uint8_t, kTotalBytes> memory{};
};

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

template <typename VectorStateLike>
std::array<std::array<uint8_t, kRegisterBytes>, 32> capture_vector_regs(const VectorStateLike& vector_state) {
    std::array<std::array<uint8_t, kRegisterBytes>, 32> regs{};
    for (size_t i = 0; i < regs.size(); ++i) {
        regs[i] = vector_state.read_reg(static_cast<uint32_t>(i));
    }
    return regs;
}

void place_register_dump(std::array<uint8_t, kDumpBytes>& dump,
                         size_t register_index,
                         const std::array<uint8_t, kRegisterBytes>& reg) {
    const size_t base = register_index * static_cast<size_t>(kRegisterBytes);
    for (size_t i = 0; i < reg.size(); ++i) {
        dump[base + i] = reg[i];
    }
}

std::array<uint8_t, kDumpBytes> expected_dump() {
    const std::array<int16_t, 4> a{1, -2, 30000, -7};
    const std::array<int16_t, 4> b{3, 5, -2, -7};

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

    std::array<uint8_t, kDumpBytes> dump{};
    place_register_dump(dump, 0, pack_i16x8(a));
    place_register_dump(dump, 1, pack_i16x8(b));
    place_register_dump(dump, 2, pack_i16x8(add));
    place_register_dump(dump, 3, pack_i16x8(mul));
    place_register_dump(dump, 4, pack_i16x8(vmax));
    place_register_dump(dump, 5, pack_dot_i64(static_cast<int64_t>(dot_acc)));
    return dump;
}

std::array<std::array<uint8_t, kRegisterBytes>, 32> expected_vector_regs() {
    const std::array<int16_t, 4> a{1, -2, 30000, -7};
    const std::array<int16_t, 4> b{3, 5, -2, -7};

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

    std::array<std::array<uint8_t, kRegisterBytes>, 32> regs{};
    regs[1] = pack_i16x8(a);
    regs[2] = pack_i16x8(b);
    regs[3] = pack_i16x8(add);
    regs[4] = pack_i16x8(mul);
    regs[5] = pack_i16x8(vmax);
    regs[6] = pack_dot_i64(static_cast<int64_t>(dot_acc));
    return regs;
}

std::array<uint8_t, kTotalBytes> expected_memory() {
    std::array<uint8_t, kTotalBytes> out{};
    out.fill(kGuardPattern);
    const std::array<uint8_t, kDumpBytes> dump = expected_dump();
    for (size_t i = 0; i < dump.size(); ++i) {
        out[static_cast<size_t>(kGuardBytes) + i] = dump[i];
    }
    return out;
}

void load_program(Ram& ram) {
    const std::array<uint32_t, 15> program{
        encode_vsetcfg(2, 4),
        encode_vle(1, 10),
        encode_vle(2, 11),
        encode_vv(0x00, 3, 1, 2),
        encode_vv(0x20, 4, 1, 2),
        encode_vv(0x21, 5, 1, 2),
        encode_vv(0x22, 6, 1, 2),
        encode_vsetcfg(1, 16),
        encode_vse(1, 12, 0),
        encode_vse(2, 12, 16),
        encode_vse(3, 12, 32),
        encode_vse(4, 12, 48),
        encode_vse(5, 12, 64),
        encode_vse(6, 12, 80),
        kEcall,
    };

    for (size_t i = 0; i < program.size(); ++i) {
        write32(ram, kEntry + static_cast<uint64_t>(i * 4), program[i]);
    }
}

FinalState run_backend(BackendKind kind) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    const std::array<int16_t, 4> a{1, -2, 30000, -7};
    const std::array<int16_t, 4> b{3, 5, -2, -7};
    write_buffer(ram, kInputA, pack_i16x8(a));
    write_buffer(ram, kInputB, pack_i16x8(b));

    ram.fill(kOutputBase - kGuardBytes, kGuardPattern, kTotalBytes);

    load_program(ram);

    cpu.core().write_gpr(10, kInputA);
    cpu.core().write_gpr(11, kInputB);
    cpu.core().write_gpr(12, kOutputBase);
    cpu.core().write_gpr(17, 93);

    std::unique_ptr<ExecutionBackend> backend = make_backend(kind, cpu, bus);
    const int max_steps = (kind == BackendKind::Pipeline) ? 1024 : 256;
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
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
    out.memory = read_buffer<kTotalBytes>(ram, kOutputBase - kGuardBytes);
    return out;
}

bool compare_final_states(const FinalState& functional,
                          const FinalState& pipeline) {
    if (!expect(functional.halted, "functional vector backend scenario should halt")) {
        return false;
    }
    if (!expect(pipeline.halted, "pipeline vector backend scenario should halt")) {
        return false;
    }
    if (!expect(functional.pc == pipeline.pc, "functional/pipeline final pc should match")) {
        return false;
    }
    if (!expect(functional.instret == pipeline.instret, "functional/pipeline final instret should match")) {
        return false;
    }
    if (!expect(functional.sew_bytes == pipeline.sew_bytes,
                "functional/pipeline final sew_bytes should match")) {
        return false;
    }
    if (!expect(functional.vl == pipeline.vl, "functional/pipeline final vl should match")) {
        return false;
    }
    if (!expect(functional.vector_regs == pipeline.vector_regs,
                "functional/pipeline final vector registers should match")) {
        return false;
    }
    if (!expect(functional.memory == pipeline.memory,
                "functional/pipeline final memory dump should match")) {
        return false;
    }

    const std::array<std::array<uint8_t, kRegisterBytes>, 32> expected_regs = expected_vector_regs();
    const std::array<uint8_t, kTotalBytes> expected = expected_memory();
    if (!expect(functional.vector_regs == expected_regs,
                "functional backend final vector registers should match expected values")) {
        return false;
    }
    if (!expect(pipeline.vector_regs == expected_regs,
                "pipeline backend final vector registers should match expected values")) {
        return false;
    }
    if (!expect(functional.memory == expected,
                "functional backend final vector dump should match expected values")) {
        return false;
    }
    if (!expect(pipeline.memory == expected,
                "pipeline backend final vector dump should match expected values")) {
        return false;
    }

    if (!expect(functional.sew_bytes == 1,
                "final vector configuration should preserve sew_bytes=1 after dump mode")) {
        return false;
    }
    if (!expect(functional.vl == 16,
                "final vector configuration should preserve vl=16 after dump mode")) {
        return false;
    }
    return true;
}

}  // namespace

int main() {
    const FinalState functional = run_backend(BackendKind::Functional);
    const FinalState pipeline = run_backend(BackendKind::Pipeline);
    if (!compare_final_states(functional, pipeline)) {
        return 1;
    }
    return 0;
}
