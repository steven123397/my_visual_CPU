#include "pipeline_hazards.h"

#include "../isa/instruction_semantics.h"

namespace {

bool forward_operand_from_slot(const StageSlot& slot, uint32_t phys, uint64_t& value) {
    if (!slot.valid || phys == 0) {
        return false;
    }

    const uint32_t slot_phys = slot.rd_phys != 0
                                   ? slot.rd_phys
                                   : (slot.effects.rd_write.enable
                                          ? static_cast<uint32_t>(slot.effects.rd_write.rd)
                                          : static_cast<uint32_t>(slot.insn.rd));
    if (slot_phys != phys) {
        return false;
    }

    if (slot.effects.rd_write.enable) {
        value = slot.effects.rd_write.value;
        return true;
    }

    return false;
}

}  // namespace

namespace pipeline_hazards {

bool reads_rs1(const Insn& insn) {
    return InstructionSemantics::describe_registers(insn).rs1 == RegisterOperandKind::Gpr;
}

bool reads_rs2(const Insn& insn) {
    return InstructionSemantics::describe_registers(insn).rs2 == RegisterOperandKind::Gpr;
}

bool writes_rd(const Insn& insn) {
    return InstructionSemantics::describe_registers(insn).rd == RegisterOperandKind::Gpr;
}

bool is_load_slot(const StageSlot& slot) {
    return slot.valid &&
           InstructionSemantics::describe_memory(slot.insn).kind == MemoryRequest::Kind::Load;
}

bool is_store_slot(const StageSlot& slot) {
    return slot.valid &&
           InstructionSemantics::describe_memory(slot.insn).kind == MemoryRequest::Kind::Store;
}

uint32_t inflight_dest_phys(const StageSlot& slot) {
    if (!slot.valid) {
        return 0;
    }
    if (slot.effects.mem.target == MemoryRequest::Target::Float) {
        return 0;
    }
    if (slot.rd_phys != 0) {
        return slot.rd_phys;
    }
    if (slot.effects.mem.kind == MemoryRequest::Kind::Load) {
        return slot.effects.mem.rd;
    }
    return slot.insn.rd;
}

uint8_t inflight_rd(const StageSlot& slot) {
    if (!slot.valid) {
        return 0;
    }
    return slot.insn.rd;
}

bool has_decode_hazard(const StageSlot& id_ex_slot, const Insn& insn) {
    const uint32_t rs1_phys = reads_rs1(insn) ? static_cast<uint32_t>(insn.rs1) : 0;
    const uint32_t rs2_phys = reads_rs2(insn) ? static_cast<uint32_t>(insn.rs2) : 0;
    return has_decode_hazard(id_ex_slot, rs1_phys, rs2_phys);
}

bool has_decode_hazard(const StageSlot& id_ex_slot, uint32_t rs1_phys, uint32_t rs2_phys) {
    if (!is_load_slot(id_ex_slot)) {
        return false;
    }

    const uint32_t id_ex_phys = inflight_dest_phys(id_ex_slot);
    if (id_ex_phys == 0) {
        return false;
    }

    if (rs1_phys != 0 && rs1_phys == id_ex_phys) {
        return true;
    }
    if (rs2_phys != 0 && rs2_phys == id_ex_phys) {
        return true;
    }
    return false;
}

uint64_t resolve_register_value(const PipelineForwardingSources& sources,
                                uint8_t rs,
                                uint64_t latched_value) {
    return resolve_register_value(sources, static_cast<uint32_t>(rs), latched_value);
}

uint64_t resolve_register_value(const PipelineForwardingSources& sources,
                                uint32_t phys,
                                uint64_t latched_value) {
    uint64_t value = latched_value;

    if (sources.ex_mem != nullptr && forward_operand_from_slot(*sources.ex_mem, phys, value)) {
        return value;
    }
    if (sources.mem_wb != nullptr && forward_operand_from_slot(*sources.mem_wb, phys, value)) {
        return value;
    }
    return value;
}

uint64_t resolve_ex_operand(const PipelineForwardingSources& sources,
                            const Insn& insn,
                            bool use_rs1,
                            uint64_t latched_value) {
    const uint32_t phys = use_rs1 ? static_cast<uint32_t>(insn.rs1) : static_cast<uint32_t>(insn.rs2);
    return resolve_ex_operand(sources, phys, latched_value);
}

uint64_t resolve_ex_operand(const PipelineForwardingSources& sources,
                            uint32_t phys,
                            uint64_t latched_value) {
    return resolve_register_value(sources, phys, latched_value);
}

}  // namespace pipeline_hazards
