#pragma once

#include <cstdint>

#include "pipeline_types.h"

namespace pipeline_hazards {

bool reads_rs1(const Insn& insn);
bool reads_rs2(const Insn& insn);
bool writes_rd(const Insn& insn);
bool is_load_slot(const StageSlot& slot);
bool is_store_slot(const StageSlot& slot);
uint8_t inflight_rd(const StageSlot& slot);
uint32_t inflight_dest_phys(const StageSlot& slot);
bool has_decode_hazard(const StageSlot& id_ex_slot, const Insn& insn);
bool has_decode_hazard(const StageSlot& id_ex_slot, uint32_t rs1_phys, uint32_t rs2_phys);
bool has_load_store_order_hazard(const StageSlot& id_ex_slot,
                                 const StageSlot& ex_mem_slot,
                                 const StageSlot& mem_wb_slot,
                                 bool mem_wb_committed,
                                 const Insn& insn);
uint64_t resolve_register_value(const PipelineForwardingSources& sources,
                                uint8_t rs,
                                uint64_t latched_value);
uint64_t resolve_register_value(const PipelineForwardingSources& sources,
                                uint32_t phys,
                                uint64_t latched_value);
uint64_t resolve_ex_operand(const PipelineForwardingSources& sources,
                            const Insn& insn,
                            bool use_rs1,
                            uint64_t latched_value);
uint64_t resolve_ex_operand(const PipelineForwardingSources& sources,
                            uint32_t phys,
                            uint64_t latched_value);

}  // namespace pipeline_hazards
