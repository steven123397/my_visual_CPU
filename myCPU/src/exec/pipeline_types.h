#pragma once

#include <cstdint>

#include "branch_predictor.h"
#include "pipeline_sequence.h"
#include "../isa/effects.h"

extern "C" {
#include "../decode.h"
}

struct StageSlot {
    bool valid{false};
    SequenceId sequence_id{};
    uint64_t pc{0};
    uint32_t raw{0};
    Insn insn{};
    PredictorQueryResult prediction{};
    uint64_t rs1v{0};
    uint64_t rs2v{0};
    InsnEffects effects{};
};

struct PipelineForwardingSources {
    const StageSlot* ex_mem{nullptr};
    const StageSlot* mem_wb{nullptr};
};

struct IfIdReg {
    StageSlot slot{};
};

struct IdExReg {
    StageSlot slot{};
};

struct ExMemReg {
    StageSlot slot{};
};

struct MemWbReg {
    StageSlot slot{};
};
