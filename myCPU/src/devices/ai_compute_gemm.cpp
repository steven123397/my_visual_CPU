#include "ai_compute_gemm.h"

#include <vector>

#include "tensor_golden_ops.h"

namespace {

uint64_t tensor_element_count(const AiTensorMetadata& tensor) {
    uint64_t count = 1;
    for (uint8_t i = 0; i < tensor.rank; ++i) {
        count *= tensor.dims[i];
    }
    return count;
}

const AiMemoryPlanEntry* required_entry(const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                                        uint16_t tensor_index,
                                        std::string& error) {
    if (tensor_index >= memory_plan_by_tensor.size() || memory_plan_by_tensor[tensor_index] == nullptr) {
        error = "tensor memory plan is missing";
        return nullptr;
    }
    const AiMemoryPlanEntry* entry = memory_plan_by_tensor[tensor_index];
    if (entry->scratchpad_bytes < entry->byte_size) {
        error = "partial tensor scratchpad residency is not supported";
        return nullptr;
    }
    return entry;
}

template <typename T>
bool read_tensor_values(const AiScratchpad& scratchpad,
                        const AiMemoryPlanEntry& entry,
                        std::vector<T>& values,
                        std::string& error) {
    if (entry.byte_size % sizeof(T) != 0) {
        error = "tensor byte size is misaligned";
        return false;
    }
    values.resize(entry.byte_size / sizeof(T));
    if (!scratchpad.read(AiScratchpadSpace::Scratchpad,
                         entry.scratchpad_offset,
                         values.data(),
                         entry.byte_size)) {
        error = "could not read tensor bytes from scratchpad";
        return false;
    }
    return true;
}

template <typename T>
bool write_tensor_values(AiScratchpad& scratchpad,
                         const AiMemoryPlanEntry& entry,
                         const std::vector<T>& values,
                         std::string& error) {
    if (values.size() * sizeof(T) != entry.byte_size) {
        error = "tensor byte size does not match compute output";
        return false;
    }
    if (!scratchpad.write(AiScratchpadSpace::Scratchpad,
                          entry.scratchpad_offset,
                          values.data(),
                          entry.byte_size)) {
        error = "could not write tensor bytes into scratchpad";
        return false;
    }
    return true;
}

}  // namespace

bool ai_execute_gemm_op(const AiGraphPackage& package,
                        const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                        const AiOpDescriptor& op,
                        AiScratchpad& scratchpad,
                        uint64_t& retired_ops,
                        uint32_t& fault_code,
                        std::string& error) {
    retired_ops = 0;
    fault_code = AI_ACCEL_FAULT_NONE;
    error.clear();

    const AiTensorMetadata& lhs_tensor = package.tensors[op.input0];
    const AiTensorMetadata& rhs_tensor = package.tensors[op.input1];
    const AiTensorMetadata& output_tensor = package.tensors[op.output];
    if (lhs_tensor.rank != 2 || rhs_tensor.rank != 2 || output_tensor.rank != 2) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "GEMM requires rank-2 tensors";
        return false;
    }

    const uint32_t m = lhs_tensor.dims[0];
    const uint32_t k = lhs_tensor.dims[1];
    const uint32_t rhs_k = rhs_tensor.dims[0];
    const uint32_t n = rhs_tensor.dims[1];
    if (k == 0 || rhs_k == 0 || m == 0 || n == 0 || k != rhs_k ||
        output_tensor.dims[0] != m || output_tensor.dims[1] != n) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "GEMM tensor shapes are inconsistent";
        return false;
    }
    if (lhs_tensor.dtype != op.input_dtype || rhs_tensor.dtype != op.input_dtype) {
        fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
        error = "GEMM input tensor dtype does not match opcode";
        return false;
    }

    const AiMemoryPlanEntry* lhs_entry = required_entry(memory_plan_by_tensor, op.input0, error);
    if (lhs_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* rhs_entry = required_entry(memory_plan_by_tensor, op.input1, error);
    if (rhs_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* output_entry = required_entry(memory_plan_by_tensor, op.output, error);
    if (output_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }

    switch (op.input_dtype) {
    case AiDataType::Int8: {
        if (output_tensor.dtype != AiDataType::Int32) {
            fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
            error = "INT8 GEMM requires INT32 output";
            return false;
        }
        std::vector<int8_t> lhs{};
        std::vector<int8_t> rhs{};
        if (!read_tensor_values(scratchpad, *lhs_entry, lhs, error) ||
            !read_tensor_values(scratchpad, *rhs_entry, rhs, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<int32_t> output = tensor_golden_gemm_i8_to_i32(lhs, rhs, m, k, n);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Int16: {
        if (output_tensor.dtype != AiDataType::Int32) {
            fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
            error = "INT16 GEMM requires INT32 output";
            return false;
        }
        std::vector<int16_t> lhs{};
        std::vector<int16_t> rhs{};
        if (!read_tensor_values(scratchpad, *lhs_entry, lhs, error) ||
            !read_tensor_values(scratchpad, *rhs_entry, rhs, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<int32_t> output = tensor_golden_gemm_i16_to_i32(lhs, rhs, m, k, n);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Fp16: {
        if (output_tensor.dtype != AiDataType::Fp32) {
            fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
            error = "FP16 GEMM requires FP32 output";
            return false;
        }
        std::vector<uint16_t> lhs{};
        std::vector<uint16_t> rhs{};
        if (!read_tensor_values(scratchpad, *lhs_entry, lhs, error) ||
            !read_tensor_values(scratchpad, *rhs_entry, rhs, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<float> output = tensor_golden_gemm_fp16_to_fp32(lhs, rhs, m, k, n);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Bf16: {
        if (output_tensor.dtype != AiDataType::Fp32) {
            fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
            error = "BF16 GEMM requires FP32 output";
            return false;
        }
        std::vector<uint16_t> lhs{};
        std::vector<uint16_t> rhs{};
        if (!read_tensor_values(scratchpad, *lhs_entry, lhs, error) ||
            !read_tensor_values(scratchpad, *rhs_entry, rhs, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<float> output = tensor_golden_gemm_bf16_to_fp32(lhs, rhs, m, k, n);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Int32:
    case AiDataType::Fp32:
    case AiDataType::Invalid:
        fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
        error = "GEMM dtype is unsupported";
        return false;
    }

    retired_ops = static_cast<uint64_t>(m) * static_cast<uint64_t>(k) * static_cast<uint64_t>(n);
    if (tensor_element_count(output_tensor) != static_cast<uint64_t>(m) * static_cast<uint64_t>(n)) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "GEMM output element count is inconsistent";
        return false;
    }
    return true;
}
