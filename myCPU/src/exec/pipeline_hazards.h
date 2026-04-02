#pragma once

#include <cstdint>

#include "pipeline_types.h"

namespace pipeline_hazards {

bool reads_rs1(const Insn& insn);
bool reads_rs2(const Insn& insn);
bool is_load_slot(const StageSlot& slot);
uint8_t inflight_rd(const StageSlot& slot);
bool has_decode_hazard(const StageSlot& id_ex_slot, const Insn& insn);
uint64_t resolve_register_value(const PipelineForwardingSources& sources,
                                uint8_t rs,
                                uint64_t latched_value);
uint64_t resolve_ex_operand(const PipelineForwardingSources& sources,
                            const Insn& insn,
                            bool use_rs1,
                            uint64_t latched_value);

}  // namespace pipeline_hazards
