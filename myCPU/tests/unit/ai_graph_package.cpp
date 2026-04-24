#include <array>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include "../../src/devices/ai_graph_package.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

AiGraphPackage make_valid_package() {
    AiGraphPackage package;
    package.scratchpad_budget_bytes = 256;

    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Input,
        .rank = 4,
        .dims = {1, 4, 4, 1},
        .tile_dims = {1, 2, 2, 1},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Weight,
        .rank = 4,
        .dims = {3, 3, 1, 2},
        .tile_dims = {3, 3, 1, 1},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 4,
        .dims = {1, 2, 2, 2},
        .tile_dims = {1, 2, 2, 1},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Output,
        .rank = 4,
        .dims = {1, 2, 2, 2},
        .tile_dims = {1, 2, 2, 1},
    });

    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Conv2d,
        .input_dtype = AiDataType::Int8,
        .accum_dtype = AiDataType::Int32,
        .input0 = 0,
        .input1 = 1,
        .input2 = kAiInvalidTensorIndex,
        .output = 2,
        .attrs = {1, 1, 0, 0},
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::EltwiseRelu,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 2,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 3,
        .attrs = {0, 0, 0, 0},
    });

    package.dependencies.push_back(AiDependencyEdge{
        .source_op = 0,
        .target_op = 1,
    });

    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 0,
        .system_offset = 0x0000,
        .scratchpad_offset = 0x0000,
        .byte_size = 16,
        .scratchpad_bytes = 8,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 1,
        .system_offset = 0x0100,
        .scratchpad_offset = 0x0020,
        .byte_size = 18,
        .scratchpad_bytes = 18,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 2,
        .system_offset = 0x0200,
        .scratchpad_offset = 0x0040,
        .byte_size = 32,
        .scratchpad_bytes = 32,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 3,
        .system_offset = 0x0300,
        .scratchpad_offset = 0x0080,
        .byte_size = 32,
        .scratchpad_bytes = 32,
    });
    return package;
}

bool expect_parse_failure(const AiGraphPackage& package, const char* needle) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!serialize_ai_graph_package(package, bytes, error)) {
        std::fprintf(stderr, "failed to serialize mutated package: %s\n", error.c_str());
        return false;
    }
    AiGraphPackage parsed;
    if (parse_ai_graph_package(bytes, parsed, error)) {
        std::fprintf(stderr, "expected parse failure containing: %s\n", needle);
        return false;
    }
    if (error.find(needle) == std::string::npos) {
        std::fprintf(stderr, "unexpected parse error: %s\n", error.c_str());
        return false;
    }
    return true;
}

AiGraphPackage make_dynamic_bounded_package() {
    AiGraphPackage package = make_valid_package();
    package.shape_mode = AiShapeMode::DynamicBounded;
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{
        .tensor_index = 0,
        .max_tensor_bytes = 16,
    });
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{
        .tensor_index = 3,
        .max_tensor_bytes = 32,
    });
    return package;
}

}  // namespace

int main() {
    try {
        const AiGraphPackage package = make_valid_package();
        std::vector<uint8_t> bytes;
        std::string error;
        if (!expect(serialize_ai_graph_package(package, bytes, error),
                    "expected valid graph package to serialize") ||
            !expect(!bytes.empty(), "expected serialized graph package bytes")) {
            return 1;
        }

        AiGraphPackage parsed;
        const bool parsed_ok = parse_ai_graph_package(bytes, parsed, error);
        if (!parsed_ok) {
            std::fprintf(stderr, "parse error: %s\n", error.c_str());
        }
        if (!expect(parsed_ok, "expected serialized graph package to parse") ||
            !expect(parsed.tensors.size() == package.tensors.size(), "expected tensor table roundtrip") ||
            !expect(parsed.ops.size() == package.ops.size(), "expected op table roundtrip") ||
            !expect(parsed.dependencies.size() == package.dependencies.size(),
                    "expected dependency table roundtrip") ||
            !expect(parsed.memory_plan.size() == package.memory_plan.size(),
                    "expected memory plan roundtrip") ||
            !expect(parsed.scratchpad_budget_bytes == package.scratchpad_budget_bytes,
                    "expected scratchpad budget roundtrip")) {
            return 1;
        }

        AiGraphPackage invalid_dtype = package;
        invalid_dtype.tensors[0].dtype = static_cast<AiDataType>(0xFF);
        if (!expect(expect_parse_failure(invalid_dtype, "dtype"), "expected invalid dtype rejection")) {
            return 1;
        }

        AiGraphPackage invalid_rank = package;
        invalid_rank.tensors[0].tile_dims[1] = 8;
        if (!expect(expect_parse_failure(invalid_rank, "tile"), "expected invalid tile rejection")) {
            return 1;
        }

        AiGraphPackage invalid_dependency = package;
        invalid_dependency.dependencies[0].target_op = 9;
        if (!expect(expect_parse_failure(invalid_dependency, "dependency"),
                    "expected invalid dependency rejection")) {
            return 1;
        }

        AiGraphPackage invalid_budget = package;
        invalid_budget.memory_plan[2].scratchpad_offset = 240;
        invalid_budget.memory_plan[2].scratchpad_bytes = 32;
        if (!expect(expect_parse_failure(invalid_budget, "scratchpad"),
                    "expected invalid scratchpad budget rejection")) {
            return 1;
        }

        AiGraphPackage invalid_opcode = package;
        invalid_opcode.ops[0].opcode = static_cast<AiOpCode>(0xFE);
        if (!expect(expect_parse_failure(invalid_opcode, "opcode"), "expected invalid opcode rejection")) {
            return 1;
        }

        const AiGraphPackage dynamic_package = make_dynamic_bounded_package();
        bytes.clear();
        if (!expect(serialize_ai_graph_package(dynamic_package, bytes, error),
                    "expected dynamic_bounded package to serialize")) {
            return 1;
        }
        parsed = {};
        if (!expect(parse_ai_graph_package(bytes, parsed, error),
                    "expected dynamic_bounded package to parse") ||
            !expect(parsed.shape_mode == AiShapeMode::DynamicBounded,
                    "expected dynamic_bounded shape mode roundtrip") ||
            !expect(parsed.dynamic_tensors.size() == 2,
                    "expected dynamic tensor metadata roundtrip")) {
            return 1;
        }

        const std::vector<AiRuntimeShapeEntry> valid_runtime_shapes{
            AiRuntimeShapeEntry{
                .tensor_index = 0,
                .rank = 4,
                .dims = {1, 4, 4, 1},
            },
            AiRuntimeShapeEntry{
                .tensor_index = 3,
                .rank = 4,
                .dims = {1, 2, 2, 2},
            },
        };
        if (!expect(validate_ai_runtime_shape_table(parsed, valid_runtime_shapes, error),
                    "expected bounded runtime shapes to validate")) {
            return 1;
        }

        AiGraphPackage missing_dynamic_metadata = dynamic_package;
        missing_dynamic_metadata.dynamic_tensors.clear();
        if (!expect(expect_parse_failure(missing_dynamic_metadata, "dynamic tensor"),
                    "expected dynamic_bounded package without metadata to fail")) {
            return 1;
        }

        AiGraphPackage invalid_max_tensor_bytes = dynamic_package;
        invalid_max_tensor_bytes.dynamic_tensors[0].max_tensor_bytes = 0;
        if (!expect(expect_parse_failure(invalid_max_tensor_bytes, "max tensor bytes"),
                    "expected invalid max tensor bytes rejection")) {
            return 1;
        }

        const std::vector<AiRuntimeShapeEntry> invalid_runtime_rank{
            AiRuntimeShapeEntry{
                .tensor_index = 0,
                .rank = 5,
                .dims = {1, 4, 4, 1},
            },
            AiRuntimeShapeEntry{
                .tensor_index = 3,
                .rank = 4,
                .dims = {1, 2, 2, 2},
            },
        };
        if (!expect(!validate_ai_runtime_shape_table(parsed, invalid_runtime_rank, error) &&
                        error.find("runtime shape rank") != std::string::npos,
                    "expected out-of-range runtime rank rejection")) {
            return 1;
        }

        const std::vector<AiRuntimeShapeEntry> invalid_runtime_dims{
            AiRuntimeShapeEntry{
                .tensor_index = 0,
                .rank = 4,
                .dims = {1, 5, 4, 1},
            },
            AiRuntimeShapeEntry{
                .tensor_index = 3,
                .rank = 4,
                .dims = {1, 2, 2, 2},
            },
        };
        if (!expect(!validate_ai_runtime_shape_table(parsed, invalid_runtime_dims, error) &&
                        error.find("runtime shape") != std::string::npos,
                    "expected runtime dims over max rejection")) {
            return 1;
        }

        AiGraphPackage training_reserved = dynamic_package;
        training_reserved.training_mode = AiTrainingMode::TrainingReserved;
        if (!expect(expect_parse_failure(training_reserved, "training mode"),
                    "expected reserved training mode rejection")) {
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
