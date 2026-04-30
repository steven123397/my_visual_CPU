#include "dbt_ir.h"

const char* dbt_ir_opcode_name(DbtIrOpcode opcode) {
    switch (opcode) {
    case DbtIrOpcode::WriteRegImm:
        return "write-reg-imm";
    case DbtIrOpcode::AddRegImm:
        return "add-reg-imm";
    case DbtIrOpcode::AddRegReg:
        return "add-reg-reg";
    case DbtIrOpcode::SubRegReg:
        return "sub-reg-reg";
    case DbtIrOpcode::Fallthrough:
        return "fallthrough";
    }
    return "unknown";
}
