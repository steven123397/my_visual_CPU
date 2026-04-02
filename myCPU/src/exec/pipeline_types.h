#pragma once

#include <cstdint>

#include "branch_predictor.h"
#include "load_store_queue.h"
#include "pipeline_sequence.h"
#include "reorder_buffer.h"
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
    uint32_t rs1_phys{0};
    uint32_t rs2_phys{0};
    uint32_t ecall_a7_phys{0};
    uint32_t rd_phys{0};
    uint32_t previous_rd_phys{0};
    RobIndex rob_index{};
    LsqIndex lsq_index{};
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
