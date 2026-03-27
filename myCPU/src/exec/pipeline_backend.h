#pragma once

#include "branch_predictor.h"
#include "backend.h"
#include "pipeline_types.h"

class CPU;
class Bus;

class PipelineBackend : public ExecutionBackend {
public:
    PipelineBackend(CPU& cpu, Bus& bus);

    void step() override;
    const char* name() const override;
    BackendDebugSnapshot debug_snapshot() const override;

private:
    DebugStageSnapshot build_fetch_stage_snapshot() const;
    DebugStageSnapshot build_stage_snapshot(const StageSlot& slot) const;
    bool pipeline_empty() const;
    bool reads_rs1(const Insn& insn) const;
    bool reads_rs2(const Insn& insn) const;
    uint8_t inflight_rd(const StageSlot& slot) const;
    bool has_decode_hazard(const Insn& insn) const;
    bool is_load_slot(const StageSlot& slot) const;
    bool forward_operand_from_slot(const StageSlot& slot, uint8_t rs, uint64_t& value) const;
    uint64_t resolve_ex_operand(const Insn& insn, bool use_rs1, uint64_t latched_value) const;
    uint64_t resolve_ex_counter_value(uint32_t addr) const;
    uint64_t resolve_ex_csr_value(const Insn& insn) const;
    void reset_stage_state();
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
    uint64_t fetch_pc_{0};
    IfIdReg if_id_{};
    IdExReg id_ex_{};
    ExMemReg ex_mem_{};
    MemWbReg mem_wb_{};
    IfIdReg next_if_id_{};
    IdExReg next_id_ex_{};
    ExMemReg next_ex_mem_{};
    MemWbReg next_mem_wb_{};
    bool last_cycle_stalled_{false};
    bool last_cycle_trap_flush_{false};
    bool last_cycle_committed_{false};
    bool interrupt_serviceable_at_cycle_start_{false};
    bool redirect_pending_{false};
    uint64_t redirect_target_{0};
    TrapRequest pending_fetch_fault_{};
    uint64_t pending_fetch_fault_pc_{0};
    BranchPredictor predictor_{};
};
