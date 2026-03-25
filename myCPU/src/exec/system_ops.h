#pragma once

struct InsnEffects;
struct SemanticInputs;
class ExecutionContext;

extern "C" {
#include "../decode.h"
}

InsnEffects build_system_effects(const Insn& insn, ExecutionContext& ctx, const SemanticInputs& inputs);
