    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));
    Insn fmadd_s{};
    decode(0x60b50443U, &fmadd_s);
    if (!expect(InstructionSemantics::supports(fmadd_s), "fmadd.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmadd_s_effects = InstructionSemantics::execute(fmadd_s, ctx);
    if (!expect(fmadd_s_effects.fp_write.enable, "fmadd.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_s_effects.fp_write.rd == 8, "fmadd.s should target fs0/f8")) {
        return 1;
    }
    if (!expect(fmadd_s_effects.fp_write.value == float_to_fpr_raw_bits(10.0f),
                "fmadd.s should compute rs1*rs2+rs3 and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fmadd_s_effects.trap.valid, "legal fmadd.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(1.0f));
    const InsnEffects fmadd_s_invalid_effects = InstructionSemantics::execute(fmadd_s, ctx);
    if (!expect(fmadd_s_invalid_effects.fp_write.enable, "fmadd.s inf*0+1 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_float(fmadd_s_invalid_effects.fp_write.value)),
                "fmadd.s inf*0+1 should produce a NaN result")) {
        return 1;
    }
    if (!expect(fmadd_s_invalid_effects.csr_write.enable,
                "fmadd.s inf*0+1 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fmadd_s_invalid_effects.csr_write.addr == CSR_FCSR,
                "fmadd.s inf*0+1 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_s_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fmadd.s inf*0+1 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fmadd_s_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmadd.s inf*0+1 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(std::ldexp(1.0f, -25)));
    const InsnEffects fmadd_s_inexact_effects = InstructionSemantics::execute(fmadd_s, ctx);
    if (!expect(fmadd_s_inexact_effects.fp_write.enable, "fmadd.s 1*1+2^-25 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_s_inexact_effects.fp_write.value == float_to_fpr_raw_bits(1.0f),
                "fmadd.s 1*1+2^-25 should round back to exactly 1.0f in single precision")) {
        return 1;
    }
    if (!expect(fmadd_s_inexact_effects.csr_write.enable,
                "fmadd.s 1*1+2^-25 should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fmadd_s_inexact_effects.csr_write.addr == CSR_FCSR,
                "fmadd.s 1*1+2^-25 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_s_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fmadd.s 1*1+2^-25 should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fmadd_s_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmadd.s 1*1+2^-25 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fmadd_s_overflow_effects = InstructionSemantics::execute(fmadd_s, ctx);
    if (!expect(fmadd_s_overflow_effects.fp_write.enable, "fmadd.s max*max+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_s_overflow_effects.fp_write.value == float_to_fpr_raw_bits(INFINITY),
                "fmadd.s max*max+0 should overflow to IEEE754 single infinity")) {
        return 1;
    }
    if (!expect(fmadd_s_overflow_effects.csr_write.enable,
                "fmadd.s max*max+0 should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fmadd_s_overflow_effects.csr_write.addr == CSR_FCSR,
                "fmadd.s max*max+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_s_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fmadd.s max*max+0 should set the overflow flag")) {
        return 1;
    }
    if (!expect((fmadd_s_overflow_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fmadd.s max*max+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fmadd_s_underflow_effects = InstructionSemantics::execute(fmadd_s, ctx);
    if (!expect(fmadd_s_underflow_effects.fp_write.enable, "fmadd.s min*min+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_s_underflow_effects.fp_write.value == float_to_fpr_raw_bits(0.0f),
                "fmadd.s min*min+0 should underflow to zero")) {
        return 1;
    }
    if (!expect(fmadd_s_underflow_effects.csr_write.enable,
                "fmadd.s min*min+0 should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fmadd_s_underflow_effects.csr_write.addr == CSR_FCSR,
                "fmadd.s min*min+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_s_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fmadd.s min*min+0 should set the underflow flag")) {
        return 1;
    }
    if (!expect((fmadd_s_underflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fmadd.s min*min+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));
    Insn fmsub_s{};
    decode(0x60b50447U, &fmsub_s);
    if (!expect(InstructionSemantics::supports(fmsub_s), "fmsub.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmsub_s_effects = InstructionSemantics::execute(fmsub_s, ctx);
    if (!expect(fmsub_s_effects.fp_write.enable, "fmsub.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_s_effects.fp_write.rd == 8, "fmsub.s should target fs0/f8")) {
        return 1;
    }
    if (!expect(fmsub_s_effects.fp_write.value == float_to_fpr_raw_bits(2.0f),
                "fmsub.s should compute rs1*rs2-rs3 and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fmsub_s_effects.trap.valid, "legal fmsub.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(1.0f));
    const InsnEffects fmsub_s_invalid_effects = InstructionSemantics::execute(fmsub_s, ctx);
    if (!expect(fmsub_s_invalid_effects.fp_write.enable, "fmsub.s inf*0-1 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_float(fmsub_s_invalid_effects.fp_write.value)),
                "fmsub.s inf*0-1 should produce a NaN result")) {
        return 1;
    }
    if (!expect(fmsub_s_invalid_effects.csr_write.enable,
                "fmsub.s inf*0-1 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fmsub_s_invalid_effects.csr_write.addr == CSR_FCSR,
                "fmsub.s inf*0-1 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_s_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fmsub.s inf*0-1 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fmsub_s_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmsub.s inf*0-1 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(std::ldexp(1.0f, -25)));
    const InsnEffects fmsub_s_inexact_effects = InstructionSemantics::execute(fmsub_s, ctx);
    if (!expect(fmsub_s_inexact_effects.fp_write.enable, "fmsub.s 1*1-2^-25 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_s_inexact_effects.fp_write.value == float_to_fpr_raw_bits(1.0f),
                "fmsub.s 1*1-2^-25 should round back to exactly 1.0f in single precision")) {
        return 1;
    }
    if (!expect(fmsub_s_inexact_effects.csr_write.enable,
                "fmsub.s 1*1-2^-25 should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fmsub_s_inexact_effects.csr_write.addr == CSR_FCSR,
                "fmsub.s 1*1-2^-25 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_s_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fmsub.s 1*1-2^-25 should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fmsub_s_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmsub.s 1*1-2^-25 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fmsub_s_overflow_effects = InstructionSemantics::execute(fmsub_s, ctx);
    if (!expect(fmsub_s_overflow_effects.fp_write.enable, "fmsub.s max*max-0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_s_overflow_effects.fp_write.value == float_to_fpr_raw_bits(INFINITY),
                "fmsub.s max*max-0 should overflow to IEEE754 single infinity")) {
        return 1;
    }
    if (!expect(fmsub_s_overflow_effects.csr_write.enable,
                "fmsub.s max*max-0 should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fmsub_s_overflow_effects.csr_write.addr == CSR_FCSR,
                "fmsub.s max*max-0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_s_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fmsub.s max*max-0 should set the overflow flag")) {
        return 1;
    }
    if (!expect((fmsub_s_overflow_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fmsub.s max*max-0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fmsub_s_underflow_effects = InstructionSemantics::execute(fmsub_s, ctx);
    if (!expect(fmsub_s_underflow_effects.fp_write.enable, "fmsub.s min*min-0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_s_underflow_effects.fp_write.value == float_to_fpr_raw_bits(0.0f),
                "fmsub.s min*min-0 should underflow to zero")) {
        return 1;
    }
    if (!expect(fmsub_s_underflow_effects.csr_write.enable,
                "fmsub.s min*min-0 should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fmsub_s_underflow_effects.csr_write.addr == CSR_FCSR,
                "fmsub.s min*min-0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_s_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fmsub.s min*min-0 should set the underflow flag")) {
        return 1;
    }
    if (!expect((fmsub_s_underflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fmsub.s min*min-0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));

    Insn fnmsub_s{};
    decode(0x60b5044bU, &fnmsub_s);
    if (!expect(InstructionSemantics::supports(fnmsub_s), "fnmsub.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fnmsub_s_effects = InstructionSemantics::execute(fnmsub_s, ctx);
    if (!expect(fnmsub_s_effects.fp_write.enable, "fnmsub.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_s_effects.fp_write.rd == 8, "fnmsub.s should target fs0/f8")) {
        return 1;
    }
    if (!expect(fnmsub_s_effects.fp_write.value == float_to_fpr_raw_bits(-2.0f),
                "fnmsub.s should compute -(rs1*rs2)+rs3 and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fnmsub_s_effects.trap.valid, "legal fnmsub.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(1.0f));
    const InsnEffects fnmsub_s_invalid_effects = InstructionSemantics::execute(fnmsub_s, ctx);
    if (!expect(fnmsub_s_invalid_effects.fp_write.enable, "fnmsub.s -(inf*0)+1 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_float(fnmsub_s_invalid_effects.fp_write.value)),
                "fnmsub.s -(inf*0)+1 should produce a NaN result")) {
        return 1;
    }
    if (!expect(fnmsub_s_invalid_effects.csr_write.enable,
                "fnmsub.s -(inf*0)+1 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fnmsub_s_invalid_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.s -(inf*0)+1 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_s_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fnmsub.s -(inf*0)+1 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fnmsub_s_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fnmsub.s -(inf*0)+1 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(std::ldexp(1.0f, -25)));
    const InsnEffects fnmsub_s_inexact_effects = InstructionSemantics::execute(fnmsub_s, ctx);
    if (!expect(fnmsub_s_inexact_effects.fp_write.enable, "fnmsub.s -(1*1)+2^-25 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_s_inexact_effects.fp_write.value == float_to_fpr_raw_bits(-1.0f),
                "fnmsub.s -(1*1)+2^-25 should round back to exactly -1.0f in single precision")) {
        return 1;
    }
    if (!expect(fnmsub_s_inexact_effects.csr_write.enable,
                "fnmsub.s -(1*1)+2^-25 should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fnmsub_s_inexact_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.s -(1*1)+2^-25 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_s_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fnmsub.s -(1*1)+2^-25 should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fnmsub_s_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fnmsub.s -(1*1)+2^-25 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fnmsub_s_overflow_effects = InstructionSemantics::execute(fnmsub_s, ctx);
    if (!expect(fnmsub_s_overflow_effects.fp_write.enable, "fnmsub.s -(max*max)+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_s_overflow_effects.fp_write.value == float_to_fpr_raw_bits(-INFINITY),
                "fnmsub.s -(max*max)+0 should overflow to IEEE754 single negative infinity")) {
        return 1;
    }
    if (!expect(fnmsub_s_overflow_effects.csr_write.enable,
                "fnmsub.s -(max*max)+0 should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fnmsub_s_overflow_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.s -(max*max)+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_s_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fnmsub.s -(max*max)+0 should set the overflow flag")) {
        return 1;
    }
    if (!expect((fnmsub_s_overflow_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fnmsub.s -(max*max)+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fnmsub_s_underflow_effects = InstructionSemantics::execute(fnmsub_s, ctx);
    if (!expect(fnmsub_s_underflow_effects.fp_write.enable, "fnmsub.s -(min*min)+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_s_underflow_effects.fp_write.value == float_to_fpr_raw_bits(-0.0f),
                "fnmsub.s -(min*min)+0 should underflow to negative zero")) {
        return 1;
    }
    if (!expect(fnmsub_s_underflow_effects.csr_write.enable,
                "fnmsub.s -(min*min)+0 should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fnmsub_s_underflow_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.s -(min*min)+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_s_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fnmsub.s -(min*min)+0 should set the underflow flag")) {
        return 1;
    }
    if (!expect((fnmsub_s_underflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fnmsub.s -(min*min)+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));

    Insn fnmadd_s{};
    decode(0x60b5044fU, &fnmadd_s);
    if (!expect(InstructionSemantics::supports(fnmadd_s), "fnmadd.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fnmadd_s_effects = InstructionSemantics::execute(fnmadd_s, ctx);
    if (!expect(fnmadd_s_effects.fp_write.enable, "fnmadd.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_s_effects.fp_write.rd == 8, "fnmadd.s should target fs0/f8")) {
        return 1;
    }
    if (!expect(fnmadd_s_effects.fp_write.value == float_to_fpr_raw_bits(-10.0f),
                "fnmadd.s should compute -(rs1*rs2)-rs3 and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fnmadd_s_effects.trap.valid, "legal fnmadd.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(-1.0f));
    const InsnEffects fnmadd_s_invalid_effects = InstructionSemantics::execute(fnmadd_s, ctx);
    if (!expect(fnmadd_s_invalid_effects.fp_write.enable, "fnmadd.s -(inf*0)-(-1) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_float(fnmadd_s_invalid_effects.fp_write.value)),
                "fnmadd.s -(inf*0)-(-1) should produce a NaN result")) {
        return 1;
    }
    if (!expect(fnmadd_s_invalid_effects.csr_write.enable,
                "fnmadd.s -(inf*0)-(-1) should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fnmadd_s_invalid_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.s -(inf*0)-(-1) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_s_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fnmadd.s -(inf*0)-(-1) should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fnmadd_s_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fnmadd.s -(inf*0)-(-1) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(-std::ldexp(1.0f, -25)));
    const InsnEffects fnmadd_s_inexact_effects = InstructionSemantics::execute(fnmadd_s, ctx);
    if (!expect(fnmadd_s_inexact_effects.fp_write.enable, "fnmadd.s -(1*1)-(-2^-25) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_s_inexact_effects.fp_write.value == float_to_fpr_raw_bits(-1.0f),
                "fnmadd.s -(1*1)-(-2^-25) should round back to exactly -1.0f in single precision")) {
        return 1;
    }
    if (!expect(fnmadd_s_inexact_effects.csr_write.enable,
                "fnmadd.s -(1*1)-(-2^-25) should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fnmadd_s_inexact_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.s -(1*1)-(-2^-25) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_s_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fnmadd.s -(1*1)-(-2^-25) should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fnmadd_s_inexact_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fnmadd.s -(1*1)-(-2^-25) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(-0.0f));
    const InsnEffects fnmadd_s_overflow_effects = InstructionSemantics::execute(fnmadd_s, ctx);
    if (!expect(fnmadd_s_overflow_effects.fp_write.enable, "fnmadd.s -(max*max)-(-0) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_s_overflow_effects.fp_write.value == float_to_fpr_raw_bits(-INFINITY),
                "fnmadd.s -(max*max)-(-0) should overflow to IEEE754 single negative infinity")) {
        return 1;
    }
    if (!expect(fnmadd_s_overflow_effects.csr_write.enable,
                "fnmadd.s -(max*max)-(-0) should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fnmadd_s_overflow_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.s -(max*max)-(-0) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_s_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fnmadd.s -(max*max)-(-0) should set the overflow flag")) {
        return 1;
    }
    if (!expect((fnmadd_s_overflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fnmadd.s -(max*max)-(-0) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(-0.0f));
    const InsnEffects fnmadd_s_underflow_effects = InstructionSemantics::execute(fnmadd_s, ctx);
    if (!expect(fnmadd_s_underflow_effects.fp_write.enable, "fnmadd.s -(min*min)-(-0) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_s_underflow_effects.fp_write.value == float_to_fpr_raw_bits(-0.0f),
                "fnmadd.s -(min*min)-(-0) should underflow to negative zero")) {
        return 1;
    }
    if (!expect(fnmadd_s_underflow_effects.csr_write.enable,
                "fnmadd.s -(min*min)-(-0) should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fnmadd_s_underflow_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.s -(min*min)-(-0) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_s_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fnmadd.s -(min*min)-(-0) should set the underflow flag")) {
        return 1;
    }
    if (!expect((fnmadd_s_underflow_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fnmadd.s -(min*min)-(-0) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(9, double_to_raw_bits(7.0));
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    Insn fdiv_d{};
    fdiv_d.raw = 0x1aa4f4d3U;
    fdiv_d.opcode = 0x53;
    fdiv_d.rd = 9;
    fdiv_d.funct3 = 7;
    fdiv_d.rs1 = 9;
    fdiv_d.rs2 = 10;
    fdiv_d.funct7 = 0x0d;

    if (!expect(InstructionSemantics::supports(fdiv_d), "fdiv.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fdiv_d_effects = InstructionSemantics::execute(fdiv_d, ctx);
    if (!expect(fdiv_d_effects.fp_write.enable, "fdiv.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fdiv_d_effects.fp_write.rd == 9, "fdiv.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fdiv_d_effects.fp_write.value == double_to_raw_bits(3.5),
                "fdiv.d should divide the source fp registers and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fdiv_d_effects.rd_write.enable, "fdiv.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fdiv_d_effects.trap.valid, "legal fdiv.d should not raise a trap")) {
        return 1;
    }
    if (!expect(fdiv_d_effects.floating_state_touched,
                "fdiv.d should mark that the architectural floating-point state was touched")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(9, double_to_raw_bits(7.0));
    cpu.core().write_fpr(10, double_to_raw_bits(0.0));
    const InsnEffects fdiv_d_flags_effects = InstructionSemantics::execute(fdiv_d, ctx);
    if (!expect(fdiv_d_flags_effects.csr_write.enable, "fdiv.d should produce a floating-point CSR write for exception flags")) {
        return 1;
    }
    if (!expect(fdiv_d_flags_effects.csr_write.addr == CSR_FCSR, "fdiv.d should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fdiv_d_flags_effects.csr_write.value & 0x1fULL) == 0x8ULL,
                "fdiv.d by zero should set the divide-by-zero flag")) {
        return 1;
    }
    if (!expect((fdiv_d_flags_effects.csr_write.value & ~0x1fULL) == 0,
                "fdiv.d by zero should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    cpu.core().write_fpr(11, double_to_raw_bits(3.0));
    cpu.core().write_fpr(12, double_to_raw_bits(4.0));
    Insn fmadd_d{};
    decode(0x62b504c3U, &fmadd_d);
    if (!expect(InstructionSemantics::supports(fmadd_d), "fmadd.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmadd_d_effects = InstructionSemantics::execute(fmadd_d, ctx);
    if (!expect(fmadd_d_effects.fp_write.enable, "fmadd.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_d_effects.fp_write.rd == 9, "fmadd.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fmadd_d_effects.fp_write.value == double_to_raw_bits(10.0),
                "fmadd.d should compute rs1*rs2+rs3 and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fmadd_d_effects.rd_write.enable, "fmadd.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fmadd_d_effects.trap.valid, "legal fmadd.d should not raise a trap")) {
        return 1;
    }
    if (!expect(fmadd_d_effects.floating_state_touched,
                "fmadd.d should mark that the architectural floating-point state was touched")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
    cpu.core().write_fpr(11, double_to_raw_bits(0.0));
    cpu.core().write_fpr(12, double_to_raw_bits(1.0));
    const InsnEffects fmadd_d_invalid_effects = InstructionSemantics::execute(fmadd_d, ctx);
    if (!expect(fmadd_d_invalid_effects.fp_write.enable, "fmadd.d inf*0+1 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_double(fmadd_d_invalid_effects.fp_write.value)),
                "fmadd.d inf*0+1 should produce a NaN result")) {
        return 1;
    }
    if (!expect(fmadd_d_invalid_effects.csr_write.enable,
                "fmadd.d inf*0+1 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fmadd_d_invalid_effects.csr_write.addr == CSR_FCSR,
                "fmadd.d inf*0+1 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_d_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fmadd.d inf*0+1 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fmadd_d_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmadd.d inf*0+1 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(1.0));
    cpu.core().write_fpr(11, double_to_raw_bits(1.0));
    cpu.core().write_fpr(12, double_to_raw_bits(std::ldexp(1.0, -54)));
    const InsnEffects fmadd_d_inexact_effects = InstructionSemantics::execute(fmadd_d, ctx);
    if (!expect(fmadd_d_inexact_effects.fp_write.enable, "fmadd.d 1*1+2^-54 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_d_inexact_effects.fp_write.value == double_to_raw_bits(1.0),
                "fmadd.d 1*1+2^-54 should round back to exactly 1.0 in double precision")) {
        return 1;
    }
    if (!expect(fmadd_d_inexact_effects.csr_write.enable,
                "fmadd.d 1*1+2^-54 should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fmadd_d_inexact_effects.csr_write.addr == CSR_FCSR,
                "fmadd.d 1*1+2^-54 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_d_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fmadd.d 1*1+2^-54 should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fmadd_d_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmadd.d 1*1+2^-54 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(12, double_to_raw_bits(0.0));
    const InsnEffects fmadd_d_overflow_effects = InstructionSemantics::execute(fmadd_d, ctx);
    if (!expect(fmadd_d_overflow_effects.fp_write.enable, "fmadd.d max*max+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_d_overflow_effects.fp_write.value == double_to_raw_bits(INFINITY),
                "fmadd.d max*max+0 should overflow to IEEE754 double infinity")) {
        return 1;
    }
    if (!expect(fmadd_d_overflow_effects.csr_write.enable,
                "fmadd.d max*max+0 should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fmadd_d_overflow_effects.csr_write.addr == CSR_FCSR,
                "fmadd.d max*max+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_d_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fmadd.d max*max+0 should set the overflow flag")) {
        return 1;
    }
    if (!expect((fmadd_d_overflow_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fmadd.d max*max+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(12, double_to_raw_bits(0.0));
    const InsnEffects fmadd_d_underflow_effects = InstructionSemantics::execute(fmadd_d, ctx);
    if (!expect(fmadd_d_underflow_effects.fp_write.enable, "fmadd.d min*min+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmadd_d_underflow_effects.fp_write.value == double_to_raw_bits(0.0),
                "fmadd.d min*min+0 should underflow to zero")) {
        return 1;
    }
    if (!expect(fmadd_d_underflow_effects.csr_write.enable,
                "fmadd.d min*min+0 should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fmadd_d_underflow_effects.csr_write.addr == CSR_FCSR,
                "fmadd.d min*min+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmadd_d_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fmadd.d min*min+0 should set the underflow flag")) {
        return 1;
    }
    if (!expect((fmadd_d_underflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fmadd.d min*min+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    cpu.core().write_fpr(11, double_to_raw_bits(3.0));
    cpu.core().write_fpr(12, double_to_raw_bits(4.0));
    Insn fmsub_d{};
    decode(0x62b504c7U, &fmsub_d);
    if (!expect(InstructionSemantics::supports(fmsub_d), "fmsub.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmsub_d_effects = InstructionSemantics::execute(fmsub_d, ctx);
    if (!expect(fmsub_d_effects.fp_write.enable, "fmsub.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_d_effects.fp_write.rd == 9, "fmsub.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fmsub_d_effects.fp_write.value == double_to_raw_bits(2.0),
                "fmsub.d should compute rs1*rs2-rs3 and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fmsub_d_effects.trap.valid, "legal fmsub.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
    cpu.core().write_fpr(11, double_to_raw_bits(0.0));
    cpu.core().write_fpr(12, double_to_raw_bits(1.0));
    const InsnEffects fmsub_d_invalid_effects = InstructionSemantics::execute(fmsub_d, ctx);
    if (!expect(fmsub_d_invalid_effects.fp_write.enable, "fmsub.d inf*0-1 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_double(fmsub_d_invalid_effects.fp_write.value)),
                "fmsub.d inf*0-1 should produce a NaN result")) {
        return 1;
    }
    if (!expect(fmsub_d_invalid_effects.csr_write.enable,
                "fmsub.d inf*0-1 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fmsub_d_invalid_effects.csr_write.addr == CSR_FCSR,
                "fmsub.d inf*0-1 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_d_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fmsub.d inf*0-1 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fmsub_d_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmsub.d inf*0-1 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(1.0));
    cpu.core().write_fpr(11, double_to_raw_bits(1.0));
    cpu.core().write_fpr(12, double_to_raw_bits(std::ldexp(1.0, -54)));
    const InsnEffects fmsub_d_inexact_effects = InstructionSemantics::execute(fmsub_d, ctx);
    if (!expect(fmsub_d_inexact_effects.fp_write.enable, "fmsub.d 1*1-2^-54 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_d_inexact_effects.fp_write.value == double_to_raw_bits(1.0),
                "fmsub.d 1*1-2^-54 should round back to exactly 1.0 in double precision")) {
        return 1;
    }
    if (!expect(fmsub_d_inexact_effects.csr_write.enable,
                "fmsub.d 1*1-2^-54 should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fmsub_d_inexact_effects.csr_write.addr == CSR_FCSR,
                "fmsub.d 1*1-2^-54 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_d_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fmsub.d 1*1-2^-54 should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fmsub_d_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fmsub.d 1*1-2^-54 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(12, double_to_raw_bits(0.0));
    const InsnEffects fmsub_d_overflow_effects = InstructionSemantics::execute(fmsub_d, ctx);
    if (!expect(fmsub_d_overflow_effects.fp_write.enable, "fmsub.d max*max-0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_d_overflow_effects.fp_write.value == double_to_raw_bits(INFINITY),
                "fmsub.d max*max-0 should overflow to IEEE754 double infinity")) {
        return 1;
    }
    if (!expect(fmsub_d_overflow_effects.csr_write.enable,
                "fmsub.d max*max-0 should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fmsub_d_overflow_effects.csr_write.addr == CSR_FCSR,
                "fmsub.d max*max-0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_d_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fmsub.d max*max-0 should set the overflow flag")) {
        return 1;
    }
    if (!expect((fmsub_d_overflow_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fmsub.d max*max-0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(12, double_to_raw_bits(0.0));
    const InsnEffects fmsub_d_underflow_effects = InstructionSemantics::execute(fmsub_d, ctx);
    if (!expect(fmsub_d_underflow_effects.fp_write.enable, "fmsub.d min*min-0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmsub_d_underflow_effects.fp_write.value == double_to_raw_bits(0.0),
                "fmsub.d min*min-0 should underflow to zero")) {
        return 1;
    }
    if (!expect(fmsub_d_underflow_effects.csr_write.enable,
                "fmsub.d min*min-0 should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fmsub_d_underflow_effects.csr_write.addr == CSR_FCSR,
                "fmsub.d min*min-0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmsub_d_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fmsub.d min*min-0 should set the underflow flag")) {
        return 1;
    }
    if (!expect((fmsub_d_underflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fmsub.d min*min-0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    cpu.core().write_fpr(11, double_to_raw_bits(3.0));
    cpu.core().write_fpr(12, double_to_raw_bits(4.0));

    Insn fnmsub_d{};
    decode(0x62b504cbU, &fnmsub_d);
    if (!expect(InstructionSemantics::supports(fnmsub_d), "fnmsub.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fnmsub_d_effects = InstructionSemantics::execute(fnmsub_d, ctx);
    if (!expect(fnmsub_d_effects.fp_write.enable, "fnmsub.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_d_effects.fp_write.rd == 9, "fnmsub.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fnmsub_d_effects.fp_write.value == double_to_raw_bits(-2.0),
                "fnmsub.d should compute -(rs1*rs2)+rs3 and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fnmsub_d_effects.trap.valid, "legal fnmsub.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
    cpu.core().write_fpr(11, double_to_raw_bits(0.0));
    cpu.core().write_fpr(12, double_to_raw_bits(1.0));
    const InsnEffects fnmsub_d_invalid_effects = InstructionSemantics::execute(fnmsub_d, ctx);
    if (!expect(fnmsub_d_invalid_effects.fp_write.enable, "fnmsub.d -(inf*0)+1 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_double(fnmsub_d_invalid_effects.fp_write.value)),
                "fnmsub.d -(inf*0)+1 should produce a NaN result")) {
        return 1;
    }
    if (!expect(fnmsub_d_invalid_effects.csr_write.enable,
                "fnmsub.d -(inf*0)+1 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fnmsub_d_invalid_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.d -(inf*0)+1 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_d_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fnmsub.d -(inf*0)+1 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fnmsub_d_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fnmsub.d -(inf*0)+1 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(1.0));
    cpu.core().write_fpr(11, double_to_raw_bits(1.0));
    cpu.core().write_fpr(12, double_to_raw_bits(std::ldexp(1.0, -54)));
    const InsnEffects fnmsub_d_inexact_effects = InstructionSemantics::execute(fnmsub_d, ctx);
    if (!expect(fnmsub_d_inexact_effects.fp_write.enable, "fnmsub.d -(1*1)+2^-54 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_d_inexact_effects.fp_write.value == double_to_raw_bits(-1.0),
                "fnmsub.d -(1*1)+2^-54 should round back to exactly -1.0 in double precision")) {
        return 1;
    }
    if (!expect(fnmsub_d_inexact_effects.csr_write.enable,
                "fnmsub.d -(1*1)+2^-54 should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fnmsub_d_inexact_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.d -(1*1)+2^-54 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_d_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fnmsub.d -(1*1)+2^-54 should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fnmsub_d_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fnmsub.d -(1*1)+2^-54 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(12, double_to_raw_bits(0.0));
    const InsnEffects fnmsub_d_overflow_effects = InstructionSemantics::execute(fnmsub_d, ctx);
    if (!expect(fnmsub_d_overflow_effects.fp_write.enable, "fnmsub.d -(max*max)+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_d_overflow_effects.fp_write.value == double_to_raw_bits(-INFINITY),
                "fnmsub.d -(max*max)+0 should overflow to IEEE754 double negative infinity")) {
        return 1;
    }
    if (!expect(fnmsub_d_overflow_effects.csr_write.enable,
                "fnmsub.d -(max*max)+0 should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fnmsub_d_overflow_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.d -(max*max)+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_d_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fnmsub.d -(max*max)+0 should set the overflow flag")) {
        return 1;
    }
    if (!expect((fnmsub_d_overflow_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fnmsub.d -(max*max)+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(12, double_to_raw_bits(0.0));
    const InsnEffects fnmsub_d_underflow_effects = InstructionSemantics::execute(fnmsub_d, ctx);
    if (!expect(fnmsub_d_underflow_effects.fp_write.enable, "fnmsub.d -(min*min)+0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmsub_d_underflow_effects.fp_write.value == double_to_raw_bits(-0.0),
                "fnmsub.d -(min*min)+0 should underflow to negative zero")) {
        return 1;
    }
    if (!expect(fnmsub_d_underflow_effects.csr_write.enable,
                "fnmsub.d -(min*min)+0 should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fnmsub_d_underflow_effects.csr_write.addr == CSR_FCSR,
                "fnmsub.d -(min*min)+0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmsub_d_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fnmsub.d -(min*min)+0 should set the underflow flag")) {
        return 1;
    }
    if (!expect((fnmsub_d_underflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fnmsub.d -(min*min)+0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    cpu.core().write_fpr(11, double_to_raw_bits(3.0));
    cpu.core().write_fpr(12, double_to_raw_bits(4.0));

    Insn fnmadd_d{};
    decode(0x62b504cfU, &fnmadd_d);
    if (!expect(InstructionSemantics::supports(fnmadd_d), "fnmadd.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fnmadd_d_effects = InstructionSemantics::execute(fnmadd_d, ctx);
    if (!expect(fnmadd_d_effects.fp_write.enable, "fnmadd.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_d_effects.fp_write.rd == 9, "fnmadd.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fnmadd_d_effects.fp_write.value == double_to_raw_bits(-10.0),
                "fnmadd.d should compute -(rs1*rs2)-rs3 and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fnmadd_d_effects.trap.valid, "legal fnmadd.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
    cpu.core().write_fpr(11, double_to_raw_bits(0.0));
    cpu.core().write_fpr(12, double_to_raw_bits(-1.0));
    const InsnEffects fnmadd_d_invalid_effects = InstructionSemantics::execute(fnmadd_d, ctx);
    if (!expect(fnmadd_d_invalid_effects.fp_write.enable, "fnmadd.d -(inf*0)-(-1) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_double(fnmadd_d_invalid_effects.fp_write.value)),
                "fnmadd.d -(inf*0)-(-1) should produce a NaN result")) {
        return 1;
    }
    if (!expect(fnmadd_d_invalid_effects.csr_write.enable,
                "fnmadd.d -(inf*0)-(-1) should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fnmadd_d_invalid_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.d -(inf*0)-(-1) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_d_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fnmadd.d -(inf*0)-(-1) should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fnmadd_d_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fnmadd.d -(inf*0)-(-1) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(1.0));
    cpu.core().write_fpr(11, double_to_raw_bits(1.0));
    cpu.core().write_fpr(12, double_to_raw_bits(-std::ldexp(1.0, -54)));
    const InsnEffects fnmadd_d_inexact_effects = InstructionSemantics::execute(fnmadd_d, ctx);
    if (!expect(fnmadd_d_inexact_effects.fp_write.enable, "fnmadd.d -(1*1)-(-2^-54) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_d_inexact_effects.fp_write.value == double_to_raw_bits(-1.0),
                "fnmadd.d -(1*1)-(-2^-54) should round back to exactly -1.0 in double precision")) {
        return 1;
    }
    if (!expect(fnmadd_d_inexact_effects.csr_write.enable,
                "fnmadd.d -(1*1)-(-2^-54) should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fnmadd_d_inexact_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.d -(1*1)-(-2^-54) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_d_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fnmadd.d -(1*1)-(-2^-54) should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fnmadd_d_inexact_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fnmadd.d -(1*1)-(-2^-54) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
    cpu.core().write_fpr(12, double_to_raw_bits(-0.0));
    const InsnEffects fnmadd_d_overflow_effects = InstructionSemantics::execute(fnmadd_d, ctx);
    if (!expect(fnmadd_d_overflow_effects.fp_write.enable, "fnmadd.d -(max*max)-(-0) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_d_overflow_effects.fp_write.value == double_to_raw_bits(-INFINITY),
                "fnmadd.d -(max*max)-(-0) should overflow to IEEE754 double negative infinity")) {
        return 1;
    }
    if (!expect(fnmadd_d_overflow_effects.csr_write.enable,
                "fnmadd.d -(max*max)-(-0) should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fnmadd_d_overflow_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.d -(max*max)-(-0) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_d_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fnmadd.d -(max*max)-(-0) should set the overflow flag")) {
        return 1;
    }
    if (!expect((fnmadd_d_overflow_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fnmadd.d -(max*max)-(-0) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
    cpu.core().write_fpr(12, double_to_raw_bits(-0.0));
    const InsnEffects fnmadd_d_underflow_effects = InstructionSemantics::execute(fnmadd_d, ctx);
    if (!expect(fnmadd_d_underflow_effects.fp_write.enable, "fnmadd.d -(min*min)-(-0) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fnmadd_d_underflow_effects.fp_write.value == double_to_raw_bits(-0.0),
                "fnmadd.d -(min*min)-(-0) should underflow to negative zero")) {
        return 1;
    }
    if (!expect(fnmadd_d_underflow_effects.csr_write.enable,
                "fnmadd.d -(min*min)-(-0) should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fnmadd_d_underflow_effects.csr_write.addr == CSR_FCSR,
                "fnmadd.d -(min*min)-(-0) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fnmadd_d_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fnmadd.d -(min*min)-(-0) should set the underflow flag")) {
        return 1;
    }
    if (!expect((fnmadd_d_underflow_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fnmadd.d -(min*min)-(-0) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.25f));
    Insn fsqrt_s{};
    fsqrt_s.raw = 0x580507d3U;
    fsqrt_s.opcode = 0x53;
    fsqrt_s.rd = 15;
    fsqrt_s.funct3 = 0;
    fsqrt_s.rs1 = 10;
    fsqrt_s.rs2 = 0;
    fsqrt_s.funct7 = 0x2c;

    if (!expect(InstructionSemantics::supports(fsqrt_s), "fsqrt.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsqrt_s_effects = InstructionSemantics::execute(fsqrt_s, ctx);
    if (!expect(fsqrt_s_effects.fp_write.enable, "fsqrt.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsqrt_s_effects.fp_write.rd == 15, "fsqrt.s should target fa5/f15")) {
        return 1;
    }
    if (!expect(fsqrt_s_effects.fp_write.value == float_to_fpr_raw_bits(1.5f),
                "fsqrt.s should compute the square root and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fsqrt_s_effects.rd_write.enable, "fsqrt.s should not write the integer register file")) {
        return 1;
    }
    if (!expect(fsqrt_s_effects.floating_state_touched,
                "fsqrt.s should mark that the architectural floating-point state was touched")) {
        return 1;
    }
    if (!expect(!fsqrt_s_effects.trap.valid, "legal fsqrt.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, 0xffffffffbf800000ULL);
    const InsnEffects fsqrt_s_invalid_effects = InstructionSemantics::execute(fsqrt_s, ctx);
    if (!expect(fsqrt_s_invalid_effects.fp_write.enable, "fsqrt.s sqrt(-1.0f) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_float(fsqrt_s_invalid_effects.fp_write.value)),
                "fsqrt.s sqrt(-1.0f) should produce a NaN result")) {
        return 1;
    }
    if (!expect(fsqrt_s_invalid_effects.csr_write.enable,
                "fsqrt.s sqrt(-1.0f) should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fsqrt_s_invalid_effects.csr_write.addr == CSR_FCSR,
                "fsqrt.s sqrt(-1.0f) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fsqrt_s_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fsqrt.s sqrt(-1.0f) should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fsqrt_s_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fsqrt.s sqrt(-1.0f) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    const InsnEffects fsqrt_s_inexact_effects = InstructionSemantics::execute(fsqrt_s, ctx);
    if (!expect(fsqrt_s_inexact_effects.fp_write.enable, "fsqrt.s sqrt(2.0f) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fsqrt_s_inexact_effects.csr_write.enable,
                "fsqrt.s sqrt(2.0f) should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fsqrt_s_inexact_effects.csr_write.addr == CSR_FCSR,
                "fsqrt.s sqrt(2.0f) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fsqrt_s_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fsqrt.s sqrt(2.0f) should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fsqrt_s_inexact_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fsqrt.s sqrt(2.0f) should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.core().write_fpr(10, double_to_raw_bits(4.0));
    Insn fsqrt_d{};
    fsqrt_d.raw = 0x5a057553U;
    fsqrt_d.opcode = 0x53;
    fsqrt_d.rd = 10;
    fsqrt_d.funct3 = 7;
    fsqrt_d.rs1 = 10;
    fsqrt_d.rs2 = 0;
    fsqrt_d.funct7 = 0x2d;

    if (!expect(InstructionSemantics::supports(fsqrt_d), "fsqrt.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsqrt_d_effects = InstructionSemantics::execute(fsqrt_d, ctx);
    if (!expect(fsqrt_d_effects.fp_write.enable, "fsqrt.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsqrt_d_effects.fp_write.rd == 10, "fsqrt.d should target fa0/f10")) {
        return 1;
    }
    if (!expect(fsqrt_d_effects.fp_write.value == double_to_raw_bits(2.0),
                "fsqrt.d should compute the square root and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fsqrt_d_effects.rd_write.enable, "fsqrt.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(fsqrt_d_effects.floating_state_touched,
                "fsqrt.d should mark that the architectural floating-point state was touched")) {
        return 1;
    }
    if (!expect(!fsqrt_d_effects.trap.valid, "legal fsqrt.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(-1.0));
    const InsnEffects fsqrt_d_invalid_effects = InstructionSemantics::execute(fsqrt_d, ctx);
    if (!expect(fsqrt_d_invalid_effects.fp_write.enable, "fsqrt.d sqrt(-1.0) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_double(fsqrt_d_invalid_effects.fp_write.value)),
                "fsqrt.d sqrt(-1.0) should produce a NaN result")) {
        return 1;
    }
    if (!expect(fsqrt_d_invalid_effects.csr_write.enable,
                "fsqrt.d sqrt(-1.0) should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fsqrt_d_invalid_effects.csr_write.addr == CSR_FCSR,
                "fsqrt.d sqrt(-1.0) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fsqrt_d_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fsqrt.d sqrt(-1.0) should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fsqrt_d_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fsqrt.d sqrt(-1.0) should preserve the current rounding-mode bits")) {
        return 1;
    }

    {
        CPU boundary_cpu;
        cpu_init(boundary_cpu, kEntry);
        boundary_cpu.csr().write(CSR_MSTATUS, MSTATUS_FS_INITIAL, boundary_cpu.core());
        const CommitBoundaryResult boundary_result =
            apply_commit_boundary(boundary_cpu,
                                  bus,
                                  CommitBoundaryInput{
                                      .pc = kEntry,
                                      .next_pc = kEntry + 4,
                                      .effects = fsqrt_d_invalid_effects,
                                  });
        if (!expect(boundary_result.retired && !boundary_result.trap_taken,
                    "floating-point commit-boundary smoke should retire cleanly")) {
            return 1;
        }
        if (!expect((boundary_cpu.csr().read(CSR_MSTATUS, boundary_cpu.core()) & kMstatusFsDirty) == kMstatusFsDirty,
                    "floating-point commit-boundary smoke should mark mstatus.FS dirty")) {
            return 1;
        }
        if (!expect((boundary_cpu.csr().read(CSR_MSTATUS, boundary_cpu.core()) & kMstatusSd) != 0,
                    "floating-point commit-boundary smoke should raise mstatus.SD when FS becomes dirty")) {
            return 1;
        }
        if (!expect((boundary_cpu.csr().read(CSR_SSTATUS, boundary_cpu.core()) & kMstatusFsDirty) == kMstatusFsDirty,
                    "floating-point commit-boundary smoke should expose the dirty FS state through sstatus")) {
            return 1;
        }
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    const InsnEffects fsqrt_d_inexact_effects = InstructionSemantics::execute(fsqrt_d, ctx);
    if (!expect(fsqrt_d_inexact_effects.fp_write.enable, "fsqrt.d sqrt(2.0) should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fsqrt_d_inexact_effects.csr_write.enable,
                "fsqrt.d sqrt(2.0) should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fsqrt_d_inexact_effects.csr_write.addr == CSR_FCSR,
                "fsqrt.d sqrt(2.0) should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fsqrt_d_inexact_effects.csr_write.value & 0x1fULL) == 0x1ULL,
                "fsqrt.d sqrt(2.0) should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fsqrt_d_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fsqrt.d sqrt(2.0) should preserve the current rounding-mode bits")) {
        return 1;
    }

    Insn fmul_d_flags{};
    decode(0x12f57553U, &fmul_d_flags);
    if (!expect(InstructionSemantics::supports(fmul_d_flags),
                "fmul.d overflow/underflow contract should remain supported by instruction semantics")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(1.0e308));
    cpu.core().write_fpr(15, double_to_raw_bits(1.0e308));
    const InsnEffects fmul_d_overflow_effects = InstructionSemantics::execute(fmul_d_flags, ctx);
    if (!expect(fmul_d_overflow_effects.fp_write.enable, "fmul.d max*max should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmul_d_overflow_effects.fp_write.value == 0x7ff0000000000000ULL,
                "fmul.d max*max should overflow to IEEE754 infinity")) {
        return 1;
    }
    if (!expect(fmul_d_overflow_effects.csr_write.enable,
                "fmul.d max*max should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fmul_d_overflow_effects.csr_write.addr == CSR_FCSR,
                "fmul.d max*max should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmul_d_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fmul.d max*max should set the overflow flag")) {
        return 1;
    }
    if (!expect((fmul_d_overflow_effects.csr_write.value & ~0x1fULL) == 0,
                "fmul.d max*max should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(1.0e-308));
    cpu.core().write_fpr(15, double_to_raw_bits(1.0e-308));
    const InsnEffects fmul_d_underflow_effects = InstructionSemantics::execute(fmul_d_flags, ctx);
    if (!expect(fmul_d_underflow_effects.fp_write.enable, "fmul.d min*min should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmul_d_underflow_effects.fp_write.value == 0x0000000000000000ULL,
                "fmul.d min*min should underflow to zero")) {
        return 1;
    }
    if (!expect(fmul_d_underflow_effects.csr_write.enable,
                "fmul.d min*min should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fmul_d_underflow_effects.csr_write.addr == CSR_FCSR,
                "fmul.d min*min should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmul_d_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fmul.d min*min should set the underflow flag")) {
        return 1;
    }
    if (!expect((fmul_d_underflow_effects.csr_write.value & ~0x1fULL) == 0,
                "fmul.d min*min should preserve the current rounding-mode bits")) {
        return 1;
    }
