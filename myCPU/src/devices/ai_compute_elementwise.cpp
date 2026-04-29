#include "ai_compute_elementwise.h"

#include <algorithm>
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

template <typename T>
std::vector<T> relu_values(const std::vector<T>& input) {
    std::vector<T> output(input.size(), 0);
    std::transform(input.begin(), input.end(), output.begin(), [](T value) {
        return value < static_cast<T>(0) ? static_cast<T>(0) : value;
    });
    return output;
}

bool execute_relu(const AiGraphPackage& package,
                  const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                  const AiOpDescriptor& op,
                  AiScratchpad& scratchpad,
                  uint64_t& retired_ops,
                  uint32_t& fault_code,
                  std::string& error) {
    const AiTensorMetadata& input_tensor = package.tensors[op.input0];
    const AiTensorMetadata& output_tensor = package.tensors[op.output];
    if (input_tensor.rank != output_tensor.rank || input_tensor.dims != output_tensor.dims ||
        input_tensor.dtype != output_tensor.dtype || input_tensor.dtype != op.input_dtype) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "EltwiseRelu tensor shape or dtype is inconsistent";
        return false;
    }

    const AiMemoryPlanEntry* input_entry = required_entry(memory_plan_by_tensor, op.input0, error);
    if (input_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* output_entry = required_entry(memory_plan_by_tensor, op.output, error);
    if (output_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }

    switch (input_tensor.dtype) {
    case AiDataType::Int8: {
        std::vector<int8_t> input{};
        if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<int8_t> output = relu_values(input);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Int16: {
        std::vector<int16_t> input{};
        if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<int16_t> output = relu_values(input);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Int32: {
        std::vector<int32_t> input{};
        if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        const std::vector<int32_t> output = tensor_golden_relu_i32(input);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Fp32: {
        std::vector<float> input{};
        if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<float> output = relu_values(input);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Fp16:
    case AiDataType::Bf16:
    case AiDataType::Invalid:
        fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
        error = "EltwiseRelu dtype is unsupported";
        return false;
    }

    retired_ops = tensor_element_count(output_tensor);
    return true;
}

bool execute_pool_max(const AiGraphPackage& package,
                      const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                      const AiOpDescriptor& op,
                      AiScratchpad& scratchpad,
                      uint64_t& retired_ops,
                      uint32_t& fault_code,
                      std::string& error) {
    const AiTensorMetadata& input_tensor = package.tensors[op.input0];
    const AiTensorMetadata& output_tensor = package.tensors[op.output];
    if (input_tensor.rank != 2 || output_tensor.rank != 2 ||
        input_tensor.dtype != AiDataType::Fp32 || output_tensor.dtype != AiDataType::Fp32) {
        fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
        error = "PoolMax requires FP32 rank-2 tensors";
        return false;
    }
    const uint32_t window_h = static_cast<uint32_t>(op.attrs[0]);
    const uint32_t window_w = static_cast<uint32_t>(op.attrs[1]);
    const uint32_t stride_h = static_cast<uint32_t>(op.attrs[2]);
    const uint32_t stride_w = static_cast<uint32_t>(op.attrs[3]);
    if (window_h == 0 || window_w == 0 || stride_h == 0 || stride_w == 0 ||
        window_h > input_tensor.dims[0] || window_w > input_tensor.dims[1]) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "PoolMax attrs are invalid";
        return false;
    }

    const uint32_t expected_h = 1 + (input_tensor.dims[0] - window_h) / stride_h;
    const uint32_t expected_w = 1 + (input_tensor.dims[1] - window_w) / stride_w;
    if (output_tensor.dims[0] != expected_h || output_tensor.dims[1] != expected_w) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "PoolMax output shape is inconsistent";
        return false;
    }

    const AiMemoryPlanEntry* input_entry = required_entry(memory_plan_by_tensor, op.input0, error);
    if (input_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* output_entry = required_entry(memory_plan_by_tensor, op.output, error);
    if (output_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }

    std::vector<float> input{};
    if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }
    const std::vector<float> output = tensor_golden_max_pool_2d_f32(input,
                                                                    input_tensor.dims[0],
                                                                    input_tensor.dims[1],
                                                                    window_h,
                                                                    window_w,
                                                                    stride_h,
                                                                    stride_w);
    if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }

    retired_ops = tensor_element_count(output_tensor) * static_cast<uint64_t>(window_h) *
                  static_cast<uint64_t>(window_w);
    return true;
}

bool execute_reduce_sum(const AiGraphPackage& package,
                        const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                        const AiOpDescriptor& op,
                        AiScratchpad& scratchpad,
                        uint64_t& retired_ops,
                        uint32_t& fault_code,
                        std::string& error) {
    const AiTensorMetadata& input_tensor = package.tensors[op.input0];
    const AiTensorMetadata& output_tensor = package.tensors[op.output];
    if (input_tensor.rank != 2 || output_tensor.rank != 1 ||
        input_tensor.dtype != AiDataType::Int32 || output_tensor.dtype != AiDataType::Int32 ||
        output_tensor.dims[0] != input_tensor.dims[0]) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "ReduceSum tensor shape or dtype is inconsistent";
        return false;
    }

    const AiMemoryPlanEntry* input_entry = required_entry(memory_plan_by_tensor, op.input0, error);
    if (input_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* output_entry = required_entry(memory_plan_by_tensor, op.output, error);
    if (output_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }

    std::vector<int32_t> input{};
    if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }
    const std::vector<int32_t> output =
        tensor_golden_reduce_sum_rows_i32(input, input_tensor.dims[0], input_tensor.dims[1]);
    if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }

    retired_ops = static_cast<uint64_t>(input_tensor.dims[0]) * static_cast<uint64_t>(input_tensor.dims[1]);
    return true;
}

bool execute_layout_transpose(const AiGraphPackage& package,
                              const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                              const AiOpDescriptor& op,
                              AiScratchpad& scratchpad,
                              uint64_t& retired_ops,
                              uint32_t& fault_code,
                              std::string& error) {
    const AiTensorMetadata& input_tensor = package.tensors[op.input0];
    const AiTensorMetadata& output_tensor = package.tensors[op.output];
    if (input_tensor.rank != 2 || output_tensor.rank != 2 ||
        input_tensor.dtype != AiDataType::Int32 || output_tensor.dtype != AiDataType::Int32 ||
        output_tensor.dims[0] != input_tensor.dims[1] || output_tensor.dims[1] != input_tensor.dims[0]) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "LayoutTranspose tensor shape or dtype is inconsistent";
        return false;
    }

    const AiMemoryPlanEntry* input_entry = required_entry(memory_plan_by_tensor, op.input0, error);
    if (input_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* output_entry = required_entry(memory_plan_by_tensor, op.output, error);
    if (output_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }

    std::vector<int32_t> input{};
    if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }
    const std::vector<int32_t> output =
        tensor_golden_transpose_2d_i32(input, input_tensor.dims[0], input_tensor.dims[1]);
    if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }

    retired_ops = static_cast<uint64_t>(input_tensor.dims[0]) * static_cast<uint64_t>(input_tensor.dims[1]);
    return true;
}

bool execute_softmax(const AiGraphPackage& package,
                     const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                     const AiOpDescriptor& op,
                     AiScratchpad& scratchpad,
                     uint64_t& retired_ops,
                     uint32_t& fault_code,
                     std::string& error) {
    const AiTensorMetadata& input_tensor = package.tensors[op.input0];
    const AiTensorMetadata& output_tensor = package.tensors[op.output];
    if (input_tensor.rank != 2 || output_tensor.rank != 2 ||
        input_tensor.dims != output_tensor.dims ||
        input_tensor.dtype != AiDataType::Fp32 || output_tensor.dtype != AiDataType::Fp32 ||
        op.input_dtype != AiDataType::Fp32 || op.accum_dtype != AiDataType::Fp32) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "Softmax tensor shape or dtype is inconsistent";
        return false;
    }

    const AiMemoryPlanEntry* input_entry = required_entry(memory_plan_by_tensor, op.input0, error);
    if (input_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* output_entry = required_entry(memory_plan_by_tensor, op.output, error);
    if (output_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }

    std::vector<float> input{};
    if (!read_tensor_values(scratchpad, *input_entry, input, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }
    const std::vector<float> output =
        tensor_golden_softmax_rows_f32(input, input_tensor.dims[0], input_tensor.dims[1]);
    if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
        fault_code = AI_ACCEL_FAULT_EXECUTION;
        return false;
    }

    retired_ops = tensor_element_count(output_tensor);
    return true;
}

}  // namespace

bool ai_execute_elementwise_op(const AiGraphPackage& package,
                               const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                               const AiOpDescriptor& op,
                               AiScratchpad& scratchpad,
                               uint64_t& retired_ops,
                               uint32_t& fault_code,
                               std::string& error) {
    retired_ops = 0;
    fault_code = AI_ACCEL_FAULT_NONE;
    error.clear();

    switch (op.opcode) {
    case AiOpCode::EltwiseRelu:
        return execute_relu(package,
                            memory_plan_by_tensor,
                            op,
                            scratchpad,
                            retired_ops,
                            fault_code,
                            error);
    case AiOpCode::PoolMax:
        return execute_pool_max(package,
                                memory_plan_by_tensor,
                                op,
                                scratchpad,
                                retired_ops,
                                fault_code,
                                error);
    case AiOpCode::ReduceSum:
        return execute_reduce_sum(package,
                                  memory_plan_by_tensor,
                                  op,
                                  scratchpad,
                                  retired_ops,
                                  fault_code,
                                  error);
    case AiOpCode::LayoutTranspose:
        return execute_layout_transpose(package,
                                        memory_plan_by_tensor,
                                        op,
                                        scratchpad,
                                        retired_ops,
                                        fault_code,
                                        error);
    case AiOpCode::Softmax:
        return execute_softmax(package,
                               memory_plan_by_tensor,
                               op,
                               scratchpad,
                               retired_ops,
                               fault_code,
                               error);
    case AiOpCode::Invalid:
    case AiOpCode::Gemm:
    case AiOpCode::Conv2d:
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "elementwise engine cannot execute this opcode";
        return false;
    }
    fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
    error = "unknown opcode";
    return false;
}
