#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ai_graph_package.h"
#include "ai_scratchpad.h"
#include "ai_submission_queue.h"

bool ai_execute_elementwise_op(const AiGraphPackage& package,
                               const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                               const AiOpDescriptor& op,
                               AiScratchpad& scratchpad,
                               uint64_t& retired_ops,
                               uint32_t& fault_code,
                               std::string& error);
