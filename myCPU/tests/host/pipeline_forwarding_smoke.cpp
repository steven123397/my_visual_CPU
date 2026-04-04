#include "pipeline_smoke_common.h"

int main() {
    {
        Insn addi{};
        decode(kAddiX2FromX1Plus5, &addi);
        addi.raw = kAddiX2FromX1Plus5;
        if (!expect(pipeline_hazards::reads_rs1(addi) &&
                        !pipeline_hazards::reads_rs2(addi),
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
            pipeline_hazards::resolve_ex_operand({.ex_mem = &ex_mem_slot},
                                                 addi,
                                                 true,
                                                 0);
        if (!expect(forwarded_rs1 == 42,
                    "hazard helpers should forward EX/MEM operands into execute")) {
            return 1;
        }

        StageSlot mem_wb_slot;
        mem_wb_slot.valid = true;
        mem_wb_slot.effects.rd_write.enable = true;
        mem_wb_slot.effects.rd_write.rd = 1;
        mem_wb_slot.effects.rd_write.value = 77;
        const uint64_t forwarded_from_wb =
            pipeline_hazards::resolve_ex_operand({.mem_wb = &mem_wb_slot},
                                                 addi,
                                                 true,
                                                 0);
        if (!expect(forwarded_from_wb == 77,
                    "hazard helpers should fall back to MEM/WB forwarding")) {
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
        if (!expect(cpu.core().read_gpr(3) == 4,
                    "pipeline should forward ALU results across dependent instructions")) {
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
        if (!expect(cpu.core().read_gpr(2) == 42,
                    "pipeline should resolve load-use hazards with a single interlock")) {
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
        if (!expect(staged_store.has_value() &&
                        staged_store->kind == LsqEntryKind::Store &&
                        !staged_store->address_ready &&
                        !staged_store->data_ready &&
                        !staged_store->order_ready,
                    "decode should stage an older store into the LSQ before its execute-time address/data are ready")) {
            return 1;
        }

        backend.step();
        const auto prepared_store = backend.testing_state().lsq().peek_oldest();
        if (!expect(prepared_store.has_value() &&
                        prepared_store->kind == LsqEntryKind::Store &&
                        prepared_store->address_ready &&
                        prepared_store->data_ready &&
                        !prepared_store->order_ready,
                    "executed store should publish address/data before it releases younger loads")) {
            return 1;
        }
        if (!expect(!backend.testing_state().stalled,
                    "executed store should stop stalling younger loads once its address/data are published")) {
            return 1;
        }
        if (!expect(backend.testing_state().id_ex.slot.valid &&
                        backend.testing_state().id_ex.slot.raw ==
                            kLbX6FromX10Plus8,
                    "non-overlapping younger load should enter decode as soon as the older store has published address/data in the LSQ")) {
            return 1;
        }
    }

    return 0;
}
