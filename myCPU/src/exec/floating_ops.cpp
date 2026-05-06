#include "floating_ops.h"

#include <cfenv>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>

#include "../arch/csr_file.h"
#include "../isa/effects.h"
#include "memory_ops.h"

namespace {

constexpr uint64_t CAUSE_ILLEGAL_INSN = 2;
constexpr uint64_t FCSR_FRM_RNE = 0;
constexpr uint64_t FCSR_FRM_RTZ = 1;
constexpr uint64_t FCSR_FRM_RDN = 2;
constexpr uint64_t FCSR_FRM_RUP = 3;
constexpr uint64_t FCSR_FRM_RMM = 4;
constexpr uint64_t FCSR_FRM_DYN = 7;
constexpr uint8_t OPCODE_FP = 0x53;
constexpr uint8_t OPCODE_FMADD = 0x43;
constexpr uint8_t OPCODE_FMSUB = 0x47;
constexpr uint8_t OPCODE_FNMSUB = 0x4b;
constexpr uint8_t OPCODE_FNMADD = 0x4f;
constexpr uint8_t FUNCT7_FEQ_D = 0x51;
constexpr uint8_t FUNCT7_FADD_S = 0x00;
constexpr uint8_t FUNCT7_FADD_D = 0x01;
constexpr uint8_t FUNCT7_FSUB_S = 0x04;
constexpr uint8_t FUNCT7_FSUB_D = 0x05;
constexpr uint8_t FUNCT7_FMUL_S = 0x08;
constexpr uint8_t FUNCT7_FMUL_D = 0x09;
constexpr uint8_t FUNCT7_FDIV_S = 0x0c;
constexpr uint8_t FUNCT7_FDIV_D = 0x0d;
constexpr uint8_t FUNCT7_FMINMAX_D = 0x15;
constexpr uint8_t FUNCT7_FMINMAX_S = 0x14;
constexpr uint8_t FUNCT7_FSQRT_S = 0x2c;
constexpr uint8_t FUNCT7_FSQRT_D = 0x2d;
constexpr uint8_t FUNCT7_FSGNJ_S = 0x10;
constexpr uint8_t FUNCT7_FCVT_S_D = 0x20;
constexpr uint8_t FUNCT7_FCVT_D_S = 0x21;
constexpr uint8_t FUNCT7_FCVT_S_INT = 0x68;
constexpr uint8_t FUNCT7_FCVT_L_FROM_S = 0x60;
constexpr uint8_t FUNCT7_FCVT_L_FROM_D = 0x61;
constexpr uint8_t FUNCT7_FCVT_D_INT = 0x69;
constexpr uint8_t FUNCT7_FMV_D = 0x11;
constexpr uint8_t FUNCT7_FMV_X_W = 0x70;
constexpr uint8_t FUNCT7_FCLASS_S = 0x70;
constexpr uint8_t FUNCT7_FMV_X_D = 0x71;
constexpr uint8_t FUNCT7_FMV_W_X = 0x78;
constexpr uint8_t FUNCT7_FMV_D_X = 0x79;
constexpr uint8_t FUNCT7_FEQ_S = 0x50;
constexpr uint64_t FCSR_FLAG_NX = 1ULL << 0;
constexpr uint64_t FCSR_FLAG_UF = 1ULL << 1;
constexpr uint64_t FCSR_FLAG_OF = 1ULL << 2;
constexpr uint64_t FCSR_FLAG_NV = 1ULL << 4;
constexpr uint64_t FCSR_FLAG_DZ = 1ULL << 3;
constexpr uint64_t FCSR_FFLAGS_MASK = 0x1FULL;
constexpr uint64_t FCSR_FRM_MASK = 0x7ULL << 5;

InsnEffects illegal_floating_instruction(uint32_t raw) {
    InsnEffects effects;
    effects.trap.valid = true;
    effects.trap.cause = CAUSE_ILLEGAL_INSN;
    effects.trap.tval = raw;
    effects.retired = false;
    return effects;
}

uint64_t double_to_raw_bits(double value) {
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

uint32_t float_to_raw_bits(float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

double raw_bits_to_double(uint64_t bits) {
    double value = 0.0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

float raw_bits_to_float(uint64_t bits) {
    const uint32_t narrowed = static_cast<uint32_t>(bits);
    float value = 0.0f;
    static_assert(sizeof(narrowed) == sizeof(value));
    std::memcpy(&value, &narrowed, sizeof(narrowed));
    return value;
}

uint64_t box_single_result(float value) {
    return 0xffffffff00000000ULL | static_cast<uint64_t>(float_to_raw_bits(value));
}

bool is_single_nan_bits(uint64_t bits) {
    const uint32_t raw = static_cast<uint32_t>(bits);
    return (raw & 0x7f800000U) == 0x7f800000U && (raw & 0x007fffffU) != 0;
}

bool is_double_nan_bits(uint64_t bits) {
    return (bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL &&
           (bits & 0x000fffffffffffffULL) != 0;
}

bool is_single_signaling_nan_bits(uint64_t bits) {
    const uint32_t raw = static_cast<uint32_t>(bits);
    return is_single_nan_bits(bits) && (raw & 0x00400000U) == 0;
}

bool is_double_signaling_nan_bits(uint64_t bits) {
    return is_double_nan_bits(bits) && (bits & 0x0008000000000000ULL) == 0;
}

uint64_t canonical_single_nan_bits() {
    return 0xffffffff7fc00000ULL;
}

uint64_t canonical_double_nan_bits() {
    return 0x7ff8000000000000ULL;
}

uint64_t classify_single(float value) {
    if (std::isnan(value)) {
        const uint32_t bits = float_to_raw_bits(value);
        return (bits & (1U << 22)) != 0 ? (1ULL << 9) : (1ULL << 8);
    }
    if (std::isinf(value)) {
        return std::signbit(value) ? (1ULL << 0) : (1ULL << 7);
    }
    if (value == 0.0f) {
        return std::signbit(value) ? (1ULL << 3) : (1ULL << 4);
    }
    const bool negative = std::signbit(value);
    const bool subnormal = std::fpclassify(value) == FP_SUBNORMAL;
    if (subnormal) {
        return negative ? (1ULL << 2) : (1ULL << 5);
    }
    return negative ? (1ULL << 1) : (1ULL << 6);
}

uint64_t classify_double(double value) {
    if (std::isnan(value)) {
        const uint64_t bits = double_to_raw_bits(value);
        return (bits & (1ULL << 51)) != 0 ? (1ULL << 9) : (1ULL << 8);
    }
    if (std::isinf(value)) {
        return std::signbit(value) ? (1ULL << 0) : (1ULL << 7);
    }
    if (value == 0.0) {
        return std::signbit(value) ? (1ULL << 3) : (1ULL << 4);
    }
    const bool negative = std::signbit(value);
    const bool subnormal = std::fpclassify(value) == FP_SUBNORMAL;
    if (subnormal) {
        return negative ? (1ULL << 2) : (1ULL << 5);
    }
    return negative ? (1ULL << 1) : (1ULL << 6);
}

bool set_rounding_mode(uint64_t rm, int& previous_mode) {
    switch (rm) {
    case FCSR_FRM_RNE:
        previous_mode = std::fegetround();
        return std::fesetround(FE_TONEAREST) == 0;
    case FCSR_FRM_RTZ:
        previous_mode = std::fegetround();
        return std::fesetround(FE_TOWARDZERO) == 0;
    case FCSR_FRM_RDN:
        previous_mode = std::fegetround();
        return std::fesetround(FE_DOWNWARD) == 0;
    case FCSR_FRM_RUP:
        previous_mode = std::fegetround();
        return std::fesetround(FE_UPWARD) == 0;
    default:
        return false;
    }
}

bool resolve_rounding_mode(const Insn& insn, uint64_t frm, uint64_t& resolved_rm) {
    const uint64_t encoded_rm = insn.funct3;
    if (encoded_rm == FCSR_FRM_DYN) {
        if (frm > FCSR_FRM_RMM) {
            return false;
        }
        resolved_rm = frm;
        return true;
    }
    if (encoded_rm > FCSR_FRM_RMM) {
        return false;
    }
    resolved_rm = encoded_rm;
    return true;
}

bool compute_binary_double_result(const Insn& insn,
                                  uint64_t frm,
                                  uint64_t rs1v,
                                  uint64_t rs2v,
                                  const std::function<double(double, double)>& op,
                                  uint64_t& result_bits,
                                  uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const double lhs = raw_bits_to_double(rs1v);
    const double rhs = raw_bits_to_double(rs2v);
    const double result = op(lhs, rhs);
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = double_to_raw_bits(result);
    return true;
}

bool compute_binary_single_result(const Insn& insn,
                                  uint64_t frm,
                                  uint64_t rs1v,
                                  uint64_t rs2v,
                                  const std::function<float(float, float)>& op,
                                  uint64_t& result_bits,
                                  uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const float lhs = raw_bits_to_float(rs1v);
    const float rhs = raw_bits_to_float(rs2v);
    const float result = op(lhs, rhs);
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = box_single_result(result);
    return true;
}

bool compute_unary_double_result(const Insn& insn,
                                 uint64_t frm,
                                 uint64_t rs1v,
                                 const std::function<double(double)>& op,
                                 uint64_t& result_bits,
                                 uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const double lhs = raw_bits_to_double(rs1v);
    const double result = op(lhs);
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = double_to_raw_bits(result);
    return true;
}

bool compute_unary_single_result(const Insn& insn,
                                 uint64_t frm,
                                 uint64_t rs1v,
                                 const std::function<float(float)>& op,
                                 uint64_t& result_bits,
                                 uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const float lhs = raw_bits_to_float(rs1v);
    const float result = op(lhs);
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = box_single_result(result);
    return true;
}

bool compute_ternary_double_result(const Insn& insn,
                                   uint64_t frm,
                                   uint64_t rs1v,
                                   uint64_t rs2v,
                                   uint64_t rs3v,
                                   const std::function<double(double, double, double)>& op,
                                   uint64_t& result_bits,
                                   uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const double lhs = raw_bits_to_double(rs1v);
    const double rhs = raw_bits_to_double(rs2v);
    const double addend = raw_bits_to_double(rs3v);
    const double result = op(lhs, rhs, addend);
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = double_to_raw_bits(result);
    return true;
}

bool compute_ternary_single_result(const Insn& insn,
                                   uint64_t frm,
                                   uint64_t rs1v,
                                   uint64_t rs2v,
                                   uint64_t rs3v,
                                   const std::function<float(float, float, float)>& op,
                                   uint64_t& result_bits,
                                   uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const float lhs = raw_bits_to_float(rs1v);
    const float rhs = raw_bits_to_float(rs2v);
    const float addend = raw_bits_to_float(rs3v);
    const float result = op(lhs, rhs, addend);
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = box_single_result(result);
    return true;
}

uint64_t update_fcsr_flags(uint64_t current_fcsr, uint64_t exception_flags) {
    return (current_fcsr & ~FCSR_FFLAGS_MASK) | ((current_fcsr | exception_flags) & FCSR_FFLAGS_MASK);
}

uint64_t compare_single_result_bits(const Insn& insn, uint64_t rs1v, uint64_t rs2v, uint64_t& exception_flags) {
    const bool rs1_nan = is_single_nan_bits(rs1v);
    const bool rs2_nan = is_single_nan_bits(rs2v);
    exception_flags = 0;
    if (rs1_nan || rs2_nan) {
        if (is_feq_s(insn)) {
            if (is_single_signaling_nan_bits(rs1v) || is_single_signaling_nan_bits(rs2v)) {
                exception_flags |= FCSR_FLAG_NV;
            }
        } else {
            exception_flags |= FCSR_FLAG_NV;
        }
        return 0;
    }

    const float lhs = raw_bits_to_float(rs1v);
    const float rhs = raw_bits_to_float(rs2v);
    if (is_feq_s(insn)) {
        return lhs == rhs ? 1ULL : 0ULL;
    }
    if (is_flt_s(insn)) {
        return lhs < rhs ? 1ULL : 0ULL;
    }
    return lhs <= rhs ? 1ULL : 0ULL;
}

uint64_t compare_double_result_bits(const Insn& insn, uint64_t rs1v, uint64_t rs2v, uint64_t& exception_flags) {
    const bool rs1_nan = is_double_nan_bits(rs1v);
    const bool rs2_nan = is_double_nan_bits(rs2v);
    exception_flags = 0;
    if (rs1_nan || rs2_nan) {
        if (is_feq_d(insn)) {
            if (is_double_signaling_nan_bits(rs1v) || is_double_signaling_nan_bits(rs2v)) {
                exception_flags |= FCSR_FLAG_NV;
            }
        } else {
            exception_flags |= FCSR_FLAG_NV;
        }
        return 0;
    }

    const double lhs = raw_bits_to_double(rs1v);
    const double rhs = raw_bits_to_double(rs2v);
    if (is_feq_d(insn)) {
        return lhs == rhs ? 1ULL : 0ULL;
    }
    if (is_flt_d(insn)) {
        return lhs < rhs ? 1ULL : 0ULL;
    }
    return lhs <= rhs ? 1ULL : 0ULL;
}

uint64_t minmax_single_result_bits(const Insn& insn, uint64_t rs1v, uint64_t rs2v, uint64_t& exception_flags) {
    const bool rs1_nan = is_single_nan_bits(rs1v);
    const bool rs2_nan = is_single_nan_bits(rs2v);
    exception_flags = 0;
    if (is_single_signaling_nan_bits(rs1v) || is_single_signaling_nan_bits(rs2v)) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (rs1_nan && rs2_nan) {
        return canonical_single_nan_bits();
    }
    if (rs1_nan) {
        return rs2v;
    }
    if (rs2_nan) {
        return rs1v;
    }

    const float lhs = raw_bits_to_float(rs1v);
    const float rhs = raw_bits_to_float(rs2v);
    return box_single_result(is_fmax_s(insn) ? std::fmax(lhs, rhs) : std::fmin(lhs, rhs));
}

uint64_t minmax_double_result_bits(const Insn& insn, uint64_t rs1v, uint64_t rs2v, uint64_t& exception_flags) {
    const bool rs1_nan = is_double_nan_bits(rs1v);
    const bool rs2_nan = is_double_nan_bits(rs2v);
    exception_flags = 0;
    if (is_double_signaling_nan_bits(rs1v) || is_double_signaling_nan_bits(rs2v)) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (rs1_nan && rs2_nan) {
        return canonical_double_nan_bits();
    }
    if (rs1_nan) {
        return rs2v;
    }
    if (rs2_nan) {
        return rs1v;
    }

    const double lhs = raw_bits_to_double(rs1v);
    const double rhs = raw_bits_to_double(rs2v);
    return double_to_raw_bits(is_fmax_d(insn) ? std::fmax(lhs, rhs) : std::fmin(lhs, rhs));
}

bool compute_integer_to_double_result(const Insn& insn,
                                      uint64_t frm,
                                      uint64_t rs1v,
                                      uint64_t& result_bits) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    const double result =
        is_fcvt_d_l(insn)  ? static_cast<double>(static_cast<int64_t>(rs1v))
        : is_fcvt_d_lu(insn) ? static_cast<double>(rs1v)
        : is_fcvt_d_wu(insn) ? static_cast<double>(static_cast<uint32_t>(rs1v))
                             : static_cast<double>(static_cast<int32_t>(static_cast<uint32_t>(rs1v)));
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = double_to_raw_bits(result);
    return true;
}

bool compute_double_to_single_result(const Insn& insn,
                                     uint64_t frm,
                                     uint64_t rs1v,
                                     uint64_t& result_bits,
                                     uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const float result = static_cast<float>(raw_bits_to_double(rs1v));
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = box_single_result(result);
    return true;
}

bool compute_integer_to_single_result(const Insn& insn,
                                      uint64_t frm,
                                      uint64_t rs1v,
                                      uint64_t& result_bits,
                                      uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    int previous_mode = FE_TONEAREST;
    if (resolved_rm == FCSR_FRM_RMM || !set_rounding_mode(resolved_rm, previous_mode)) {
        return false;
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    const float result =
        is_fcvt_s_l(insn)   ? static_cast<float>(static_cast<int64_t>(rs1v))
        : is_fcvt_s_lu(insn) ? static_cast<float>(rs1v)
        : is_fcvt_s_wu(insn) ? static_cast<float>(static_cast<uint32_t>(rs1v))
                             : static_cast<float>(static_cast<int32_t>(static_cast<uint32_t>(rs1v)));
    exception_flags = 0;
    if (std::fetestexcept(FE_INVALID) != 0) {
        exception_flags |= FCSR_FLAG_NV;
    }
    if (std::fetestexcept(FE_DIVBYZERO) != 0) {
        exception_flags |= FCSR_FLAG_DZ;
    }
    if (std::fetestexcept(FE_OVERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_OF;
    }
    if (std::fetestexcept(FE_UNDERFLOW) != 0) {
        exception_flags |= FCSR_FLAG_UF;
    }
    if (std::fetestexcept(FE_INEXACT) != 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    if (std::fesetround(previous_mode) != 0) {
        return false;
    }
    result_bits = box_single_result(result);
    return true;
}

template <typename Float>
bool compute_float_to_integer_result(const Insn& insn,
                                     uint64_t frm,
                                     uint64_t rs1v,
                                     uint64_t& result_value,
                                     uint64_t& exception_flags) {
    uint64_t resolved_rm = 0;
    if (!resolve_rounding_mode(insn, frm, resolved_rm)) {
        return false;
    }
    const Float input = sizeof(Float) == sizeof(float) ? static_cast<Float>(raw_bits_to_float(rs1v))
                                                       : static_cast<Float>(raw_bits_to_double(rs1v));
    const auto round_to_guest_integer = [&](long double& rounded) {
        const long double wide_input = static_cast<long double>(input);
        switch (resolved_rm) {
        case FCSR_FRM_RNE: {
            long double integer_part = 0.0L;
            const long double fractional = std::modf(wide_input, &integer_part);
            const long double magnitude = std::fabs(fractional);
            if (magnitude < 0.5L) {
                rounded = integer_part;
                return true;
            }
            if (magnitude > 0.5L) {
                rounded = integer_part + std::copysign(1.0L, wide_input);
                return true;
            }
            const bool integer_part_is_even = std::fmod(std::fabs(integer_part), 2.0L) == 0.0L;
            rounded = integer_part_is_even ? integer_part : integer_part + std::copysign(1.0L, wide_input);
            return true;
        }
        case FCSR_FRM_RTZ:
            rounded = std::truncl(wide_input);
            return true;
        case FCSR_FRM_RDN:
            rounded = std::floor(wide_input);
            return true;
        case FCSR_FRM_RUP:
            rounded = std::ceil(wide_input);
            return true;
        case FCSR_FRM_RMM: {
            long double integer_part = 0.0L;
            const long double fractional = std::modf(wide_input, &integer_part);
            const long double magnitude = std::fabs(fractional);
            if (magnitude < 0.5L) {
                rounded = integer_part;
                return true;
            }
            if (magnitude > 0.5L || magnitude == 0.5L) {
                rounded = integer_part + std::copysign(1.0L, wide_input);
                return true;
            }
            rounded = integer_part;
            return true;
        }
        default:
            return false;
        }
    };

    long double rounded = 0.0L;
    if (!round_to_guest_integer(rounded)) {
        return false;
    }
    exception_flags = 0;

    const auto finish_invalid = [&](uint64_t clipped_result) {
        result_value = clipped_result;
        exception_flags = FCSR_FLAG_NV;
        return true;
    };

    if (is_fcvt_w_s(insn) || is_fcvt_w_d(insn)) {
        constexpr long double kMin = static_cast<long double>(std::numeric_limits<int32_t>::min());
        constexpr long double kMax = static_cast<long double>(std::numeric_limits<int32_t>::max());
        if (std::isnan(input) || rounded > kMax) {
            return finish_invalid(static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())));
        }
        if (rounded < kMin) {
            return finish_invalid(static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::min())));
        }
        result_value = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rounded)));
    } else if (is_fcvt_wu_s(insn) || is_fcvt_wu_d(insn)) {
        constexpr long double kMin = 0.0L;
        constexpr long double kMax = static_cast<long double>(std::numeric_limits<uint32_t>::max());
        if (std::isnan(input) || rounded > kMax) {
            return finish_invalid(
                static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(std::numeric_limits<uint32_t>::max()))));
        }
        if (rounded < kMin) {
            return finish_invalid(0);
        }
        result_value = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(rounded))));
    } else if (is_fcvt_l_s(insn) || is_fcvt_l_d(insn)) {
        constexpr long double kMin = static_cast<long double>(std::numeric_limits<int64_t>::min());
        constexpr long double kMax = static_cast<long double>(std::numeric_limits<int64_t>::max());
        if (std::isnan(input) || rounded > kMax) {
            return finish_invalid(static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
        }
        if (rounded < kMin) {
            return finish_invalid(static_cast<uint64_t>(std::numeric_limits<int64_t>::min()));
        }
        result_value = static_cast<uint64_t>(static_cast<int64_t>(rounded));
    } else {
        constexpr long double kMin = 0.0L;
        constexpr long double kMax = static_cast<long double>(std::numeric_limits<uint64_t>::max());
        if (std::isnan(input) || rounded > kMax) {
            return finish_invalid(std::numeric_limits<uint64_t>::max());
        }
        if (rounded < kMin) {
            return finish_invalid(0);
        }
        result_value = static_cast<uint64_t>(rounded);
    }

    if (!std::isnan(input) && rounded != static_cast<long double>(input) && exception_flags == 0) {
        exception_flags |= FCSR_FLAG_NX;
    }
    return true;
}

}  // namespace

bool is_fmv_d_x(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct3 == 0 && insn.rs2 == 0 && insn.funct7 == FUNCT7_FMV_D_X;
}

bool is_fmv_x_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct3 == 0 && insn.rs2 == 0 && insn.funct7 == FUNCT7_FMV_X_D;
}

bool is_fmv_w_x(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct3 == 0 && insn.rs2 == 0 && insn.funct7 == FUNCT7_FMV_W_X;
}

bool is_fmv_x_w(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct3 == 0 && insn.rs2 == 0 && insn.funct7 == FUNCT7_FMV_X_W;
}

bool is_fmv_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct3 == 0 && insn.funct7 == FUNCT7_FMV_D && insn.rs1 == insn.rs2;
}

bool is_fneg_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct3 == 1 && insn.funct7 == FUNCT7_FMV_D && insn.rs1 == insn.rs2;
}

bool is_fsgnj_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMV_D && insn.funct3 == 0;
}

bool is_fsgnjn_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMV_D && insn.funct3 == 1;
}

bool is_fsgnjx_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMV_D && insn.funct3 == 2;
}

bool is_fsgnj_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FSGNJ_S && insn.funct3 == 0;
}

bool is_fsgnjn_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FSGNJ_S && insn.funct3 == 1;
}

bool is_fsgnjx_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FSGNJ_S && insn.funct3 == 2;
}

bool is_fadd_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FADD_S;
}

bool is_fsub_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FSUB_S;
}

bool is_fmul_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMUL_S;
}

bool is_fdiv_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FDIV_S;
}

bool is_fadd_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FADD_D;
}

bool is_fsub_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FSUB_D;
}

bool is_fmul_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMUL_D;
}

bool is_fdiv_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FDIV_D;
}

bool is_fmax_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMINMAX_D && insn.funct3 == 1;
}

bool is_fmin_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMINMAX_D && insn.funct3 == 0;
}

bool is_fmax_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMINMAX_S && insn.funct3 == 1;
}

bool is_fmin_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMINMAX_S && insn.funct3 == 0;
}

bool is_fmadd_s(const Insn& insn) {
    return insn.opcode == OPCODE_FMADD && (insn.funct7 & 0x3) == 0x0;
}

bool is_fmsub_s(const Insn& insn) {
    return insn.opcode == OPCODE_FMSUB && (insn.funct7 & 0x3) == 0x0;
}

bool is_fnmsub_s(const Insn& insn) {
    return insn.opcode == OPCODE_FNMSUB && (insn.funct7 & 0x3) == 0x0;
}

bool is_fnmadd_s(const Insn& insn) {
    return insn.opcode == OPCODE_FNMADD && (insn.funct7 & 0x3) == 0x0;
}

bool is_fmadd_d(const Insn& insn) {
    return insn.opcode == OPCODE_FMADD && (insn.funct7 & 0x3) == 0x1;
}

bool is_fmsub_d(const Insn& insn) {
    return insn.opcode == OPCODE_FMSUB && (insn.funct7 & 0x3) == 0x1;
}

bool is_fnmsub_d(const Insn& insn) {
    return insn.opcode == OPCODE_FNMSUB && (insn.funct7 & 0x3) == 0x1;
}

bool is_fnmadd_d(const Insn& insn) {
    return insn.opcode == OPCODE_FNMADD && (insn.funct7 & 0x3) == 0x1;
}

bool is_fsqrt_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FSQRT_S && insn.rs2 == 0;
}

bool is_fsqrt_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FSQRT_D && insn.rs2 == 0;
}

bool is_fcvt_w_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_D && insn.rs2 == 0;
}

bool is_fcvt_wu_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_D && insn.rs2 == 1;
}

bool is_fcvt_l_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_D && insn.rs2 == 2;
}

bool is_fcvt_lu_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_D && insn.rs2 == 3;
}

bool is_fcvt_d_w(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_D_INT && insn.rs2 == 0;
}

bool is_fcvt_d_wu(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_D_INT && insn.rs2 == 1;
}

bool is_fcvt_d_l(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_D_INT && insn.rs2 == 2;
}

bool is_fcvt_d_lu(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_D_INT && insn.rs2 == 3;
}

bool is_fcvt_w_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_S && insn.rs2 == 0;
}

bool is_fcvt_wu_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_S && insn.rs2 == 1;
}

bool is_fcvt_l_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_S && insn.rs2 == 2;
}

bool is_fcvt_lu_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_L_FROM_S && insn.rs2 == 3;
}

bool is_fcvt_s_w(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_S_INT && insn.rs2 == 0;
}

bool is_fcvt_s_wu(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_S_INT && insn.rs2 == 1;
}

bool is_fcvt_s_l(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_S_INT && insn.rs2 == 2;
}

bool is_fcvt_s_lu(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_S_INT && insn.rs2 == 3;
}

bool is_fcvt_d_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_D_S && insn.rs2 == 1;
}

bool is_fcvt_s_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCVT_S_D && insn.rs2 == 1;
}

bool is_fclass_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FCLASS_S && insn.funct3 == 1 && insn.rs2 == 0;
}

bool is_fclass_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FMV_X_D && insn.funct3 == 1 && insn.rs2 == 0;
}

bool is_feq_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FEQ_S && insn.funct3 == 2;
}

bool is_flt_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FEQ_S && insn.funct3 == 1;
}

bool is_fle_s(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FEQ_S && insn.funct3 == 0;
}

bool is_feq_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FEQ_D && insn.funct3 == 2;
}

bool is_flt_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FEQ_D && insn.funct3 == 1;
}

bool is_fle_d(const Insn& insn) {
    return insn.opcode == OPCODE_FP && insn.funct7 == FUNCT7_FEQ_D && insn.funct3 == 0;
}

bool floating_rs1_from_fpr(const Insn& insn) {
    return is_fmv_x_d(insn) || is_fmv_x_w(insn) || is_fmv_d(insn) || is_fneg_d(insn) || is_fsgnj_d(insn) || is_fsgnjn_d(insn) ||
           is_fsgnjx_d(insn) || is_fsgnj_s(insn) || is_fsgnjn_s(insn) || is_fsgnjx_s(insn) ||
           is_fadd_s(insn) || is_fsub_s(insn) || is_fmul_s(insn) || is_fdiv_s(insn) ||
           is_fadd_d(insn) || is_fsub_d(insn) || is_fmul_d(insn) || is_fmax_d(insn) || is_fmin_d(insn) ||
           is_fmax_s(insn) || is_fmin_s(insn) || is_fsqrt_s(insn) ||
           is_fmadd_s(insn) || is_fmsub_s(insn) || is_fnmsub_s(insn) || is_fnmadd_s(insn) ||
           is_fdiv_d(insn) || is_fmadd_d(insn) || is_fmsub_d(insn) || is_fnmsub_d(insn) || is_fnmadd_d(insn) ||
           is_fsqrt_d(insn) ||
           is_fcvt_w_d(insn) || is_fcvt_wu_d(insn) || is_fcvt_l_d(insn) || is_fcvt_w_s(insn) || is_fcvt_wu_s(insn) ||
           is_fcvt_l_s(insn) || is_fcvt_lu_s(insn) ||
           is_fcvt_lu_d(insn) || is_fcvt_d_s(insn) || is_fcvt_s_d(insn) || is_fclass_s(insn) || is_fclass_d(insn) ||
           is_feq_s(insn) || is_flt_s(insn) || is_fle_s(insn) ||
           is_feq_d(insn) || is_flt_d(insn) || is_fle_d(insn);
}

bool floating_rs2_from_fpr(const Insn& insn) {
    return is_standard_fp_store(insn) || is_fmv_d(insn) || is_fneg_d(insn) || is_fsgnj_d(insn) || is_fsgnjn_d(insn) ||
           is_fsgnjx_d(insn) || is_fsgnj_s(insn) || is_fsgnjn_s(insn) || is_fsgnjx_s(insn) ||
           is_fadd_s(insn) || is_fsub_s(insn) || is_fmul_s(insn) || is_fdiv_s(insn) ||
           is_fadd_d(insn) || is_fsub_d(insn) || is_fmax_d(insn) || is_fmin_d(insn) || is_fmax_s(insn) ||
           is_fmin_s(insn) ||
           is_fmadd_s(insn) || is_fmsub_s(insn) || is_fnmsub_s(insn) || is_fnmadd_s(insn) ||
           is_fmul_d(insn) || is_fdiv_d(insn) || is_fmadd_d(insn) || is_fmsub_d(insn) || is_fnmsub_d(insn) ||
           is_fnmadd_d(insn) ||
           is_feq_s(insn) || is_flt_s(insn) || is_fle_s(insn) ||
           is_feq_d(insn) || is_flt_d(insn) || is_fle_d(insn);
}

bool floating_rs3_from_fpr(const Insn& insn) {
    return is_fmadd_s(insn) || is_fmsub_s(insn) || is_fnmsub_s(insn) || is_fnmadd_s(insn) ||
           is_fmadd_d(insn) || is_fmsub_d(insn) || is_fnmsub_d(insn) || is_fnmadd_d(insn);
}

InsnEffects build_floating_effects(const Insn& insn, uint64_t rs1v, uint64_t rs2v, uint64_t rs3v, uint64_t fcsr) {

    if (insn.opcode != OPCODE_FP && insn.opcode != OPCODE_FMADD &&
        insn.opcode != OPCODE_FMSUB && insn.opcode != OPCODE_FNMSUB &&
        insn.opcode != OPCODE_FNMADD) {
        return illegal_floating_instruction(insn.raw);
    }

    InsnEffects effects;
    effects.floating_state_touched = true;
    if (is_fmadd_s(insn) || is_fmsub_s(insn) || is_fnmsub_s(insn) || is_fnmadd_s(insn)) {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        const auto op =
            is_fmadd_s(insn)
                ? std::function<float(float, float, float)>{[](float lhs, float rhs, float addend) {
                      return std::fma(lhs, rhs, addend);
                  }}
                : is_fmsub_s(insn)
                      ? std::function<float(float, float, float)>{[](float lhs, float rhs, float addend) {
                            return std::fma(lhs, rhs, -addend);
                        }}
                      : is_fnmsub_s(insn)
                            ? std::function<float(float, float, float)>{[](float lhs, float rhs, float addend) {
                                  return -std::fma(lhs, rhs, -addend);
                              }}
                            : std::function<float(float, float, float)>{[](float lhs, float rhs, float addend) {
                                  return -std::fma(lhs, rhs, addend);
                              }};
        if (!compute_ternary_single_result(insn,
                                           (fcsr & FCSR_FRM_MASK) >> 5,
                                           rs1v,
                                           rs2v,
                                           rs3v,
                                           op,
                                           result_bits,
                                           exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    if (is_fmadd_d(insn) || is_fmsub_d(insn) || is_fnmsub_d(insn) || is_fnmadd_d(insn)) {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        const auto op =
            is_fmadd_d(insn)
                ? std::function<double(double, double, double)>{[](double lhs, double rhs, double addend) {
                      return std::fma(lhs, rhs, addend);
                  }}
                : is_fmsub_d(insn)
                      ? std::function<double(double, double, double)>{[](double lhs, double rhs, double addend) {
                            return std::fma(lhs, rhs, -addend);
                        }}
                      : is_fnmsub_d(insn)
                            ? std::function<double(double, double, double)>{[](double lhs, double rhs, double addend) {
                                  return -std::fma(lhs, rhs, -addend);
                              }}
                            : std::function<double(double, double, double)>{[](double lhs, double rhs, double addend) {
                                  return -std::fma(lhs, rhs, addend);
                              }};
        if (!compute_ternary_double_result(insn,
                                           (fcsr & FCSR_FRM_MASK) >> 5,
                                           rs1v,
                                           rs2v,
                                           rs3v,
                                           op,
                                           result_bits,
                                           exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    switch (insn.funct7) {
    case FUNCT7_FEQ_S:
        if (!is_feq_s(insn) && !is_flt_s(insn) && !is_fle_s(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        {
            uint64_t exception_flags = 0;
            const uint64_t result_value = compare_single_result_bits(insn, rs1v, rs2v, exception_flags);
            effects.rd_write.enable = true;
            effects.rd_write.rd = insn.rd;
            effects.rd_write.value = result_value;
            if (exception_flags != 0) {
                effects.csr_write.enable = true;
                effects.csr_write.addr = CSR_FCSR;
                effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
            }
            return effects;
        }
    case FUNCT7_FEQ_D:
        if (!is_feq_d(insn) && !is_flt_d(insn) && !is_fle_d(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        {
            uint64_t exception_flags = 0;
            const uint64_t result_value = compare_double_result_bits(insn, rs1v, rs2v, exception_flags);
            effects.rd_write.enable = true;
            effects.rd_write.rd = insn.rd;
            effects.rd_write.value = result_value;
            if (exception_flags != 0) {
                effects.csr_write.enable = true;
                effects.csr_write.addr = CSR_FCSR;
                effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
            }
            return effects;
        }
    case FUNCT7_FADD_S: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_single_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](float lhs, float rhs) { return lhs + rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FADD_D: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_double_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](double lhs, double rhs) { return lhs + rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FSUB_S: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_single_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](float lhs, float rhs) { return lhs - rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FSUB_D: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_double_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](double lhs, double rhs) { return lhs - rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FMUL_S: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_single_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](float lhs, float rhs) { return lhs * rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FMUL_D: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_double_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](double lhs, double rhs) { return lhs * rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FDIV_S: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_single_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](float lhs, float rhs) { return lhs / rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FDIV_D: {
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_binary_double_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                rs2v,
                [](double lhs, double rhs) { return lhs / rhs; },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FMINMAX_D:
        if (!is_fmax_d(insn) && !is_fmin_d(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        {
            uint64_t exception_flags = 0;
            effects.fp_write.value = minmax_double_result_bits(insn, rs1v, rs2v, exception_flags);
            if (exception_flags != 0) {
                effects.csr_write.enable = true;
                effects.csr_write.addr = CSR_FCSR;
                effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
            }
        }
        return effects;
    case FUNCT7_FMINMAX_S:
        if (!is_fmax_s(insn) && !is_fmin_s(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        {
            uint64_t exception_flags = 0;
            effects.fp_write.value = minmax_single_result_bits(insn, rs1v, rs2v, exception_flags);
            if (exception_flags != 0) {
                effects.csr_write.enable = true;
                effects.csr_write.addr = CSR_FCSR;
                effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
            }
        }
        return effects;
    case FUNCT7_FSQRT_S: {
        if (!is_fsqrt_s(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_unary_single_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                [](float lhs) { return std::sqrt(lhs); },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FSQRT_D: {
        if (!is_fsqrt_d(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_unary_double_result(
                insn,
                (fcsr & FCSR_FRM_MASK) >> 5,
                rs1v,
                [](double lhs) { return std::sqrt(lhs); },
                result_bits,
                exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FCVT_S_INT: {
        if (!is_fcvt_s_w(insn) && !is_fcvt_s_wu(insn) && !is_fcvt_s_l(insn) && !is_fcvt_s_lu(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_integer_to_single_result(insn,
                                              (fcsr & FCSR_FRM_MASK) >> 5,
                                              rs1v,
                                              result_bits,
                                              exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FCVT_S_D: {
        if (!is_fcvt_s_d(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        uint64_t result_bits = 0;
        uint64_t exception_flags = 0;
        if (!compute_double_to_single_result(insn,
                                             (fcsr & FCSR_FRM_MASK) >> 5,
                                             rs1v,
                                             result_bits,
                                             exception_flags)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = result_bits;
        if (exception_flags != 0) {
            effects.csr_write.enable = true;
            effects.csr_write.addr = CSR_FCSR;
            effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
        }
        return effects;
    }
    case FUNCT7_FCLASS_S:
        if (is_fmv_x_w(insn)) {
            effects.rd_write.enable = true;
            effects.rd_write.rd = insn.rd;
            effects.rd_write.value = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(rs1v)));
            return effects;
        }
        if (!is_fclass_s(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.rd_write.enable = true;
        effects.rd_write.rd = insn.rd;
        effects.rd_write.value = classify_single(raw_bits_to_float(rs1v));
        return effects;
    case FUNCT7_FCVT_D_S:
        if (!is_fcvt_d_s(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = double_to_raw_bits(static_cast<double>(raw_bits_to_float(rs1v)));
        return effects;
    case FUNCT7_FCVT_L_FROM_S:
        effects.rd_write.enable = true;
        effects.rd_write.rd = insn.rd;
        if (!is_fcvt_w_s(insn) && !is_fcvt_wu_s(insn) && !is_fcvt_l_s(insn) && !is_fcvt_lu_s(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        {
            uint64_t exception_flags = 0;
            if (!compute_float_to_integer_result<float>(insn,
                                                        (fcsr & FCSR_FRM_MASK) >> 5,
                                                        rs1v,
                                                        effects.rd_write.value,
                                                        exception_flags)) {
                return illegal_floating_instruction(insn.raw);
            }
            if (exception_flags != 0) {
                effects.csr_write.enable = true;
                effects.csr_write.addr = CSR_FCSR;
                effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
            }
        }
        return effects;
    case FUNCT7_FCVT_L_FROM_D:
        effects.rd_write.enable = true;
        effects.rd_write.rd = insn.rd;
        if (!is_fcvt_w_d(insn) && !is_fcvt_wu_d(insn) && !is_fcvt_l_d(insn) && !is_fcvt_lu_d(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        {
            uint64_t exception_flags = 0;
            if (!compute_float_to_integer_result<double>(insn,
                                                         (fcsr & FCSR_FRM_MASK) >> 5,
                                                         rs1v,
                                                         effects.rd_write.value,
                                                         exception_flags)) {
                return illegal_floating_instruction(insn.raw);
            }
            if (exception_flags != 0) {
                effects.csr_write.enable = true;
                effects.csr_write.addr = CSR_FCSR;
                effects.csr_write.value = update_fcsr_flags(fcsr, exception_flags);
            }
        }
        return effects;
    case FUNCT7_FMV_D:
        if (insn.funct3 != 0 && insn.funct3 != 1 && insn.funct3 != 2) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        if (is_fmv_d(insn)) {
            effects.fp_write.value = rs1v;
            return effects;
        }
        if (is_fneg_d(insn)) {
            effects.fp_write.value = (rs1v & ~(1ULL << 63)) | ((rs2v ^ (1ULL << 63)) & (1ULL << 63));
            return effects;
        }
        if (is_fsgnjn_d(insn)) {
            effects.fp_write.value = (rs1v & ~(1ULL << 63)) | ((rs2v ^ (1ULL << 63)) & (1ULL << 63));
            return effects;
        }
        if (is_fsgnjx_d(insn)) {
            effects.fp_write.value = rs1v ^ (rs2v & (1ULL << 63));
            return effects;
        }
        effects.fp_write.value = (rs1v & ~(1ULL << 63)) | (rs2v & (1ULL << 63));
        return effects;
    case FUNCT7_FSGNJ_S:
        if (!is_fsgnj_s(insn) && !is_fsgnjn_s(insn) && !is_fsgnjx_s(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        if (is_fsgnjn_s(insn)) {
            effects.fp_write.value =
                (rs1v & ~static_cast<uint64_t>(1ULL << 31)) |
                ((rs2v ^ static_cast<uint64_t>(1ULL << 31)) & static_cast<uint64_t>(1ULL << 31));
            return effects;
        }
        if (is_fsgnjx_s(insn)) {
            effects.fp_write.value = rs1v ^ (rs2v & static_cast<uint64_t>(1ULL << 31));
            return effects;
        }
        effects.fp_write.value =
            (rs1v & ~static_cast<uint64_t>(1ULL << 31)) | (rs2v & static_cast<uint64_t>(1ULL << 31));
        return effects;
    case FUNCT7_FCVT_D_INT:
        if (!is_fcvt_d_w(insn) && !is_fcvt_d_wu(insn) && !is_fcvt_d_l(insn) && !is_fcvt_d_lu(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        {
            uint64_t result_bits = 0;
            if (!compute_integer_to_double_result(insn, (fcsr & FCSR_FRM_MASK) >> 5, rs1v, result_bits)) {
                return illegal_floating_instruction(insn.raw);
            }
            effects.fp_write.enable = true;
            effects.fp_write.rd = insn.rd;
            effects.fp_write.value = result_bits;
            return effects;
        }
    case FUNCT7_FMV_W_X:
        if (!is_fmv_w_x(insn)) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = 0xffffffff00000000ULL | (rs1v & 0xffffffffULL);
        return effects;
    case FUNCT7_FMV_D_X:
        if (insn.funct3 != 0 || insn.rs2 != 0) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.fp_write.enable = true;
        effects.fp_write.rd = insn.rd;
        effects.fp_write.value = rs1v;
        return effects;
    case FUNCT7_FMV_X_D:
        if (is_fclass_d(insn)) {
            effects.rd_write.enable = true;
            effects.rd_write.rd = insn.rd;
            effects.rd_write.value = classify_double(raw_bits_to_double(rs1v));
            return effects;
        }
        if (insn.funct3 != 0 || insn.rs2 != 0) {
            return illegal_floating_instruction(insn.raw);
        }
        effects.rd_write.enable = true;
        effects.rd_write.rd = insn.rd;
        effects.rd_write.value = rs1v;
        return effects;
    default:
        return illegal_floating_instruction(insn.raw);
    }
}
