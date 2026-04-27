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

constexpr uint64_t kInputBase = kEntry + 0x200;
constexpr uint64_t kKernelBase = kEntry + 0x210;
constexpr uint64_t kZeroBase = kEntry + 0x220;
constexpr uint64_t kWatchBase = kEntry + 0x240;

constexpr uint8_t kRegisterBytes = 16;
constexpr uint8_t kGuardBytes = 16;
constexpr size_t kConvBytes = 12;
constexpr size_t kReluBytes = 12;
constexpr size_t kWatchBytes = static_cast<size_t>(kGuardBytes) + kConvBytes +
                               kReluBytes + static_cast<size_t>(kGuardBytes);
constexpr uint8_t kGuardPattern = 0xa7;

constexpr int32_t kConvOutOffset0 = 0;
constexpr int32_t kConvOutOffset1 = 4;
constexpr int32_t kConvOutOffset2 = 8;
constexpr int32_t kReluOutOffset = static_cast<int32_t>(kConvBytes);

enum class BackendKind : uint8_t {
    Functional,
    Pipeline,
};

struct Sample {
    const char* name;
    std::array<int8_t, 6> input;
    std::array<int8_t, 4> kernel;
    std::array<int32_t, 3> expected_conv;
    std::array<int32_t, 3> expected_relu;
};

struct FinalState {
    bool halted{false};
    uint64_t pc{0};
    uint64_t instret{0};
    uint8_t sew_bytes{0};
    uint8_t vl{0};
    std::array<std::array<uint8_t, kRegisterBytes>, 32> vector_regs{};
    std::array<uint8_t, kWatchBytes> watched{};
    ExecutionProfileSnapshot profile{};
};

template <typename>
struct always_false : std::false_type {};

template <typename T, typename = void>
struct has_sew_field : std::false_type {};

template <typename T>
struct has_sew_field<T, std::void_t<decltype(std::declval<const T&>().sew_bytes)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_sew_getter : std::false_type {};

template <typename T>
struct has_sew_getter<T, std::void_t<decltype(std::declval<const T&>().sew_bytes())>>
    : std::true_type {};

template <typename T, typename = void>
struct has_vl_field : std::false_type {};

template <typename T>
struct has_vl_field<T, std::void_t<decltype(std::declval<const T&>().vl)>>
    : std::true_type {};

template <typename T, typename = void>
struct has_vl_getter : std::false_type {};

template <typename T>
struct has_vl_getter<T, std::void_t<decltype(std::declval<const T&>().vl())>>
    : std::true_type {};

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

const ExecutionMemoryRegionEntry* find_profile_region_kind(const ExecutionProfileSnapshot& profile,
                                                           const char* kind) {
    for (const ExecutionMemoryRegionEntry& entry : profile.memory_regions) {
        if (entry.kind == kind && entry.accesses != 0) {
            return &entry;
        }
    }
    return nullptr;
}

bool profile_has_region_kind(const ExecutionProfileSnapshot& profile, const char* kind) {
    return find_profile_region_kind(profile, kind) != nullptr;
}

bool profile_has_shadow_cache_ram_signal(const ExecutionProfileSnapshot& profile) {
    const ExecutionShadowCacheSnapshot& shadow = profile.shadow_cache;
    const ExecutionMemoryRegionEntry* ram_region = find_profile_region_kind(profile, "ram");
    if (ram_region == nullptr) {
        return false;
    }
    return shadow.line_size_bytes == 64 &&
           shadow.capacity_lines == 64 &&
           shadow.resident_lines != 0 &&
           shadow.line_accesses != 0 &&
           shadow.hits != 0 &&
           shadow.misses != 0 &&
           shadow.evictions == 0 &&
           shadow.bypasses == 0 &&
           ram_region->shadow_cache_line_accesses == shadow.line_accesses &&
           ram_region->shadow_cache_hits == shadow.hits &&
           ram_region->shadow_cache_misses == shadow.misses &&
           ram_region->shadow_cache_evictions == shadow.evictions &&
           ram_region->shadow_cache_bypasses == shadow.bypasses;
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

std::array<uint8_t, 6> pack_i8x6(const std::array<int8_t, 6>& values) {
    std::array<uint8_t, 6> out{};
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

std::array<uint8_t, 12> pack_i32x3(const std::array<int32_t, 3>& values) {
    std::array<uint8_t, 12> out{};
    for (size_t i = 0; i < values.size(); ++i) {
        const uint32_t bits = static_cast<uint32_t>(values[i]);
        for (size_t b = 0; b < sizeof(uint32_t); ++b) {
            out[i * sizeof(uint32_t) + b] =
                static_cast<uint8_t>((bits >> (b * 8U)) & 0xFFU);
        }
    }
    return out;
}

std::array<uint8_t, kWatchBytes> expected_watch(const Sample& sample) {
    std::array<uint8_t, kWatchBytes> out{};
    out.fill(kGuardPattern);
    size_t cursor = kGuardBytes;

    const auto conv = pack_i32x3(sample.expected_conv);
    for (uint8_t value : conv) {
        out[cursor++] = value;
    }

    const auto relu = pack_i32x3(sample.expected_relu);
    for (uint8_t value : relu) {
        out[cursor++] = value;
    }

    return out;
}

void load_program(Ram& ram) {
    std::vector<uint32_t> program;
    program.reserve(24);

    program.push_back(encode_vsetcfg(1, 4));
    program.push_back(encode_vle(1, 10, 0));
    program.push_back(encode_vle(2, 11, 0));
    program.push_back(encode_vv(0x22, 3, 1, 2));
    program.push_back(encode_vsetcfg(4, 1));
    program.push_back(encode_vse(3, 12, kConvOutOffset0));

    program.push_back(encode_vsetcfg(1, 4));
    program.push_back(encode_vle(1, 10, 1));
    program.push_back(encode_vle(2, 11, 0));
    program.push_back(encode_vv(0x22, 3, 1, 2));
    program.push_back(encode_vsetcfg(4, 1));
    program.push_back(encode_vse(3, 12, kConvOutOffset1));

    program.push_back(encode_vsetcfg(1, 4));
    program.push_back(encode_vle(1, 10, 2));
    program.push_back(encode_vle(2, 11, 0));
    program.push_back(encode_vv(0x22, 3, 1, 2));
    program.push_back(encode_vsetcfg(4, 1));
    program.push_back(encode_vse(3, 12, kConvOutOffset2));

    program.push_back(encode_vsetcfg(4, 3));
    program.push_back(encode_vle(4, 12));
    program.push_back(encode_vle(0, 12));
    program.push_back(encode_vle(31, 14));
    program.push_back(encode_vv(0x21, 5, 4, 31));
    program.push_back(encode_vse(5, 13));

    program.push_back(kEcall);

    uint64_t pc = kEntry;
    for (uint32_t raw : program) {
        write32(ram, pc, raw);
        pc += 4;
    }
}

void seed_memory(Ram& ram, const Sample& sample) {
    std::array<uint8_t, kWatchBytes> watch_init{};
    watch_init.fill(kGuardPattern);
    write_buffer(ram, kInputBase, pack_i8x6(sample.input));
    write_buffer(ram, kKernelBase, pack_i8x4(sample.kernel));
    write_buffer(ram, kZeroBase, std::array<uint8_t, kRegisterBytes>{});
    write_buffer(ram, kWatchBase, watch_init);
}

FinalState run_case(const Sample& sample, BackendKind kind) {
    Ram ram;
    Bus bus(ram);
    CPU cpu;
    cpu_init(cpu, kEntry);

    load_program(ram);
    seed_memory(ram, sample);

    cpu.core().write_gpr(10, kInputBase);
    cpu.core().write_gpr(11, kKernelBase);
    cpu.core().write_gpr(12, kWatchBase + kGuardBytes);
    cpu.core().write_gpr(13, kWatchBase + kGuardBytes + kConvBytes);
    cpu.core().write_gpr(14, kZeroBase);
    cpu.core().write_gpr(17, 93);

    std::unique_ptr<ExecutionBackend> backend = make_backend(kind, cpu, bus);
    for (int step = 0; step < 256 && !cpu.core().halted(); ++step) {
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
    out.watched = read_buffer<kWatchBytes>(ram, kWatchBase);
    out.profile = backend->debug_snapshot().profile;
    return out;
}

bool test_vector_cnn_sample(const Sample& sample) {
    const FinalState functional = run_case(sample, BackendKind::Functional);
    const FinalState pipeline = run_case(sample, BackendKind::Pipeline);
    const auto expected = expected_watch(sample);

    if (!expect(functional.halted, "functional vector CNN smoke should halt")) {
        return false;
    }
    if (!expect(pipeline.halted, "pipeline vector CNN smoke should halt")) {
        return false;
    }
    if (!expect(functional.pc == pipeline.pc,
                "functional/pipeline final pc should match for vector CNN smoke")) {
        return false;
    }
    if (!expect(functional.instret == pipeline.instret,
                "functional/pipeline instret should match for vector CNN smoke")) {
        return false;
    }
    if (!expect(functional.sew_bytes == pipeline.sew_bytes && functional.vl == pipeline.vl,
                "functional/pipeline final vector config should match for vector CNN smoke")) {
        return false;
    }
    if (!expect(functional.vector_regs == pipeline.vector_regs,
                "functional/pipeline final vector regs should match for vector CNN smoke")) {
        return false;
    }
    if (!expect(functional.watched == pipeline.watched,
                "functional/pipeline watched memory should match for vector CNN smoke")) {
        return false;
    }
    if (!expect(!pipeline.profile.hot_paths.empty(),
                "pipeline vector CNN smoke should expose hot-path profile entries")) {
        return false;
    }
    if (!expect(profile_has_region_kind(pipeline.profile, "ram"),
                "pipeline vector CNN smoke should expose RAM memory-region profile entries")) {
        return false;
    }
    if (!expect(profile_has_shadow_cache_ram_signal(pipeline.profile),
                "pipeline vector CNN smoke should expose RAM shadow-cache profile signal")) {
        return false;
    }
    if (!expect(functional.watched == expected,
                sample.name)) {
        return false;
    }
    if (!expect(functional.sew_bytes == 4 && functional.vl == 3,
                "vector CNN smoke should finish with sew_bytes=4 and vl=3")) {
        return false;
    }
    return true;
}

bool test_vector_cnn_workloads() {
    const std::array<Sample, 2> samples{{
        Sample{
            "mixed SEW/VL conv->relu chain should match expected int32 outputs",
            {2, -1, 3, 4, -2, 1},
            {1, 0, -1, 2},
            {7, -9, 7},
            {7, 0, 7},
        },
        Sample{
            "all-negative conv outputs should clamp to zero after relu",
            {1, 1, 1, 1, 1, 1},
            {-1, -1, -1, -1},
            {-4, -4, -4},
            {0, 0, 0},
        },
    }};

    for (const Sample& sample : samples) {
        if (!test_vector_cnn_sample(sample)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!test_vector_cnn_workloads()) {
        return 1;
    }
    std::puts("vector_cnn_smoke: PASS");
    return 0;
}
