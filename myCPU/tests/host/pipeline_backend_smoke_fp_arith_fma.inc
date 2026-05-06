    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(3.75f));
        cpu.core().write_fpr(15, float_to_fpr_raw_bits(2.0f));
        write32(ram, kEntry + 0, kFmulSF10F10F15);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmul.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == float_to_fpr_raw_bits(7.5f),
                    "pipeline fmul.s should multiply the source fp registers and commit the exact single bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline fmul.s should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(15, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        write32(ram, kEntry + 0, kFmulSF10F10F15);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmul.s overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect((cpu.core().read_fpr(10) & 0xffffffffULL) == 0x7f800000ULL,
                    "pipeline fmul.s max*max should overflow to IEEE754 single infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fmul.s max*max should set the overflow flag")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(15, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        write32(ram, kEntry + 0, kFmulSF10F10F15);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmul.s underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect((cpu.core().read_fpr(10) & 0xffffffffULL) == 0x00000000ULL,
                    "pipeline fmul.s min*min should underflow to zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fmul.s min*min should set the underflow flag")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, 0x400e000000000000ULL);  // 3.75
        cpu.core().write_fpr(15, 0x4000000000000000ULL);  // 2.0
        write32(ram, kEntry + 0, kFmulDF10F10F15);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmul.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == 0x401e000000000000ULL,
                    "pipeline fmul.d should multiply the source fp registers and commit the exact double bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline fmul.d should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(9, 0x400e000000000000ULL);   // 3.75
        cpu.core().write_fpr(10, 0x4000000000000000ULL);  // 2.0
        write32(ram, kEntry + 0, kFaddDF9F9F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fadd.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == 0x4017000000000000ULL,
                    "pipeline fadd.d should add the source fp registers and commit the exact double bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(9) == 0,
                    "pipeline fadd.d should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(9, float_to_fpr_raw_bits(3.75f));
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        write32(ram, kEntry + 0, kFaddSF9F9F10);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fadd.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == float_to_fpr_raw_bits(5.75f),
                    "pipeline fadd.s should add the source fp registers and commit the exact single bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(9) == 0,
                    "pipeline fadd.s should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));
        write32(ram, kEntry + 0, kFmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(10.0f),
                    "pipeline fmadd.s should compute rs1*rs2+rs3 and commit the exact single bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(1.0f));
        write32(ram, kEntry + 0, kFmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.s invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_float(cpu.core().read_fpr(8))),
                    "pipeline fmadd.s inf*0+1 should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fmadd.s inf*0+1 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmadd.s inf*0+1 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(std::ldexp(1.0f, -25)));
        write32(ram, kEntry + 0, kFmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.s inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(1.0f),
                    "pipeline fmadd.s 1*1+2^-25 should round back to exactly 1.0f in single precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fmadd.s 1*1+2^-25 should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmadd.s 1*1+2^-25 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.s overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(INFINITY),
                    "pipeline fmadd.s max*max+0 should overflow to IEEE754 single infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fmadd.s max*max+0 should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fmadd.s max*max+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.s underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(0.0f),
                    "pipeline fmadd.s min*min+0 should underflow to zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fmadd.s min*min+0 should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fmadd.s min*min+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));
        write32(ram, kEntry + 0, kFmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(2.0f),
                    "pipeline fmsub.s should compute rs1*rs2-rs3 and commit the exact single bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(1.0f));
        write32(ram, kEntry + 0, kFmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.s invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_float(cpu.core().read_fpr(8))),
                    "pipeline fmsub.s inf*0-1 should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fmsub.s inf*0-1 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmsub.s inf*0-1 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(std::ldexp(1.0f, -25)));
        write32(ram, kEntry + 0, kFmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.s inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(1.0f),
                    "pipeline fmsub.s 1*1-2^-25 should round back to exactly 1.0f in single precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fmsub.s 1*1-2^-25 should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmsub.s 1*1-2^-25 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.s overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(INFINITY),
                    "pipeline fmsub.s max*max-0 should overflow to IEEE754 single infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fmsub.s max*max-0 should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fmsub.s max*max-0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.s underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(0.0f),
                    "pipeline fmsub.s min*min-0 should underflow to zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fmsub.s min*min-0 should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fmsub.s min*min-0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));
        write32(ram, kEntry + 0, kFnmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-2.0f),
                    "pipeline fnmsub.s should compute -(rs1*rs2)+rs3 and commit the exact single bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(1.0f));
        write32(ram, kEntry + 0, kFnmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.s invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_float(cpu.core().read_fpr(8))),
                    "pipeline fnmsub.s -(inf*0)+1 should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fnmsub.s -(inf*0)+1 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fnmsub.s -(inf*0)+1 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(std::ldexp(1.0f, -25)));
        write32(ram, kEntry + 0, kFnmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.s inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-1.0f),
                    "pipeline fnmsub.s -(1*1)+2^-25 should round back to exactly -1.0f in single precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fnmsub.s -(1*1)+2^-25 should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fnmsub.s -(1*1)+2^-25 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFnmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.s overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-INFINITY),
                    "pipeline fnmsub.s -(max*max)+0 should overflow to IEEE754 single negative infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fnmsub.s -(max*max)+0 should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fnmsub.s -(max*max)+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(0.0f));
        write32(ram, kEntry + 0, kFnmsubSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.s underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-0.0f),
                    "pipeline fnmsub.s -(min*min)+0 should underflow to negative zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fnmsub.s -(min*min)+0 should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fnmsub.s -(min*min)+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));
        write32(ram, kEntry + 0, kFnmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-10.0f),
                    "pipeline fnmadd.s should compute -(rs1*rs2)-rs3 and commit the exact single bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(INFINITY));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(0.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(-1.0f));
        write32(ram, kEntry + 0, kFnmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.s invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_float(cpu.core().read_fpr(8))),
                    "pipeline fnmadd.s -(inf*0)-(-1) should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fnmadd.s -(inf*0)-(-1) should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fnmadd.s -(inf*0)-(-1) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(1.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(-std::ldexp(1.0f, -25)));
        write32(ram, kEntry + 0, kFnmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.s inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-1.0f),
                    "pipeline fnmadd.s -(1*1)-(-2^-25) should round back to exactly -1.0f in single precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fnmadd.s -(1*1)-(-2^-25) should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fnmadd.s -(1*1)-(-2^-25) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::max()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(-0.0f));
        write32(ram, kEntry + 0, kFnmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.s overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-INFINITY),
                    "pipeline fnmadd.s -(max*max)-(-0) should overflow to IEEE754 single negative infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fnmadd.s -(max*max)-(-0) should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fnmadd.s -(max*max)-(-0) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(std::numeric_limits<float>::min()));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(-0.0f));
        write32(ram, kEntry + 0, kFnmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.s underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(-0.0f),
                    "pipeline fnmadd.s -(min*min)-(-0) should underflow to negative zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fnmadd.s -(min*min)-(-0) should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fnmadd.s -(min*min)-(-0) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(2.0));
        cpu.core().write_fpr(11, double_to_raw_bits(3.0));
        cpu.core().write_fpr(12, double_to_raw_bits(4.0));
        write32(ram, kEntry + 0, kFmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(10.0),
                    "pipeline fmadd.d should compute rs1*rs2+rs3 and commit the exact double bits")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(9) == 0,
                    "pipeline fmadd.d should not allocate or commit an integer register destination")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
        cpu.core().write_fpr(11, double_to_raw_bits(0.0));
        cpu.core().write_fpr(12, double_to_raw_bits(1.0));
        write32(ram, kEntry + 0, kFmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.d invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_double(cpu.core().read_fpr(9))),
                    "pipeline fmadd.d inf*0+1 should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fmadd.d inf*0+1 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmadd.d inf*0+1 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(1.0));
        cpu.core().write_fpr(11, double_to_raw_bits(1.0));
        cpu.core().write_fpr(12, double_to_raw_bits(std::ldexp(1.0, -54)));
        write32(ram, kEntry + 0, kFmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.d inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(1.0),
                    "pipeline fmadd.d 1*1+2^-54 should round back to exactly 1.0 in double precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fmadd.d 1*1+2^-54 should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmadd.d 1*1+2^-54 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(12, double_to_raw_bits(0.0));
        write32(ram, kEntry + 0, kFmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.d overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(INFINITY),
                    "pipeline fmadd.d max*max+0 should overflow to IEEE754 double infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fmadd.d max*max+0 should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fmadd.d max*max+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(12, double_to_raw_bits(0.0));
        write32(ram, kEntry + 0, kFmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.d underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(0.0),
                    "pipeline fmadd.d min*min+0 should underflow to zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fmadd.d min*min+0 should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fmadd.d min*min+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(2.0));
        cpu.core().write_fpr(11, double_to_raw_bits(3.0));
        cpu.core().write_fpr(12, double_to_raw_bits(4.0));
        write32(ram, kEntry + 0, kFmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(2.0),
                    "pipeline fmsub.d should compute rs1*rs2-rs3 and commit the exact double bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
        cpu.core().write_fpr(11, double_to_raw_bits(0.0));
        cpu.core().write_fpr(12, double_to_raw_bits(1.0));
        write32(ram, kEntry + 0, kFmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.d invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_double(cpu.core().read_fpr(9))),
                    "pipeline fmsub.d inf*0-1 should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fmsub.d inf*0-1 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmsub.d inf*0-1 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(1.0));
        cpu.core().write_fpr(11, double_to_raw_bits(1.0));
        cpu.core().write_fpr(12, double_to_raw_bits(std::ldexp(1.0, -54)));
        write32(ram, kEntry + 0, kFmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.d inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(1.0),
                    "pipeline fmsub.d 1*1-2^-54 should round back to exactly 1.0 in double precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fmsub.d 1*1-2^-54 should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fmsub.d 1*1-2^-54 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(12, double_to_raw_bits(0.0));
        write32(ram, kEntry + 0, kFmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.d overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(INFINITY),
                    "pipeline fmsub.d max*max-0 should overflow to IEEE754 double infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fmsub.d max*max-0 should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fmsub.d max*max-0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(12, double_to_raw_bits(0.0));
        write32(ram, kEntry + 0, kFmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmsub.d underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(0.0),
                    "pipeline fmsub.d min*min-0 should underflow to zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fmsub.d min*min-0 should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fmsub.d min*min-0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(2.0));
        cpu.core().write_fpr(11, double_to_raw_bits(3.0));
        cpu.core().write_fpr(12, double_to_raw_bits(4.0));
        write32(ram, kEntry + 0, kFnmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-2.0),
                    "pipeline fnmsub.d should compute -(rs1*rs2)+rs3 and commit the exact double bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
        cpu.core().write_fpr(11, double_to_raw_bits(0.0));
        cpu.core().write_fpr(12, double_to_raw_bits(1.0));
        write32(ram, kEntry + 0, kFnmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.d invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_double(cpu.core().read_fpr(9))),
                    "pipeline fnmsub.d -(inf*0)+1 should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fnmsub.d -(inf*0)+1 should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fnmsub.d -(inf*0)+1 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(1.0));
        cpu.core().write_fpr(11, double_to_raw_bits(1.0));
        cpu.core().write_fpr(12, double_to_raw_bits(std::ldexp(1.0, -54)));
        write32(ram, kEntry + 0, kFnmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.d inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-1.0),
                    "pipeline fnmsub.d -(1*1)+2^-54 should round back to exactly -1.0 in double precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fnmsub.d -(1*1)+2^-54 should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fnmsub.d -(1*1)+2^-54 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(12, double_to_raw_bits(0.0));
        write32(ram, kEntry + 0, kFnmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.d overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-INFINITY),
                    "pipeline fnmsub.d -(max*max)+0 should overflow to IEEE754 double negative infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fnmsub.d -(max*max)+0 should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fnmsub.d -(max*max)+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(12, double_to_raw_bits(0.0));
        write32(ram, kEntry + 0, kFnmsubDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmsub.d underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-0.0),
                    "pipeline fnmsub.d -(min*min)+0 should underflow to negative zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fnmsub.d -(min*min)+0 should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fnmsub.d -(min*min)+0 should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FRM, 0, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(2.0));
        cpu.core().write_fpr(11, double_to_raw_bits(3.0));
        cpu.core().write_fpr(12, double_to_raw_bits(4.0));
        write32(ram, kEntry + 0, kFnmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-10.0),
                    "pipeline fnmadd.d should compute -(rs1*rs2)-rs3 and commit the exact double bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 1ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(INFINITY));
        cpu.core().write_fpr(11, double_to_raw_bits(0.0));
        cpu.core().write_fpr(12, double_to_raw_bits(-1.0));
        write32(ram, kEntry + 0, kFnmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.d invalid smoke should eventually halt")) {
            return 1;
        }
        if (!expect(std::isnan(raw_bits_to_double(cpu.core().read_fpr(9))),
                    "pipeline fnmadd.d -(inf*0)-(-1) should produce a NaN result")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x10ULL) != 0,
                    "pipeline fnmadd.d -(inf*0)-(-1) should set the invalid-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (1ULL << 5),
                    "pipeline fnmadd.d -(inf*0)-(-1) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 2ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(1.0));
        cpu.core().write_fpr(11, double_to_raw_bits(1.0));
        cpu.core().write_fpr(12, double_to_raw_bits(-std::ldexp(1.0, -54)));
        write32(ram, kEntry + 0, kFnmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.d inexact smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-1.0),
                    "pipeline fnmadd.d -(1*1)-(-2^-54) should round back to exactly -1.0 in double precision")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x1ULL) != 0,
                    "pipeline fnmadd.d -(1*1)-(-2^-54) should set the inexact-operation flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (2ULL << 5),
                    "pipeline fnmadd.d -(1*1)-(-2^-54) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 3ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::max()));
        cpu.core().write_fpr(12, double_to_raw_bits(-0.0));
        write32(ram, kEntry + 0, kFnmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.d overflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-INFINITY),
                    "pipeline fnmadd.d -(max*max)-(-0) should overflow to IEEE754 double negative infinity")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x4ULL) != 0,
                    "pipeline fnmadd.d -(max*max)-(-0) should set the overflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (3ULL << 5),
                    "pipeline fnmadd.d -(max*max)-(-0) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        cpu.csr().write(CSR_FCSR, 4ULL << 5, cpu.core());
        cpu.core().write_fpr(10, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(11, double_to_raw_bits(std::numeric_limits<double>::min()));
        cpu.core().write_fpr(12, double_to_raw_bits(-0.0));
        write32(ram, kEntry + 0, kFnmaddDF9F10F11F12);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fnmadd.d underflow smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(-0.0),
                    "pipeline fnmadd.d -(min*min)-(-0) should underflow to negative zero")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FFLAGS, cpu.core()) & 0x2ULL) != 0,
                    "pipeline fnmadd.d -(min*min)-(-0) should set the underflow flag")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_FCSR, cpu.core()) & ~0x1fULL) == (4ULL << 5),
                    "pipeline fnmadd.d -(min*min)-(-0) should preserve the current rounding-mode bits")) {
            return 1;
        }
    }

