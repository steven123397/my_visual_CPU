#include <cstdio>

#include "../../src/arch/csr_file.h"
#include "../../src/cpu.h"
#include "../../src/devices/plic.h"
#include "../../src/devices/uart16550.h"
#include "../../src/exec/pipeline_backend.h"
#include "../../src/exec/pipeline_commit_boundary.h"
#include "../../src/mem/bus.h"
#include "../../src/mem/ram.h"

namespace {

constexpr uint64_t kEntry = MEM_BASE;
constexpr uint64_t kTrapVector = kEntry + 0x80;
constexpr uint64_t kDataAddr = kEntry + 0x100;
constexpr uint32_t kAddiX1FromX0Plus1 = 0x00100093U;
constexpr uint32_t kAddiX2FromX0Plus2 = 0x00200113U;
constexpr uint32_t kAddiX3FromX0Plus3 = 0x00300193U;
constexpr uint32_t kAddiX4FromX0Plus4 = 0x00400213U;
constexpr uint32_t kLwX1FromX10 = 0x00052083U;
constexpr uint32_t kAddiA7Exit = 0x05d00893U;      // addi a7, x0, 93
constexpr uint32_t kEcall = 0x00000073U;           // ecall
constexpr uint32_t kMret = 0x30200073U;
constexpr uint32_t kSbX5ToX10Plus1 = 0x005500a3U;  // sb x5, 1(x10)
constexpr uint32_t kLbX6FromX10Plus1 = 0x00150303U;
constexpr uint32_t kSbX6ToX10Plus2 = 0x00650123U;  // sb x6, 2(x10)
constexpr uint32_t kCsrwMepcX5 = 0x34129073U;      // csrw mepc, x5
constexpr uint32_t kInvalidInsn = 0xffffffffU;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

void write32(Ram& ram, uint64_t addr, uint32_t value) {
    ram.write_bytes(addr, &value, sizeof(value));
}

bool run_until_halt(PipelineBackend& backend, CPU& cpu, int max_steps) {
    for (int i = 0; i < max_steps && !cpu.core().halted(); ++i) {
        backend.step();
    }
    return cpu.core().halted();
}

}  // namespace

int main() {
    CommitBoundaryInput commit_input{};
    (void)commit_input;

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.core().write_gpr(10, 0);

        write32(ram, kEntry + 0, kLwX1FromX10);
        write32(ram, kEntry + 4, kAddiX2FromX0Plus2);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);
        write32(ram, kTrapVector + 0, kAddiA7Exit);
        write32(ram, kTrapVector + 4, kEcall);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 4; ++i) {
            backend.step();
        }

        const PipelineCoreState& state = backend.testing_state();
        const auto rob_head = state.rob().peek_head();
        const uint32_t x2_phys = state.rename_map().map_source(2);
        if (!expect(rob_head.has_value() && rob_head->sequence_id == 1 && !rob_head->ready,
                    "older faulting load should still be pending while younger ALU completion is already speculative-ready")) {
            return 1;
        }
        if (!expect(x2_phys != 0 && state.phys_regs().is_ready(x2_phys) && state.phys_regs().read(x2_phys) == 2,
                    "younger ALU should be able to finish before the older memory fault resolves")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0,
                    "younger ALU result must stay invisible architecturally until the faulting older head resolves")) {
            return 1;
        }

        if (!expect(run_until_halt(backend, cpu, 32), "faulting memory OoO speculation contract should halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(2) == 0,
                    "older memory fault must still squash the already-ready younger ALU result")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 5,
                    "older unresolved load should still retire as a precise load access fault")) {
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
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        if (!expect(ram.load(kDataAddr + 1, 1) == 0, "RAM store must stay invisible before commit boundary")) {
            return 1;
        }
        if (!expect(run_until_halt(backend, cpu, 16), "RAM store commit-boundary contract should halt")) {
            return 1;
        }
        if (!expect(ram.load(kDataAddr + 1, 1) == 0x7a, "RAM store should become visible after commit boundary")) {
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
        cpu.core().write_gpr(5, UART_IER_THRI);
        cpu.core().write_gpr(10, UART_BASE);

        write32(ram, kEntry + 0, kSbX5ToX10Plus1);
        write32(ram, kEntry + 4, kLbX6FromX10Plus1);
        write32(ram, kEntry + 8, kAddiA7Exit);
        write32(ram, kEntry + 12, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "MMIO store->load ordering contract should halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(6) == UART_IER_THRI,
                    "younger MMIO load must observe the older MMIO store result")) {
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
        cpu.core().write_gpr(5, UART_IER_THRI);
        cpu.core().write_gpr(10, UART_BASE);

        write32(ram, kEntry + 0, kSbX5ToX10Plus1);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        if (!expect(uart.ier() == 0, "MMIO store must stay invisible before commit boundary")) {
            return 1;
        }
        if (!expect(run_until_halt(backend, cpu, 16), "MMIO store commit-boundary contract should halt")) {
            return 1;
        }
        if (!expect(uart.ier() == UART_IER_THRI, "MMIO store should become visible at commit boundary")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());

        write32(ram, kEntry + 0, kAddiX1FromX0Plus1);
        write32(ram, kEntry + 4, kInvalidInsn);
        write32(ram, kEntry + 8, kAddiX2FromX0Plus2);
        write32(ram, kEntry + 12, kAddiX3FromX0Plus3);
        write32(ram, kTrapVector + 0, kAddiX4FromX0Plus4);
        write32(ram, kTrapVector + 4, kAddiA7Exit);
        write32(ram, kTrapVector + 8, kEcall);

        PipelineBackend backend(cpu, bus);
        for (int i = 0; i < 4; ++i) {
            backend.step();
        }
        const PipelineCoreState& state = backend.testing_state();
        const uint32_t recycled_phys = state.id_ex.slot.rd_phys;
        if (!expect(state.id_ex.slot.raw == kAddiX2FromX0Plus2 && recycled_phys != 0,
                    "a younger rename should consume the stale phys tag that was recycled by the older ROB head commit")) {
            return 1;
        }

        for (int i = 0; i < 2; ++i) {
            backend.step();
        }
        if (!expect(state.id_ex.slot.raw == kAddiX4FromX0Plus4 && state.id_ex.slot.rd_phys == recycled_phys,
                    "trap rollback should restore the recycled free-list so the squashed phys tag can be reused again")) {
            return 1;
        }

        if (!expect(run_until_halt(backend, cpu, 32), "trap rollback free-list contract should halt")) {
            return 1;
        }
        if (!expect(cpu.core().read_gpr(4) == 4 && cpu.core().read_gpr(3) == 0,
                    "trap rollback should keep the handler result while preventing the squashed younger rename from leaking")) {
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
        cpu.csr().write(CSR_MSTATUS,
                        static_cast<uint64_t>(PrivilegeMode::Machine) << MSTATUS_MPP_SHIFT,
                        cpu.core());
        cpu.core().write_gpr(5, 0x7a);
        cpu.core().write_gpr(10, kDataAddr);

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kSbX5ToX10Plus1);
        write32(ram, kTrapVector + 0, kAddiA7Exit);
        write32(ram, kTrapVector + 4, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "mret + wrong-path RAM store contract should halt")) {
            return 1;
        }
        if (!expect(ram.load(kDataAddr + 1, 1) == 0, "squashed younger RAM store must not update memory")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MCAUSE, cpu.core()) == 1, "trap-return path should still report precise fetch fault")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0, "trap-return fetch fault should preserve the faulting pc")) {
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
        cpu.csr().write(CSR_MTVEC, kTrapVector, cpu.core());
        cpu.csr().write(CSR_MEPC, 0, cpu.core());
        cpu.csr().write(CSR_MSTATUS,
                        static_cast<uint64_t>(PrivilegeMode::Machine) << MSTATUS_MPP_SHIFT,
                        cpu.core());
        cpu.core().write_gpr(5, UART_IER_THRI);
        cpu.core().write_gpr(10, UART_BASE);

        write32(ram, kEntry + 0, kMret);
        write32(ram, kEntry + 4, kSbX5ToX10Plus1);
        write32(ram, kTrapVector + 0, kAddiA7Exit);
        write32(ram, kTrapVector + 4, kEcall);

        PipelineBackend backend(cpu, bus);
        if (!expect(run_until_halt(backend, cpu, 32), "mret + wrong-path MMIO store contract should halt")) {
            return 1;
        }
        if (!expect(uart.ier() == 0, "squashed younger MMIO store must not update device state")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.csr().write(CSR_MEPC, 0x1234, cpu.core());
        cpu.core().write_gpr(5, 0x5678);

        write32(ram, kEntry + 0, kCsrwMepcX5);
        write32(ram, kEntry + 4, kAddiA7Exit);
        write32(ram, kEntry + 8, kEcall);

        PipelineBackend backend(cpu, bus);

        for (int i = 0; i < 3; ++i) {
            backend.step();
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0x1234, "CSR write must stay invisible before commit boundary")) {
            return 1;
        }

        if (!expect(run_until_halt(backend, cpu, 16), "CSR commit-boundary contract should halt")) {
            return 1;
        }
        if (!expect(cpu.csr().read(CSR_MEPC, cpu.core()) == 0x5678, "CSR write should become visible after commit boundary")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kDataAddr + 0x20);

        PipelineBackend backend(cpu, bus);
        PipelineCoreState& state = backend.testing_state();
        LoadStoreQueue& lsq = state.lsq();
        const LsqIndex older_store = lsq.enqueue_store({
            .sequence_id = 1,
            .size = 4,
        });
        lsq.mark_address_ready(older_store, kDataAddr);

        state.if_id.slot.valid = true;
        state.if_id.slot.sequence_id.value = 2;
        state.if_id.slot.pc = kEntry;
        state.if_id.slot.raw = kLwX1FromX10;

        backend.step();

        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(!state.stalled && state.lsq_observed_load_status.state == LsqLoadState::None,
                    "non-overlapping younger load should no longer stall decode once the older store address is known")) {
            return 1;
        }
        if (!expect(state.id_ex.slot.valid && state.id_ex.slot.sequence_id.value == 2,
                    "non-overlapping younger load should advance past decode instead of being held in if/id")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.ooo.lsq_load_state == "none",
                    "debug snapshot should stop reporting unresolved-store block for the non-overlapping decode case")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        cpu.core().write_gpr(10, kDataAddr);

        PipelineBackend backend(cpu, bus);
        PipelineCoreState& state = backend.testing_state();
        LoadStoreQueue& lsq = state.lsq();
        const LsqIndex older_store = lsq.enqueue_store({
            .sequence_id = 1,
            .size = 4,
        });
        lsq.mark_address_ready(older_store, kDataAddr);

        state.if_id.slot.valid = true;
        state.if_id.slot.sequence_id.value = 2;
        state.if_id.slot.pc = kEntry;
        state.if_id.slot.raw = kLwX1FromX10;

        backend.step();

        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(state.stalled &&
                        state.lsq_observed_load_status.state == LsqLoadState::BlockedByOverlappingStore &&
                        state.lsq_observed_load_status.load_sequence_id == 2 &&
                        state.lsq_observed_load_status.store_sequence_id == 1,
                    "overlapping younger load should still stall at decode with an explicit overlapping-store reason")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.stalled &&
                        snapshot.pipeline.ooo.lsq_load_state == "blocked_by_overlapping_store" &&
                        snapshot.pipeline.ooo.lsq_load_sequence_id == 2 &&
                        snapshot.pipeline.ooo.lsq_store_sequence_id == 1,
                    "debug snapshot should continue exposing the overlapping-store decode stall reason")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        PipelineBackend backend(cpu, bus);
        LoadStoreQueue& lsq = backend.testing_state().lsq();
        const LsqIndex older_store = lsq.enqueue_store({
            .sequence_id = 1,
            .size = 4,
        });
        const LsqIndex younger_load = lsq.enqueue_load({
            .sequence_id = 2,
            .rd = 6,
            .size = 4,
        });

        lsq.mark_address_ready(younger_load, kDataAddr);
        lsq.mark_data_ready(younger_load, 0x11223344ULL);
        lsq.mark_order_ready(younger_load);
        lsq.mark_address_ready(older_store, kDataAddr);

        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(snapshot.pipeline.ooo.lsq_load_state == "replay_required" &&
                        snapshot.pipeline.ooo.lsq_load_sequence_id == 2 &&
                        snapshot.pipeline.ooo.lsq_store_sequence_id == 1,
                    "pipeline snapshot should expose LSQ replay-needed when a late overlapping older store appears")) {
            return 1;
        }
        if (!expect(cpu.core().instret() == 0 && ram.load(kDataAddr, 4) == 0,
                    "observing replay-needed should not itself change architected state or memory")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);

        PipelineBackend backend(cpu, bus);
        PipelineCoreState& state = backend.testing_state();
        LoadStoreQueue& lsq = state.lsq();
        const LsqIndex older_store = lsq.enqueue_store({
            .sequence_id = 1,
            .size = 4,
        });
        const LsqIndex younger_load = lsq.enqueue_load({
            .sequence_id = 2,
            .rd = 6,
            .size = 4,
        });
        lsq.mark_address_ready(younger_load, kDataAddr);
        lsq.mark_data_ready(younger_load, 0x11223344ULL);
        lsq.mark_order_ready(younger_load);
        lsq.mark_address_ready(older_store, kDataAddr);

        constexpr uint32_t kReplayPhys = 33;
        state.phys_regs().write(kReplayPhys, 0x11223344ULL);
        const RobIndex replay_rob = state.rob().allocate({
            .sequence_id = 2,
            .pc = kEntry,
            .raw = kLbX6FromX10Plus1,
            .arch_rd = 6,
            .phys_rd = kReplayPhys,
            .previous_phys_rd = 6,
        });
        state.rob().mark_ready(replay_rob, {
            .value_ready = true,
            .value = 0x11223344ULL,
        });
        state.mem_wb.slot.valid = true;
        state.mem_wb.slot.sequence_id.value = 2;
        state.mem_wb.slot.pc = kEntry;
        state.mem_wb.slot.raw = kLbX6FromX10Plus1;
        state.mem_wb.slot.rd_phys = kReplayPhys;
        state.mem_wb.slot.rob_index = replay_rob;
        state.mem_wb.slot.lsq_index = younger_load;
        state.mem_wb.slot.effects.rd_write.enable = true;
        state.mem_wb.slot.effects.rd_write.rd = 6;
        state.mem_wb.slot.effects.rd_write.value = 0x11223344ULL;

        backend.step();
        const BackendDebugSnapshot snapshot = backend.debug_snapshot();
        if (!expect(cpu.core().instret() == 0 && cpu.core().read_gpr(6) == 0 && cpu.core().pc() == kEntry,
                    "automatic replay must squash the replay-required younger load before it commits")) {
            return 1;
        }
        if (!expect(state.rob().size() == 0 && state.lsq().size() == 0,
                    "automatic replay should roll speculative ROB/LSQ state back to the committed boundary")) {
            return 1;
        }
        if (!expect(snapshot.pipeline.replay_flush && !snapshot.pipeline.trap_flush,
                    "automatic replay should surface replay_flush without pretending a trap occurred")) {
            return 1;
        }
    }

    {
        Ram ram;
        Bus bus(ram);
        CPU cpu;
        cpu_init(cpu, kEntry);
        ram.store(kDataAddr + 1, 0, 1);

        PipelineBackend backend(cpu, bus);
        PipelineCoreState& state = backend.testing_state();
        LoadStoreQueue& lsq = state.lsq();
        const LsqIndex older_store = lsq.enqueue_store({
            .sequence_id = 1,
            .size = 1,
        });
        lsq.mark_address_ready(older_store, kDataAddr + 1);
        lsq.mark_data_ready(older_store, 0x7aULL);
        lsq.mark_order_ready(older_store);

        constexpr uint32_t kLoadPhys = 33;
        state.phys_regs().set_pending(kLoadPhys);
        const RobIndex load_rob = state.rob().allocate({
            .sequence_id = 2,
            .pc = kEntry + 4,
            .raw = kLbX6FromX10Plus1,
            .arch_rd = 6,
            .phys_rd = kLoadPhys,
            .previous_phys_rd = 6,
        });
        state.ex_mem.slot.valid = true;
        state.ex_mem.slot.sequence_id.value = 2;
        state.ex_mem.slot.pc = kEntry + 4;
        state.ex_mem.slot.raw = kLbX6FromX10Plus1;
        state.ex_mem.slot.rd_phys = kLoadPhys;
        state.ex_mem.slot.rob_index = load_rob;
        state.ex_mem.slot.lsq_index = lsq.enqueue_load({
            .sequence_id = 2,
            .rd = 6,
            .size = 1,
            .sign_extend = true,
        });
        state.ex_mem.slot.effects.mem.kind = MemoryRequest::Kind::Load;
        state.ex_mem.slot.effects.mem.addr = kDataAddr + 1;
        state.ex_mem.slot.effects.mem.rd = 6;
        state.ex_mem.slot.effects.mem.size = 1;
        state.ex_mem.slot.effects.mem.sign_extend = true;
        lsq.mark_address_ready(state.ex_mem.slot.lsq_index, kDataAddr + 1);

        backend.step();
        if (!expect(state.mem_wb.slot.valid && state.mem_wb.slot.effects.rd_write.enable &&
                        state.mem_wb.slot.effects.rd_write.value == 0x7a &&
                        state.phys_regs().read(kLoadPhys) == 0x7a,
                    "younger RAM load should observe the older ready store via LSQ forwarding before the store commits")) {
            return 1;
        }
        if (!expect(ram.load(kDataAddr + 1, 1) == 0,
                    "forwarded load value must not come from prematurely updating RAM before store commit")) {
            return 1;
        }
    }

    return 0;
}
