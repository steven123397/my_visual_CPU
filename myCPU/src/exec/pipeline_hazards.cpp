#include "pipeline_hazards.h"

namespace {

bool forward_operand_from_slot(const StageSlot& slot, uint8_t rs, uint64_t& value) {
    if (!slot.valid || rs == 0) {
        return false;
    }

    if (slot.effects.rd_write.enable && slot.effects.rd_write.rd == rs) {
        value = slot.effects.rd_write.value;
        return true;
    }

    return false;
}

}  // namespace

namespace pipeline_hazards {

bool reads_rs1(const Insn& insn) {
    switch (insn.opcode) {
    case 0x13:
    case 0x1B:
    case 0x33:
    case 0x3B:
    case 0x67:
    case 0x63:
    case 0x03:
    case 0x23:
        return true;
    case 0x73:
        switch (insn.funct3) {
        case 1:
        case 2:
        case 3:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

bool reads_rs2(const Insn& insn) {
    switch (insn.opcode) {
    case 0x33:
    case 0x3B:
    case 0x63:
    case 0x23:
        return true;
    default:
        return false;
    }
}

bool is_load_slot(const StageSlot& slot) {
    return slot.valid && slot.insn.opcode == 0x03;
}

uint8_t inflight_rd(const StageSlot& slot) {
    if (!slot.valid) {
        return 0;
    }
    if (!slot.effects.rd_write.enable) {
        switch (slot.insn.opcode) {
        case 0x37:
        case 0x17:
        case 0x13:
        case 0x1B:
        case 0x33:
        case 0x3B:
        case 0x6F:
        case 0x67:
        case 0x03:
            return slot.insn.rd;
        default:
            return 0;
        }
    }
    if (slot.effects.mem.kind == MemoryRequest::Kind::Load) {
        return slot.effects.mem.rd;
    }
    return slot.effects.rd_write.rd;
}

bool has_decode_hazard(const StageSlot& id_ex_slot, const Insn& insn) {
    const uint8_t rs1 = reads_rs1(insn) ? insn.rs1 : 0;
    const uint8_t rs2 = reads_rs2(insn) ? insn.rs2 : 0;
    if (!is_load_slot(id_ex_slot)) {
        return false;
    }

    const uint8_t id_ex_rd = inflight_rd(id_ex_slot);
    if (id_ex_rd == 0) {
        return false;
    }

    if (rs1 != 0 && rs1 == id_ex_rd) {
        return true;
    }
    if (rs2 != 0 && rs2 == id_ex_rd) {
        return true;
    }
    return false;
}

uint64_t resolve_register_value(const PipelineForwardingSources& sources,
                                uint8_t rs,
                                uint64_t latched_value) {
    uint64_t value = latched_value;

    if (sources.ex_mem != nullptr && forward_operand_from_slot(*sources.ex_mem, rs, value)) {
        return value;
    }
    if (sources.mem_wb != nullptr && forward_operand_from_slot(*sources.mem_wb, rs, value)) {
        return value;
    }
    return value;
}

uint64_t resolve_ex_operand(const PipelineForwardingSources& sources,
                            const Insn& insn,
                            bool use_rs1,
                            uint64_t latched_value) {
    const uint8_t rs = use_rs1 ? insn.rs1 : insn.rs2;
    return resolve_register_value(sources, rs, latched_value);
}

}  // namespace pipeline_hazards
