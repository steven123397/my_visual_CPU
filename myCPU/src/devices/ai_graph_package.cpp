#include "ai_graph_package.h"

#include <limits>

namespace {

constexpr uint16_t kAiGraphPackageBaseHeaderBytes = 40;
constexpr uint16_t kAiGraphPackageExtendedHeaderBytes = 56;
constexpr size_t kTensorRecordBytes = 36;
constexpr size_t kOpRecordBytes = 28;
constexpr size_t kDependencyRecordBytes = 4;
constexpr size_t kMemoryPlanRecordBytes = 20;
constexpr size_t kDynamicTensorRecordBytes = 8;

struct ParsedHeader {
    uint32_t magic{0};
    uint16_t version{0};
    uint16_t header_bytes{0};
    AiShapeMode shape_mode{AiShapeMode::Static};
    AiTrainingMode training_mode{AiTrainingMode::Inference};
    uint32_t scratchpad_budget_bytes{0};
    uint16_t tensor_count{0};
    uint16_t op_count{0};
    uint16_t dependency_count{0};
    uint16_t memory_plan_count{0};
    uint16_t dynamic_tensor_count{0};
    uint32_t tensors_offset{0};
    uint32_t ops_offset{0};
    uint32_t dependencies_offset{0};
    uint32_t memory_plan_offset{0};
    uint32_t dynamic_tensors_offset{0};
    uint32_t package_bytes{0};
};

template <typename T>
bool fits_u16_count(const std::vector<T>& values) {
    return values.size() <= std::numeric_limits<uint16_t>::max();
}

bool requires_extended_header(const AiGraphPackage& package) {
    return package.shape_mode != AiShapeMode::Static ||
           package.training_mode != AiTrainingMode::Inference ||
           !package.dynamic_tensors.empty();
}

void append_u8(std::vector<uint8_t>& bytes, uint8_t value) {
    bytes.push_back(value);
}

void append_u16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>(value & 0xFFU));
    bytes.push_back(static_cast<uint8_t>((value >> 8) & 0xFFU));
}

void append_u32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFFU));
    }
}

void append_i32(std::vector<uint8_t>& bytes, int32_t value) {
    append_u32(bytes, static_cast<uint32_t>(value));
}

bool read_u8(const std::vector<uint8_t>& bytes, size_t& pos, uint8_t& value) {
    if (pos + 1 > bytes.size()) {
        return false;
    }
    value = bytes[pos++];
    return true;
}

bool read_u16(const std::vector<uint8_t>& bytes, size_t& pos, uint16_t& value) {
    if (pos + 2 > bytes.size()) {
        return false;
    }
    value = static_cast<uint16_t>(bytes[pos]) |
            (static_cast<uint16_t>(bytes[pos + 1]) << 8);
    pos += 2;
    return true;
}

bool read_u32(const std::vector<uint8_t>& bytes, size_t& pos, uint32_t& value) {
    if (pos + 4 > bytes.size()) {
        return false;
    }
    value = static_cast<uint32_t>(bytes[pos]) |
            (static_cast<uint32_t>(bytes[pos + 1]) << 8) |
            (static_cast<uint32_t>(bytes[pos + 2]) << 16) |
            (static_cast<uint32_t>(bytes[pos + 3]) << 24);
    pos += 4;
    return true;
}

bool read_i32(const std::vector<uint8_t>& bytes, size_t& pos, int32_t& value) {
    uint32_t bits = 0;
    if (!read_u32(bytes, pos, bits)) {
        return false;
    }
    value = static_cast<int32_t>(bits);
    return true;
}

bool read_header(const std::vector<uint8_t>& bytes, ParsedHeader& header, std::string& error) {
    error.clear();
    if (bytes.size() < kAiGraphPackageBaseHeaderBytes) {
        error = "graph package header truncated";
        return false;
    }

    size_t pos = 0;
    if (!read_u32(bytes, pos, header.magic) ||
        !read_u16(bytes, pos, header.version) ||
        !read_u16(bytes, pos, header.header_bytes) ||
        !read_u32(bytes, pos, header.scratchpad_budget_bytes) ||
        !read_u16(bytes, pos, header.tensor_count) ||
        !read_u16(bytes, pos, header.op_count) ||
        !read_u16(bytes, pos, header.dependency_count) ||
        !read_u16(bytes, pos, header.memory_plan_count) ||
        !read_u32(bytes, pos, header.tensors_offset) ||
        !read_u32(bytes, pos, header.ops_offset) ||
        !read_u32(bytes, pos, header.dependencies_offset) ||
        !read_u32(bytes, pos, header.memory_plan_offset) ||
        !read_u32(bytes, pos, header.package_bytes)) {
        error = "graph package header decode failed";
        return false;
    }

    if (header.header_bytes == kAiGraphPackageExtendedHeaderBytes) {
        uint8_t shape_mode = 0;
        uint8_t training_mode = 0;
        uint32_t reserved0 = 0;
        uint32_t reserved1 = 0;
        if (bytes.size() < kAiGraphPackageExtendedHeaderBytes ||
            !read_u8(bytes, pos, shape_mode) ||
            !read_u8(bytes, pos, training_mode) ||
            !read_u16(bytes, pos, header.dynamic_tensor_count) ||
            !read_u32(bytes, pos, header.dynamic_tensors_offset) ||
            !read_u32(bytes, pos, reserved0) ||
            !read_u32(bytes, pos, reserved1)) {
            error = "graph package extended header decode failed";
            return false;
        }
        header.shape_mode = static_cast<AiShapeMode>(shape_mode);
        header.training_mode = static_cast<AiTrainingMode>(training_mode);
        if (reserved0 != 0 || reserved1 != 0) {
            error = "graph package extended header reserved fields must be zero";
            return false;
        }
    }
    return true;
}

bool validate_table_range(uint32_t offset,
                          size_t count,
                          size_t record_bytes,
                          size_t package_bytes,
                          const char* label,
                          std::string& error) {
    if (offset > package_bytes) {
        error = std::string(label) + " offset is out of range";
        return false;
    }
    const uint64_t size = static_cast<uint64_t>(count) * static_cast<uint64_t>(record_bytes);
    if (size > std::numeric_limits<uint32_t>::max()) {
        error = std::string(label) + " table is too large";
        return false;
    }
    if (static_cast<uint64_t>(offset) + size > package_bytes) {
        error = std::string(label) + " table exceeds package bytes";
        return false;
    }
    return true;
}

uint64_t tensor_element_count(const AiTensorMetadata& tensor) {
    uint64_t count = 1;
    for (uint8_t i = 0; i < tensor.rank; ++i) {
        count *= tensor.dims[i];
    }
    return count;
}

bool requires_binary_inputs(AiOpCode opcode) {
    return opcode == AiOpCode::Gemm || opcode == AiOpCode::Conv2d;
}

bool requires_unary_input(AiOpCode opcode) {
    return opcode == AiOpCode::EltwiseRelu || opcode == AiOpCode::PoolMax ||
           opcode == AiOpCode::ReduceSum || opcode == AiOpCode::LayoutTranspose;
}

bool compatible_accumulator_dtype(AiDataType input_dtype, AiDataType accum_dtype) {
    switch (input_dtype) {
    case AiDataType::Int8:
    case AiDataType::Int16:
        return accum_dtype == AiDataType::Int32;
    case AiDataType::Fp16:
    case AiDataType::Bf16:
        return accum_dtype == AiDataType::Fp32;
    case AiDataType::Int32:
    case AiDataType::Fp32:
        return accum_dtype == input_dtype;
    case AiDataType::Invalid:
        break;
    }
    return false;
}

uint64_t tensor_byte_size(const AiTensorMetadata& tensor) {
    return tensor_element_count(tensor) * ai_dtype_size_bytes(tensor.dtype);
}

uint64_t runtime_shape_element_count(const AiRuntimeShapeEntry& runtime_shape) {
    uint64_t count = 1;
    for (uint8_t axis = 0; axis < runtime_shape.rank; ++axis) {
        count *= runtime_shape.dims[axis];
    }
    return count;
}

const AiMemoryPlanEntry* find_memory_plan_entry(
    const AiGraphPackage& package,
    uint16_t tensor_index) {
    for (const AiMemoryPlanEntry& entry : package.memory_plan) {
        if (entry.tensor_index == tensor_index) {
            return &entry;
        }
    }
    return nullptr;
}

AiMemoryPlanEntry* find_memory_plan_entry(
    AiGraphPackage& package,
    uint16_t tensor_index) {
    for (AiMemoryPlanEntry& entry : package.memory_plan) {
        if (entry.tensor_index == tensor_index) {
            return &entry;
        }
    }
    return nullptr;
}

const AiDynamicTensorMetadata* find_dynamic_tensor_metadata(
    const AiGraphPackage& package,
    uint16_t tensor_index) {
    for (const AiDynamicTensorMetadata& metadata : package.dynamic_tensors) {
        if (metadata.tensor_index == tensor_index) {
            return &metadata;
        }
    }
    return nullptr;
}

uint64_t runtime_tensor_byte_size(const AiRuntimeShapeEntry& runtime_shape, AiDataType dtype) {
    return runtime_shape_element_count(runtime_shape) * ai_dtype_size_bytes(dtype);
}

}  // namespace

bool ai_dtype_supported(AiDataType dtype) {
    return dtype == AiDataType::Int8 || dtype == AiDataType::Int16 || dtype == AiDataType::Int32 ||
           dtype == AiDataType::Fp16 || dtype == AiDataType::Bf16 || dtype == AiDataType::Fp32;
}

bool ai_tensor_role_supported(AiTensorRole role) {
    return role == AiTensorRole::Input || role == AiTensorRole::Output ||
           role == AiTensorRole::Weight || role == AiTensorRole::Intermediate ||
           role == AiTensorRole::Constant;
}

bool ai_opcode_supported(AiOpCode opcode) {
    return opcode == AiOpCode::Gemm || opcode == AiOpCode::Conv2d ||
           opcode == AiOpCode::EltwiseRelu || opcode == AiOpCode::PoolMax ||
           opcode == AiOpCode::ReduceSum || opcode == AiOpCode::LayoutTranspose;
}

bool ai_shape_mode_supported(AiShapeMode shape_mode) {
    return shape_mode == AiShapeMode::Static || shape_mode == AiShapeMode::DynamicBounded;
}

size_t ai_dtype_size_bytes(AiDataType dtype) {
    switch (dtype) {
    case AiDataType::Int8:
        return 1;
    case AiDataType::Int16:
    case AiDataType::Fp16:
    case AiDataType::Bf16:
        return 2;
    case AiDataType::Int32:
    case AiDataType::Fp32:
        return 4;
    case AiDataType::Invalid:
        break;
    }
    return 0;
}

const char* ai_dtype_name(AiDataType dtype) {
    switch (dtype) {
    case AiDataType::Int8:
        return "int8";
    case AiDataType::Int16:
        return "int16";
    case AiDataType::Int32:
        return "int32";
    case AiDataType::Fp16:
        return "fp16";
    case AiDataType::Bf16:
        return "bf16";
    case AiDataType::Fp32:
        return "fp32";
    case AiDataType::Invalid:
        return "invalid";
    }
    return "unknown";
}

const char* ai_opcode_name(AiOpCode opcode) {
    switch (opcode) {
    case AiOpCode::Gemm:
        return "gemm";
    case AiOpCode::Conv2d:
        return "conv2d";
    case AiOpCode::EltwiseRelu:
        return "eltwise_relu";
    case AiOpCode::PoolMax:
        return "pool_max";
    case AiOpCode::ReduceSum:
        return "reduce_sum";
    case AiOpCode::LayoutTranspose:
        return "layout_transpose";
    case AiOpCode::Invalid:
        return "invalid";
    }
    return "unknown";
}

bool validate_ai_graph_package(const AiGraphPackage& package, std::string& error) {
    error.clear();
    if (!ai_shape_mode_supported(package.shape_mode)) {
        error = "shape mode is unsupported";
        return false;
    }
    if (package.training_mode != AiTrainingMode::Inference) {
        error = "training mode is reserved";
        return false;
    }
    if (package.scratchpad_budget_bytes == 0) {
        error = "scratchpad budget must be non-zero";
        return false;
    }
    if (package.tensors.empty()) {
        error = "graph package requires at least one tensor";
        return false;
    }
    if (package.ops.empty()) {
        error = "graph package requires at least one opcode entry";
        return false;
    }
    if (package.memory_plan.empty()) {
        error = "graph package requires at least one memory plan entry";
        return false;
    }

    for (size_t i = 0; i < package.tensors.size(); ++i) {
        const AiTensorMetadata& tensor = package.tensors[i];
        if (!ai_dtype_supported(tensor.dtype)) {
            error = "tensor dtype is unsupported";
            return false;
        }
        if (!ai_tensor_role_supported(tensor.role)) {
            error = "tensor role is unsupported";
            return false;
        }
        if (tensor.rank == 0 || tensor.rank > 4) {
            error = "tensor rank is out of range";
            return false;
        }
        for (size_t axis = 0; axis < tensor.dims.size(); ++axis) {
            if (axis < tensor.rank) {
                if (tensor.dims[axis] == 0) {
                    error = "tensor dimension must be non-zero";
                    return false;
                }
                if (tensor.tile_dims[axis] == 0 || tensor.tile_dims[axis] > tensor.dims[axis]) {
                    error = "tensor tile is out of range";
                    return false;
                }
            } else if (tensor.dims[axis] != 0 || tensor.tile_dims[axis] != 0) {
                error = "tensor trailing dims/tile dims must be zero";
                return false;
            }
        }
        const uint64_t element_count = tensor_element_count(tensor);
        const uint64_t byte_size = element_count * ai_dtype_size_bytes(tensor.dtype);
        if (byte_size == 0 || byte_size > std::numeric_limits<uint32_t>::max()) {
            error = "tensor byte size is invalid";
            return false;
        }
    }

    for (size_t i = 0; i < package.ops.size(); ++i) {
        const AiOpDescriptor& op = package.ops[i];
        if (!ai_opcode_supported(op.opcode)) {
            error = "opcode is unsupported";
            return false;
        }
        if (!ai_dtype_supported(op.input_dtype)) {
            error = "opcode input dtype is unsupported";
            return false;
        }
        if (!ai_dtype_supported(op.accum_dtype) ||
            !compatible_accumulator_dtype(op.input_dtype, op.accum_dtype)) {
            error = "opcode accumulator dtype is incompatible";
            return false;
        }
        if (op.output >= package.tensors.size()) {
            error = "opcode output tensor index is out of range";
            return false;
        }
        if (requires_binary_inputs(op.opcode)) {
            if (op.input0 >= package.tensors.size() || op.input1 >= package.tensors.size() ||
                op.input2 != kAiInvalidTensorIndex) {
                error = "opcode binary tensor indexes are invalid";
                return false;
            }
        } else if (requires_unary_input(op.opcode)) {
            if (op.input0 >= package.tensors.size() || op.input1 != kAiInvalidTensorIndex ||
                op.input2 != kAiInvalidTensorIndex) {
                error = "opcode unary tensor indexes are invalid";
                return false;
            }
        }
    }

    for (const AiDependencyEdge& edge : package.dependencies) {
        if (edge.source_op >= package.ops.size() || edge.target_op >= package.ops.size() ||
            edge.source_op == edge.target_op) {
            error = "dependency edge is invalid";
            return false;
        }
    }

    for (const AiMemoryPlanEntry& entry : package.memory_plan) {
        if (entry.tensor_index >= package.tensors.size()) {
            error = "memory plan tensor index is out of range";
            return false;
        }
        const AiTensorMetadata& tensor = package.tensors[entry.tensor_index];
        const uint64_t expected_bytes = tensor_byte_size(tensor);
        if (entry.byte_size != expected_bytes) {
            error = "memory plan byte size does not match tensor";
            return false;
        }
        if (entry.scratchpad_bytes == 0 || entry.scratchpad_bytes > entry.byte_size) {
            error = "scratchpad bytes are invalid";
            return false;
        }
        if (static_cast<uint64_t>(entry.scratchpad_offset) + entry.scratchpad_bytes >
            package.scratchpad_budget_bytes) {
            error = "scratchpad budget is exceeded";
            return false;
        }
    }

    if (package.shape_mode == AiShapeMode::Static) {
        if (!package.dynamic_tensors.empty()) {
            error = "dynamic tensor metadata requires dynamic_bounded shape mode";
            return false;
        }
        return true;
    }

    if (package.dynamic_tensors.empty()) {
        error = "dynamic tensor metadata is required";
        return false;
    }
    for (size_t i = 0; i < package.dynamic_tensors.size(); ++i) {
        const AiDynamicTensorMetadata& metadata = package.dynamic_tensors[i];
        if (metadata.tensor_index >= package.tensors.size()) {
            error = "dynamic tensor index is out of range";
            return false;
        }
        if (metadata.max_tensor_bytes == 0) {
            error = "dynamic tensor max tensor bytes must be non-zero";
            return false;
        }
        for (size_t j = i + 1; j < package.dynamic_tensors.size(); ++j) {
            if (package.dynamic_tensors[j].tensor_index == metadata.tensor_index) {
                error = "dynamic tensor metadata is duplicated";
                return false;
            }
        }
        const AiTensorMetadata& tensor = package.tensors[metadata.tensor_index];
        const uint64_t expected_max_tensor_bytes = tensor_byte_size(tensor);
        if (expected_max_tensor_bytes == 0 ||
            expected_max_tensor_bytes > std::numeric_limits<uint32_t>::max() ||
            metadata.max_tensor_bytes != expected_max_tensor_bytes) {
            error = "dynamic tensor max tensor bytes do not match tensor";
            return false;
        }
        const AiMemoryPlanEntry* memory_plan = find_memory_plan_entry(package, metadata.tensor_index);
        if (memory_plan == nullptr || memory_plan->byte_size != metadata.max_tensor_bytes) {
            error = "dynamic tensor memory plan bytes do not match max tensor bytes";
            return false;
        }
    }
    return true;
}

bool validate_ai_runtime_shape_table(
    const AiGraphPackage& package,
    const std::vector<AiRuntimeShapeEntry>& runtime_shapes,
    std::string& error) {
    error.clear();
    if (package.shape_mode == AiShapeMode::Static) {
        if (!runtime_shapes.empty()) {
            error = "runtime shape table is not allowed for static packages";
            return false;
        }
        return true;
    }
    if (package.shape_mode != AiShapeMode::DynamicBounded) {
        error = "runtime shape table requires supported dynamic shape mode";
        return false;
    }
    if (runtime_shapes.size() != package.dynamic_tensors.size()) {
        error = "runtime shape table does not match dynamic tensor metadata";
        return false;
    }

    for (size_t i = 0; i < runtime_shapes.size(); ++i) {
        const AiRuntimeShapeEntry& runtime_shape = runtime_shapes[i];
        if (runtime_shape.rank == 0 || runtime_shape.rank > kAiMaxTensorRank) {
            error = "runtime shape rank is out of range";
            return false;
        }
        for (size_t j = i + 1; j < runtime_shapes.size(); ++j) {
            if (runtime_shapes[j].tensor_index == runtime_shape.tensor_index) {
                error = "runtime shape tensor index is duplicated";
                return false;
            }
        }
        const AiDynamicTensorMetadata* metadata =
            find_dynamic_tensor_metadata(package, runtime_shape.tensor_index);
        if (metadata == nullptr) {
            error = "runtime shape tensor index is not declared as dynamic";
            return false;
        }
        const AiTensorMetadata& tensor = package.tensors[runtime_shape.tensor_index];
        if (runtime_shape.rank != tensor.rank) {
            error = "runtime shape rank does not match tensor rank";
            return false;
        }
        for (size_t axis = 0; axis < runtime_shape.dims.size(); ++axis) {
            if (axis < runtime_shape.rank) {
                if (runtime_shape.dims[axis] == 0 || runtime_shape.dims[axis] > tensor.dims[axis]) {
                    error = "runtime shape dims exceed bounded tensor dims";
                    return false;
                }
            } else if (runtime_shape.dims[axis] != 0) {
                error = "runtime shape trailing dims must be zero";
                return false;
            }
        }
        const uint64_t runtime_tensor_bytes =
            runtime_shape_element_count(runtime_shape) * ai_dtype_size_bytes(tensor.dtype);
        if (runtime_tensor_bytes == 0 ||
            runtime_tensor_bytes > metadata->max_tensor_bytes ||
            runtime_tensor_bytes > std::numeric_limits<uint32_t>::max()) {
            error = "runtime shape bytes exceed max tensor bytes";
            return false;
        }
    }

    return true;
}

bool serialize_ai_runtime_shape_table(
    const std::vector<AiRuntimeShapeEntry>& runtime_shapes,
    std::vector<uint8_t>& bytes,
    std::string& error) {
    error.clear();
    bytes.clear();
    bytes.reserve(runtime_shapes.size() * kAiRuntimeShapeEntryBytes);
    for (const AiRuntimeShapeEntry& runtime_shape : runtime_shapes) {
        if (runtime_shape.rank == 0 || runtime_shape.rank > kAiMaxTensorRank) {
            error = "runtime shape rank is out of range";
            return false;
        }
        for (size_t axis = 0; axis < runtime_shape.dims.size(); ++axis) {
            if (axis < runtime_shape.rank) {
                if (runtime_shape.dims[axis] == 0) {
                    error = "runtime shape dims must be non-zero";
                    return false;
                }
            } else if (runtime_shape.dims[axis] != 0) {
                error = "runtime shape trailing dims must be zero";
                return false;
            }
        }
        append_u16(bytes, runtime_shape.tensor_index);
        append_u8(bytes, runtime_shape.rank);
        append_u8(bytes, 0);
        for (uint32_t dim : runtime_shape.dims) {
            append_u32(bytes, dim);
        }
    }
    return true;
}

bool parse_ai_runtime_shape_table(
    const std::vector<uint8_t>& bytes,
    size_t expected_entries,
    std::vector<AiRuntimeShapeEntry>& runtime_shapes,
    std::string& error) {
    error.clear();
    runtime_shapes.clear();
    const uint64_t expected_bytes =
        static_cast<uint64_t>(expected_entries) * static_cast<uint64_t>(kAiRuntimeShapeEntryBytes);
    if (expected_bytes > std::numeric_limits<size_t>::max() || bytes.size() != expected_bytes) {
        error = "runtime shape table byte length mismatch";
        return false;
    }
    runtime_shapes.reserve(expected_entries);
    size_t pos = 0;
    for (size_t i = 0; i < expected_entries; ++i) {
        AiRuntimeShapeEntry runtime_shape{};
        uint8_t reserved = 0;
        if (!read_u16(bytes, pos, runtime_shape.tensor_index) ||
            !read_u8(bytes, pos, runtime_shape.rank) ||
            !read_u8(bytes, pos, reserved)) {
            error = "runtime shape table truncated";
            return false;
        }
        if (reserved != 0) {
            error = "runtime shape table reserved byte must be zero";
            return false;
        }
        for (uint32_t& dim : runtime_shape.dims) {
            if (!read_u32(bytes, pos, dim)) {
                error = "runtime shape dims table truncated";
                return false;
            }
        }
        runtime_shapes.push_back(runtime_shape);
    }
    return true;
}

bool resolve_ai_runtime_shape_package(
    const AiGraphPackage& package,
    const std::vector<AiRuntimeShapeEntry>& runtime_shapes,
    AiGraphPackage& resolved_package,
    std::string& error) {
    error.clear();
    if (!validate_ai_runtime_shape_table(package, runtime_shapes, error)) {
        return false;
    }

    resolved_package = package;
    for (const AiRuntimeShapeEntry& runtime_shape : runtime_shapes) {
        AiTensorMetadata& tensor = resolved_package.tensors[runtime_shape.tensor_index];
        tensor.rank = runtime_shape.rank;
        for (size_t axis = 0; axis < tensor.dims.size(); ++axis) {
            if (axis < runtime_shape.rank) {
                tensor.dims[axis] = runtime_shape.dims[axis];
                if (tensor.tile_dims[axis] > tensor.dims[axis]) {
                    tensor.tile_dims[axis] = tensor.dims[axis];
                }
            } else {
                tensor.dims[axis] = 0;
                tensor.tile_dims[axis] = 0;
            }
        }
        AiMemoryPlanEntry* memory_plan = find_memory_plan_entry(resolved_package, runtime_shape.tensor_index);
        if (memory_plan == nullptr) {
            error = "runtime shape tensor memory plan is missing";
            return false;
        }
        const uint64_t runtime_bytes = runtime_tensor_byte_size(runtime_shape, tensor.dtype);
        if (runtime_bytes == 0 || runtime_bytes > std::numeric_limits<uint32_t>::max()) {
            error = "runtime shape bytes are invalid";
            return false;
        }
        memory_plan->byte_size = static_cast<uint32_t>(runtime_bytes);
        memory_plan->scratchpad_bytes = static_cast<uint32_t>(runtime_bytes);
    }

    resolved_package.shape_mode = AiShapeMode::Static;
    resolved_package.training_mode = AiTrainingMode::Inference;
    resolved_package.dynamic_tensors.clear();
    return validate_ai_graph_package(resolved_package, error);
}

bool serialize_ai_graph_package(
    const AiGraphPackage& package,
    std::vector<uint8_t>& bytes,
    std::string& error) {
    error.clear();
    if (!fits_u16_count(package.tensors) || !fits_u16_count(package.ops) ||
        !fits_u16_count(package.dependencies) || !fits_u16_count(package.memory_plan) ||
        !fits_u16_count(package.dynamic_tensors)) {
        error = "graph package table count exceeds 16-bit encoding";
        return false;
    }

    const uint16_t header_bytes =
        requires_extended_header(package) ? kAiGraphPackageExtendedHeaderBytes
                                          : kAiGraphPackageBaseHeaderBytes;
    const uint32_t tensors_offset = header_bytes;
    const uint32_t ops_offset =
        tensors_offset + static_cast<uint32_t>(package.tensors.size() * kTensorRecordBytes);
    const uint32_t dependencies_offset =
        ops_offset + static_cast<uint32_t>(package.ops.size() * kOpRecordBytes);
    const uint32_t memory_plan_offset =
        dependencies_offset +
        static_cast<uint32_t>(package.dependencies.size() * kDependencyRecordBytes);
    const uint32_t dynamic_tensors_offset =
        memory_plan_offset +
        static_cast<uint32_t>(package.memory_plan.size() * kMemoryPlanRecordBytes);
    const uint32_t package_bytes =
        dynamic_tensors_offset +
        static_cast<uint32_t>(package.dynamic_tensors.size() * kDynamicTensorRecordBytes);

    bytes.clear();
    bytes.reserve(package_bytes);
    append_u32(bytes, kAiGraphPackageMagic);
    append_u16(bytes, kAiGraphPackageVersion);
    append_u16(bytes, header_bytes);
    append_u32(bytes, package.scratchpad_budget_bytes);
    append_u16(bytes, static_cast<uint16_t>(package.tensors.size()));
    append_u16(bytes, static_cast<uint16_t>(package.ops.size()));
    append_u16(bytes, static_cast<uint16_t>(package.dependencies.size()));
    append_u16(bytes, static_cast<uint16_t>(package.memory_plan.size()));
    append_u32(bytes, tensors_offset);
    append_u32(bytes, ops_offset);
    append_u32(bytes, dependencies_offset);
    append_u32(bytes, memory_plan_offset);
    append_u32(bytes, package_bytes);
    if (header_bytes == kAiGraphPackageExtendedHeaderBytes) {
        append_u8(bytes, static_cast<uint8_t>(package.shape_mode));
        append_u8(bytes, static_cast<uint8_t>(package.training_mode));
        append_u16(bytes, static_cast<uint16_t>(package.dynamic_tensors.size()));
        append_u32(bytes, dynamic_tensors_offset);
        append_u32(bytes, 0);
        append_u32(bytes, 0);
    }

    for (const AiTensorMetadata& tensor : package.tensors) {
        append_u8(bytes, static_cast<uint8_t>(tensor.dtype));
        append_u8(bytes, static_cast<uint8_t>(tensor.role));
        append_u8(bytes, tensor.rank);
        append_u8(bytes, 0);
        for (uint32_t dim : tensor.dims) {
            append_u32(bytes, dim);
        }
        for (uint32_t tile_dim : tensor.tile_dims) {
            append_u32(bytes, tile_dim);
        }
    }

    for (const AiOpDescriptor& op : package.ops) {
        append_u8(bytes, static_cast<uint8_t>(op.opcode));
        append_u8(bytes, static_cast<uint8_t>(op.input_dtype));
        append_u8(bytes, static_cast<uint8_t>(op.accum_dtype));
        append_u8(bytes, 0);
        append_u16(bytes, op.input0);
        append_u16(bytes, op.input1);
        append_u16(bytes, op.input2);
        append_u16(bytes, op.output);
        for (int32_t attr : op.attrs) {
            append_i32(bytes, attr);
        }
    }

    for (const AiDependencyEdge& edge : package.dependencies) {
        append_u16(bytes, edge.source_op);
        append_u16(bytes, edge.target_op);
    }

    for (const AiMemoryPlanEntry& entry : package.memory_plan) {
        append_u16(bytes, entry.tensor_index);
        append_u16(bytes, 0);
        append_u32(bytes, entry.system_offset);
        append_u32(bytes, entry.scratchpad_offset);
        append_u32(bytes, entry.byte_size);
        append_u32(bytes, entry.scratchpad_bytes);
    }

    for (const AiDynamicTensorMetadata& metadata : package.dynamic_tensors) {
        append_u16(bytes, metadata.tensor_index);
        append_u16(bytes, 0);
        append_u32(bytes, metadata.max_tensor_bytes);
    }
    return true;
}

bool parse_ai_graph_package(
    const std::vector<uint8_t>& bytes,
    AiGraphPackage& package,
    std::string& error) {
    package = {};
    ParsedHeader header;
    if (!read_header(bytes, header, error)) {
        return false;
    }
    if (header.magic != kAiGraphPackageMagic) {
        error = "graph package magic is invalid";
        return false;
    }
    if (header.version != kAiGraphPackageVersion) {
        error = "graph package version is unsupported";
        return false;
    }
    if (header.header_bytes != kAiGraphPackageBaseHeaderBytes &&
        header.header_bytes != kAiGraphPackageExtendedHeaderBytes) {
        error = "graph package header bytes are invalid";
        return false;
    }
    if (header.package_bytes != bytes.size()) {
        error = "graph package byte length mismatch";
        return false;
    }
    if (!validate_table_range(
            header.tensors_offset, header.tensor_count, kTensorRecordBytes, header.package_bytes,
            "tensor", error) ||
        !validate_table_range(
            header.ops_offset, header.op_count, kOpRecordBytes, header.package_bytes, "opcode", error) ||
        !validate_table_range(
            header.dependencies_offset, header.dependency_count, kDependencyRecordBytes,
            header.package_bytes, "dependency", error) ||
        !validate_table_range(
            header.memory_plan_offset, header.memory_plan_count, kMemoryPlanRecordBytes,
            header.package_bytes, "memory plan", error) ||
        !validate_table_range(
            header.dynamic_tensors_offset, header.dynamic_tensor_count, kDynamicTensorRecordBytes,
            header.package_bytes, "dynamic tensor", error)) {
        return false;
    }

    package.shape_mode = header.shape_mode;
    package.training_mode = header.training_mode;
    package.scratchpad_budget_bytes = header.scratchpad_budget_bytes;

    size_t pos = header.tensors_offset;
    package.tensors.reserve(header.tensor_count);
    for (uint16_t i = 0; i < header.tensor_count; ++i) {
        AiTensorMetadata tensor;
        uint8_t dtype = 0;
        uint8_t role = 0;
        uint8_t reserved = 0;
        if (!read_u8(bytes, pos, dtype) || !read_u8(bytes, pos, role) || !read_u8(bytes, pos, tensor.rank) ||
            !read_u8(bytes, pos, reserved)) {
            error = "tensor table truncated";
            return false;
        }
        tensor.dtype = static_cast<AiDataType>(dtype);
        tensor.role = static_cast<AiTensorRole>(role);
        for (uint32_t& dim : tensor.dims) {
            if (!read_u32(bytes, pos, dim)) {
                error = "tensor dims table truncated";
                return false;
            }
        }
        for (uint32_t& tile_dim : tensor.tile_dims) {
            if (!read_u32(bytes, pos, tile_dim)) {
                error = "tensor tile table truncated";
                return false;
            }
        }
        package.tensors.push_back(tensor);
    }

    pos = header.ops_offset;
    package.ops.reserve(header.op_count);
    for (uint16_t i = 0; i < header.op_count; ++i) {
        AiOpDescriptor op;
        uint8_t opcode = 0;
        uint8_t input_dtype = 0;
        uint8_t accum_dtype = 0;
        uint8_t reserved = 0;
        if (!read_u8(bytes, pos, opcode) || !read_u8(bytes, pos, input_dtype) ||
            !read_u8(bytes, pos, accum_dtype) || !read_u8(bytes, pos, reserved) ||
            !read_u16(bytes, pos, op.input0) || !read_u16(bytes, pos, op.input1) ||
            !read_u16(bytes, pos, op.input2) || !read_u16(bytes, pos, op.output)) {
            error = "opcode table truncated";
            return false;
        }
        op.opcode = static_cast<AiOpCode>(opcode);
        op.input_dtype = static_cast<AiDataType>(input_dtype);
        op.accum_dtype = static_cast<AiDataType>(accum_dtype);
        for (int32_t& attr : op.attrs) {
            if (!read_i32(bytes, pos, attr)) {
                error = "opcode attrs table truncated";
                return false;
            }
        }
        package.ops.push_back(op);
    }

    pos = header.dependencies_offset;
    package.dependencies.reserve(header.dependency_count);
    for (uint16_t i = 0; i < header.dependency_count; ++i) {
        AiDependencyEdge edge;
        if (!read_u16(bytes, pos, edge.source_op) || !read_u16(bytes, pos, edge.target_op)) {
            error = "dependency table truncated";
            return false;
        }
        package.dependencies.push_back(edge);
    }

    pos = header.memory_plan_offset;
    package.memory_plan.reserve(header.memory_plan_count);
    for (uint16_t i = 0; i < header.memory_plan_count; ++i) {
        AiMemoryPlanEntry entry;
        uint16_t reserved = 0;
        if (!read_u16(bytes, pos, entry.tensor_index) || !read_u16(bytes, pos, reserved) ||
            !read_u32(bytes, pos, entry.system_offset) ||
            !read_u32(bytes, pos, entry.scratchpad_offset) ||
            !read_u32(bytes, pos, entry.byte_size) ||
            !read_u32(bytes, pos, entry.scratchpad_bytes)) {
            error = "memory plan table truncated";
            return false;
        }
        package.memory_plan.push_back(entry);
    }

    pos = header.dynamic_tensors_offset;
    package.dynamic_tensors.reserve(header.dynamic_tensor_count);
    for (uint16_t i = 0; i < header.dynamic_tensor_count; ++i) {
        AiDynamicTensorMetadata metadata;
        uint16_t reserved = 0;
        if (!read_u16(bytes, pos, metadata.tensor_index) ||
            !read_u16(bytes, pos, reserved) ||
            !read_u32(bytes, pos, metadata.max_tensor_bytes)) {
            error = "dynamic tensor table truncated";
            return false;
        }
        package.dynamic_tensors.push_back(metadata);
    }

    return validate_ai_graph_package(package, error);
}
