#include "ai_graph_scheduler.h"

#include <deque>
#include <vector>

#include "ai_compute_conv.h"
#include "ai_compute_elementwise.h"
#include "ai_compute_gemm.h"

namespace {

uint64_t tile_count(const AiTensorMetadata& tensor) {
    uint64_t count = 1;
    for (uint8_t axis = 0; axis < tensor.rank; ++axis) {
        const uint32_t dim = tensor.dims[axis];
        const uint32_t tile = tensor.tile_dims[axis] == 0 ? dim : tensor.tile_dims[axis];
        count *= (static_cast<uint64_t>(dim) + tile - 1) / tile;
    }
    return count;
}

uint64_t op_compute_cycles(uint64_t retired_ops,
                           const AiGraphSchedulerTiming& timing) {
    return retired_ops == 0 ? 0
                            : (retired_ops + timing.ops_per_cycle - 1) /
                                  static_cast<uint64_t>(timing.ops_per_cycle == 0 ? 1
                                                                                   : timing.ops_per_cycle);
}

uint64_t op_stall_cycles(uint64_t output_tiles,
                         const AiGraphSchedulerTiming& timing) {
    return output_tiles * static_cast<uint64_t>(timing.tile_setup_cycles);
}

uint32_t scratchpad_peak_bytes(const AiGraphPackage& package) {
    uint64_t peak = 0;
    for (const AiMemoryPlanEntry& entry : package.memory_plan) {
        const uint64_t end = static_cast<uint64_t>(entry.scratchpad_offset) +
                             static_cast<uint64_t>(entry.scratchpad_bytes);
        if (end > peak) {
            peak = end;
        }
    }
    return static_cast<uint32_t>(peak);
}

}  // namespace

AiGraphScheduler::AiGraphScheduler(AiScratchpad& scratchpad, AiGraphSchedulerTiming timing)
    : scratchpad_(scratchpad),
      timing_{
          .ops_per_cycle = timing.ops_per_cycle == 0 ? 1U : timing.ops_per_cycle,
          .tile_setup_cycles = timing.tile_setup_cycles,
          .allow_dma_compute_overlap = timing.allow_dma_compute_overlap,
      } {}

const AiGraphSchedulerTiming& AiGraphScheduler::timing() const {
    return timing_;
}

bool AiGraphScheduler::execute(const AiGraphPackage& package,
                               AiGraphExecutionResult& result,
                               std::string& error) const {
    result = {};
    error.clear();
    result.scratchpad_peak_bytes = scratchpad_peak_bytes(package);

    std::vector<const AiMemoryPlanEntry*> memory_plan_by_tensor(package.tensors.size(), nullptr);
    for (const AiMemoryPlanEntry& entry : package.memory_plan) {
        if (entry.tensor_index >= memory_plan_by_tensor.size()) {
            result.fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            result.fault_detail = entry.tensor_index;
            error = "tensor memory plan index is out of range";
            return false;
        }
        if (memory_plan_by_tensor[entry.tensor_index] != nullptr) {
            result.fault = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
            result.fault_detail = entry.tensor_index;
            error = "duplicate tensor memory plan entry";
            return false;
        }
        memory_plan_by_tensor[entry.tensor_index] = &entry;
    }

    std::vector<uint32_t> indegree(package.ops.size(), 0);
    std::vector<std::vector<uint16_t>> successors(package.ops.size());
    for (const AiDependencyEdge& edge : package.dependencies) {
        ++indegree[edge.target_op];
        successors[edge.source_op].push_back(edge.target_op);
    }

    std::deque<uint16_t> ready{};
    for (uint16_t op_index = 0; op_index < package.ops.size(); ++op_index) {
        if (indegree[op_index] == 0) {
            ready.push_back(op_index);
        }
    }

    size_t executed_ops = 0;
    while (!ready.empty()) {
        const uint16_t op_index = ready.front();
        ready.pop_front();
        const AiOpDescriptor& op = package.ops[op_index];

        uint64_t retired_ops = 0;
        uint32_t fault_code = AI_ACCEL_FAULT_NONE;
        bool ok = false;
        switch (op.opcode) {
        case AiOpCode::Gemm:
            ok = ai_execute_gemm_op(package,
                                    memory_plan_by_tensor,
                                    op,
                                    scratchpad_,
                                    retired_ops,
                                    fault_code,
                                    error);
            break;
        case AiOpCode::Conv2d:
            ok = ai_execute_conv_op(package,
                                    memory_plan_by_tensor,
                                    op,
                                    scratchpad_,
                                    retired_ops,
                                    fault_code,
                                    error);
            break;
        case AiOpCode::EltwiseRelu:
        case AiOpCode::PoolMax:
        case AiOpCode::ReduceSum:
        case AiOpCode::LayoutTranspose:
        case AiOpCode::Softmax:
            ok = ai_execute_elementwise_op(package,
                                           memory_plan_by_tensor,
                                           op,
                                           scratchpad_,
                                           retired_ops,
                                           fault_code,
                                           error);
            break;
        case AiOpCode::Invalid:
            fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
            error = "invalid opcode in graph scheduler";
            ok = false;
            break;
        }
        if (!ok) {
            result.fault = fault_code == AI_ACCEL_FAULT_NONE ? AI_ACCEL_FAULT_EXECUTION : fault_code;
            result.fault_detail = op_index;
            return false;
        }

        const uint64_t output_tiles = tile_count(package.tensors[op.output]);
        const uint64_t compute_cycles = op_compute_cycles(retired_ops, timing_);
        const uint64_t stall_cycles = op_stall_cycles(output_tiles, timing_);
        result.retired_ops += retired_ops;
        result.compute_cycles += compute_cycles;
        result.stall_cycles += stall_cycles;
        result.tile_count += output_tiles;
        result.op_summaries.push_back(AiOpProfileSummary{
            .op_index = op_index,
            .opcode = op.opcode,
            .retired_ops = retired_ops,
            .compute_cycles = compute_cycles,
            .stall_cycles = stall_cycles,
            .tile_count = output_tiles,
        });
        ++executed_ops;

        for (uint16_t successor : successors[op_index]) {
            if (--indegree[successor] == 0) {
                ready.push_back(successor);
            }
        }
    }

    if (executed_ops != package.ops.size()) {
        result.fault = AI_ACCEL_FAULT_EXECUTION;
        result.fault_detail = static_cast<uint32_t>(executed_ops);
        error = "graph dependency cycle detected";
        return false;
    }

    return true;
}
