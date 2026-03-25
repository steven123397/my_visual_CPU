#pragma once

#include "effects.h"

extern "C" {
#include "../decode.h"
}

class ExecutionContext;

struct SemanticInputs {
    uint64_t pc{0};
    uint64_t rs1v{0};
    uint64_t rs2v{0};
    bool has_csrv{false};
    uint64_t csrv{0};
    bool has_ecall_a7{false};
    uint64_t ecall_a7{0};
};

class InstructionSemantics {
public:
    static bool supports(const Insn& insn);
    static InsnEffects execute(const Insn& insn, ExecutionContext& ctx);
    static InsnEffects execute(const Insn& insn, ExecutionContext& ctx, const SemanticInputs& inputs);
};
