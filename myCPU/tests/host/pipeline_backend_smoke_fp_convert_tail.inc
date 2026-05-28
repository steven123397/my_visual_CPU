    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
        write32(ram, kEntry + 0, 0xc0057553U);           // fcvt.w.s a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.s dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 8,
                    "pipeline fcvt.w.s dyn should obey frm=RUP and commit the signed 32-bit integer")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.w.s dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fcvt.w.s dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.5f));
        write32(ram, kEntry + 0, 0xc0157553U);           // fcvt.wu.s a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.s dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 4,
                    "pipeline fcvt.wu.s dyn should obey frm=RMM and commit the unsigned 32-bit integer")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.wu.s dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fcvt.wu.s dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.25f));
        write32(ram, kEntry + 0, 0xc0157553U);           // fcvt.wu.s a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.s dyn nearest smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 3,
                    "pipeline fcvt.wu.s dyn should round 3.25f to the nearest integer under frm=RMM")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.wu.s dyn nearest smoke should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fcvt.wu.s dyn nearest smoke should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
        write32(ram, kEntry + 0, 0xc0257553U);           // fcvt.l.s a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.s dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 8,
                    "pipeline fcvt.l.s dyn should obey frm=RUP and commit the signed 64-bit integer")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.l.s dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fcvt.l.s dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.5f));
        write32(ram, kEntry + 0, 0xc0357553U);           // fcvt.lu.s a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.s dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 4,
                    "pipeline fcvt.lu.s dyn should obey frm=RMM and commit the unsigned 64-bit integer")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.lu.s dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fcvt.lu.s dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_gpr(13, (1ULL << 53) + 1ULL);
        write32(ram, kEntry + 0, 0xd226f7d3U);           // fcvt.d.l fa5, a3, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.d.l dyn rounding smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == double_to_raw_bits(9007199254740994.0),
                    "pipeline fcvt.d.l dyn should obey frm=RUP for 2^53+1")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_FCSR, cpu.core()) == ((3ULL << 5) | 0x1ULL),
                    "pipeline fcvt.d.l dyn should preserve frm and set NX in the combined fcsr alias")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_gpr(13, (1ULL << 53) + 1ULL);
        write32(ram, kEntry + 0, 0xd236f7d3U);           // fcvt.d.lu fa5, a3, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.d.lu dyn rounding smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == double_to_raw_bits(9007199254740992.0),
                    "pipeline fcvt.d.lu dyn should obey frm=RDN for 2^53+1")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_FCSR, cpu.core()) == ((2ULL << 5) | 0x1ULL),
                    "pipeline fcvt.d.lu dyn should preserve frm and set NX in the combined fcsr alias")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
        write32(ram, kEntry + 0, kFcvtWSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 7,
                    "pipeline fcvt.w.s should round the source single toward zero and commit the signed 32-bit integer")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == float_to_fpr_raw_bits(7.5f),
                    "pipeline fcvt.w.s should not clobber the source floating-point register")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
        write32(ram, kEntry + 0, kFcvtWSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.s positive invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) ==
                        static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                    "pipeline fcvt.w.s +inf should clip to INT32_MAX on invalid positive input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.w.s +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.w.s +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
        write32(ram, kEntry + 0, kFcvtWSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.s qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) ==
                        static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                    "pipeline fcvt.w.s qNaN should clip to INT32_MAX on invalid NaN input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.w.s qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.w.s qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(-std::numeric_limits<float>::infinity()));
        write32(ram, kEntry + 0, kFcvtWSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.s negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) ==
                        static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::min())),
                    "pipeline fcvt.w.s -inf should clip to INT32_MIN on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.w.s -inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.w.s -inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(7.5f));
        write32(ram, kEntry + 0, kFcvtLSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 7,
                    "pipeline fcvt.l.s should round the source single toward zero and commit the signed 64-bit integer")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == float_to_fpr_raw_bits(7.5f),
                    "pipeline fcvt.l.s should not clobber the source floating-point register")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
        write32(ram, kEntry + 0, kFcvtLSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.s positive invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == std::numeric_limits<int64_t>::max(),
                    "pipeline fcvt.l.s +inf should clip to INT64_MAX on invalid positive input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.l.s +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.l.s +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
        write32(ram, kEntry + 0, kFcvtLSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.s qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == std::numeric_limits<int64_t>::max(),
                    "pipeline fcvt.l.s qNaN should clip to INT64_MAX on invalid NaN input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.l.s qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.l.s qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(-std::numeric_limits<float>::infinity()));
        write32(ram, kEntry + 0, kFcvtLSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.s negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
                    "pipeline fcvt.l.s -inf should clip to INT64_MIN on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.l.s -inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.l.s -inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(3000000000.0f));
        write32(ram, kEntry + 0, kFcvtWUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) ==
                        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(3000000000u))),
                    "pipeline fcvt.wu.s should sign-extend the 32-bit unsigned result on RV64")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == float_to_fpr_raw_bits(3000000000.0f),
                    "pipeline fcvt.wu.s should not clobber the source floating-point register")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(-1.0f));
        write32(ram, kEntry + 0, kFcvtWUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.s negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline fcvt.wu.s -1.0f should clip to 0 on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.wu.s -1.0f should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.wu.s -1.0f should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
        write32(ram, kEntry + 0, kFcvtWUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.s invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0xffffffffffffffffULL,
                    "pipeline fcvt.wu.s +inf should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.wu.s +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.wu.s +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
        write32(ram, kEntry + 0, kFcvtWUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.s qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0xffffffffffffffffULL,
                    "pipeline fcvt.wu.s qNaN should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.wu.s qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.wu.s qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(4294967296.0f));
        write32(ram, kEntry + 0, kFcvtLUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 4294967296ULL,
                    "pipeline fcvt.lu.s should round the source single toward zero and commit the unsigned 64-bit integer")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == float_to_fpr_raw_bits(4294967296.0f),
                    "pipeline fcvt.lu.s should not clobber the source floating-point register")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(-1.0f));
        write32(ram, kEntry + 0, kFcvtLUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.s negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline fcvt.lu.s -1.0f should clip to 0 on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.lu.s -1.0f should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.lu.s -1.0f should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::infinity()));
        write32(ram, kEntry + 0, kFcvtLUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.s invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == std::numeric_limits<uint64_t>::max(),
                    "pipeline fcvt.lu.s +inf should clip to UINT64_MAX on invalid positive input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.lu.s +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.lu.s +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
        write32(ram, kEntry + 0, kFcvtLUSX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.s qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == std::numeric_limits<uint64_t>::max(),
                    "pipeline fcvt.lu.s qNaN should clip to UINT64_MAX on invalid NaN input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.lu.s qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.lu.s qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, 0x401e000000000000ULL);  // 7.5
        write32(ram, kEntry + 0, kFcvtLDX13FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(13) == 7,
                    "pipeline fcvt.l.d should round the source double toward zero and commit the signed 64-bit integer")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(13) == 0,
                    "pipeline fcvt.l.d should not allocate or commit an fp register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(-3.5));
        write32(ram, kEntry + 0, 0xc22576d3U);           // fcvt.l.d a3, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.d dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(13) == static_cast<uint64_t>(static_cast<int64_t>(-4)),
                    "pipeline fcvt.l.d dyn should obey frm=RDN and commit the signed 64-bit integer")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.l.d dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fcvt.l.d dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::infinity()));
        write32(ram, kEntry + 0, kFcvtLDX13FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.d positive invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(13) == std::numeric_limits<int64_t>::max(),
                    "pipeline fcvt.l.d +inf should clip to INT64_MAX on invalid positive input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.l.d +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.l.d +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0x7ff8000000000000ULL);
        write32(ram, kEntry + 0, kFcvtLDX13FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.d qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(13) == std::numeric_limits<int64_t>::max(),
                    "pipeline fcvt.l.d qNaN should clip to INT64_MAX on invalid NaN input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.l.d qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.l.d qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(-std::numeric_limits<double>::infinity()));
        write32(ram, kEntry + 0, kFcvtLDX13FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.l.d negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(13) == static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
                    "pipeline fcvt.l.d -inf should clip to INT64_MIN on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.l.d -inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.l.d -inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(-1.0));
        write32(ram, kEntry + 0, 0xc2351553U);           // fcvt.lu.d a0, fa0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.d negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline fcvt.lu.d -1.0 should clip to 0 on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.lu.d -1.0 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.lu.d -1.0 should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::infinity()));
        write32(ram, kEntry + 0, 0xc2351553U);           // fcvt.lu.d a0, fa0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.d positive invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == std::numeric_limits<uint64_t>::max(),
                    "pipeline fcvt.lu.d +inf should clip to UINT64_MAX on invalid positive input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.lu.d +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.lu.d +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0x7ff8000000000000ULL);
        write32(ram, kEntry + 0, 0xc2351553U);           // fcvt.lu.d a0, fa0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.d qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == std::numeric_limits<uint64_t>::max(),
                    "pipeline fcvt.lu.d qNaN should clip to UINT64_MAX on invalid NaN input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.lu.d qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.lu.d qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(3.25));
        write32(ram, kEntry + 0, 0xc2357553U);           // fcvt.lu.d a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.lu.d dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 3,
                    "pipeline fcvt.lu.d dyn should round 3.25 to the nearest integer under frm=RMM")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.lu.d dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fcvt.lu.d dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, double_to_raw_bits(4294967295.0));
        write32(ram, kEntry + 0, kFcvtWUDX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0xffffffffffffffffULL,
                    "pipeline fcvt.wu.d should sign-extend the 32-bit unsigned result on RV64")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == double_to_raw_bits(4294967295.0),
                    "pipeline fcvt.wu.d should not clobber the source floating-point register")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(-1.0));
        write32(ram, kEntry + 0, 0xc2151553U);           // fcvt.wu.d a0, fa0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.d negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline fcvt.wu.d -1.0 should clip to 0 on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.wu.d -1.0 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.wu.d -1.0 should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::infinity()));
        write32(ram, kEntry + 0, kFcvtWUDX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.d invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0xffffffffffffffffULL,
                    "pipeline fcvt.wu.d +inf should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.wu.d +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.wu.d +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0x7ff8000000000000ULL);
        write32(ram, kEntry + 0, kFcvtWUDX10FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.d qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0xffffffffffffffffULL,
                    "pipeline fcvt.wu.d qNaN should clip to UINT32_MAX and sign-extend the 32-bit result on RV64")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.wu.d qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.wu.d qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(13, 4294967295ULL);
        write32(ram, kEntry + 0, kFcvtDWUDF15FromX13);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.d.wu smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == double_to_raw_bits(4294967295.0),
                    "pipeline fcvt.d.wu should convert the unsigned 32-bit integer source into IEEE754 double bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(15) == 0,
                    "pipeline fcvt.d.wu should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(13, 0x8000000000000000ULL);
        write32(ram, kEntry + 0, kFcvtDLUDF15FromX13);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.d.lu smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == double_to_raw_bits(9223372036854775808.0),
                    "pipeline fcvt.d.lu should convert the unsigned 64-bit integer source into IEEE754 double bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(15) == 0,
                    "pipeline fcvt.d.lu should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(15, double_to_raw_bits(2.0));
        cpu.core().write_fpr(10, double_to_raw_bits(3.0));
        write32(ram, kEntry + 0, kFltDX10F15F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline flt.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 1,
                    "pipeline flt.d should compare the source doubles and commit integer 1 when lhs < rhs")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == double_to_raw_bits(3.0),
                    "pipeline flt.d should not write the floating-point destination file")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(15, 0x7ff8000000000000ULL);
        cpu.core().write_fpr(10, double_to_raw_bits(3.0));
        write32(ram, kEntry + 0, 0xa2a7a7d3U);            // feq.d fa5, fa5, fa0
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline feq.d qNaN smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(15) == 0,
                    "pipeline feq.d with qNaN should report unordered as integer 0")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_FFLAGS, cpu.core()) == 0,
                    "pipeline feq.d with qNaN should remain a quiet comparison and keep exception flags clear")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(15, 0x7ff8000000000000ULL);
        cpu.core().write_fpr(10, double_to_raw_bits(3.0));
        write32(ram, kEntry + 0, kFltDX10F15F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline flt.d qNaN smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline flt.d with qNaN should report unordered as integer 0")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline flt.d with qNaN should set the invalid-operation flag")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(8, 0xc00c000000000000ULL);  // -3.5
        write32(ram, kEntry + 0, 0xc20475d3U);           // fcvt.w.d a1, fs0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.d dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) == static_cast<uint64_t>(static_cast<int64_t>(-4)),
                    "pipeline fcvt.w.d dyn should obey frm=RDN and commit the signed 32-bit integer")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.w.d dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fcvt.w.d dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(8, double_to_raw_bits(std::numeric_limits<double>::infinity()));
        write32(ram, kEntry + 0, 0xc20415d3U);           // fcvt.w.d a1, fs0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.d positive invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) ==
                        static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                    "pipeline fcvt.w.d +inf should clip to INT32_MAX on invalid positive input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.w.d +inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.w.d +inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(8, 0x7ff8000000000000ULL);
        write32(ram, kEntry + 0, 0xc20415d3U);           // fcvt.w.d a1, fs0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.d qNaN invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) ==
                        static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::max())),
                    "pipeline fcvt.w.d qNaN should clip to INT32_MAX on invalid NaN input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.w.d qNaN should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.w.d qNaN should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(8, double_to_raw_bits(-std::numeric_limits<double>::infinity()));
        write32(ram, kEntry + 0, 0xc20415d3U);           // fcvt.w.d a1, fs0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.d negative invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) ==
                        static_cast<uint64_t>(static_cast<int64_t>(std::numeric_limits<int32_t>::min())),
                    "pipeline fcvt.w.d -inf should clip to INT32_MIN on invalid negative input")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fcvt.w.d -inf should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) == 0,
                    "pipeline fcvt.w.d -inf should not set the inexact-operation flag when invalid is raised")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(3.5));
        write32(ram, kEntry + 0, 0xc2157553U);           // fcvt.wu.d a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.d dyn smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 4,
                    "pipeline fcvt.wu.d dyn should obey frm=RMM and commit the unsigned 32-bit integer")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.wu.d dyn should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fcvt.wu.d dyn should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(3.25));
        write32(ram, kEntry + 0, 0xc2157553U);           // fcvt.wu.d a0, fa0, dyn
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.wu.d dyn nearest smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 3,
                    "pipeline fcvt.wu.d dyn should round 3.25 to the nearest integer under frm=RMM")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fcvt.wu.d dyn nearest smoke should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fcvt.wu.d dyn nearest smoke should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(8, 0x400c000000000000ULL);  // 3.5
        write32(ram, kEntry + 0, 0xc20415d3U);           // fcvt.w.d a1, fs0, rtz
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.w.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) == 3,
                    "pipeline fcvt.w.d should round the source double toward zero and commit the signed 32-bit integer")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(11) == 0,
                    "pipeline fcvt.w.d should not allocate or commit an fp register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, 0xbff0000000000000ULL);  // -1.0
        write32(ram, kEntry + 0, 0x22a51953U);            // fneg.d fs2, fa0
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fneg.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(18) == 0x3ff0000000000000ULL,
                    "pipeline fneg.d should flip the sign bit and commit the exact double bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(18) == 0,
                    "pipeline fneg.d should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, double_to_raw_bits(1.5));
        cpu.core().write_fpr(11, double_to_raw_bits(-2.0));
        write32(ram, kEntry + 0, kFsgnjDF9F10F11);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsgnj.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-1.5),
                    "pipeline fsgnj.d should copy the sign bit from rs2 into rs1")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(-2.0f));
        write32(ram, kEntry + 0, kFsgnjSF9F10F11);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsgnj.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == float_to_fpr_raw_bits(-1.5f),
                    "pipeline fsgnj.s should copy the sign bit from rs2 into the source single")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(-2.0f));
        write32(ram, kEntry + 0, kFsgnjnSF9F10F11);
        write32(ram, kEntry + 4, kFabsSF9F10);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsgnjn.s/fabs.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == float_to_fpr_raw_bits(1.5f),
                    "pipeline fsgnjn.s/fabs.s should commit the expected single-precision sign-manipulated result")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(-1.5f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(-2.0f));
        write32(ram, kEntry + 0, kFsgnjxSF9F10F11);
        write32(ram, kEntry + 4, kFnegSF9F9);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsgnjx.s/fneg.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == float_to_fpr_raw_bits(-1.5f),
                    "pipeline fsgnjx.s/fneg.s should commit the expected single-precision sign-manipulated result")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(10, 0x80000000ULL);
        write32(ram, kEntry + 0, kFmvWXF10FromX10);
        write32(ram, kEntry + 4, kFmvXWA0FromF10);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmv.w.x/fmv.x.w smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == float_to_fpr_raw_bits(-0.0f),
                    "pipeline fmv.w.x should commit the expected NaN-boxed single payload")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0xffffffff80000000ULL,
                    "pipeline fmv.x.w should sign-extend the moved single-precision payload into xlen")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.5f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(2.5f));
        write32(ram, kEntry + 0, kFeqSA0F11F12);
        write32(ram, kEntry + 4, kFltSA1F10F11);
        write32(ram, kEntry + 8, kFleSA2F11F12);
        write32(ram, kEntry + 12, kAddiA7Exit);
        write32(ram, kEntry + 16, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 40 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline feq.s/flt.s/fle.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 1,
                    "pipeline feq.s should report equal singles as integer 1")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) == 1,
                    "pipeline flt.s should report lhs < rhs as integer 1")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(12) == 1,
                    "pipeline fle.s should report lhs <= rhs as integer 1")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(11, 0xffffffff7fc00000ULL);
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(2.5f));
        write32(ram, kEntry + 0, kFeqSA0F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline feq.s qNaN smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline feq.s with qNaN should report unordered as integer 0")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_FFLAGS, cpu.core()) == 0,
                    "pipeline feq.s with qNaN should remain a quiet comparison and keep exception flags clear")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0xffffffff7fc00000ULL);
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.5f));
        write32(ram, kEntry + 0, kFltSA1F10F11);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline flt.s qNaN smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) == 0,
                    "pipeline flt.s with qNaN should report unordered as integer 0")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline flt.s with qNaN should set the invalid-operation flag")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(10, 7);
        write32(ram, kEntry + 0, kFcvtSWF15FromX10);
        write32(ram, kEntry + 4, kFclassSA1F15);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.s.w -> fclass.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) == (1ULL << 6),
                    "pipeline fclass.s should classify a positive single-precision normal with bit 6")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(10, 0xffffffffULL);
        write32(ram, kEntry + 0, kFcvtSWUF15FromX10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.s.wu smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) ==
                        float_to_fpr_raw_bits(static_cast<float>(static_cast<uint32_t>(0xffffffffULL))),
                    "pipeline fcvt.s.wu should convert the unsigned 32-bit integer into nan-boxed single-precision bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(10, 1ULL << 40);
        write32(ram, kEntry + 0, kFcvtSLF15FromX10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.s.l smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) ==
                        float_to_fpr_raw_bits(static_cast<float>(static_cast<int64_t>(1ULL << 40))),
                    "pipeline fcvt.s.l should convert the signed 64-bit integer into nan-boxed single-precision bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(10, 1ULL << 63);
        write32(ram, kEntry + 0, kFcvtSLUF15FromX10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.s.lu smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) ==
                        float_to_fpr_raw_bits(static_cast<float>(1ULL << 63)),
                    "pipeline fcvt.s.lu should convert the unsigned 64-bit integer into nan-boxed single-precision bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.25f));
        write32(ram, kEntry + 0, kFsqrtSF15FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsqrt.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == float_to_fpr_raw_bits(1.5f),
                    "pipeline fsqrt.s should compute the square root and preserve exact IEEE754 single bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, 0xffffffffbf800000ULL);
        write32(ram, kEntry + 0, kFsqrtSF15FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsqrt.s sqrt(-1.0f) smoke should eventually halt")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fsqrt.s sqrt(-1.0f) should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fsqrt.s sqrt(-1.0f) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        write32(ram, kEntry + 0, kFsqrtSF15FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsqrt.s sqrt(2.0f) smoke should eventually halt")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fsqrt.s sqrt(2.0f) should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fsqrt.s sqrt(2.0f) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
        write32(ram, kEntry + 0, kFcvtDSF15FromF10);
        write32(ram, kEntry + 4, kFclassDA1F15);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.d.s -> fclass.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(11) == (1ULL << 6),
                    "pipeline fclass.d should classify a positive double-precision normal with bit 6")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.25f));
        write32(ram, kEntry + 0, kFmaxSF9F10F11);
        write32(ram, kEntry + 4, kFminSF9F10F11);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmax.s/fmin.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == float_to_fpr_raw_bits(1.5f),
                    "pipeline fmax.s/fmin.s should commit the selected single result")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0xffffffff7f800001ULL);
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(2.25f));
        write32(ram, kEntry + 0, kFminSF9F10F11);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmin.s sNaN smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == float_to_fpr_raw_bits(2.25f),
                    "pipeline fmin.s with sNaN should return the non-NaN operand")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fmin.s with sNaN should set the invalid-operation flag")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, double_to_raw_bits(1.5));
        cpu.core().write_fpr(11, double_to_raw_bits(2.25));
        write32(ram, kEntry + 0, kFmaxDF9F10F11);
        write32(ram, kEntry + 4, kFminDF9F10F11);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmax.d/fmin.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(1.5),
                    "pipeline fmax.d/fmin.d should commit the selected double result")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(10, 0x7ff0000000000001ULL);
        cpu.core().write_fpr(11, double_to_raw_bits(2.25));
        write32(ram, kEntry + 0, kFminDF9F10F11);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmin.d sNaN smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(2.25),
                    "pipeline fmin.d with sNaN should return the non-NaN operand")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fmin.d with sNaN should set the invalid-operation flag")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, double_to_raw_bits(1.5));
        cpu.core().write_fpr(11, double_to_raw_bits(-2.0));
        write32(ram, kEntry + 0, kFsgnjnDF9F10F11);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsgnjn.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(1.5),
                    "pipeline fsgnjn.d should copy the inverted rs2 sign bit into rs1")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, double_to_raw_bits(-1.5));
        write32(ram, kEntry + 0, kFabsDF9F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fabs.d alias smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(1.5),
                    "pipeline fabs.d alias should clear the sign bit and commit the exact double bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
        write32(ram, kEntry + 0, kFcvtDSF15FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.d.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == double_to_raw_bits(1.5),
                    "pipeline fcvt.d.s should widen the source single-precision value into exact double bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, double_to_raw_bits(1.5));
        write32(ram, kEntry + 0, kFcvtSDF15FromF10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.s.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == float_to_fpr_raw_bits(1.5f),
                    "pipeline fcvt.s.d should narrow the source double into nan-boxed single-precision bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(15, 0x4002000000000000ULL);  // 2.25
        cpu.core().write_fpr(10, 0x4002000000000000ULL);  // 2.25
        write32(ram, kEntry + 0, 0xa2a78cd3U);            // fle.d s9, fa5, fa0
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fle.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(25) == 1,
                    "pipeline fle.d should report lhs <= rhs as integer 1")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(25) == 0,
                    "pipeline fle.d should not allocate or commit an fp register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(18, float_to_fpr_raw_bits(5.75f));
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        write32(ram, kEntry + 0, kFsubSF10F18F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsub.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == float_to_fpr_raw_bits(3.75f),
                    "pipeline fsub.s should subtract the source fp registers and commit the exact single bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(9, 0x401c000000000000ULL);   // 7.0
        cpu.core().write_fpr(10, 0x0000000000000000ULL);  // 0.0
        write32(ram, kEntry + 0, 0x1aa4f4d3U);            // fdiv.d fs1, fs1, fa0
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fdiv.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == 0x7ff0000000000000ULL,
                    "pipeline fdiv.d should compute the IEEE754 infinite result for division by zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x8ULL) != 0,
                    "pipeline fdiv.d by zero should set the divide-by-zero flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_MSTATUS, cpu.core()) & kMstatusFsDirty) == kMstatusFsDirty,
                    "pipeline fdiv.d should mark mstatus.FS dirty once the fp result commits")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_MSTATUS, cpu.core()) & kMstatusSd) != 0,
                    "pipeline fdiv.d should raise mstatus.SD once FS becomes dirty")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_SSTATUS, cpu.core()) & kMstatusFsDirty) == kMstatusFsDirty,
                    "pipeline fdiv.d should expose the dirty FS state through sstatus")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FFLAGS, 0, cpu.core());
        cpu.core().write_fpr(9, float_to_fpr_raw_bits(7.0f));
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFdivSF9F9F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fdiv.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == float_to_fpr_raw_bits(INFINITY),
                    "pipeline fdiv.s should compute the IEEE754 infinite result for division by zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x8ULL) != 0,
                    "pipeline fdiv.s by zero should set the divide-by-zero flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_MSTATUS, cpu.core()) & kMstatusFsDirty) == kMstatusFsDirty,
                    "pipeline fdiv.s should mark mstatus.FS dirty once the fp result commits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(9, float_to_fpr_raw_bits(0.0f));
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFdivSF9F9F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fdiv.s invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_float(cpu.core().read_fpr(9))),
                    "pipeline fdiv.s 0/0 should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fdiv.s 0/0 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fdiv.s 0/0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(9, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.0f));
        write32(ram, kEntry + 0, kFdivSF9F9F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fdiv.s inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fdiv.s 1/3 should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fdiv.s 1/3 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, 0xbff0000000000000ULL);  // -1.0
        write32(ram, kEntry + 0, 0x5a057553U);            // fsqrt.d fa0, fa0
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsqrt.d invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fsqrt.d sqrt(-1.0) should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fsqrt.d sqrt(-1.0) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, 0x4000000000000000ULL);  // 2.0
        write32(ram, kEntry + 0, 0x5a057553U);            // fsqrt.d fa0, fa0
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fsqrt.d inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fsqrt.d sqrt(2.0) should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fsqrt.d sqrt(2.0) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(1.0e308));
        cpu.core().write_fpr(15, double_to_raw_bits(1.0e308));
        write32(ram, kEntry + 0, kFmulDF10F10F15);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmul.d overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == 0x7ff0000000000000ULL,
                    "pipeline fmul.d max*max should overflow to IEEE754 infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fmul.d max*max should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == 0,
                    "pipeline fmul.d max*max should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.csr().write(CSR_FCSR, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(1.0e-308));
        cpu.core().write_fpr(15, double_to_raw_bits(1.0e-308));
        write32(ram, kEntry + 0, kFmulDF10F10F15);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmul.d underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == 0x0000000000000000ULL,
                    "pipeline fmul.d min*min should underflow to zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fmul.d min*min should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == 0,
                    "pipeline fmul.d min*min should preserve the current rounding-mode bits")) {
            return 1;
        }
    }
