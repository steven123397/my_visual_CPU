#include "ai_compute_conv.h"

#include <vector>

#include "tensor_golden_ops.h"

namespace {

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

bool write_tensor_values(AiScratchpad& scratchpad,
                         const AiMemoryPlanEntry& entry,
                         const std::vector<int32_t>& values,
                         std::string& error) {
    if (values.size() * sizeof(int32_t) != entry.byte_size) {
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

bool ai_execute_conv_op(const AiGraphPackage& package,
                        const std::vector<const AiMemoryPlanEntry*>& memory_plan_by_tensor,
                        const AiOpDescriptor& op,
                        AiScratchpad& scratchpad,
                        uint64_t& retired_ops,
                        uint32_t& fault_code,
                        std::string& error) {
    retired_ops = 0;
    fault_code = AI_ACCEL_FAULT_NONE;
    error.clear();

    const AiTensorMetadata& input_tensor = package.tensors[op.input0];
    const AiTensorMetadata& kernel_tensor = package.tensors[op.input1];
    const AiTensorMetadata& output_tensor = package.tensors[op.output];
    if (input_tensor.rank != 2 || kernel_tensor.rank != 2 || output_tensor.rank != 2) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "Conv2d requires rank-2 tensors";
        return false;
    }

    const uint32_t input_h = input_tensor.dims[0];
    const uint32_t input_w = input_tensor.dims[1];
    const uint32_t kernel_h = kernel_tensor.dims[0];
    const uint32_t kernel_w = kernel_tensor.dims[1];
    if (kernel_h == 0 || kernel_w == 0 || kernel_h > input_h || kernel_w > input_w) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "Conv2d kernel shape is invalid";
        return false;
    }
    const uint32_t output_h = input_h - kernel_h + 1;
    const uint32_t output_w = input_w - kernel_w + 1;
    if (output_tensor.dims[0] != output_h || output_tensor.dims[1] != output_w) {
        fault_code = AI_ACCEL_FAULT_ILLEGAL_OP;
        error = "Conv2d output shape is inconsistent";
        return false;
    }
    if (input_tensor.dtype != op.input_dtype || kernel_tensor.dtype != op.input_dtype ||
        output_tensor.dtype != AiDataType::Int32) {
        fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
        error = "Conv2d tensor dtype does not match opcode";
        return false;
    }

    const AiMemoryPlanEntry* input_entry = required_entry(memory_plan_by_tensor, op.input0, error);
    if (input_entry == nullptr) {
        fault_code = AI_ACCEL_FAULT_INVALID_DESCRIPTOR;
        return false;
    }
    const AiMemoryPlanEntry* kernel_entry = required_entry(memory_plan_by_tensor, op.input1, error);
    if (kernel_entry == nullptr) {
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
        std::vector<int8_t> input{};
        std::vector<int8_t> kernel{};
        if (!read_tensor_values(scratchpad, *input_entry, input, error) ||
            !read_tensor_values(scratchpad, *kernel_entry, kernel, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<int32_t> output =
            tensor_golden_conv2d_valid_i8_to_i32(input, input_h, input_w, kernel, kernel_h, kernel_w);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Int16: {
        std::vector<int16_t> input{};
        std::vector<int16_t> kernel{};
        if (!read_tensor_values(scratchpad, *input_entry, input, error) ||
            !read_tensor_values(scratchpad, *kernel_entry, kernel, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        std::vector<int32_t> output =
            tensor_golden_conv2d_valid_i16_to_i32(input, input_h, input_w, kernel, kernel_h, kernel_w);
        if (!write_tensor_values(scratchpad, *output_entry, output, error)) {
            fault_code = AI_ACCEL_FAULT_EXECUTION;
            return false;
        }
        break;
    }
    case AiDataType::Int32:
    case AiDataType::Fp16:
    case AiDataType::Bf16:
    case AiDataType::Fp32:
    case AiDataType::Invalid:
        fault_code = AI_ACCEL_FAULT_UNSUPPORTED_DTYPE;
        error = "Conv2d dtype is unsupported";
        return false;
    }

    retired_ops = static_cast<uint64_t>(output_h) * static_cast<uint64_t>(output_w) *
                  static_cast<uint64_t>(kernel_h) * static_cast<uint64_t>(kernel_w);
    return true;
}
