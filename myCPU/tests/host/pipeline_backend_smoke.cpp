#include <cstdio>
#include <vector>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/devices/clint.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/uart16550.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/exec/pipeline_core_state.h"
#include "../../src/exec/pipeline_hazards.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"
#include "../../include/platform_mmio.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint64_t kDataAddr = kEntry + 0x100;
constexpr uint32_t kNop = 0x00000013U;                    // addi x0, x0, 0
constexpr uint32_t kAddiX1 = 0x00100093U;                // addi x1, x0, 1
constexpr uint32_t kAddiX1Inc = 0x00108093U;             // addi x1, x1, 1
constexpr uint32_t kAddiX2FromX1Plus2 = 0x00208113U;     // addi x2, x1, 2
constexpr uint32_t kAddX3FromX2X1 = 0x001101b3U;         // add x3, x2, x1
constexpr uint32_t kAddiX1WrongPath = 0x06300093U;       // addi x1, x0, 99
constexpr uint32_t kAddiX2WrongPath = 0x06300113U;       // addi x2, x0, 99
constexpr uint32_t kAddiX2FromX0Plus7 = 0x00700113U;     // addi x2, x0, 7
constexpr uint32_t kAddiX2FromX0Plus5 = 0x00500113U;     // addi x2, x0, 5
constexpr uint32_t kAddiA7Exit = 0x05d00893U;            // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;                 // ecall
constexpr uint32_t kJalX0Skip8 = 0x0080006fU;            // jal x0, 8
constexpr uint32_t kBeqX0Taken = 0x00000463U;            // beq x0, x0, 8
constexpr uint32_t kInvalidInsn = 0xffffffffU;
constexpr uint32_t kLwX1FromX10 = 0x00052083U;           // lw x1, 0(x10)
constexpr uint32_t kInvalidLoadFunct3X1FromX10 = 0x00057083U;
constexpr uint32_t kAddiX2FromX1Plus5 = 0x00508113U;     // addi x2, x1, 5
constexpr uint32_t kMret = 0x30200073U;
constexpr uint32_t kSbX5ToX10Plus1 = 0x005500a3U;        // sb x5, 1(x10)
constexpr uint32_t kSbX0ToX10Plus1 = 0x000500a3U;        // sb x0, 1(x10)
constexpr uint32_t kLbX6FromX10Plus1 = 0x00150303U;      // lb x6, 1(x10)
constexpr uint32_t kLbX6FromX10Plus8 = 0x00850303U;      // lb x6, 8(x10)
constexpr uint32_t kLwX6FromX20 = 0x000a2303U;           // lw x6, 0(x20)
constexpr uint32_t kSwX6ToX20 = 0x006a2023U;             // sw x6, 0(x20)
constexpr uint32_t kInvalidStoreFunct3X6ToX20 = 0x006a7023U;
constexpr uint32_t kSdX21ToX20 = 0x015a3023U;            // sd x21, 0(x20)
constexpr uint32_t kCsrrcSipX5 = 0x1442b073U;            // csrrc x0, sip, x5
constexpr uint32_t kCsrwSepcX7 = 0x14139073U;            // csrw sepc, x7
constexpr uint32_t kSret = 0x10200073U;                  // sret
constexpr uint32_t kBltX1X2Loop = 0xfe20cee3U;           // blt x1, x2, -4

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool trace_contains_raw(const std::vector<RetireTraceEntry>& trace, uint32_t raw) {
    for (const RetireTraceEntry& entry : trace) {
        if (entry.raw == raw) {
            return true;
        }
    }
    return false;
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

int run_until_halt_and_count_redirects(PipelineBackend& backend, CPU& cpu, int max_steps) {
    int redirects = 0;
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
        backend.step();
        if (backend.debug_snapshot().pipeline.redirected) {
            ++redirects;
        }
    }
    return redirects;
}

}  // namespace

int main() {
    {
        PipelineCoreState state;
        const uint64_t first_sequence = state.allocate_sequence();
        state.record_retire({
            .sequence_id = first_sequence,
            .pc = kEntry,
            .raw = kAddiX1,
        });
        state.if_id.slot.valid = true;
        state.pending_fetch_fault = {.valid = true, .cause = 1, .tval = kEntry};
        state.pending_fetch_fault_pc = kEntry;
        state.redirect_pending = true;
        state.redirect_target = kTrapVector;
        state.flush(kEntry + 0x20);
        if (!expect(state.fetch_pc == kEntry + 0x20, "pipeline core state flush should retarget fetch pc")) {
            return 1;
        }
        if (!expect(!state.if_id.slot.valid && !state.pending_fetch_fault.valid && !state.redirect_pending,
                    "pipeline core state flush should clear in-flight stage and redirect state")) {
            return 1;
        }
        if (!expect(state.last_sequence_id() == first_sequence && state.retire_trace().size() == 1,
                    "pipeline core state flush should preserve sequence history")) {
            return 1;
        }

        state.reset(kEntry + 0x40);
        if (!expect(state.fetch_pc == kEntry + 0x40, "pipeline core state reset should install the reset fetch pc")) {
            return 1;
        }
        if (!expect(state.last_sequence_id() == 0 && state.retire_trace().empty(),
                    "pipeline core state reset should clear sequence history")) {
            return 1;
        }

        state.next_if_id.slot.valid = true;
        state.next_id_ex.slot.valid = true;
        state.stalled = true;
        state.trap_flush = true;
        state.committed = true;
        state.redirect_pending = true;
        state.redirect_target = kTrapVector;
        state.begin_cycle(true);
        if (!expect(state.interrupt_serviceable_at_cycle_start && !state.next_if_id.slot.valid &&
                        !state.next_id_ex.slot.valid && !state.stalled && !state.trap_flush &&
                        !state.committed && !state.redirect_pending && state.redirect_target == 0,
                    "pipeline core state begin_cycle should reset per-cycle transient state")) {
            return 1;
        }

        state.next_if_id.slot.valid = true;
        state.next_id_ex.slot.valid = true;
        state.commit_next_state();
        if (!expect(state.if_id.slot.valid && state.id_ex.slot.valid && !state.pipeline_empty(),
                    "pipeline core state commit_next_state should rotate next registers into the active pipe")) {
            return 1;
        }
    }

    {
        Insn addi{};
        decode(kAddiX2FromX1Plus5, &addi);
        addi.raw = kAddiX2FromX1Plus5;
        if (!expect(pipeline_hazards::reads_rs1(addi) && !pipeline_hazards::reads_rs2(addi),
                    "hazard helpers should classify addi as rs1-only")) {
            return 1;
        }

        StageSlot load_slot;
        load_slot.valid = true;
        decode(kLwX1FromX10, &load_slot.insn);
        load_slot.insn.raw = kLwX1FromX10;
        load_slot.effects.mem.kind = MemoryRequest::Kind::Load;
        load_slot.effects.mem.rd = load_slot.insn.rd;
        if (!expect(pipeline_hazards::is_load_slot(load_slot) &&
                        pipeline_hazards::inflight_rd(load_slot) == 1 &&
                        pipeline_hazards::has_decode_hazard(load_slot, addi),
                    "hazard helpers should detect load-use interlocks from the ID/EX slot")) {
            return 1;
        }

        StageSlot ex_mem_slot;
        ex_mem_slot.valid = true;
        ex_mem_slot.effects.rd_write.enable = true;
        ex_mem_slot.effects.rd_write.rd = 1;
        ex_mem_slot.effects.rd_write.value = 42;
        const uint64_t forwarded_rs1 =
            pipeline_hazards::resolve_ex_operand({.ex_mem = &ex_mem_slot}, addi, true, 0);
        if (!expect(forwarded_rs1 == 42, "hazard helpers should forward EX/MEM operands into execute")) {
            return 1;
        }

        StageSlot mem_wb_slot;
        mem_wb_slot.valid = true;
        mem_wb_slot.effects.rd_write.enable = true;
        mem_wb_slot.effects.rd_write.rd = 1;
        mem_wb_slot.effects.rd_write.value = 77;
        const uint64_t forwarded_from_wb =
            pipeline_hazards::resolve_ex_operand({.mem_wb = &mem_wb_slot}, addi, true, 0);
        if (!expect(forwarded_from_wb == 77, "hazard helpers should fall back to MEM/WB forwarding")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        write32(ram, kEntry + 0, kAddiX1);
        write32(ram, kEntry + 4, kAddiX2FromX1Plus2);
        write32(ram, kEntry + 8, kAddX3FromX2X1);
        write32(ram, kEntry + 12, kNop);
        write32(ram, kEntry + 16, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 7; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(3) == 4, "pipeline should forward ALU results across dependent instructions")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kDataAddr);
        ram.store(kDataAddr, 37, 4);

        write32(ram, kEntry + 0, kLwX1FromX10);
        write32(ram, kEntry + 4, kAddiX2FromX1Plus5);
        write32(ram, kEntry + 8, kNop);
        write32(ram, kEntry + 12, kNop);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 7; ++i) {
            backend.step();
        }
        if (!expect(cpu.core().read_gpr(2) == 42, "pipeline should resolve load-use hazards with a single interlock")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(5, 0x7a);
        cpu.core().write_gpr(10, kDataAddr);

        write32(ram, kEntry + 0, kSbX5ToX10Plus1);
        write32(ram, kEntry + 4, kLbX6FromX10Plus8);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);

        backend.step();
        backend.step();

        const auto staged_store = backend.testing_state().lsq().peek_oldest();
        if (!expect(staged_store.has_value() && staged_store->kind == LsqEntryKind::Store &&
                        !staged_store->address_ready && !staged_store->data_ready && !staged_store->order_ready,
                    "decode should stage an older store into the LSQ before its execute-time address/data are ready")) {
            return 1;
        }

        backend.step();
        const auto prepared_store = backend.testing_state().lsq().peek_oldest();
        if (!expect(prepared_store.has_value() && prepared_store->kind == LsqEntryKind::Store &&
                        prepared_store->address_ready && prepared_store->data_ready &&
                        !prepared_store->order_ready,
                    "executed store should publish address/data before it releases younger loads")) {
            return 1;
        }
        if (!expect(!backend.testing_state().stalled,
                    "executed store should stop stalling younger loads once its address/data are published")) {
            return 1;
        }
        if (!expect(backend.testing_state().id_ex.slot.valid &&
                        backend.testing_state().id_ex.slot.raw == kLbX6FromX10Plus8,
                    "non-overlapping younger load should enter decode as soon as the older store has published address/data in the LSQ")) {
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

    return 0;
}
