#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

inline constexpr uint32_t kAiGraphPackageMagic = 0x31475041U;
inline constexpr uint16_t kAiGraphPackageVersion = 1;
inline constexpr uint16_t kAiInvalidTensorIndex = 0xFFFFU;

enum class AiDataType : uint8_t {
    Invalid = 0,
    Int8 = 1,
    Int16 = 2,
    Int32 = 3,
    Fp16 = 4,
    Bf16 = 5,
    Fp32 = 6,
};

enum class AiTensorRole : uint8_t {
    Invalid = 0,
    Input = 1,
    Output = 2,
    Weight = 3,
    Intermediate = 4,
    Constant = 5,
};

enum class AiOpCode : uint8_t {
    Invalid = 0,
    Gemm = 1,
    Conv2d = 2,
    EltwiseRelu = 3,
    PoolMax = 4,
    ReduceSum = 5,
    LayoutTranspose = 6,
};

struct AiTensorMetadata {
    AiDataType dtype{AiDataType::Invalid};
    AiTensorRole role{AiTensorRole::Invalid};
    uint8_t rank{0};
    std::array<uint32_t, 4> dims{};
    std::array<uint32_t, 4> tile_dims{};
};

struct AiOpDescriptor {
    AiOpCode opcode{AiOpCode::Invalid};
    AiDataType input_dtype{AiDataType::Invalid};
    AiDataType accum_dtype{AiDataType::Invalid};
    uint16_t input0{kAiInvalidTensorIndex};
    uint16_t input1{kAiInvalidTensorIndex};
    uint16_t input2{kAiInvalidTensorIndex};
    uint16_t output{kAiInvalidTensorIndex};
    std::array<int32_t, 4> attrs{};
};

struct AiDependencyEdge {
    uint16_t source_op{0};
    uint16_t target_op{0};
};

struct AiMemoryPlanEntry {
    uint16_t tensor_index{kAiInvalidTensorIndex};
    uint32_t system_offset{0};
    uint32_t scratchpad_offset{0};
    uint32_t byte_size{0};
    uint32_t scratchpad_bytes{0};
};

struct AiGraphPackage {
    uint32_t scratchpad_budget_bytes{0};
    std::vector<AiTensorMetadata> tensors{};
    std::vector<AiOpDescriptor> ops{};
    std::vector<AiDependencyEdge> dependencies{};
    std::vector<AiMemoryPlanEntry> memory_plan{};
};

bool ai_dtype_supported(AiDataType dtype);
bool ai_tensor_role_supported(AiTensorRole role);
bool ai_opcode_supported(AiOpCode opcode);
size_t ai_dtype_size_bytes(AiDataType dtype);
const char* ai_dtype_name(AiDataType dtype);
const char* ai_opcode_name(AiOpCode opcode);

bool validate_ai_graph_package(const AiGraphPackage& package, std::string& error);
bool serialize_ai_graph_package(
    const AiGraphPackage& package,
    std::vector<uint8_t>& bytes,
    std::string& error);
bool parse_ai_graph_package(
    const std::vector<uint8_t>& bytes,
    AiGraphPackage& package,
    std::string& error);

