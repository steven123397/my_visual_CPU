#include "ai_graph_package.h"

#include <limits>

namespace {

constexpr uint16_t kAiGraphPackageHeaderBytes = 40;
constexpr size_t kTensorRecordBytes = 36;
constexpr size_t kOpRecordBytes = 28;
constexpr size_t kDependencyRecordBytes = 4;
constexpr size_t kMemoryPlanRecordBytes = 20;

struct ParsedHeader {
    uint32_t magic{0};
    uint16_t version{0};
    uint16_t header_bytes{0};
    uint32_t scratchpad_budget_bytes{0};
    uint16_t tensor_count{0};
    uint16_t op_count{0};
    uint16_t dependency_count{0};
    uint16_t memory_plan_count{0};
    uint32_t tensors_offset{0};
    uint32_t ops_offset{0};
    uint32_t dependencies_offset{0};
    uint32_t memory_plan_offset{0};
    uint32_t package_bytes{0};
};

template <typename T>
bool fits_u16_count(const std::vector<T>& values) {
    return values.size() <= std::numeric_limits<uint16_t>::max();
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
    if (bytes.size() < kAiGraphPackageHeaderBytes) {
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
        const uint64_t expected_bytes =
            tensor_element_count(tensor) * ai_dtype_size_bytes(tensor.dtype);
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
    return true;
}

bool serialize_ai_graph_package(
    const AiGraphPackage& package,
    std::vector<uint8_t>& bytes,
    std::string& error) {
    error.clear();
    if (!fits_u16_count(package.tensors) || !fits_u16_count(package.ops) ||
        !fits_u16_count(package.dependencies) || !fits_u16_count(package.memory_plan)) {
        error = "graph package table count exceeds 16-bit encoding";
        return false;
    }

    const uint32_t tensors_offset = kAiGraphPackageHeaderBytes;
    const uint32_t ops_offset =
        tensors_offset + static_cast<uint32_t>(package.tensors.size() * kTensorRecordBytes);
    const uint32_t dependencies_offset =
        ops_offset + static_cast<uint32_t>(package.ops.size() * kOpRecordBytes);
    const uint32_t memory_plan_offset =
        dependencies_offset +
        static_cast<uint32_t>(package.dependencies.size() * kDependencyRecordBytes);
    const uint32_t package_bytes =
        memory_plan_offset +
        static_cast<uint32_t>(package.memory_plan.size() * kMemoryPlanRecordBytes);

    bytes.clear();
    bytes.reserve(package_bytes);
    append_u32(bytes, kAiGraphPackageMagic);
    append_u16(bytes, kAiGraphPackageVersion);
    append_u16(bytes, kAiGraphPackageHeaderBytes);
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
    if (header.header_bytes != kAiGraphPackageHeaderBytes) {
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
            header.package_bytes, "memory plan", error)) {
        return false;
    }

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

    return validate_ai_graph_package(package, error);
}
