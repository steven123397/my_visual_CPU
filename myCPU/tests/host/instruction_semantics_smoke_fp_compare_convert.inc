    cpu.core().write_fpr(15, 0xffffffff7fc00000ULL);
    Insn fclass_s{};
    decode(0xe00795d3U, &fclass_s);
    if (!expect(InstructionSemantics::supports(fclass_s), "fclass.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fclass_s_effects = InstructionSemantics::execute(fclass_s, ctx);
    if (!expect(fclass_s_effects.rd_write.enable, "fclass.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(fclass_s_effects.rd_write.rd == 11, "fclass.s should target a1/x11")) {
        return 1;
    }
    if (!expect(fclass_s_effects.rd_write.value == (1ULL << 9),
                "fclass.s should classify a quiet single-precision NaN with bit 9")) {
        return 1;
    }
    if (!expect(!fclass_s_effects.trap.valid, "legal fclass.s should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.5f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(2.5f));
    Insn feq_s{};
    decode(0xa0c5a553U, &feq_s);
    if (!expect(InstructionSemantics::supports(feq_s), "feq.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects feq_s_effects = InstructionSemantics::execute(feq_s, ctx);
    if (!expect(feq_s_effects.rd_write.enable, "feq.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(feq_s_effects.rd_write.rd == 10, "feq.s should target a0/x10")) {
        return 1;
    }
    if (!expect(feq_s_effects.rd_write.value == 1, "feq.s should report equal singles as integer 1")) {
        return 1;
    }
    if (!expect(!feq_s_effects.fp_write.enable, "feq.s should not write the fp register file")) {
        return 1;
    }
    if (!expect(!feq_s_effects.trap.valid, "legal feq.s should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.5f));
    Insn flt_s{};
    decode(0xa0b515d3U, &flt_s);
    if (!expect(InstructionSemantics::supports(flt_s), "flt.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects flt_s_effects = InstructionSemantics::execute(flt_s, ctx);
    if (!expect(flt_s_effects.rd_write.enable, "flt.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(flt_s_effects.rd_write.rd == 11, "flt.s should target a1/x11")) {
        return 1;
    }
    if (!expect(flt_s_effects.rd_write.value == 1, "flt.s should report lhs < rhs as integer 1")) {
        return 1;
    }
    if (!expect(!flt_s_effects.fp_write.enable, "flt.s should not write the fp register file")) {
        return 1;
    }
    if (!expect(!flt_s_effects.trap.valid, "legal flt.s should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.5f));
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(2.5f));
    Insn fle_s{};
    decode(0xa0c58653U, &fle_s);
    if (!expect(InstructionSemantics::supports(fle_s), "fle.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fle_s_effects = InstructionSemantics::execute(fle_s, ctx);
    if (!expect(fle_s_effects.rd_write.enable, "fle.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(fle_s_effects.rd_write.rd == 12, "fle.s should target a2/x12")) {
        return 1;
    }
    if (!expect(fle_s_effects.rd_write.value == 1, "fle.s should report lhs <= rhs as integer 1")) {
        return 1;
    }
    if (!expect(!fle_s_effects.fp_write.enable, "fle.s should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fle_s_effects.trap.valid, "legal fle.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(11, 0xffffffff7fc00000ULL);
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(2.5f));
    const InsnEffects feq_s_qnan_effects = InstructionSemantics::execute(feq_s, ctx);
    if (!expect(feq_s_qnan_effects.rd_write.enable, "feq.s with qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(feq_s_qnan_effects.rd_write.value == 0,
                "feq.s with qNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(!feq_s_qnan_effects.csr_write.enable,
                "feq.s with qNaN should remain a quiet comparison and avoid setting invalid-operation flags")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(11, 0xffffffff7f800001ULL);
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(2.5f));
    const InsnEffects feq_s_snan_effects = InstructionSemantics::execute(feq_s, ctx);
    if (!expect(feq_s_snan_effects.rd_write.enable, "feq.s with sNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(feq_s_snan_effects.rd_write.value == 0,
                "feq.s with sNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(feq_s_snan_effects.csr_write.enable,
                "feq.s with sNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(feq_s_snan_effects.csr_write.addr == CSR_FCSR,
                "feq.s with sNaN should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((feq_s_snan_effects.csr_write.value & 0x10ULL) != 0,
                "feq.s with sNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.5f));
    const InsnEffects flt_s_qnan_effects = InstructionSemantics::execute(flt_s, ctx);
    if (!expect(flt_s_qnan_effects.rd_write.enable, "flt.s with qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(flt_s_qnan_effects.rd_write.value == 0,
                "flt.s with qNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(flt_s_qnan_effects.csr_write.enable,
                "flt.s with qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((flt_s_qnan_effects.csr_write.value & 0x10ULL) != 0,
                "flt.s with qNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(11, 0xffffffff7f800001ULL);
    cpu.core().write_fpr(12, float_to_fpr_raw_bits(2.5f));
    const InsnEffects fle_s_snan_effects = InstructionSemantics::execute(fle_s, ctx);
    if (!expect(fle_s_snan_effects.rd_write.enable, "fle.s with sNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fle_s_snan_effects.rd_write.value == 0,
                "fle.s with sNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(fle_s_snan_effects.csr_write.enable,
                "fle.s with sNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fle_s_snan_effects.csr_write.value & 0x10ULL) != 0,
                "fle.s with sNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.core().write_fpr(15, 0x7ff8000000000000ULL);
    Insn fclass_d{};
    decode(0xe20795d3U, &fclass_d);
    if (!expect(InstructionSemantics::supports(fclass_d), "fclass.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fclass_d_effects = InstructionSemantics::execute(fclass_d, ctx);
    if (!expect(fclass_d_effects.rd_write.enable, "fclass.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(fclass_d_effects.rd_write.rd == 11, "fclass.d should target a1/x11")) {
        return 1;
    }
    if (!expect(fclass_d_effects.rd_write.value == (1ULL << 9),
                "fclass.d should classify a quiet double-precision NaN with bit 9")) {
        return 1;
    }
    if (!expect(!fclass_d_effects.trap.valid, "legal fclass.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.25f));
    Insn fmax_s{};
    decode(0x28b514d3U, &fmax_s);
    if (!expect(InstructionSemantics::supports(fmax_s), "fmax.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmax_s_effects = InstructionSemantics::execute(fmax_s, ctx);
    if (!expect(fmax_s_effects.fp_write.enable, "fmax.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmax_s_effects.fp_write.rd == 9, "fmax.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fmax_s_effects.fp_write.value == float_to_fpr_raw_bits(2.25f),
                "fmax.s should select the larger source single")) {
        return 1;
    }
    if (!expect(!fmax_s_effects.trap.valid, "legal fmax.s should not raise a trap")) {
        return 1;
    }

    Insn fmin_s{};
    decode(0x28b504d3U, &fmin_s);
    if (!expect(InstructionSemantics::supports(fmin_s), "fmin.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmin_s_effects = InstructionSemantics::execute(fmin_s, ctx);
    if (!expect(fmin_s_effects.fp_write.enable, "fmin.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmin_s_effects.fp_write.rd == 9, "fmin.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fmin_s_effects.fp_write.value == float_to_fpr_raw_bits(1.5f),
                "fmin.s should select the smaller source single")) {
        return 1;
    }
    if (!expect(!fmin_s_effects.trap.valid, "legal fmin.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.25f));
    const InsnEffects fmax_s_qnan_effects = InstructionSemantics::execute(fmax_s, ctx);
    if (!expect(fmax_s_qnan_effects.fp_write.enable, "fmax.s with qNaN should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmax_s_qnan_effects.fp_write.value == float_to_fpr_raw_bits(2.25f),
                "fmax.s with qNaN should return the non-NaN operand")) {
        return 1;
    }
    if (!expect(!fmax_s_qnan_effects.csr_write.enable,
                "fmax.s with qNaN should not set invalid-operation flags")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7f800001ULL);
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.25f));
    const InsnEffects fmin_s_snan_effects = InstructionSemantics::execute(fmin_s, ctx);
    if (!expect(fmin_s_snan_effects.fp_write.enable, "fmin.s with sNaN should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmin_s_snan_effects.fp_write.value == float_to_fpr_raw_bits(2.25f),
                "fmin.s with sNaN should return the non-NaN operand")) {
        return 1;
    }
    if (!expect(fmin_s_snan_effects.csr_write.enable,
                "fmin.s with sNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fmin_s_snan_effects.csr_write.value & 0x10ULL) != 0,
                "fmin.s with sNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7fc00001ULL);
    cpu.core().write_fpr(11, 0xffffffff7fc00000ULL);
    const InsnEffects fmax_s_both_qnan_effects = InstructionSemantics::execute(fmax_s, ctx);
    if (!expect(fmax_s_both_qnan_effects.fp_write.enable, "fmax.s with two qNaNs should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmax_s_both_qnan_effects.fp_write.value == 0xffffffff7fc00000ULL,
                "fmax.s with two qNaNs should produce the canonical single-precision NaN")) {
        return 1;
    }
    if (!expect(!fmax_s_both_qnan_effects.csr_write.enable,
                "fmax.s with two qNaNs should not set invalid-operation flags")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
    Insn fcvt_w_s_dyn{};
    fcvt_w_s_dyn.raw = 0xc0057553U;
    fcvt_w_s_dyn.opcode = 0x53;
    fcvt_w_s_dyn.rd = 10;
    fcvt_w_s_dyn.funct3 = 7;
    fcvt_w_s_dyn.rs1 = 10;
    fcvt_w_s_dyn.rs2 = 0;
    fcvt_w_s_dyn.funct7 = 0x60;
    const InsnEffects fcvt_w_s_dyn_effects = InstructionSemantics::execute(fcvt_w_s_dyn, ctx);
    if (!expect(fcvt_w_s_dyn_effects.rd_write.enable,
                "fcvt.w.s dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_s_dyn_effects.rd_write.value == 8,
                "fcvt.w.s dyn should obey frm=RUP and round 7.5f toward positive infinity")) {
        return 1;
    }
    if (!expect(fcvt_w_s_dyn_effects.csr_write.enable,
                "fcvt.w.s dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_w_s_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.w.s dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_w_s_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.w.s dyn should set the inexact-operation flag for 7.5f")) {
        return 1;
    }
    if (!expect((fcvt_w_s_dyn_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fcvt.w.s dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.5f));
    Insn fcvt_wu_s_dyn{};
    fcvt_wu_s_dyn.raw = 0xc0157553U;
    fcvt_wu_s_dyn.opcode = 0x53;
    fcvt_wu_s_dyn.rd = 10;
    fcvt_wu_s_dyn.funct3 = 7;
    fcvt_wu_s_dyn.rs1 = 10;
    fcvt_wu_s_dyn.rs2 = 1;
    fcvt_wu_s_dyn.funct7 = 0x60;
    const InsnEffects fcvt_wu_s_dyn_effects = InstructionSemantics::execute(fcvt_wu_s_dyn, ctx);
    if (!expect(fcvt_wu_s_dyn_effects.rd_write.enable,
                "fcvt.wu.s dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_dyn_effects.rd_write.value == 4,
                "fcvt.wu.s dyn should obey frm=RMM and round 3.5f away from zero")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_dyn_effects.csr_write.enable,
                "fcvt.wu.s dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.wu.s dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.wu.s dyn should set the inexact-operation flag for 3.5f")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_dyn_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fcvt.wu.s dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.25f));
    const InsnEffects fcvt_wu_s_dyn_nearest_effects = InstructionSemantics::execute(fcvt_wu_s_dyn, ctx);
    if (!expect(fcvt_wu_s_dyn_nearest_effects.rd_write.enable,
                "fcvt.wu.s dyn with non-tie input should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_dyn_nearest_effects.rd_write.value == 3,
                "fcvt.wu.s dyn should round 3.25f to the nearest integer under frm=RMM")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_dyn_nearest_effects.csr_write.enable,
                "fcvt.wu.s dyn with non-tie input should still produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_dyn_nearest_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.wu.s dyn should set the inexact-operation flag for 3.25f")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_dyn_nearest_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fcvt.wu.s dyn with non-tie input should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
    Insn fcvt_w_s{};
    fcvt_w_s.raw = 0xc0051553U;
    fcvt_w_s.opcode = 0x53;
    fcvt_w_s.rd = 10;
    fcvt_w_s.funct3 = 1;
    fcvt_w_s.rs1 = 10;
    fcvt_w_s.rs2 = 0;
    fcvt_w_s.funct7 = 0x60;
    if (!expect(InstructionSemantics::supports(fcvt_w_s), "fcvt.w.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_w_s_effects = InstructionSemantics::execute(fcvt_w_s, ctx);
    if (!expect(fcvt_w_s_effects.rd_write.enable, "fcvt.w.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_s_effects.rd_write.rd == 10, "fcvt.w.s should target a0/x10")) {
        return 1;
    }
    if (!expect(fcvt_w_s_effects.rd_write.value == 7,
                "fcvt.w.s should round the source single toward zero and write the signed 32-bit integer")) {
        return 1;
    }
    if (!expect(!fcvt_w_s_effects.fp_write.enable, "fcvt.w.s should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_w_s_effects.trap.valid, "legal fcvt.w.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
    const InsnEffects fcvt_w_s_positive_invalid_effects = InstructionSemantics::execute(fcvt_w_s, ctx);
    if (!expect(fcvt_w_s_positive_invalid_effects.rd_write.enable,
                "fcvt.w.s +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_s_positive_invalid_effects.rd_write.value ==
                    static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                "fcvt.w.s +inf should clip to INT32_MAX on invalid positive input")) {
        return 1;
    }
    if (!expect(fcvt_w_s_positive_invalid_effects.csr_write.enable,
                "fcvt.w.s +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_w_s_positive_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.w.s +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_w_s_positive_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.w.s +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(-std::numeric_limits<float>::infinity()));
    const InsnEffects fcvt_w_s_negative_invalid_effects = InstructionSemantics::execute(fcvt_w_s, ctx);
    if (!expect(fcvt_w_s_negative_invalid_effects.rd_write.enable,
                "fcvt.w.s -inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_s_negative_invalid_effects.rd_write.value ==
                    static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::min())),
                "fcvt.w.s -inf should clip to INT32_MIN on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_w_s_negative_invalid_effects.csr_write.enable,
                "fcvt.w.s -inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_w_s_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.w.s -inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_w_s_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.w.s -inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
    const InsnEffects fcvt_w_s_qnan_invalid_effects = InstructionSemantics::execute(fcvt_w_s, ctx);
    if (!expect(fcvt_w_s_qnan_invalid_effects.rd_write.enable,
                "fcvt.w.s qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_s_qnan_invalid_effects.rd_write.value ==
                    static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                "fcvt.w.s qNaN should clip to INT32_MAX on invalid NaN input")) {
        return 1;
    }
    if (!expect(fcvt_w_s_qnan_invalid_effects.csr_write.enable,
                "fcvt.w.s qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_w_s_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.w.s qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_w_s_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.w.s qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
    Insn fcvt_l_s{};
    fcvt_l_s.raw = 0xc0251553U;
    fcvt_l_s.opcode = 0x53;
    fcvt_l_s.rd = 10;
    fcvt_l_s.funct3 = 1;
    fcvt_l_s.rs1 = 10;
    fcvt_l_s.rs2 = 2;
    fcvt_l_s.funct7 = 0x60;
    if (!expect(InstructionSemantics::supports(fcvt_l_s), "fcvt.l.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_l_s_effects = InstructionSemantics::execute(fcvt_l_s, ctx);
    if (!expect(fcvt_l_s_effects.rd_write.enable, "fcvt.l.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_s_effects.rd_write.rd == 10, "fcvt.l.s should target a0/x10")) {
        return 1;
    }
    if (!expect(fcvt_l_s_effects.rd_write.value == 7,
                "fcvt.l.s should round the source single toward zero and write the signed 64-bit integer")) {
        return 1;
    }
    if (!expect(!fcvt_l_s_effects.fp_write.enable, "fcvt.l.s should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_l_s_effects.trap.valid, "legal fcvt.l.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
    Insn fcvt_l_s_dyn{};
    fcvt_l_s_dyn.raw = 0xc0257553U;
    fcvt_l_s_dyn.opcode = 0x53;
    fcvt_l_s_dyn.rd = 10;
    fcvt_l_s_dyn.funct3 = 7;
    fcvt_l_s_dyn.rs1 = 10;
    fcvt_l_s_dyn.rs2 = 2;
    fcvt_l_s_dyn.funct7 = 0x60;
    const InsnEffects fcvt_l_s_dyn_effects = InstructionSemantics::execute(fcvt_l_s_dyn, ctx);
    if (!expect(fcvt_l_s_dyn_effects.rd_write.enable,
                "fcvt.l.s dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_s_dyn_effects.rd_write.value == 8,
                "fcvt.l.s dyn should obey frm=RUP and round 7.5f toward positive infinity")) {
        return 1;
    }
    if (!expect(fcvt_l_s_dyn_effects.csr_write.enable,
                "fcvt.l.s dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_l_s_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.l.s dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_l_s_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.l.s dyn should set the inexact-operation flag for 7.5f")) {
        return 1;
    }
    if (!expect((fcvt_l_s_dyn_effects.csr_write.value & ~0x1fULL) == (3ULL << 5),
                "fcvt.l.s dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
    const InsnEffects fcvt_l_s_positive_invalid_effects = InstructionSemantics::execute(fcvt_l_s, ctx);
    if (!expect(fcvt_l_s_positive_invalid_effects.rd_write.enable,
                "fcvt.l.s +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_s_positive_invalid_effects.rd_write.value == std::numeric_limits<int64_t>::max(),
                "fcvt.l.s +inf should clip to INT64_MAX on invalid positive input")) {
        return 1;
    }
    if (!expect(fcvt_l_s_positive_invalid_effects.csr_write.enable,
                "fcvt.l.s +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_l_s_positive_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.l.s +inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_l_s_positive_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.l.s +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_l_s_positive_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.l.s +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(-std::numeric_limits<float>::infinity()));
    const InsnEffects fcvt_l_s_negative_invalid_effects = InstructionSemantics::execute(fcvt_l_s, ctx);
    if (!expect(fcvt_l_s_negative_invalid_effects.rd_write.enable,
                "fcvt.l.s -inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_s_negative_invalid_effects.rd_write.value == static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
                "fcvt.l.s -inf should clip to INT64_MIN on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_l_s_negative_invalid_effects.csr_write.enable,
                "fcvt.l.s -inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_l_s_negative_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.l.s -inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_l_s_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.l.s -inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_l_s_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.l.s -inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
    const InsnEffects fcvt_l_s_qnan_invalid_effects = InstructionSemantics::execute(fcvt_l_s, ctx);
    if (!expect(fcvt_l_s_qnan_invalid_effects.rd_write.enable,
                "fcvt.l.s qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_s_qnan_invalid_effects.rd_write.value == std::numeric_limits<int64_t>::max(),
                "fcvt.l.s qNaN should clip to INT64_MAX on invalid NaN input")) {
        return 1;
    }
    if (!expect(fcvt_l_s_qnan_invalid_effects.csr_write.enable,
                "fcvt.l.s qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_l_s_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.l.s qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_l_s_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.l.s qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(3000000000.0f));
    Insn fcvt_wu_s{};
    fcvt_wu_s.raw = 0xc0151553U;
    fcvt_wu_s.opcode = 0x53;
    fcvt_wu_s.rd = 10;
    fcvt_wu_s.funct3 = 1;
    fcvt_wu_s.rs1 = 10;
    fcvt_wu_s.rs2 = 1;
    fcvt_wu_s.funct7 = 0x60;
    if (!expect(InstructionSemantics::supports(fcvt_wu_s), "fcvt.wu.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_wu_s_effects = InstructionSemantics::execute(fcvt_wu_s, ctx);
    if (!expect(fcvt_wu_s_effects.rd_write.enable, "fcvt.wu.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_effects.rd_write.rd == 10, "fcvt.wu.s should target a0/x10")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_effects.rd_write.value ==
                    static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(3000000000u))),
                "fcvt.wu.s should sign-extend the 32-bit unsigned result on RV64")) {
        return 1;
    }
    if (!expect(!fcvt_wu_s_effects.fp_write.enable, "fcvt.wu.s should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_wu_s_effects.trap.valid, "legal fcvt.wu.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(-1.0f));
    const InsnEffects fcvt_wu_s_negative_invalid_effects = InstructionSemantics::execute(fcvt_wu_s, ctx);
    if (!expect(fcvt_wu_s_negative_invalid_effects.rd_write.enable,
                "fcvt.wu.s negative invalid input should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_negative_invalid_effects.rd_write.value == 0,
                "fcvt.wu.s -1.0f should clip to 0 on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_negative_invalid_effects.csr_write.enable,
                "fcvt.wu.s -1.0f should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_negative_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.wu.s -1.0f should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.wu.s -1.0f should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.wu.s -1.0f should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
    const InsnEffects fcvt_wu_s_invalid_effects = InstructionSemantics::execute(fcvt_wu_s, ctx);
    if (!expect(fcvt_wu_s_invalid_effects.rd_write.enable,
                "fcvt.wu.s +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_invalid_effects.rd_write.value == 0xffffffffffffffffULL,
                "fcvt.wu.s +inf should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_invalid_effects.csr_write.enable,
                "fcvt.wu.s +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.wu.s +inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.wu.s +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.wu.s +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(4294967296.0f));
    Insn fcvt_lu_s{};
    fcvt_lu_s.raw = 0xc0351553U;
    fcvt_lu_s.opcode = 0x53;
    fcvt_lu_s.rd = 10;
    fcvt_lu_s.funct3 = 1;
    fcvt_lu_s.rs1 = 10;
    fcvt_lu_s.rs2 = 3;
    fcvt_lu_s.funct7 = 0x60;
    if (!expect(InstructionSemantics::supports(fcvt_lu_s), "fcvt.lu.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_lu_s_effects = InstructionSemantics::execute(fcvt_lu_s, ctx);
    if (!expect(fcvt_lu_s_effects.rd_write.enable, "fcvt.lu.s should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_effects.rd_write.rd == 10, "fcvt.lu.s should target a0/x10")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_effects.rd_write.value == 4294967296ULL,
                "fcvt.lu.s should round the source single toward zero and write the unsigned 64-bit integer")) {
        return 1;
    }
    if (!expect(!fcvt_lu_s_effects.fp_write.enable, "fcvt.lu.s should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_lu_s_effects.trap.valid, "legal fcvt.lu.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.5f));
    Insn fcvt_lu_s_dyn{};
    fcvt_lu_s_dyn.raw = 0xc0357553U;
    fcvt_lu_s_dyn.opcode = 0x53;
    fcvt_lu_s_dyn.rd = 10;
    fcvt_lu_s_dyn.funct3 = 7;
    fcvt_lu_s_dyn.rs1 = 10;
    fcvt_lu_s_dyn.rs2 = 3;
    fcvt_lu_s_dyn.funct7 = 0x60;
    const InsnEffects fcvt_lu_s_dyn_effects = InstructionSemantics::execute(fcvt_lu_s_dyn, ctx);
    if (!expect(fcvt_lu_s_dyn_effects.rd_write.enable,
                "fcvt.lu.s dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_dyn_effects.rd_write.value == 4,
                "fcvt.lu.s dyn should obey frm=RMM and round 3.5f to the nearest integer")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_dyn_effects.csr_write.enable,
                "fcvt.lu.s dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.lu.s dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.lu.s dyn should set the inexact-operation flag for 3.5f")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_dyn_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fcvt.lu.s dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(-1.0f));
    const InsnEffects fcvt_lu_s_negative_invalid_effects = InstructionSemantics::execute(fcvt_lu_s, ctx);
    if (!expect(fcvt_lu_s_negative_invalid_effects.rd_write.enable,
                "fcvt.lu.s negative invalid input should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_negative_invalid_effects.rd_write.value == 0,
                "fcvt.lu.s -1.0f should clip to 0 on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_negative_invalid_effects.csr_write.enable,
                "fcvt.lu.s -1.0f should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_negative_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.lu.s -1.0f should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.lu.s -1.0f should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.lu.s -1.0f should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
    const InsnEffects fcvt_lu_s_positive_invalid_effects = InstructionSemantics::execute(fcvt_lu_s, ctx);
    if (!expect(fcvt_lu_s_positive_invalid_effects.rd_write.enable,
                "fcvt.lu.s +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_positive_invalid_effects.rd_write.value == std::numeric_limits<uint64_t>::max(),
                "fcvt.lu.s +inf should clip to UINT64_MAX on invalid positive input")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_positive_invalid_effects.csr_write.enable,
                "fcvt.lu.s +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_positive_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.lu.s +inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_positive_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.lu.s +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_positive_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.lu.s +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
    const InsnEffects fcvt_wu_s_qnan_invalid_effects = InstructionSemantics::execute(fcvt_wu_s, ctx);
    if (!expect(fcvt_wu_s_qnan_invalid_effects.rd_write.enable,
                "fcvt.wu.s qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_qnan_invalid_effects.rd_write.value == 0xffffffffffffffffULL,
                "fcvt.wu.s qNaN should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
        return 1;
    }
    if (!expect(fcvt_wu_s_qnan_invalid_effects.csr_write.enable,
                "fcvt.wu.s qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.wu.s qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_wu_s_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.wu.s qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
    const InsnEffects fcvt_lu_s_qnan_invalid_effects = InstructionSemantics::execute(fcvt_lu_s, ctx);
    if (!expect(fcvt_lu_s_qnan_invalid_effects.rd_write.enable,
                "fcvt.lu.s qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_qnan_invalid_effects.rd_write.value == std::numeric_limits<uint64_t>::max(),
                "fcvt.lu.s qNaN should clip to UINT64_MAX on invalid NaN input")) {
        return 1;
    }
    if (!expect(fcvt_lu_s_qnan_invalid_effects.csr_write.enable,
                "fcvt.lu.s qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.lu.s qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_lu_s_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.lu.s qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.core().write_fpr(10, double_to_raw_bits(1.5));
    cpu.core().write_fpr(11, double_to_raw_bits(2.25));
    Insn fmax_d{};
    decode(0x2ab514d3U, &fmax_d);
    if (!expect(InstructionSemantics::supports(fmax_d), "fmax.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmax_d_effects = InstructionSemantics::execute(fmax_d, ctx);
    if (!expect(fmax_d_effects.fp_write.enable, "fmax.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmax_d_effects.fp_write.rd == 9, "fmax.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fmax_d_effects.fp_write.value == double_to_raw_bits(2.25),
                "fmax.d should select the larger source double")) {
        return 1;
    }
    if (!expect(!fmax_d_effects.trap.valid, "legal fmax.d should not raise a trap")) {
        return 1;
    }

    Insn fmin_d{};
    decode(0x2ab504d3U, &fmin_d);
    if (!expect(InstructionSemantics::supports(fmin_d), "fmin.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmin_d_effects = InstructionSemantics::execute(fmin_d, ctx);
    if (!expect(fmin_d_effects.fp_write.enable, "fmin.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmin_d_effects.fp_write.rd == 9, "fmin.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fmin_d_effects.fp_write.value == double_to_raw_bits(1.5),
                "fmin.d should select the smaller source double")) {
        return 1;
    }
    if (!expect(!fmin_d_effects.trap.valid, "legal fmin.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(10, 0x7ff8000000000000ULL);
    cpu.core().write_fpr(11, double_to_raw_bits(2.25));
    const InsnEffects fmax_d_qnan_effects = InstructionSemantics::execute(fmax_d, ctx);
    if (!expect(fmax_d_qnan_effects.fp_write.enable, "fmax.d with qNaN should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmax_d_qnan_effects.fp_write.value == double_to_raw_bits(2.25),
                "fmax.d with qNaN should return the non-NaN operand")) {
        return 1;
    }
    if (!expect(!fmax_d_qnan_effects.csr_write.enable,
                "fmax.d with qNaN should not set invalid-operation flags")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(10, 0x7ff0000000000001ULL);
    cpu.core().write_fpr(11, double_to_raw_bits(2.25));
    const InsnEffects fmin_d_snan_effects = InstructionSemantics::execute(fmin_d, ctx);
    if (!expect(fmin_d_snan_effects.fp_write.enable, "fmin.d with sNaN should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmin_d_snan_effects.fp_write.value == double_to_raw_bits(2.25),
                "fmin.d with sNaN should return the non-NaN operand")) {
        return 1;
    }
    if (!expect(fmin_d_snan_effects.csr_write.enable,
                "fmin.d with sNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fmin_d_snan_effects.csr_write.value & 0x10ULL) != 0,
                "fmin.d with sNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(10, 0x7ff8000000000001ULL);
    cpu.core().write_fpr(11, 0x7ff8000000000000ULL);
    const InsnEffects fmax_d_both_qnan_effects = InstructionSemantics::execute(fmax_d, ctx);
    if (!expect(fmax_d_both_qnan_effects.fp_write.enable, "fmax.d with two qNaNs should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fmax_d_both_qnan_effects.fp_write.value == 0x7ff8000000000000ULL,
                "fmax.d with two qNaNs should produce the canonical double-precision NaN")) {
        return 1;
    }
    if (!expect(!fmax_d_both_qnan_effects.csr_write.enable,
                "fmax.d with two qNaNs should not set invalid-operation flags")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
    Insn fcvt_d_s{};
    fcvt_d_s.raw = 0x421507d3U;
    fcvt_d_s.opcode = 0x53;
    fcvt_d_s.rd = 15;
    fcvt_d_s.funct3 = 0;
    fcvt_d_s.rs1 = 10;
    fcvt_d_s.rs2 = 1;
    fcvt_d_s.funct7 = 0x21;

    if (!expect(InstructionSemantics::supports(fcvt_d_s), "fcvt.d.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_d_s_effects = InstructionSemantics::execute(fcvt_d_s, ctx);
    if (!expect(fcvt_d_s_effects.fp_write.enable, "fcvt.d.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_d_s_effects.fp_write.rd == 15, "fcvt.d.s should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_d_s_effects.fp_write.value == double_to_raw_bits(1.5),
                "fcvt.d.s should widen the source single-precision value into exact double bits")) {
        return 1;
    }
    if (!expect(!fcvt_d_s_effects.trap.valid, "legal fcvt.d.s should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, double_to_raw_bits(1.5));
    Insn fcvt_s_d{};
    fcvt_s_d.raw = 0x401507d3U;
    fcvt_s_d.opcode = 0x53;
    fcvt_s_d.rd = 15;
    fcvt_s_d.funct3 = 0;
    fcvt_s_d.rs1 = 10;
    fcvt_s_d.rs2 = 1;
    fcvt_s_d.funct7 = 0x20;

    if (!expect(InstructionSemantics::supports(fcvt_s_d), "fcvt.s.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_s_d_effects = InstructionSemantics::execute(fcvt_s_d, ctx);
    if (!expect(fcvt_s_d_effects.fp_write.enable, "fcvt.s.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_s_d_effects.fp_write.rd == 15, "fcvt.s.d should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_s_d_effects.fp_write.value == float_to_fpr_raw_bits(1.5f),
                "fcvt.s.d should narrow the source double into nan-boxed single-precision bits")) {
        return 1;
    }
    if (!expect(!fcvt_s_d_effects.trap.valid, "legal fcvt.s.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(10, 7);
    Insn fcvt_s_w{};
    fcvt_s_w.raw = 0xd00507d3U;
    fcvt_s_w.opcode = 0x53;
    fcvt_s_w.rd = 15;
    fcvt_s_w.funct3 = 0;
    fcvt_s_w.rs1 = 10;
    fcvt_s_w.rs2 = 0;
    fcvt_s_w.funct7 = 0x68;

    if (!expect(InstructionSemantics::supports(fcvt_s_w), "fcvt.s.w should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_s_w_effects = InstructionSemantics::execute(fcvt_s_w, ctx);
    if (!expect(fcvt_s_w_effects.fp_write.enable, "fcvt.s.w should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_s_w_effects.fp_write.rd == 15, "fcvt.s.w should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_s_w_effects.fp_write.value == float_to_fpr_raw_bits(7.0f),
                "fcvt.s.w should narrow the integer source into nan-boxed single-precision bits")) {
        return 1;
    }
    if (!expect(!fcvt_s_w_effects.trap.valid, "legal fcvt.s.w should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(10, 0xffffffffULL);
    Insn fcvt_s_wu{};
    fcvt_s_wu.raw = 0xd01507d3U;
    fcvt_s_wu.opcode = 0x53;
    fcvt_s_wu.rd = 15;
    fcvt_s_wu.funct3 = 0;
    fcvt_s_wu.rs1 = 10;
    fcvt_s_wu.rs2 = 1;
    fcvt_s_wu.funct7 = 0x68;

    if (!expect(InstructionSemantics::supports(fcvt_s_wu), "fcvt.s.wu should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_s_wu_effects = InstructionSemantics::execute(fcvt_s_wu, ctx);
    if (!expect(fcvt_s_wu_effects.fp_write.enable, "fcvt.s.wu should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_s_wu_effects.fp_write.rd == 15, "fcvt.s.wu should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_s_wu_effects.fp_write.value ==
                    float_to_fpr_raw_bits(static_cast<float>(static_cast<uint32_t>(0xffffffffULL))),
                "fcvt.s.wu should convert the unsigned 32-bit integer into nan-boxed single-precision bits")) {
        return 1;
    }
    if (!expect(!fcvt_s_wu_effects.trap.valid, "legal fcvt.s.wu should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(10, 1ULL << 40);
    Insn fcvt_s_l{};
    fcvt_s_l.raw = 0xd02507d3U;
    fcvt_s_l.opcode = 0x53;
    fcvt_s_l.rd = 15;
    fcvt_s_l.funct3 = 0;
    fcvt_s_l.rs1 = 10;
    fcvt_s_l.rs2 = 2;
    fcvt_s_l.funct7 = 0x68;

    if (!expect(InstructionSemantics::supports(fcvt_s_l), "fcvt.s.l should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_s_l_effects = InstructionSemantics::execute(fcvt_s_l, ctx);
    if (!expect(fcvt_s_l_effects.fp_write.enable, "fcvt.s.l should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_s_l_effects.fp_write.rd == 15, "fcvt.s.l should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_s_l_effects.fp_write.value ==
                    float_to_fpr_raw_bits(static_cast<float>(static_cast<int64_t>(1ULL << 40))),
                "fcvt.s.l should convert the signed 64-bit integer into nan-boxed single-precision bits")) {
        return 1;
    }
    if (!expect(!fcvt_s_l_effects.trap.valid, "legal fcvt.s.l should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(10, 1ULL << 63);
    Insn fcvt_s_lu{};
    fcvt_s_lu.raw = 0xd03507d3U;
    fcvt_s_lu.opcode = 0x53;
    fcvt_s_lu.rd = 15;
    fcvt_s_lu.funct3 = 0;
    fcvt_s_lu.rs1 = 10;
    fcvt_s_lu.rs2 = 3;
    fcvt_s_lu.funct7 = 0x68;

    if (!expect(InstructionSemantics::supports(fcvt_s_lu), "fcvt.s.lu should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_s_lu_effects = InstructionSemantics::execute(fcvt_s_lu, ctx);
    if (!expect(fcvt_s_lu_effects.fp_write.enable, "fcvt.s.lu should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_s_lu_effects.fp_write.rd == 15, "fcvt.s.lu should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_s_lu_effects.fp_write.value ==
                    float_to_fpr_raw_bits(static_cast<float>(1ULL << 63)),
                "fcvt.s.lu should convert the unsigned 64-bit integer into nan-boxed single-precision bits")) {
        return 1;
    }
    if (!expect(!fcvt_s_lu_effects.trap.valid, "legal fcvt.s.lu should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FRM, 0, cpu.core());
    cpu.core().write_fpr(18, float_to_fpr_raw_bits(5.75f));
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    Insn fsub_s{};
    fsub_s.raw = 0x08a97553U;
    fsub_s.opcode = 0x53;
    fsub_s.rd = 10;
    fsub_s.funct3 = 7;
    fsub_s.rs1 = 18;
    fsub_s.rs2 = 10;
    fsub_s.funct7 = 0x04;

    if (!expect(InstructionSemantics::supports(fsub_s), "fsub.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsub_s_effects = InstructionSemantics::execute(fsub_s, ctx);
    if (!expect(fsub_s_effects.fp_write.enable, "fsub.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsub_s_effects.fp_write.rd == 10, "fsub.s should target fa0/f10")) {
        return 1;
    }
    if (!expect(fsub_s_effects.fp_write.value == float_to_fpr_raw_bits(3.75f),
                "fsub.s should subtract the source fp registers and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fsub_s_effects.rd_write.enable, "fsub.s should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fsub_s_effects.trap.valid, "legal fsub.s should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(18, double_to_raw_bits(5.75));
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    Insn fsub_d{};
    fsub_d.raw = 0x0aa97553U;
    fsub_d.opcode = 0x53;
    fsub_d.rd = 10;
    fsub_d.funct3 = 7;
    fsub_d.rs1 = 18;
    fsub_d.rs2 = 10;
    fsub_d.funct7 = 0x05;

    if (!expect(InstructionSemantics::supports(fsub_d), "fsub.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsub_d_effects = InstructionSemantics::execute(fsub_d, ctx);
    if (!expect(fsub_d_effects.fp_write.enable, "fsub.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsub_d_effects.fp_write.rd == 10, "fsub.d should target fa0/f10")) {
        return 1;
    }
    if (!expect(fsub_d_effects.fp_write.value == double_to_raw_bits(3.75),
                "fsub.d should subtract the source fp registers and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fsub_d_effects.rd_write.enable, "fsub.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fsub_d_effects.trap.valid, "legal fsub.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(9, float_to_fpr_raw_bits(7.0f));
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    Insn fdiv_s{};
    fdiv_s.raw = 0x18a4f4d3U;
    fdiv_s.opcode = 0x53;
    fdiv_s.rd = 9;
    fdiv_s.funct3 = 7;
    fdiv_s.rs1 = 9;
    fdiv_s.rs2 = 10;
    fdiv_s.funct7 = 0x0c;

    if (!expect(InstructionSemantics::supports(fdiv_s), "fdiv.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fdiv_s_effects = InstructionSemantics::execute(fdiv_s, ctx);
    if (!expect(fdiv_s_effects.fp_write.enable, "fdiv.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fdiv_s_effects.fp_write.rd == 9, "fdiv.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fdiv_s_effects.fp_write.value == float_to_fpr_raw_bits(3.5f),
                "fdiv.s should divide the source fp registers and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fdiv_s_effects.rd_write.enable, "fdiv.s should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fdiv_s_effects.trap.valid, "legal fdiv.s should not raise a trap")) {
        return 1;
    }
    if (!expect(fdiv_s_effects.floating_state_touched,
                "fdiv.s should mark that the architectural floating-point state was touched")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(9, float_to_fpr_raw_bits(7.0f));
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fdiv_s_flags_effects = InstructionSemantics::execute(fdiv_s, ctx);
    if (!expect(fdiv_s_flags_effects.csr_write.enable, "fdiv.s should produce a floating-point CSR write for exception flags")) {
        return 1;
    }
    if (!expect(fdiv_s_flags_effects.csr_write.addr == CSR_FCSR, "fdiv.s should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fdiv_s_flags_effects.csr_write.value & 0x1fULL) == 0x8ULL,
                "fdiv.s by zero should set the divide-by-zero flag")) {
        return 1;
    }
    if (!expect((fdiv_s_flags_effects.csr_write.value & ~0x1fULL) == 0,
                "fdiv.s by zero should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(9, float_to_fpr_raw_bits(0.0f));
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(0.0f));
    const InsnEffects fdiv_s_invalid_effects = InstructionSemantics::execute(fdiv_s, ctx);
    if (!expect(fdiv_s_invalid_effects.fp_write.enable, "fdiv.s 0/0 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(std::isnan(raw_bits_to_float(fdiv_s_invalid_effects.fp_write.value)),
                "fdiv.s 0/0 should produce a NaN result")) {
        return 1;
    }
    if (!expect(fdiv_s_invalid_effects.csr_write.enable,
                "fdiv.s 0/0 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fdiv_s_invalid_effects.csr_write.addr == CSR_FCSR,
                "fdiv.s 0/0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fdiv_s_invalid_effects.csr_write.value & 0x1fULL) == 0x10ULL,
                "fdiv.s 0/0 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fdiv_s_invalid_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fdiv.s 0/0 should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
    cpu.core().write_fpr(9, float_to_fpr_raw_bits(1.0f));
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.0f));
    const InsnEffects fdiv_s_inexact_effects = InstructionSemantics::execute(fdiv_s, ctx);
    if (!expect(fdiv_s_inexact_effects.fp_write.enable, "fdiv.s 1/3 should still produce an fp register write")) {
        return 1;
    }
    if (!expect(fdiv_s_inexact_effects.csr_write.enable,
                "fdiv.s 1/3 should produce a floating-point CSR write for inexact-operation flags")) {
        return 1;
    }
    if (!expect(fdiv_s_inexact_effects.csr_write.addr == CSR_FCSR,
                "fdiv.s 1/3 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fdiv_s_inexact_effects.csr_write.value & 0x1ULL) != 0,
                "fdiv.s 1/3 should set the inexact-operation flag")) {
        return 1;
    }
    if (!expect((fdiv_s_inexact_effects.csr_write.value & ~0x1fULL) == (1ULL << 5),
                "fdiv.s 1/3 should preserve the current rounding-mode bits")) {
        return 1;
    }

