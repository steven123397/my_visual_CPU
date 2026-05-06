    cpu.core().write_gpr(1, 5);
    Insn addi{};
    addi.raw = 0x00708293U;
    addi.opcode = 0x13;
    addi.rd = 5;
    addi.funct3 = 0;
    addi.rs1 = 1;
    addi.imm = 7;

    if (!expect(InstructionSemantics::supports(addi), "addi should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects addi_effects = InstructionSemantics::execute(addi, ctx);
    if (!expect(addi_effects.rd_write.enable, "addi should produce a register write")) {
        return 1;
    }
    if (!expect(addi_effects.rd_write.rd == 5, "addi should target x5")) {
        return 1;
    }
    if (!expect(addi_effects.rd_write.value == 12, "addi should compute x1 + imm")) {
        return 1;
    }
    if (!expect(!addi_effects.control.redirect_pc, "addi should not redirect pc")) {
        return 1;
    }
    if (!expect(!addi_effects.trap.valid, "addi should not raise a trap")) {
        return 1;
    }

    cpu.core().set_pc(0x80000040);
    Insn jal{};
    jal.raw = 0x010000efU;
    jal.opcode = 0x6F;
    jal.rd = 1;
    jal.imm = 16;

    if (!expect(InstructionSemantics::supports(jal), "jal should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects jal_effects = InstructionSemantics::execute(jal, ctx);
    if (!expect(jal_effects.rd_write.enable, "jal should write the link register")) {
        return 1;
    }
    if (!expect(jal_effects.rd_write.rd == 1, "jal should target x1")) {
        return 1;
    }
    if (!expect(jal_effects.rd_write.value == 0x80000044ULL, "jal should write pc + 4")) {
        return 1;
    }
    if (!expect(jal_effects.control.redirect_pc, "jal should redirect pc")) {
        return 1;
    }
    if (!expect(jal_effects.control.target_pc == 0x80000050ULL, "jal should redirect to pc + imm")) {
        return 1;
    }
    if (!expect(!jal_effects.trap.valid, "jal should not raise a trap")) {
        return 1;
    }

    Insn illegal_branch{};
    illegal_branch.raw = 0xffffffffU;
    illegal_branch.opcode = 0x63;
    illegal_branch.funct3 = 2;
    illegal_branch.rs1 = 1;
    illegal_branch.rs2 = 2;
    illegal_branch.imm = 8;

    if (!expect(InstructionSemantics::supports(illegal_branch), "branch family should be routed through instruction semantics")) {
        return 1;
    }
    const InsnEffects illegal_effects = InstructionSemantics::execute(illegal_branch, ctx);
    if (!expect(illegal_effects.trap.valid, "illegal branch encoding should raise a trap effect")) {
        return 1;
    }
    if (!expect(illegal_effects.trap.cause == 2, "illegal branch trap cause should be illegal instruction")) {
        return 1;
    }
    if (!expect(illegal_effects.trap.tval == illegal_branch.raw, "illegal branch trap tval should preserve raw instruction")) {
        return 1;
    }

    Insn load{};
    load.raw = 0x0040a303U;
    load.opcode = 0x03;
    load.rd = 6;
    load.funct3 = 2;
    load.rs1 = 1;
    load.imm = 4;

    if (!expect(InstructionSemantics::supports(load), "load should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects load_effects = InstructionSemantics::execute(load, ctx);
    if (!expect(load_effects.mem.kind == MemoryRequest::Kind::Load, "load should produce a load memory request")) {
        return 1;
    }
    if (!expect(load_effects.mem.addr == 9, "load should compute base plus offset")) {
        return 1;
    }
    if (!expect(load_effects.mem.size == 4, "lw should request 4 bytes")) {
        return 1;
    }
    if (!expect(load_effects.mem.sign_extend, "lw should request sign extension")) {
        return 1;
    }
    if (!expect(load_effects.mem.rd == 6, "load should preserve destination register")) {
        return 1;
    }
    if (!expect(!load_effects.trap.valid, "legal load should not raise a trap")) {
        return 1;
    }

    Insn store{};
    store.raw = 0x0020a223U;
    store.opcode = 0x23;
    store.funct3 = 2;
    store.rs1 = 1;
    store.rs2 = 2;
    store.imm = 8;
    cpu.core().write_gpr(2, 0x11223344ULL);

    const InsnEffects store_effects = InstructionSemantics::execute(store, ctx);
    if (!expect(store_effects.mem.kind == MemoryRequest::Kind::Store, "store should produce a store memory request")) {
        return 1;
    }
    if (!expect(store_effects.mem.addr == 13, "store should compute base plus offset")) {
        return 1;
    }
    if (!expect(store_effects.mem.size == 4, "sw should request 4 bytes")) {
        return 1;
    }
    if (!expect(store_effects.mem.store_value == 0x11223344ULL, "store should preserve the source register value")) {
        return 1;
    }
    if (!expect(!store_effects.trap.valid, "legal store should not raise a trap")) {
        return 1;
    }

    Insn fld{};
    fld.raw = 0x0080b287U;
    fld.opcode = 0x07;
    fld.rd = 5;
    fld.funct3 = 3;
    fld.rs1 = 1;
    fld.imm = 8;

    if (!expect(InstructionSemantics::supports(fld), "fld should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fld_effects = InstructionSemantics::execute(fld, ctx);
    if (!expect(fld_effects.mem.kind == MemoryRequest::Kind::Load, "fld should produce a load memory request")) {
        return 1;
    }
    if (!expect(fld_effects.mem.rd == 5, "fld should preserve destination f register")) {
        return 1;
    }
    if (!expect(fld_effects.mem.size == 8, "fld should request 8 bytes")) {
        return 1;
    }
    if (!expect(fld_effects.mem.target == MemoryRequest::Target::Float,
                "fld should target the floating-point register file")) {
        return 1;
    }
    if (!expect(!fld_effects.trap.valid, "legal fld should not raise a trap")) {
        return 1;
    }

    Insn fsd{};
    fsd.raw = 0x0050b427U;
    fsd.opcode = 0x27;
    fsd.funct3 = 3;
    fsd.rs1 = 1;
    fsd.rs2 = 5;
    fsd.imm = 8;
    cpu.core().write_fpr(5, 0x8877665544332211ULL);

    const InsnEffects fsd_effects = InstructionSemantics::execute(fsd, ctx);
    if (!expect(fsd_effects.mem.kind == MemoryRequest::Kind::Store, "fsd should produce a store memory request")) {
        return 1;
    }
    if (!expect(fsd_effects.mem.size == 8, "fsd should request 8 bytes")) {
        return 1;
    }
    if (!expect(fsd_effects.mem.store_value == 0x8877665544332211ULL,
                "fsd should preserve the source f register bits")) {
        return 1;
    }
    if (!expect(fsd_effects.mem.target == MemoryRequest::Target::Float,
                "fsd should identify the floating-point register file")) {
        return 1;
    }
    if (!expect(!fsd_effects.trap.valid, "legal fsd should not raise a trap")) {
        return 1;
    }

    Insn fmv_d_x{};
    fmv_d_x.raw = 0xf2000553U;
    fmv_d_x.opcode = 0x53;
    fmv_d_x.rd = 10;
    fmv_d_x.funct3 = 0;
    fmv_d_x.rs1 = 0;
    fmv_d_x.rs2 = 0;
    fmv_d_x.funct7 = 0x79;

    if (!expect(InstructionSemantics::supports(fmv_d_x), "fmv.d.x should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmv_d_x_effects = InstructionSemantics::execute(fmv_d_x, ctx);
    if (!expect(fmv_d_x_effects.fp_write.enable, "fmv.d.x should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmv_d_x_effects.fp_write.rd == 10, "fmv.d.x should target fa0/f10")) {
        return 1;
    }
    if (!expect(fmv_d_x_effects.fp_write.value == 0, "fmv.d.x should copy raw integer bits into the fp register file")) {
        return 1;
    }
    if (!expect(!fmv_d_x_effects.rd_write.enable, "fmv.d.x should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fmv_d_x_effects.trap.valid, "legal fmv.d.x should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, 0x8877665544332211ULL);
    Insn fmv_x_d{};
    fmv_x_d.raw = 0xe2050553U;
    fmv_x_d.opcode = 0x53;
    fmv_x_d.rd = 10;
    fmv_x_d.funct3 = 0;
    fmv_x_d.rs1 = 10;
    fmv_x_d.rs2 = 0;
    fmv_x_d.funct7 = 0x71;

    if (!expect(InstructionSemantics::supports(fmv_x_d), "fmv.x.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmv_x_d_effects = InstructionSemantics::execute(fmv_x_d, ctx);
    if (!expect(fmv_x_d_effects.rd_write.enable, "fmv.x.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(fmv_x_d_effects.rd_write.rd == 10, "fmv.x.d should target a0/x10")) {
        return 1;
    }
    if (!expect(fmv_x_d_effects.rd_write.value == 0x8877665544332211ULL,
                "fmv.x.d should copy raw fp bits into the integer register file")) {
        return 1;
    }
    if (!expect(!fmv_x_d_effects.fp_write.enable, "fmv.x.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fmv_x_d_effects.trap.valid, "legal fmv.x.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(14, static_cast<uint64_t>(static_cast<uint32_t>(-76)));
    Insn fcvt_d_w{};
    fcvt_d_w.raw = 0xd20707d3U;
    fcvt_d_w.opcode = 0x53;
    fcvt_d_w.rd = 15;
    fcvt_d_w.funct3 = 0;
    fcvt_d_w.rs1 = 14;
    fcvt_d_w.rs2 = 0;
    fcvt_d_w.funct7 = 0x69;

    if (!expect(InstructionSemantics::supports(fcvt_d_w), "fcvt.d.w should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_d_w_effects = InstructionSemantics::execute(fcvt_d_w, ctx);
    if (!expect(fcvt_d_w_effects.fp_write.enable, "fcvt.d.w should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_d_w_effects.fp_write.rd == 15, "fcvt.d.w should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_d_w_effects.fp_write.value == double_to_raw_bits(-76.0),
                "fcvt.d.w should convert the signed 32-bit integer source into IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fcvt_d_w_effects.rd_write.enable, "fcvt.d.w should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fcvt_d_w_effects.trap.valid, "legal fcvt.d.w should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(15, 0x1122334455667788ULL);
    Insn fmv_d{};
    fmv_d.raw = 0x22f78553U;
    fmv_d.opcode = 0x53;
    fmv_d.rd = 10;
    fmv_d.funct3 = 0;
    fmv_d.rs1 = 15;
    fmv_d.rs2 = 15;
    fmv_d.funct7 = 0x11;

    if (!expect(InstructionSemantics::supports(fmv_d), "fmv.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmv_d_effects = InstructionSemantics::execute(fmv_d, ctx);
    if (!expect(fmv_d_effects.fp_write.enable, "fmv.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmv_d_effects.fp_write.rd == 10, "fmv.d should target fa0/f10")) {
        return 1;
    }
    if (!expect(fmv_d_effects.fp_write.value == 0x1122334455667788ULL,
                "fmv.d should copy raw fp bits between fp registers")) {
        return 1;
    }
    if (!expect(!fmv_d_effects.rd_write.enable, "fmv.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fmv_d_effects.trap.valid, "legal fmv.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FRM, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.75f));
    cpu.core().write_fpr(15, float_to_fpr_raw_bits(2.0f));
    Insn fmul_s{};
    fmul_s.raw = 0x10f57553U;
    fmul_s.opcode = 0x53;
    fmul_s.rd = 10;
    fmul_s.funct3 = 7;
    fmul_s.rs1 = 10;
    fmul_s.rs2 = 15;
    fmul_s.funct7 = 0x08;

    if (!expect(InstructionSemantics::supports(fmul_s), "fmul.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmul_s_effects = InstructionSemantics::execute(fmul_s, ctx);
    if (!expect(fmul_s_effects.fp_write.enable, "fmul.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmul_s_effects.fp_write.rd == 10, "fmul.s should target fa0/f10")) {
        return 1;
    }
    if (!expect(fmul_s_effects.fp_write.value == float_to_fpr_raw_bits(7.5f),
                "fmul.s should multiply the source fp registers and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fmul_s_effects.rd_write.enable, "fmul.s should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fmul_s_effects.trap.valid, "legal fmul.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    cpu.core().write_fpr(15, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
    const InsnEffects fmul_s_overflow_effects = InstructionSemantics::execute(fmul_s, ctx);
    if (!expect(fmul_s_overflow_effects.fp_write.enable, "fmul.s max*max should still produce an fp register write")) {
        return 1;
    }
    if (!expect((fmul_s_overflow_effects.fp_write.value & 0xffffffffULL) == 0x7f800000ULL,
                "fmul.s max*max should overflow to IEEE754 single infinity")) {
        return 1;
    }
    if (!expect(fmul_s_overflow_effects.csr_write.enable,
                "fmul.s max*max should produce a floating-point CSR write for overflow flags")) {
        return 1;
    }
    if (!expect(fmul_s_overflow_effects.csr_write.addr == CSR_FCSR,
                "fmul.s max*max should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmul_s_overflow_effects.csr_write.value & 0x4ULL) != 0,
                "fmul.s max*max should set the overflow flag")) {
        return 1;
    }
    if (!expect((fmul_s_overflow_effects.csr_write.value & ~0x1fULL) == 0,
                "fmul.s max*max should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    cpu.core().write_fpr(15, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
    const InsnEffects fmul_s_underflow_effects = InstructionSemantics::execute(fmul_s, ctx);
    if (!expect(fmul_s_underflow_effects.fp_write.enable, "fmul.s min*min should still produce an fp register write")) {
        return 1;
    }
    if (!expect((fmul_s_underflow_effects.fp_write.value & 0xffffffffULL) == 0x00000000ULL,
                "fmul.s min*min should underflow to zero")) {
        return 1;
    }
    if (!expect(fmul_s_underflow_effects.csr_write.enable,
                "fmul.s min*min should produce a floating-point CSR write for underflow flags")) {
        return 1;
    }
    if (!expect(fmul_s_underflow_effects.csr_write.addr == CSR_FCSR,
                "fmul.s min*min should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fmul_s_underflow_effects.csr_write.value & 0x2ULL) != 0,
                "fmul.s min*min should set the underflow flag")) {
        return 1;
    }
    if (!expect((fmul_s_underflow_effects.csr_write.value & ~0x1fULL) == 0,
                "fmul.s min*min should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FRM, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(3.75));
    cpu.core().write_fpr(15, double_to_raw_bits(2.0));
    Insn fmul_d{};
    fmul_d.raw = 0x12f57553U;
    fmul_d.opcode = 0x53;
    fmul_d.rd = 10;
    fmul_d.funct3 = 7;
    fmul_d.rs1 = 10;
    fmul_d.rs2 = 15;
    fmul_d.funct7 = 0x09;

    if (!expect(InstructionSemantics::supports(fmul_d), "fmul.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmul_d_effects = InstructionSemantics::execute(fmul_d, ctx);
    if (!expect(fmul_d_effects.fp_write.enable, "fmul.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmul_d_effects.fp_write.rd == 10, "fmul.d should target fa0/f10")) {
        return 1;
    }
    if (!expect(fmul_d_effects.fp_write.value == double_to_raw_bits(7.5),
                "fmul.d should multiply the source fp registers and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fmul_d_effects.rd_write.enable, "fmul.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fmul_d_effects.trap.valid, "legal fmul.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FRM, 0, cpu.core());
    cpu.core().write_fpr(9, float_to_fpr_raw_bits(3.75f));
    cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
    Insn fadd_s{};
    fadd_s.raw = 0x00a4f4d3U;
    fadd_s.opcode = 0x53;
    fadd_s.rd = 9;
    fadd_s.funct3 = 7;
    fadd_s.rs1 = 9;
    fadd_s.rs2 = 10;
    fadd_s.funct7 = 0x00;

    if (!expect(InstructionSemantics::supports(fadd_s), "fadd.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fadd_s_effects = InstructionSemantics::execute(fadd_s, ctx);
    if (!expect(fadd_s_effects.fp_write.enable, "fadd.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fadd_s_effects.fp_write.rd == 9, "fadd.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fadd_s_effects.fp_write.value == float_to_fpr_raw_bits(5.75f),
                "fadd.s should add the source fp registers and preserve exact IEEE754 single bits")) {
        return 1;
    }
    if (!expect(!fadd_s_effects.rd_write.enable, "fadd.s should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fadd_s_effects.trap.valid, "legal fadd.s should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FRM, 0, cpu.core());
    cpu.core().write_fpr(9, double_to_raw_bits(3.75));
    cpu.core().write_fpr(10, double_to_raw_bits(2.0));
    Insn fadd_d{};
    fadd_d.raw = 0x02a4f4d3U;
    fadd_d.opcode = 0x53;
    fadd_d.rd = 9;
    fadd_d.funct3 = 7;
    fadd_d.rs1 = 9;
    fadd_d.rs2 = 10;
    fadd_d.funct7 = 0x01;

    if (!expect(InstructionSemantics::supports(fadd_d), "fadd.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fadd_d_effects = InstructionSemantics::execute(fadd_d, ctx);
    if (!expect(fadd_d_effects.fp_write.enable, "fadd.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fadd_d_effects.fp_write.rd == 9, "fadd.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fadd_d_effects.fp_write.value == double_to_raw_bits(5.75),
                "fadd.d should add the source fp registers and preserve exact IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fadd_d_effects.rd_write.enable, "fadd.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fadd_d_effects.trap.valid, "legal fadd.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, double_to_raw_bits(7.5));
    Insn fcvt_l_d{};
    fcvt_l_d.raw = 0xc22516d3U;
    fcvt_l_d.opcode = 0x53;
    fcvt_l_d.rd = 13;
    fcvt_l_d.funct3 = 1;
    fcvt_l_d.rs1 = 10;
    fcvt_l_d.rs2 = 2;
    fcvt_l_d.funct7 = 0x61;

    if (!expect(InstructionSemantics::supports(fcvt_l_d), "fcvt.l.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_l_d_effects = InstructionSemantics::execute(fcvt_l_d, ctx);
    if (!expect(fcvt_l_d_effects.rd_write.enable, "fcvt.l.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_d_effects.rd_write.rd == 13, "fcvt.l.d should target a3/x13")) {
        return 1;
    }
    if (!expect(fcvt_l_d_effects.rd_write.value == 7,
                "fcvt.l.d should round the source double toward zero and write the signed 64-bit integer")) {
        return 1;
    }
    if (!expect(!fcvt_l_d_effects.fp_write.enable, "fcvt.l.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_l_d_effects.trap.valid, "legal fcvt.l.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(-3.5));
    Insn fcvt_l_d_dyn{};
    fcvt_l_d_dyn.raw = 0xc22576d3U;
    fcvt_l_d_dyn.opcode = 0x53;
    fcvt_l_d_dyn.rd = 13;
    fcvt_l_d_dyn.funct3 = 7;
    fcvt_l_d_dyn.rs1 = 10;
    fcvt_l_d_dyn.rs2 = 2;
    fcvt_l_d_dyn.funct7 = 0x61;
    const InsnEffects fcvt_l_d_dyn_effects = InstructionSemantics::execute(fcvt_l_d_dyn, ctx);
    if (!expect(fcvt_l_d_dyn_effects.rd_write.enable,
                "fcvt.l.d dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_d_dyn_effects.rd_write.value == static_cast<uint64_t>(static_cast<int64_t>(-4)),
                "fcvt.l.d dyn should obey frm=RDN and round -3.5 toward negative infinity")) {
        return 1;
    }
    if (!expect(fcvt_l_d_dyn_effects.csr_write.enable,
                "fcvt.l.d dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_l_d_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.l.d dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_l_d_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.l.d dyn should set the inexact-operation flag for -3.5")) {
        return 1;
    }
    if (!expect((fcvt_l_d_dyn_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fcvt.l.d dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::infinity()));
    const InsnEffects fcvt_l_d_positive_invalid_effects = InstructionSemantics::execute(fcvt_l_d, ctx);
    if (!expect(fcvt_l_d_positive_invalid_effects.rd_write.enable,
                "fcvt.l.d +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_d_positive_invalid_effects.rd_write.value == std::numeric_limits<int64_t>::max(),
                "fcvt.l.d +inf should clip to INT64_MAX on invalid positive input")) {
        return 1;
    }
    if (!expect(fcvt_l_d_positive_invalid_effects.csr_write.enable,
                "fcvt.l.d +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_l_d_positive_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.l.d +inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_l_d_positive_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.l.d +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_l_d_positive_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.l.d +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(-std::numeric_limits<double>::infinity()));
    const InsnEffects fcvt_l_d_negative_invalid_effects = InstructionSemantics::execute(fcvt_l_d, ctx);
    if (!expect(fcvt_l_d_negative_invalid_effects.rd_write.enable,
                "fcvt.l.d -inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_d_negative_invalid_effects.rd_write.value == static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
                "fcvt.l.d -inf should clip to INT64_MIN on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_l_d_negative_invalid_effects.csr_write.enable,
                "fcvt.l.d -inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_l_d_negative_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.l.d -inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_l_d_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.l.d -inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_l_d_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.l.d -inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, 0x7ff8000000000000ULL);
    const InsnEffects fcvt_l_d_qnan_invalid_effects = InstructionSemantics::execute(fcvt_l_d, ctx);
    if (!expect(fcvt_l_d_qnan_invalid_effects.rd_write.enable,
                "fcvt.l.d qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_l_d_qnan_invalid_effects.rd_write.value == std::numeric_limits<int64_t>::max(),
                "fcvt.l.d qNaN should clip to INT64_MAX on invalid NaN input")) {
        return 1;
    }
    if (!expect(fcvt_l_d_qnan_invalid_effects.csr_write.enable,
                "fcvt.l.d qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_l_d_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.l.d qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_l_d_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.l.d qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.core().write_fpr(8, double_to_raw_bits(3.5));
    Insn fcvt_w_d{};
    fcvt_w_d.raw = 0xc20415d3U;
    fcvt_w_d.opcode = 0x53;
    fcvt_w_d.rd = 11;
    fcvt_w_d.funct3 = 1;
    fcvt_w_d.rs1 = 8;
    fcvt_w_d.rs2 = 0;
    fcvt_w_d.funct7 = 0x61;

    if (!expect(InstructionSemantics::supports(fcvt_w_d), "fcvt.w.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_w_d_effects = InstructionSemantics::execute(fcvt_w_d, ctx);
    if (!expect(fcvt_w_d_effects.rd_write.enable, "fcvt.w.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_d_effects.rd_write.rd == 11, "fcvt.w.d should target a1/x11")) {
        return 1;
    }
    if (!expect(fcvt_w_d_effects.rd_write.value == 3,
                "fcvt.w.d should round the source double toward zero and write the signed 32-bit integer")) {
        return 1;
    }
    if (!expect(!fcvt_w_d_effects.fp_write.enable, "fcvt.w.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_w_d_effects.trap.valid, "legal fcvt.w.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
    cpu.core().write_fpr(8, double_to_raw_bits(-3.5));
    Insn fcvt_w_d_dyn{};
    fcvt_w_d_dyn.raw = 0xc20475d3U;
    fcvt_w_d_dyn.opcode = 0x53;
    fcvt_w_d_dyn.rd = 11;
    fcvt_w_d_dyn.funct3 = 7;
    fcvt_w_d_dyn.rs1 = 8;
    fcvt_w_d_dyn.rs2 = 0;
    fcvt_w_d_dyn.funct7 = 0x61;
    const InsnEffects fcvt_w_d_dyn_effects = InstructionSemantics::execute(fcvt_w_d_dyn, ctx);
    if (!expect(fcvt_w_d_dyn_effects.rd_write.enable,
                "fcvt.w.d dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_d_dyn_effects.rd_write.value == static_cast<uint64_t>(static_cast<int64_t>(-4)),
                "fcvt.w.d dyn should obey frm=RDN and round -3.5 toward negative infinity")) {
        return 1;
    }
    if (!expect(fcvt_w_d_dyn_effects.csr_write.enable,
                "fcvt.w.d dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_w_d_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.w.d dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_w_d_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.w.d dyn should set the inexact-operation flag for -3.5")) {
        return 1;
    }
    if (!expect((fcvt_w_d_dyn_effects.csr_write.value & ~0x1fULL) == (2ULL << 5),
                "fcvt.w.d dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(8, double_to_raw_bits(std::numeric_limits<double>::infinity()));
    const InsnEffects fcvt_w_d_positive_invalid_effects = InstructionSemantics::execute(fcvt_w_d, ctx);
    if (!expect(fcvt_w_d_positive_invalid_effects.rd_write.enable,
                "fcvt.w.d +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_d_positive_invalid_effects.rd_write.value ==
                    static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                "fcvt.w.d +inf should clip to INT32_MAX on invalid positive input")) {
        return 1;
    }
    if (!expect(fcvt_w_d_positive_invalid_effects.csr_write.enable,
                "fcvt.w.d +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_w_d_positive_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.w.d +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_w_d_positive_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.w.d +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(8, double_to_raw_bits(-std::numeric_limits<double>::infinity()));
    const InsnEffects fcvt_w_d_negative_invalid_effects = InstructionSemantics::execute(fcvt_w_d, ctx);
    if (!expect(fcvt_w_d_negative_invalid_effects.rd_write.enable,
                "fcvt.w.d -inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_d_negative_invalid_effects.rd_write.value ==
                    static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::min())),
                "fcvt.w.d -inf should clip to INT32_MIN on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_w_d_negative_invalid_effects.csr_write.enable,
                "fcvt.w.d -inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_w_d_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.w.d -inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_w_d_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.w.d -inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(8, 0x7ff8000000000000ULL);
    const InsnEffects fcvt_w_d_qnan_invalid_effects = InstructionSemantics::execute(fcvt_w_d, ctx);
    if (!expect(fcvt_w_d_qnan_invalid_effects.rd_write.enable,
                "fcvt.w.d qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_w_d_qnan_invalid_effects.rd_write.value ==
                    static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                "fcvt.w.d qNaN should clip to INT32_MAX on invalid NaN input")) {
        return 1;
    }
    if (!expect(fcvt_w_d_qnan_invalid_effects.csr_write.enable,
                "fcvt.w.d qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_w_d_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.w.d qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_w_d_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.w.d qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(3.5));
    Insn fcvt_wu_d_dyn{};
    fcvt_wu_d_dyn.raw = 0xc2157553U;
    fcvt_wu_d_dyn.opcode = 0x53;
    fcvt_wu_d_dyn.rd = 10;
    fcvt_wu_d_dyn.funct3 = 7;
    fcvt_wu_d_dyn.rs1 = 10;
    fcvt_wu_d_dyn.rs2 = 1;
    fcvt_wu_d_dyn.funct7 = 0x61;
    const InsnEffects fcvt_wu_d_dyn_effects = InstructionSemantics::execute(fcvt_wu_d_dyn, ctx);
    if (!expect(fcvt_wu_d_dyn_effects.rd_write.enable,
                "fcvt.wu.d dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_dyn_effects.rd_write.value == 4,
                "fcvt.wu.d dyn should obey frm=RMM and round 3.5 away from zero")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_dyn_effects.csr_write.enable,
                "fcvt.wu.d dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.wu.d dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.wu.d dyn should set the inexact-operation flag for 3.5")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_dyn_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fcvt.wu.d dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(3.25));
    const InsnEffects fcvt_wu_d_dyn_nearest_effects = InstructionSemantics::execute(fcvt_wu_d_dyn, ctx);
    if (!expect(fcvt_wu_d_dyn_nearest_effects.rd_write.enable,
                "fcvt.wu.d dyn with non-tie input should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_dyn_nearest_effects.rd_write.value == 3,
                "fcvt.wu.d dyn should round 3.25 to the nearest integer under frm=RMM")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_dyn_nearest_effects.csr_write.enable,
                "fcvt.wu.d dyn with non-tie input should still produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_dyn_nearest_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.wu.d dyn should set the inexact-operation flag for 3.25")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_dyn_nearest_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fcvt.wu.d dyn with non-tie input should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());

    cpu.core().write_fpr(10, double_to_raw_bits(4294967295.0));
    Insn fcvt_wu_d{};
    fcvt_wu_d.raw = 0xc2151553U;
    fcvt_wu_d.opcode = 0x53;
    fcvt_wu_d.rd = 10;
    fcvt_wu_d.funct3 = 1;
    fcvt_wu_d.rs1 = 10;
    fcvt_wu_d.rs2 = 1;
    fcvt_wu_d.funct7 = 0x61;

    if (!expect(InstructionSemantics::supports(fcvt_wu_d), "fcvt.wu.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_wu_d_effects = InstructionSemantics::execute(fcvt_wu_d, ctx);
    if (!expect(fcvt_wu_d_effects.rd_write.enable, "fcvt.wu.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_effects.rd_write.rd == 10, "fcvt.wu.d should target a0/x10")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_effects.rd_write.value == 0xffffffffffffffffULL,
                "fcvt.wu.d should sign-extend the 32-bit unsigned result on RV64")) {
        return 1;
    }
    if (!expect(!fcvt_wu_d_effects.fp_write.enable, "fcvt.wu.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_wu_d_effects.trap.valid, "legal fcvt.wu.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(-1.0));
    const InsnEffects fcvt_wu_d_negative_invalid_effects = InstructionSemantics::execute(fcvt_wu_d, ctx);
    if (!expect(fcvt_wu_d_negative_invalid_effects.rd_write.enable,
                "fcvt.wu.d negative invalid input should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_negative_invalid_effects.rd_write.value == 0,
                "fcvt.wu.d -1.0 should clip to 0 on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_negative_invalid_effects.csr_write.enable,
                "fcvt.wu.d -1.0 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_negative_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.wu.d -1.0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.wu.d -1.0 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.wu.d -1.0 should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::infinity()));
    const InsnEffects fcvt_wu_d_invalid_effects = InstructionSemantics::execute(fcvt_wu_d, ctx);
    if (!expect(fcvt_wu_d_invalid_effects.rd_write.enable,
                "fcvt.wu.d +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_invalid_effects.rd_write.value == 0xffffffffffffffffULL,
                "fcvt.wu.d +inf should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_invalid_effects.csr_write.enable,
                "fcvt.wu.d +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.wu.d +inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.wu.d +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.wu.d +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, 0x7ff8000000000000ULL);
    const InsnEffects fcvt_wu_d_qnan_invalid_effects = InstructionSemantics::execute(fcvt_wu_d, ctx);
    if (!expect(fcvt_wu_d_qnan_invalid_effects.rd_write.enable,
                "fcvt.wu.d qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_qnan_invalid_effects.rd_write.value == 0xffffffffffffffffULL,
                "fcvt.wu.d qNaN should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
        return 1;
    }
    if (!expect(fcvt_wu_d_qnan_invalid_effects.csr_write.enable,
                "fcvt.wu.d qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.wu.d qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_wu_d_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.wu.d qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.core().write_fpr(10, double_to_raw_bits(9223372036854775808.0));
    Insn fcvt_lu_d{};
    fcvt_lu_d.raw = 0xc2351553U;
    fcvt_lu_d.opcode = 0x53;
    fcvt_lu_d.rd = 10;
    fcvt_lu_d.funct3 = 1;
    fcvt_lu_d.rs1 = 10;
    fcvt_lu_d.rs2 = 3;
    fcvt_lu_d.funct7 = 0x61;

    if (!expect(InstructionSemantics::supports(fcvt_lu_d), "fcvt.lu.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_lu_d_effects = InstructionSemantics::execute(fcvt_lu_d, ctx);
    if (!expect(fcvt_lu_d_effects.rd_write.enable, "fcvt.lu.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_effects.rd_write.rd == 10, "fcvt.lu.d should target a0/x10")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_effects.rd_write.value == 0x8000000000000000ULL,
                "fcvt.lu.d should round the source double toward zero and write the unsigned 64-bit integer")) {
        return 1;
    }
    if (!expect(!fcvt_lu_d_effects.fp_write.enable, "fcvt.lu.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fcvt_lu_d_effects.trap.valid, "legal fcvt.lu.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(3.25));
    Insn fcvt_lu_d_dyn{};
    fcvt_lu_d_dyn.raw = 0xc2357553U;
    fcvt_lu_d_dyn.opcode = 0x53;
    fcvt_lu_d_dyn.rd = 10;
    fcvt_lu_d_dyn.funct3 = 7;
    fcvt_lu_d_dyn.rs1 = 10;
    fcvt_lu_d_dyn.rs2 = 3;
    fcvt_lu_d_dyn.funct7 = 0x61;
    const InsnEffects fcvt_lu_d_dyn_effects = InstructionSemantics::execute(fcvt_lu_d_dyn, ctx);
    if (!expect(fcvt_lu_d_dyn_effects.rd_write.enable,
                "fcvt.lu.d dyn should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_dyn_effects.rd_write.value == 3,
                "fcvt.lu.d dyn should round 3.25 to the nearest integer under frm=RMM")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_dyn_effects.csr_write.enable,
                "fcvt.lu.d dyn should produce a floating-point CSR write for inexact flags")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_dyn_effects.csr_write.addr == CSR_FCSR,
                "fcvt.lu.d dyn should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_dyn_effects.csr_write.value & 0x1ULL) != 0,
                "fcvt.lu.d dyn should set the inexact-operation flag for 3.25")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_dyn_effects.csr_write.value & ~0x1fULL) == (4ULL << 5),
                "fcvt.lu.d dyn should preserve the current rounding-mode bits")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(-1.0));
    const InsnEffects fcvt_lu_d_negative_invalid_effects = InstructionSemantics::execute(fcvt_lu_d, ctx);
    if (!expect(fcvt_lu_d_negative_invalid_effects.rd_write.enable,
                "fcvt.lu.d negative invalid input should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_negative_invalid_effects.rd_write.value == 0,
                "fcvt.lu.d -1.0 should clip to 0 on invalid negative input")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_negative_invalid_effects.csr_write.enable,
                "fcvt.lu.d -1.0 should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_negative_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.lu.d -1.0 should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_negative_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.lu.d -1.0 should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_negative_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.lu.d -1.0 should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::infinity()));
    const InsnEffects fcvt_lu_d_positive_invalid_effects = InstructionSemantics::execute(fcvt_lu_d, ctx);
    if (!expect(fcvt_lu_d_positive_invalid_effects.rd_write.enable,
                "fcvt.lu.d +inf should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_positive_invalid_effects.rd_write.value == std::numeric_limits<uint64_t>::max(),
                "fcvt.lu.d +inf should clip to UINT64_MAX on invalid positive input")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_positive_invalid_effects.csr_write.enable,
                "fcvt.lu.d +inf should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_positive_invalid_effects.csr_write.addr == CSR_FCSR,
                "fcvt.lu.d +inf should update the combined fcsr alias")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_positive_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.lu.d +inf should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_positive_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.lu.d +inf should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.csr().write(CSR_FCSR, 0, cpu.core());
    cpu.core().write_fpr(10, 0x7ff8000000000000ULL);
    const InsnEffects fcvt_lu_d_qnan_invalid_effects = InstructionSemantics::execute(fcvt_lu_d, ctx);
    if (!expect(fcvt_lu_d_qnan_invalid_effects.rd_write.enable,
                "fcvt.lu.d qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_qnan_invalid_effects.rd_write.value == std::numeric_limits<uint64_t>::max(),
                "fcvt.lu.d qNaN should clip to UINT64_MAX on invalid NaN input")) {
        return 1;
    }
    if (!expect(fcvt_lu_d_qnan_invalid_effects.csr_write.enable,
                "fcvt.lu.d qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_qnan_invalid_effects.csr_write.value & 0x10ULL) != 0,
                "fcvt.lu.d qNaN should set the invalid-operation flag")) {
        return 1;
    }
    if (!expect((fcvt_lu_d_qnan_invalid_effects.csr_write.value & 0x1ULL) == 0,
                "fcvt.lu.d qNaN should not set the inexact-operation flag when invalid is raised")) {
        return 1;
    }

    cpu.core().write_gpr(13, 3);
    Insn fcvt_d_l{};
    fcvt_d_l.raw = 0xd226f7d3U;
    fcvt_d_l.opcode = 0x53;
    fcvt_d_l.rd = 15;
    fcvt_d_l.funct3 = 7;
    fcvt_d_l.rs1 = 13;
    fcvt_d_l.rs2 = 2;
    fcvt_d_l.funct7 = 0x69;

    if (!expect(InstructionSemantics::supports(fcvt_d_l), "fcvt.d.l should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_d_l_effects = InstructionSemantics::execute(fcvt_d_l, ctx);
    if (!expect(fcvt_d_l_effects.fp_write.enable, "fcvt.d.l should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_d_l_effects.fp_write.rd == 15, "fcvt.d.l should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_d_l_effects.fp_write.value == double_to_raw_bits(3.0),
                "fcvt.d.l should convert the signed 64-bit integer source into IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fcvt_d_l_effects.rd_write.enable, "fcvt.d.l should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fcvt_d_l_effects.trap.valid, "legal fcvt.d.l should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(13, 4294967295ULL);
    Insn fcvt_d_wu{};
    fcvt_d_wu.raw = 0xd216f7d3U;
    fcvt_d_wu.opcode = 0x53;
    fcvt_d_wu.rd = 15;
    fcvt_d_wu.funct3 = 7;
    fcvt_d_wu.rs1 = 13;
    fcvt_d_wu.rs2 = 1;
    fcvt_d_wu.funct7 = 0x69;

    if (!expect(InstructionSemantics::supports(fcvt_d_wu), "fcvt.d.wu should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_d_wu_effects = InstructionSemantics::execute(fcvt_d_wu, ctx);
    if (!expect(fcvt_d_wu_effects.fp_write.enable, "fcvt.d.wu should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_d_wu_effects.fp_write.rd == 15, "fcvt.d.wu should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_d_wu_effects.fp_write.value == double_to_raw_bits(4294967295.0),
                "fcvt.d.wu should convert the unsigned 32-bit integer source into IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fcvt_d_wu_effects.rd_write.enable, "fcvt.d.wu should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fcvt_d_wu_effects.trap.valid, "legal fcvt.d.wu should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(13, 0x8000000000000000ULL);
    Insn fcvt_d_lu{};
    fcvt_d_lu.raw = 0xd236f7d3U;
    fcvt_d_lu.opcode = 0x53;
    fcvt_d_lu.rd = 15;
    fcvt_d_lu.funct3 = 7;
    fcvt_d_lu.rs1 = 13;
    fcvt_d_lu.rs2 = 3;
    fcvt_d_lu.funct7 = 0x69;

    if (!expect(InstructionSemantics::supports(fcvt_d_lu), "fcvt.d.lu should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fcvt_d_lu_effects = InstructionSemantics::execute(fcvt_d_lu, ctx);
    if (!expect(fcvt_d_lu_effects.fp_write.enable, "fcvt.d.lu should produce an fp register write")) {
        return 1;
    }
    if (!expect(fcvt_d_lu_effects.fp_write.rd == 15, "fcvt.d.lu should target fa5/f15")) {
        return 1;
    }
    if (!expect(fcvt_d_lu_effects.fp_write.value == double_to_raw_bits(9223372036854775808.0),
                "fcvt.d.lu should convert the unsigned 64-bit integer source into IEEE754 double bits")) {
        return 1;
    }
    if (!expect(!fcvt_d_lu_effects.rd_write.enable, "fcvt.d.lu should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fcvt_d_lu_effects.trap.valid, "legal fcvt.d.lu should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(15, double_to_raw_bits(3.0));
    cpu.core().write_fpr(10, double_to_raw_bits(3.0));
    Insn feq_d{};
    feq_d.raw = 0xa2a7a7d3U;
    feq_d.opcode = 0x53;
    feq_d.rd = 15;
    feq_d.funct3 = 2;
    feq_d.rs1 = 15;
    feq_d.rs2 = 10;
    feq_d.funct7 = 0x51;

    if (!expect(InstructionSemantics::supports(feq_d), "feq.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects feq_d_effects = InstructionSemantics::execute(feq_d, ctx);
    if (!expect(feq_d_effects.rd_write.enable, "feq.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(feq_d_effects.rd_write.rd == 15, "feq.d should target fa5/x15 destination register field")) {
        return 1;
    }
    if (!expect(feq_d_effects.rd_write.value == 1, "feq.d should report equal doubles as integer 1")) {
        return 1;
    }
    if (!expect(!feq_d_effects.fp_write.enable, "feq.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!feq_d_effects.trap.valid, "legal feq.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(15, double_to_raw_bits(2.0));
    cpu.core().write_fpr(10, double_to_raw_bits(3.0));
    Insn flt_d{};
    flt_d.raw = 0xa2a79553U;
    flt_d.opcode = 0x53;
    flt_d.rd = 10;
    flt_d.funct3 = 1;
    flt_d.rs1 = 15;
    flt_d.rs2 = 10;
    flt_d.funct7 = 0x51;

    if (!expect(InstructionSemantics::supports(flt_d), "flt.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects flt_d_effects = InstructionSemantics::execute(flt_d, ctx);
    if (!expect(flt_d_effects.rd_write.enable, "flt.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(flt_d_effects.rd_write.rd == 10, "flt.d should target a0/x10 destination register field")) {
        return 1;
    }
    if (!expect(flt_d_effects.rd_write.value == 1, "flt.d should report lhs < rhs as integer 1")) {
        return 1;
    }
    if (!expect(!flt_d_effects.fp_write.enable, "flt.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!flt_d_effects.trap.valid, "legal flt.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(15, double_to_raw_bits(2.25));
    cpu.core().write_fpr(10, double_to_raw_bits(2.25));
    Insn fle_d{};
    fle_d.raw = 0xa2a78cd3U;
    fle_d.opcode = 0x53;
    fle_d.rd = 25;
    fle_d.funct3 = 0;
    fle_d.rs1 = 15;
    fle_d.rs2 = 10;
    fle_d.funct7 = 0x51;

    if (!expect(InstructionSemantics::supports(fle_d), "fle.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fle_d_effects = InstructionSemantics::execute(fle_d, ctx);
    if (!expect(fle_d_effects.rd_write.enable, "fle.d should produce an integer register write")) {
        return 1;
    }
    if (!expect(fle_d_effects.rd_write.rd == 25, "fle.d should target s9/x25 destination register field")) {
        return 1;
    }
    if (!expect(fle_d_effects.rd_write.value == 1, "fle.d should report lhs <= rhs as integer 1")) {
        return 1;
    }
    if (!expect(!fle_d_effects.fp_write.enable, "fle.d should not write the fp register file")) {
        return 1;
    }
    if (!expect(!fle_d_effects.trap.valid, "legal fle.d should not raise a trap")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(15, 0x7ff8000000000000ULL);
    cpu.core().write_fpr(10, double_to_raw_bits(3.0));
    const InsnEffects feq_d_qnan_effects = InstructionSemantics::execute(feq_d, ctx);
    if (!expect(feq_d_qnan_effects.rd_write.enable, "feq.d with qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(feq_d_qnan_effects.rd_write.value == 0,
                "feq.d with qNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(!feq_d_qnan_effects.csr_write.enable,
                "feq.d with qNaN should remain a quiet comparison and avoid setting invalid-operation flags")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(15, 0x7ff0000000000001ULL);
    cpu.core().write_fpr(10, double_to_raw_bits(3.0));
    const InsnEffects feq_d_snan_effects = InstructionSemantics::execute(feq_d, ctx);
    if (!expect(feq_d_snan_effects.rd_write.enable, "feq.d with sNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(feq_d_snan_effects.rd_write.value == 0,
                "feq.d with sNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(feq_d_snan_effects.csr_write.enable,
                "feq.d with sNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((feq_d_snan_effects.csr_write.value & 0x10ULL) != 0,
                "feq.d with sNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(15, 0x7ff8000000000000ULL);
    cpu.core().write_fpr(10, double_to_raw_bits(3.0));
    const InsnEffects flt_d_qnan_effects = InstructionSemantics::execute(flt_d, ctx);
    if (!expect(flt_d_qnan_effects.rd_write.enable, "flt.d with qNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(flt_d_qnan_effects.rd_write.value == 0,
                "flt.d with qNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(flt_d_qnan_effects.csr_write.enable,
                "flt.d with qNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((flt_d_qnan_effects.csr_write.value & 0x10ULL) != 0,
                "flt.d with qNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
    cpu.core().write_fpr(15, 0x7ff0000000000001ULL);
    cpu.core().write_fpr(10, double_to_raw_bits(3.0));
    const InsnEffects fle_d_snan_effects = InstructionSemantics::execute(fle_d, ctx);
    if (!expect(fle_d_snan_effects.rd_write.enable, "fle.d with sNaN should still produce an integer register write")) {
        return 1;
    }
    if (!expect(fle_d_snan_effects.rd_write.value == 0,
                "fle.d with sNaN should report unordered as integer 0")) {
        return 1;
    }
    if (!expect(fle_d_snan_effects.csr_write.enable,
                "fle.d with sNaN should produce a floating-point CSR write for invalid-operation flags")) {
        return 1;
    }
    if (!expect((fle_d_snan_effects.csr_write.value & 0x10ULL) != 0,
                "fle.d with sNaN should set the invalid-operation flag")) {
        return 1;
    }

    cpu.core().write_fpr(10, double_to_raw_bits(-1.0));
    Insn fneg_d{};
    fneg_d.raw = 0x22a51953U;
    fneg_d.opcode = 0x53;
    fneg_d.rd = 18;
    fneg_d.funct3 = 1;
    fneg_d.rs1 = 10;
    fneg_d.rs2 = 10;
    fneg_d.funct7 = 0x11;

    if (!expect(InstructionSemantics::supports(fneg_d), "fneg.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fneg_d_effects = InstructionSemantics::execute(fneg_d, ctx);
    if (!expect(fneg_d_effects.fp_write.enable, "fneg.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fneg_d_effects.fp_write.rd == 18, "fneg.d should target fs2/f18")) {
        return 1;
    }
    if (!expect(fneg_d_effects.fp_write.value == double_to_raw_bits(1.0),
                "fneg.d should flip the sign bit of the source double")) {
        return 1;
    }
    if (!expect(!fneg_d_effects.rd_write.enable, "fneg.d should not write the integer register file")) {
        return 1;
    }
    if (!expect(!fneg_d_effects.trap.valid, "legal fneg.d should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, double_to_raw_bits(1.5));
    cpu.core().write_fpr(11, double_to_raw_bits(-2.0));
    Insn fsgnj_d{};
    decode(0x22b504d3U, &fsgnj_d);
    if (!expect(InstructionSemantics::supports(fsgnj_d), "fsgnj.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsgnj_d_effects = InstructionSemantics::execute(fsgnj_d, ctx);
    if (!expect(fsgnj_d_effects.fp_write.enable, "fsgnj.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsgnj_d_effects.fp_write.rd == 9, "fsgnj.d should target fs1/f9")) {
        return 1;
    }
    if (!expect(fsgnj_d_effects.fp_write.value == double_to_raw_bits(-1.5),
                "fsgnj.d should copy the sign bit from rs2 into rs1")) {
        return 1;
    }
    if (!expect(!fsgnj_d_effects.trap.valid, "legal fsgnj.d should not raise a trap")) {
        return 1;
    }

    Insn fsgnjn_d{};
    decode(0x22b514d3U, &fsgnjn_d);
    if (!expect(InstructionSemantics::supports(fsgnjn_d), "fsgnjn.d should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsgnjn_d_effects = InstructionSemantics::execute(fsgnjn_d, ctx);
    if (!expect(fsgnjn_d_effects.fp_write.enable, "fsgnjn.d should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsgnjn_d_effects.fp_write.value == double_to_raw_bits(1.5),
                "fsgnjn.d should copy the inverted rs2 sign bit into rs1")) {
        return 1;
    }
    if (!expect(!fsgnjn_d_effects.trap.valid, "legal fsgnjn.d should not raise a trap")) {
        return 1;
    }

    Insn fabs_d{};
    decode(0x22a524d3U, &fabs_d);
    if (!expect(InstructionSemantics::supports(fabs_d), "fabs.d alias should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fabs_d_effects = InstructionSemantics::execute(fabs_d, ctx);
    if (!expect(fabs_d_effects.fp_write.enable, "fabs.d alias should produce an fp register write")) {
        return 1;
    }
    if (!expect(fabs_d_effects.fp_write.value == double_to_raw_bits(1.5),
                "fabs.d alias should clear the sign bit of the source double")) {
        return 1;
    }
    if (!expect(!fabs_d_effects.trap.valid, "legal fabs.d alias should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(-2.0f));
    Insn fsgnj_s{};
    decode(0x20b504d3U, &fsgnj_s);
    if (!expect(InstructionSemantics::supports(fsgnj_s), "fsgnj.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsgnj_s_effects = InstructionSemantics::execute(fsgnj_s, ctx);
    if (!expect(fsgnj_s_effects.fp_write.enable, "fsgnj.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsgnj_s_effects.fp_write.rd == 9, "fsgnj.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fsgnj_s_effects.fp_write.value == float_to_fpr_raw_bits(-1.5f),
                "fsgnj.s should copy the sign bit from rs2 into the source single")) {
        return 1;
    }
    if (!expect(!fsgnj_s_effects.trap.valid, "legal fsgnj.s should not raise a trap")) {
        return 1;
    }

    Insn fsgnjn_s{};
    decode(0x20b514d3U, &fsgnjn_s);
    if (!expect(InstructionSemantics::supports(fsgnjn_s), "fsgnjn.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsgnjn_s_effects = InstructionSemantics::execute(fsgnjn_s, ctx);
    if (!expect(fsgnjn_s_effects.fp_write.enable, "fsgnjn.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsgnjn_s_effects.fp_write.rd == 9, "fsgnjn.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fsgnjn_s_effects.fp_write.value == float_to_fpr_raw_bits(1.5f),
                "fsgnjn.s should copy the inverted sign bit from rs2 into the source single")) {
        return 1;
    }
    if (!expect(!fsgnjn_s_effects.trap.valid, "legal fsgnjn.s should not raise a trap")) {
        return 1;
    }

    Insn fabs_s{};
    decode(0x20a524d3U, &fabs_s);
    if (!expect(InstructionSemantics::supports(fabs_s), "fabs.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fabs_s_effects = InstructionSemantics::execute(fabs_s, ctx);
    if (!expect(fabs_s_effects.fp_write.enable, "fabs.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fabs_s_effects.fp_write.rd == 9, "fabs.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fabs_s_effects.fp_write.value == float_to_fpr_raw_bits(1.5f),
                "fabs.s should clear the sign bit and preserve the remaining single-precision payload")) {
        return 1;
    }
    if (!expect(!fabs_s_effects.trap.valid, "legal fabs.s should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(-1.5f));
    cpu.core().write_fpr(11, float_to_fpr_raw_bits(-2.0f));
    Insn fsgnjx_s{};
    decode(0x20b524d3U, &fsgnjx_s);
    if (!expect(InstructionSemantics::supports(fsgnjx_s), "fsgnjx.s should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fsgnjx_s_effects = InstructionSemantics::execute(fsgnjx_s, ctx);
    if (!expect(fsgnjx_s_effects.fp_write.enable, "fsgnjx.s should produce an fp register write")) {
        return 1;
    }
    if (!expect(fsgnjx_s_effects.fp_write.rd == 9, "fsgnjx.s should target fs1/f9")) {
        return 1;
    }
    if (!expect(fsgnjx_s_effects.fp_write.value == float_to_fpr_raw_bits(1.5f),
                "fsgnjx.s should xor the single-precision sign bits of rs1 and rs2")) {
        return 1;
    }
    if (!expect(!fsgnjx_s_effects.trap.valid, "legal fsgnjx.s should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
    Insn fneg_s{};
    decode(0x20a514d3U, &fneg_s);
    if (!expect(InstructionSemantics::supports(fneg_s), "fneg.s alias should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fneg_s_effects = InstructionSemantics::execute(fneg_s, ctx);
    if (!expect(fneg_s_effects.fp_write.enable, "fneg.s alias should produce an fp register write")) {
        return 1;
    }
    if (!expect(fneg_s_effects.fp_write.rd == 9, "fneg.s alias should target fs1/f9")) {
        return 1;
    }
    if (!expect(fneg_s_effects.fp_write.value == float_to_fpr_raw_bits(-1.5f),
                "fneg.s alias should flip the sign bit of the source single")) {
        return 1;
    }
    if (!expect(!fneg_s_effects.trap.valid, "legal fneg.s alias should not raise a trap")) {
        return 1;
    }

    cpu.core().write_gpr(10, 0x80000000ULL);
    Insn fmv_w_x{};
    decode(0xf0050553U, &fmv_w_x);
    if (!expect(InstructionSemantics::supports(fmv_w_x), "fmv.w.x should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmv_w_x_effects = InstructionSemantics::execute(fmv_w_x, ctx);
    if (!expect(fmv_w_x_effects.fp_write.enable, "fmv.w.x should produce an fp register write")) {
        return 1;
    }
    if (!expect(fmv_w_x_effects.fp_write.rd == 10, "fmv.w.x should target fa0/f10")) {
        return 1;
    }
    if (!expect(fmv_w_x_effects.fp_write.value == float_to_fpr_raw_bits(-0.0f),
                "fmv.w.x should copy the low 32 bits into a NaN-boxed single destination")) {
        return 1;
    }
    if (!expect(!fmv_w_x_effects.trap.valid, "legal fmv.w.x should not raise a trap")) {
        return 1;
    }

    cpu.core().write_fpr(10, float_to_fpr_raw_bits(-0.0f));
    Insn fmv_x_w{};
    decode(0xe0050553U, &fmv_x_w);
    if (!expect(InstructionSemantics::supports(fmv_x_w), "fmv.x.w should be supported by instruction semantics")) {
        return 1;
    }
    const InsnEffects fmv_x_w_effects = InstructionSemantics::execute(fmv_x_w, ctx);
    if (!expect(fmv_x_w_effects.rd_write.enable, "fmv.x.w should produce an integer register write")) {
        return 1;
    }
    if (!expect(fmv_x_w_effects.rd_write.rd == 10, "fmv.x.w should target a0/x10")) {
        return 1;
    }
    if (!expect(fmv_x_w_effects.rd_write.value == 0xffffffff80000000ULL,
                "fmv.x.w should sign-extend the moved single-precision payload into xlen")) {
        return 1;
    }
    if (!expect(!fmv_x_w_effects.trap.valid, "legal fmv.x.w should not raise a trap")) {
        return 1;
    }

