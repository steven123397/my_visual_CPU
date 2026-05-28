    {
        Ram ram;
        Bus bus(ram);
        Clint clint;
        bus.attach(clint);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MIE, MIE_MTIE, cpu.core());
        cpu.csr().write(CSR_MSTATUS, MSTATUS_MIE, cpu.core());
        clint.store(CLINT_BASE + CLINT_REG_MTIMECMP, 3, 8);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 3; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kEntry && cpu.core().read_gpr(1) == 0,
                    "timer interrupt should stay pending before the first architected commit becomes visible")) {
            return 1;
        }

        for (int i = 0; i < 4 && cpu.core().pc() != kTrapVector; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "timer interrupt should redirect at the commit boundary")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 1, "older committed work should be preserved before taking the timer interrupt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "timer interrupt should flush younger in-flight work")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == ((1ULL << 63) | 7ULL), "timer interrupt should report machine timer cause")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry + 4, "timer interrupt should capture the post-commit architectural pc")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        Clint clint;
        bus.attach(clint);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());

        clint.store(CLINT_BASE + CLINT_REG_MTIMECMP, 0, 8);
        cpu.trap().sync_platform_events(bus.peek_events());
        if (!expect((cpu.csr().read(CSR_SIP, cpu.core()) & MIE_STIE) != 0,
                    "delegated timer pending should assert when CLINT level is high")) {
            return 1;
        }

        clint.store(CLINT_BASE + CLINT_REG_MTIMECMP, ~0ULL, 8);
        cpu.trap().sync_platform_events(bus.peek_events());
        if (!expect((cpu.csr().read(CSR_SIP, cpu.core()) & MIE_STIE) == 0,
                    "delegated timer pending should clear when mtimecmp is raised above mtime")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        Clint clint;
        bus.attach(clint);
        CPU cpu;
        cpu_init(cpu, kEntry);
        install_machine_exit_handler(ram, cpu);
        cpu.csr().bind_clint(&clint);
        cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MIDELEG, MIE_STIE, cpu.core());
        cpu.csr().write(CSR_SIE, MIE_STIE, cpu.core());
        cpu.csr().write(CSR_MSTATUS, MSTATUS_SIE, cpu.core());
        cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
        cpu.core().write_gpr(5, MIE_STIE);
        cpu.core().write_gpr(7, kEntry + 8);
        cpu.core().write_gpr(20, CLINT_BASE + CLINT_REG_MTIMECMP);
        cpu.core().write_gpr(21, ~0ULL);
        clint.store(CLINT_BASE + CLINT_REG_MTIMECMP, 3, 8);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2WrongPath);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);
        write32(ram, kTrapVector + 0, kSdX21ToX20);
        write32(ram, kTrapVector + 4, kCsrrcSipX5);
        write32(ram, kTrapVector + 8, kCsrwSepcX7);
        write32(ram, kTrapVector + 12, kSret);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "supervisor timer interrupt path should eventually halt via ecall")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 1, "supervisor timer interrupt should preserve older committed work")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0, "supervisor timer interrupt should flush younger in-flight work")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_SCAUSE, cpu.core()) == ((1ULL << 63) | 5ULL), "supervisor timer interrupt should report delegated timer cause")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_SEPC, cpu.core()) == kEntry + 8, "supervisor timer interrupt handler should resume at the post-trap target")) {
            return 1;
        }
        if (!expect((cpu.csr().read(CSR_SIP, cpu.core()) & MIE_STIE) == 0, "supervisor timer interrupt handler should clear delegated pending state")) {
            return 1;
        }
        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(!snapshot.profile.traps.empty(),
                    "supervisor timer interrupt path should expose trap profile entries")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        Plic plic;
        Uart16550 uart(plic);
        uart.set_mirror_stdout(false);
        bus.attach(plic);
        bus.attach(uart);
        CPU cpu;
        cpu_init(cpu, kEntry);
        install_machine_exit_handler(ram, cpu);
        plic.store(PLIC_BASE + PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE), 1, 4);
        plic.store(PLIC_BASE + PLIC_ENABLE_OFFSET(PLIC_CONTEXT_SUPERVISOR), 1U << PLIC_SOURCE_UART_THRE, 4);
        plic.store(PLIC_BASE + PLIC_THRESHOLD_OFFSET(PLIC_CONTEXT_SUPERVISOR), 0, 4);
        cpu.csr().write(CSR_STVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MIDELEG, MIE_SEIE, cpu.core());
        cpu.csr().write(CSR_SIE, MIE_SEIE, cpu.core());
        cpu.csr().write(CSR_MSTATUS, MSTATUS_SIE, cpu.core());
        cpu.core().set_privilege_mode(PrivilegeMode::Supervisor);
        cpu.core().write_gpr(5, UART_IER_THRI);
        cpu.core().write_gpr(7, kEntry + 8);
        cpu.core().write_gpr(10, UART_BASE);
        cpu.core().write_gpr(20, PLIC_BASE + PLIC_CLAIM_OFFSET(PLIC_CONTEXT_SUPERVISOR));

        write32(ram, kEntry + 0, kSbX5ToX10Plus1);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);
        write32(ram, kTrapVector + 0, kLwX6FromX20);
        write32(ram, kTrapVector + 4, kSbX0ToX10Plus1);
        write32(ram, kTrapVector + 8, kSwX6ToX20);
        write32(ram, kTrapVector + 12, kCsrwSepcX7);
        write32(ram, kTrapVector + 16, kSret);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 24 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "supervisor external interrupt path should eventually halt via ecall")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "supervisor external interrupt should flush younger in-flight work")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_SCAUSE, cpu.core()) == ((1ULL << 63) | 9ULL), "supervisor external interrupt should report delegated external cause")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_SEPC, cpu.core()) == kEntry + 8, "supervisor external interrupt handler should resume at the post-trap target")) {
            return 1;
        }
        if (!expect(uart.ier() == 0, "supervisor external interrupt handler should clear UART interrupt enable")) {
            return 1;
        }
        if (!expect(!plic.supervisor_has_pending() && !plic.source_claimed(PLIC_SOURCE_UART_THRE), "supervisor external interrupt handler should claim and complete the PLIC source")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kInvalidInsn);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 5; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "illegal instruction should trap only when it reaches commit")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "illegal instruction should flush younger writes")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2, "illegal instruction should report illegal-instruction cause")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().set_privilege_mode(PrivilegeMode::User);
        cpu.core().write_fpr(10, 0x0123456789abcdefULL);
        cpu.csr().write(CSR_MSTATUS, MSTATUS_FS_OFF, cpu.core());
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kFmvXDA0FromF10);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8 && cpu.core().pc() != kTrapVector; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().privilege_mode() == PrivilegeMode::Machine,
                    "pipeline U-mode fmv.x.d with FS=Off should trap for Linux lazy fstate restore")) {
            return 1;
        }
        if (!expect(cpu.core().pc() == kTrapVector,
                    "pipeline U-mode fmv.x.d with FS=Off should redirect to mtvec at commit")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2,
                    "pipeline U-mode fmv.x.d with FS=Off should report illegal-instruction cause")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == kEntry,
                    "pipeline U-mode fmv.x.d with FS=Off should capture the faulting pc in mepc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == kFmvXDA0FromF10,
                    "pipeline U-mode fmv.x.d with FS=Off should report the raw instruction in mtval")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0,
                    "pipeline U-mode fmv.x.d with FS=Off must not commit stale FPR state into a0")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kInvalidLoadFunct3X1FromX10);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "invalid load funct3 should trap instead of stalling pipeline")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "invalid load funct3 trap should flush younger writes")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2, "invalid load funct3 should report illegal-instruction cause")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kInvalidStoreFunct3X6ToX20);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 8; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "invalid store funct3 should trap instead of stalling pipeline")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "invalid store funct3 trap should flush younger writes")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 2, "invalid store funct3 should report illegal-instruction cause")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, 0, cpu.core());
        cpu.csr().write(CSR_MSTATUS, 0x1800, cpu.core());

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kTrapVector + 0, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 6; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().pc() == kTrapVector, "fetch fault after mret should eventually trap to mtvec")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 1, "fetch fault after mret should report instruction access fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0, "fetch fault after mret should preserve the faulting pc")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MTVAL, cpu.core()) == 0, "fetch fault after mret should preserve mtval")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "fetch fault after mret should flush wrong-path work after the return")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kJalX0Skip8);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);

        const int redirects = run_until_halt_and_count_redirects(backend, cpu, 24);
        if (!expect(cpu.core().halted(), "jal predict-hit smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 0, "jal predict-hit smoke should keep wrong-path writes flushed")) {
            return 1;
        }
        if (!expect(redirects == 0, "jal predict-hit path should not need execute-time redirect")) {
            return 1;
        }
        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(snapshot.pipeline.last_sequence_id >= 3,
                    "jal predict-hit smoke should expose a non-zero last_sequence_id")) {
            return 1;
        }
        if (!expect(!snapshot.pipeline.retire_trace.empty(),
                    "jal predict-hit smoke should expose a bounded retire trace")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.retire_trace.front().sequence_id == 1,
                    "retire trace should preserve the earliest committed sequence id")) {
            return 1;
        }
        if (!expect(!trace_contains_raw(snapshot.pipeline.retire_trace, kAddiX1WrongPath),
                    "retire trace should not contain jal wrong-path instructions")) {
            return 1;
        }
        if (!expect(!snapshot.profile.hot_paths.empty(),
                    "jal predict-hit smoke should expose hot-path profile entries")) {
            return 1;
        }
        if (!expect(!snapshot.profile.branches.empty(),
                    "jal predict-hit smoke should expose branch profile entries")) {
            return 1;
        }
        if (!expect(!snapshot.profile.syscalls.empty(),
                    "jal predict-hit smoke should expose syscall profile entries")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kJalX0Skip8);
        write32(ram, kEntry + 4, kAddiX1WrongPath);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kBeqX0Taken);
        write32(ram, kEntry + 16, kAddiX2WrongPath);
        write32(ram, kEntry + 20, kAddiA7Exit);
        write32(ram, kEntry + 24, kEcall);

        PipelineBackend backend(cpu, bus);

        backend.step();
        backend.step();
        backend.step();

        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(snapshot.pipeline.predictor.last_prediction_valid,
                    "resolved control-flow snapshot should report a valid last prediction")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.predictor.last_prediction_taken,
                    "fetching a younger branch should not overwrite the resolved jal prediction direction")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.predictor.last_prediction_correct,
                    "jal predict-hit should remain marked correct in the same cycle")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.predictor.last_prediction_pc == kEntry,
                    "last_prediction_pc should refer to the resolved jal, not a younger fetched branch")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.predictor.last_prediction_target == kEntry + 8,
                    "last_prediction_target should stay paired with the resolved jal")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX2FromX0Plus5);
        write32(ram, kEntry + 4, kAddiX1Inc);
        write32(ram, kEntry + 8, kBltX1X2Loop);
        write32(ram, kEntry + 12, kAddiA7Exit);
        write32(ram, kEntry + 16, kEcall);

        PipelineBackend backend(cpu, bus);

        const int redirects = run_until_halt_and_count_redirects(backend, cpu, 48);
        if (!expect(cpu.core().halted(), "branch predictor loop smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(1) == 5, "branch predictor loop smoke should leave the loop counter at 5")) {
            return 1;
        }
        if (!expect(redirects == 2, "trained branch loop should only redirect on cold miss and exit mispredict")) {
            return 1;
        }
    }

    {
        Ram ram1;
        Bus bus1(ram1);
        CPU cpu1;
        cpu_init(cpu1, kEntry);
        write32(ram1, kEntry + 0, kAddiX2FromX0Plus5);
        write32(ram1, kEntry + 4, kAddiX1Inc);
        write32(ram1, kEntry + 8, kBltX1X2Loop);
        write32(ram1, kEntry + 12, kAddiA7Exit);
        write32(ram1, kEntry + 16, kEcall);

        PipelineBackend trained_backend(cpu1, bus1);
        const int trained_redirects = run_until_halt_and_count_redirects(trained_backend, cpu1, 48);
        if (!expect(trained_redirects == 2, "trained loop baseline should converge to the expected redirect count")) {
            return 1;
        }

        Ram ram2;
        Bus bus2(ram2);
        CPU cpu2;
        cpu_init(cpu2, kEntry);
        write32(ram2, kEntry + 0, kAddiX2FromX0Plus5);
        write32(ram2, kEntry + 4, kAddiX1Inc);
        write32(ram2, kEntry + 8, kBltX1X2Loop);
        write32(ram2, kEntry + 12, kAddiA7Exit);
        write32(ram2, kEntry + 16, kEcall);

        PipelineBackend reset_backend(cpu2, bus2);
        const int reset_redirects = run_until_halt_and_count_redirects(reset_backend, cpu2, 48);
        if (!expect(reset_redirects == 2, "new pipeline backend should start from a cold predictor state")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        ram.store(kDataAddr, 0x8877665544332211ULL, 8);
        cpu.core().write_gpr(10, kDataAddr);
        write32(ram, kEntry + 0, kFldF5FromX10);
        write32(ram, kEntry + 4, kFsdF5ToX10Plus8);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fp load/store smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(5) == 0x8877665544332211ULL,
                    "pipeline fld should write the floating-point register file")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(5) == 0,
                    "pipeline fld should not allocate or commit an integer register destination")) {
            return 1;
        }
        if (!expect(ram.load(kDataAddr + 8, 8) == 0x8877665544332211ULL,
                    "pipeline fsd should store raw fpr bits")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        ram.store(kDataAddr, double_to_raw_bits(3.5), 8);
        cpu.core().write_gpr(10, kDataAddr);
        cpu.core().write_fpr(10, double_to_raw_bits(2.0));
        write32(ram, kEntry + 0, kFldF5FromX10);
        write32(ram, kEntry + 4, kFaddDF6F5F10);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fp load-use smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(6) == double_to_raw_bits(5.5),
                    "pipeline should not let a younger fadd.d read a stale fpr value after fld")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(10, float_to_fpr_raw_bits(1.5f));
        cpu.core().write_fpr(11, double_to_raw_bits(2.0));
        cpu.core().write_fpr(15, double_to_raw_bits(-10.0));
        write32(ram, kEntry + 0, kFcvtDSF15FromF10);
        write32(ram, kEntry + 4, 0x02b787d3U);  // fadd.d fa5, fa5, fa1
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.d.s load-use smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(15) == double_to_raw_bits(3.5),
                    "pipeline should not let a younger fadd.d read a stale fpr value after fcvt.d.s")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(10, 7);
        cpu.core().write_fpr(9, double_to_raw_bits(-10.0));
        write32(ram, kEntry + 0, kFcvtSWF15FromX10);
        write32(ram, kEntry + 4, 0x421784d3U);  // fcvt.d.s fs1, fa5
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fcvt.s.w -> fcvt.d.s smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(9) == double_to_raw_bits(7.0),
                    "pipeline should not let a younger fcvt.d.s read stale bits after fcvt.s.w")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_fpr(8, float_to_fpr_raw_bits(-10.0f));
        cpu.core().write_fpr(10, float_to_fpr_raw_bits(2.0f));
        cpu.core().write_fpr(11, float_to_fpr_raw_bits(3.0f));
        cpu.core().write_fpr(12, float_to_fpr_raw_bits(4.0f));
        write32(ram, kEntry + 0, kFmaddSF8F10F11F12);
        write32(ram, kEntry + 4, kFaddSF8F8F10);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmadd.s consumer smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(8) == float_to_fpr_raw_bits(12.0f),
                    "pipeline should not let a younger fadd.s read stale fs0 after fmadd.s")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        init_fp_cpu(cpu);

        cpu.core().write_gpr(11, 0x1122334455667788ULL);
        write32(ram, kEntry + 0, 0xf2058553U);  // fmv.d.x fa0, a1
        write32(ram, kEntry + 4, kFmvXDA0FromF10);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 32 && !cpu.core().halted(); ++i) {
            backend.step();
        }
        if (!expect(cpu.core().halted(), "pipeline fmv.d.x/fmv.x.d smoke should eventually halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_fpr(10) == 0x1122334455667788ULL,
                    "pipeline fmv.d.x should write raw integer bits into the floating-point register file")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(10) == 0x1122334455667788ULL,
                    "pipeline fmv.x.d should write raw floating-point bits into the integer register file")) {
            return 1;
        }
    }
