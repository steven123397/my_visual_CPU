#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../src/exec/pipeline_commit_boundary.h"
#include "../../src/cpu.h"
#include "../../src/isa/execution_context.h"
#include "../../src/isa/instruction_semantics.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

constexpr uint64_t kMstatusFsDirty = MSTATUS_FS_DIRTY;
constexpr uint64_t kMstatusSd = 1ULL << 63;
constexpr uint64_t kEntry = 0x80000000ULL;

[[maybe_unused]] uint64_t double_to_raw_bits(double value) {
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

[[maybe_unused]] uint64_t float_to_fpr_raw_bits(float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return 0xffffffff00000000ULL | static_cast<uint64_t>(bits);
}

[[maybe_unused]] double raw_bits_to_double(uint64_t bits) {
    double value = 0.0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&value, &bits, sizeof(bits));
    return value;
}

[[maybe_unused]] float raw_bits_to_float(uint64_t bits) {
    const uint32_t single_bits = static_cast<uint32_t>(bits & 0xffffffffULL);
    float value = 0.0f;
    static_assert(sizeof(single_bits) == sizeof(value));
    std::memcpy(&value, &single_bits, sizeof(single_bits));
    return value;
}

[[maybe_unused]] constexpr uint32_t fp_with_rm(uint32_t raw, uint32_t rm) {
    return (raw & ~(0x7U << 12)) | ((rm & 0x7U) << 12);
}

}  // namespace
