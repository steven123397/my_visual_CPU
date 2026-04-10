#pragma once

#include "branch_predictor.h"
#include "backend.h"
#include "pipeline_core_state.h"
#include "pipeline_types.h"

class CPU;
class Bus;

class PipelineBackend : public ExecutionBackend {
public:
    PipelineBackend(CPU& cpu, Bus& bus);

    void step() override;
    const char* name() const override;
    BackendDebugSnapshot debug_snapshot() const override;
    PipelineCoreState& testing_state();
    const PipelineCoreState& testing_state() const;

private:
    bool sources_ready(const StageSlot& slot) const;
    bool is_serializing_system_slot(const StageSlot& slot) const;
    OlderVectorDependency older_vector_dependency(const StageSlot& slot) const;
    bool vector_state_busy(const StageSlot& slot) const;
    void publish_completed_slot(const StageSlot& slot);
    DebugStageSnapshot build_fetch_stage_snapshot() const;
    DebugStageSnapshot build_stage_snapshot(const StageSlot& slot) const;
    uint64_t resolve_ex_counter_value(uint32_t addr) const;
    uint64_t resolve_ex_csr_value(const Insn& insn) const;
    bool try_replay_flush();
    bool step_wb();
    bool try_commit_fetch_fault();
    void step_mem();
    void step_ex();
    void step_id();
    void step_if();
    bool try_service_interrupt_at_commit_boundary();
    void commit_next_state();

    CPU& cpu_;
    Bus& bus_;
    PipelineCoreState state_{};
    BranchPredictor predictor_{};
    bool last_prediction_valid_{false};
    bool last_prediction_taken_{false};
    bool last_prediction_correct_{false};
    uint64_t last_prediction_pc_{0};
    uint64_t last_prediction_target_{0};
    bool last_mispredict_valid_{false};
    uint64_t last_mispredict_pc_{0};
    uint64_t last_mispredict_target_{0};
};
