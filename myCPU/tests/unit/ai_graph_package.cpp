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
        .rank = 2,
        .dims = {4, 4, 0, 0},
        .tile_dims = {4, 4, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Weight,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {3, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Output,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {3, 3, 0, 0},
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
        .byte_size = 4,
        .scratchpad_bytes = 4,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 2,
        .system_offset = 0x0200,
        .scratchpad_offset = 0x0040,
        .byte_size = 36,
        .scratchpad_bytes = 36,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 3,
        .system_offset = 0x0300,
        .scratchpad_offset = 0x0080,
        .byte_size = 36,
        .scratchpad_bytes = 36,
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

bool expect_parse_bytes_failure(const std::vector<uint8_t>& bytes, const char* needle) {
    std::string error;
    AiGraphPackage parsed;
    if (parse_ai_graph_package(bytes, parsed, error)) {
        std::fprintf(stderr, "expected byte parse failure containing: %s\n", needle);
        return false;
    }
    if (error.find(needle) == std::string::npos) {
        std::fprintf(stderr, "unexpected byte parse error: %s\n", error.c_str());
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
        .max_tensor_bytes = 36,
    });
    return package;
}

AiGraphPackage make_dynamic_cnn_chain_package() {
    AiGraphPackage package;
    package.shape_mode = AiShapeMode::DynamicBounded;
    package.scratchpad_budget_bytes = 192;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Input,
        .rank = 2,
        .dims = {4, 4, 0, 0},
        .tile_dims = {2, 4, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int8,
        .role = AiTensorRole::Weight,
        .rank = 2,
        .dims = {2, 2, 0, 0},
        .tile_dims = {2, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {2, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {2, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Intermediate,
        .rank = 2,
        .dims = {3, 3, 0, 0},
        .tile_dims = {3, 2, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Int32,
        .role = AiTensorRole::Output,
        .rank = 1,
        .dims = {3, 0, 0, 0},
        .tile_dims = {2, 0, 0, 0},
    });

    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Conv2d,
        .input_dtype = AiDataType::Int8,
        .accum_dtype = AiDataType::Int32,
        .input0 = 0,
        .input1 = 1,
        .input2 = kAiInvalidTensorIndex,
        .output = 2,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::EltwiseRelu,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 2,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 3,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::LayoutTranspose,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 3,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 4,
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::ReduceSum,
        .input_dtype = AiDataType::Int32,
        .accum_dtype = AiDataType::Int32,
        .input0 = 4,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 5,
    });
    package.dependencies.push_back(AiDependencyEdge{.source_op = 0, .target_op = 1});
    package.dependencies.push_back(AiDependencyEdge{.source_op = 1, .target_op = 2});
    package.dependencies.push_back(AiDependencyEdge{.source_op = 2, .target_op = 3});

    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 0,
        .system_offset = 0x0000,
        .scratchpad_offset = 0,
        .byte_size = 16,
        .scratchpad_bytes = 16,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 1,
        .system_offset = 0x0100,
        .scratchpad_offset = 16,
        .byte_size = 4,
        .scratchpad_bytes = 4,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 2,
        .system_offset = 0x0200,
        .scratchpad_offset = 32,
        .byte_size = 36,
        .scratchpad_bytes = 36,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 3,
        .system_offset = 0x0300,
        .scratchpad_offset = 80,
        .byte_size = 36,
        .scratchpad_bytes = 36,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 4,
        .system_offset = 0x0400,
        .scratchpad_offset = 128,
        .byte_size = 36,
        .scratchpad_bytes = 36,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 5,
        .system_offset = 0x0500,
        .scratchpad_offset = 176,
        .byte_size = 12,
        .scratchpad_bytes = 12,
    });

    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 0, .max_tensor_bytes = 16});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 2, .max_tensor_bytes = 36});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 3, .max_tensor_bytes = 36});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 4, .max_tensor_bytes = 36});
    package.dynamic_tensors.push_back(AiDynamicTensorMetadata{.tensor_index = 5, .max_tensor_bytes = 12});
    return package;
}

AiGraphPackage make_static_softmax_package() {
    AiGraphPackage package;
    package.scratchpad_budget_bytes = 128;
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp32,
        .role = AiTensorRole::Input,
        .rank = 2,
        .dims = {2, 3, 0, 0},
        .tile_dims = {2, 3, 0, 0},
    });
    package.tensors.push_back(AiTensorMetadata{
        .dtype = AiDataType::Fp32,
        .role = AiTensorRole::Output,
        .rank = 2,
        .dims = {2, 3, 0, 0},
        .tile_dims = {2, 3, 0, 0},
    });
    package.ops.push_back(AiOpDescriptor{
        .opcode = AiOpCode::Softmax,
        .input_dtype = AiDataType::Fp32,
        .accum_dtype = AiDataType::Fp32,
        .input0 = 0,
        .input1 = kAiInvalidTensorIndex,
        .input2 = kAiInvalidTensorIndex,
        .output = 1,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 0,
        .system_offset = 0x0000,
        .scratchpad_offset = 0,
        .byte_size = 24,
        .scratchpad_bytes = 24,
    });
    package.memory_plan.push_back(AiMemoryPlanEntry{
        .tensor_index = 1,
        .system_offset = 0x0100,
        .scratchpad_offset = 32,
        .byte_size = 24,
        .scratchpad_bytes = 24,
    });
    return package;
}

std::vector<AiRuntimeShapeEntry> make_dynamic_cnn_runtime_shapes() {
    return std::vector<AiRuntimeShapeEntry>{
        AiRuntimeShapeEntry{
            .tensor_index = 0,
            .rank = 2,
            .dims = {3, 3, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 2,
            .rank = 2,
            .dims = {2, 2, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 3,
            .rank = 2,
            .dims = {2, 2, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 4,
            .rank = 2,
            .dims = {2, 2, 0, 0},
        },
        AiRuntimeShapeEntry{
            .tensor_index = 5,
            .rank = 1,
            .dims = {2, 0, 0, 0},
        },
    };
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

        AiGraphPackage duplicate_memory_plan = package;
        duplicate_memory_plan.memory_plan.push_back(package.memory_plan[0]);
        if (!expect(expect_parse_failure(duplicate_memory_plan, "duplicate memory plan"),
                    "expected duplicate memory plan rejection")) {
            return 1;
        }

        AiGraphPackage missing_output_memory_plan = package;
        missing_output_memory_plan.memory_plan.pop_back();
        if (!expect(expect_parse_failure(missing_output_memory_plan, "missing memory plan"),
                    "expected missing output memory plan rejection")) {
            return 1;
        }

        std::vector<uint8_t> reserved_tensor_bytes = bytes;
        reserved_tensor_bytes[40 + 3] = 0x1;
        if (!expect(expect_parse_bytes_failure(reserved_tensor_bytes, "tensor reserved"),
                    "expected tensor reserved field rejection")) {
            return 1;
        }

        std::vector<uint8_t> reserved_op_bytes = bytes;
        const size_t op_offset = 40 + package.tensors.size() * 36;
        reserved_op_bytes[op_offset + 3] = 0x1;
        if (!expect(expect_parse_bytes_failure(reserved_op_bytes, "opcode reserved"),
                    "expected opcode reserved field rejection")) {
            return 1;
        }

        std::vector<uint8_t> reserved_memory_plan_bytes = bytes;
        const size_t memory_plan_offset =
            op_offset + package.ops.size() * 28 + package.dependencies.size() * 4;
        reserved_memory_plan_bytes[memory_plan_offset + 2] = 0x1;
        if (!expect(expect_parse_bytes_failure(reserved_memory_plan_bytes, "memory plan reserved"),
                    "expected memory plan reserved field rejection")) {
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

        std::vector<uint8_t> reserved_header_bytes = bytes;
        reserved_header_bytes[48] = 0x1;
        AiGraphPackage reserved_header_parsed{};
        if (!expect(!parse_ai_graph_package(reserved_header_bytes, reserved_header_parsed, error) &&
                        error.find("reserved") != std::string::npos,
                    "expected dynamic graph extended header reserved field rejection")) {
            return 1;
        }

        std::vector<uint8_t> reserved_dynamic_tensor_bytes = bytes;
        const size_t dynamic_tensor_offset =
            56 + dynamic_package.tensors.size() * 36 + dynamic_package.ops.size() * 28 +
            dynamic_package.dependencies.size() * 4 + dynamic_package.memory_plan.size() * 20;
        reserved_dynamic_tensor_bytes[dynamic_tensor_offset + 2] = 0x1;
        if (!expect(expect_parse_bytes_failure(reserved_dynamic_tensor_bytes,
                                               "dynamic tensor reserved"),
                    "expected dynamic tensor reserved field rejection")) {
            return 1;
        }

        const std::vector<AiRuntimeShapeEntry> valid_runtime_shapes{
            AiRuntimeShapeEntry{
                .tensor_index = 0,
                .rank = 2,
                .dims = {4, 4, 0, 0},
            },
            AiRuntimeShapeEntry{
                .tensor_index = 3,
                .rank = 2,
                .dims = {3, 3, 0, 0},
            },
        };
        if (!expect(validate_ai_runtime_shape_table(parsed, valid_runtime_shapes, error),
                    "expected bounded runtime shapes to validate")) {
            return 1;
        }

        std::vector<uint8_t> runtime_shape_bytes{};
        std::vector<AiRuntimeShapeEntry> parsed_runtime_shapes{};
        if (!expect(serialize_ai_runtime_shape_table(valid_runtime_shapes, runtime_shape_bytes, error),
                    "expected runtime shape table to serialize") ||
            !expect(parse_ai_runtime_shape_table(runtime_shape_bytes,
                                                 valid_runtime_shapes.size(),
                                                 parsed_runtime_shapes,
                                                 error),
                    "expected runtime shape table to parse")) {
            return 1;
        }
        std::vector<uint8_t> reserved_runtime_shape_bytes = runtime_shape_bytes;
        reserved_runtime_shape_bytes[3] = 0x80;
        if (!expect(!parse_ai_runtime_shape_table(reserved_runtime_shape_bytes,
                                                  valid_runtime_shapes.size(),
                                                  parsed_runtime_shapes,
                                                  error) &&
                        error.find("reserved") != std::string::npos,
                    "expected runtime shape table reserved byte rejection")) {
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

        AiGraphPackage invalid_dynamic_memory_plan = dynamic_package;
        invalid_dynamic_memory_plan.memory_plan[3].byte_size = 32;
        if (!expect(expect_parse_failure(invalid_dynamic_memory_plan, "memory plan byte size"),
                    "expected dynamic tensor memory-plan byte mismatch rejection")) {
            return 1;
        }

        const std::vector<AiRuntimeShapeEntry> invalid_runtime_rank{
            AiRuntimeShapeEntry{
                .tensor_index = 0,
                .rank = 5,
                .dims = {4, 4, 0, 0},
            },
            AiRuntimeShapeEntry{
                .tensor_index = 3,
                .rank = 2,
                .dims = {3, 3, 0, 0},
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
                .rank = 2,
                .dims = {5, 4, 0, 0},
            },
            AiRuntimeShapeEntry{
                .tensor_index = 3,
                .rank = 2,
                .dims = {3, 3, 0, 0},
            },
        };
        if (!expect(!validate_ai_runtime_shape_table(parsed, invalid_runtime_dims, error) &&
                        error.find("runtime shape") != std::string::npos,
                    "expected runtime dims over max rejection")) {
            return 1;
        }

        const AiGraphPackage softmax_package = make_static_softmax_package();
        bytes.clear();
        if (!expect(serialize_ai_graph_package(softmax_package, bytes, error),
                    "expected static fp32 Softmax package to serialize")) {
            return 1;
        }
        parsed = {};
        if (!expect(parse_ai_graph_package(bytes, parsed, error),
                    "expected static fp32 Softmax package to parse") ||
            !expect(parsed.ops.size() == 1 && parsed.ops[0].opcode == AiOpCode::Softmax,
                    "expected Softmax opcode roundtrip")) {
            return 1;
        }

        AiGraphPackage softmax_invalid_dtype = softmax_package;
        softmax_invalid_dtype.tensors[0].dtype = AiDataType::Fp16;
        softmax_invalid_dtype.ops[0].input_dtype = AiDataType::Fp16;
        if (!expect(expect_parse_failure(softmax_invalid_dtype, "Softmax"),
                    "expected non-fp32 Softmax dtype rejection")) {
            return 1;
        }

        AiGraphPackage softmax_invalid_rank = softmax_package;
        softmax_invalid_rank.tensors[1].rank = 1;
        softmax_invalid_rank.tensors[1].dims = {6, 0, 0, 0};
        softmax_invalid_rank.tensors[1].tile_dims = {6, 0, 0, 0};
        if (!expect(expect_parse_failure(softmax_invalid_rank, "Softmax"),
                    "expected Softmax rank rejection")) {
            return 1;
        }

        AiGraphPackage training_reserved = dynamic_package;
        training_reserved.training_mode = AiTrainingMode::TrainingReserved;
        if (!expect(expect_parse_failure(training_reserved, "training mode"),
                    "expected reserved training mode rejection")) {
            return 1;
        }

        AiGraphPackage dynamic_cnn = make_dynamic_cnn_chain_package();
        const std::vector<AiRuntimeShapeEntry> dynamic_cnn_runtime_shapes =
            make_dynamic_cnn_runtime_shapes();
        AiGraphPackage resolved_dynamic_cnn{};
        if (!expect(resolve_ai_runtime_shape_package(dynamic_cnn,
                                                     dynamic_cnn_runtime_shapes,
                                                     resolved_dynamic_cnn,
                                                     error),
                    "expected dynamic CNN-family runtime shape resolve") ||
            !expect(resolved_dynamic_cnn.shape_mode == AiShapeMode::Static,
                    "expected resolved dynamic CNN package to become static") ||
            !expect(resolved_dynamic_cnn.tensors[0].dims[0] == 3 &&
                        resolved_dynamic_cnn.tensors[2].dims[0] == 2 &&
                        resolved_dynamic_cnn.tensors[5].dims[0] == 2,
                    "expected resolved dynamic CNN tensor dims") ||
            !expect(resolved_dynamic_cnn.memory_plan[0].byte_size == 9 &&
                        resolved_dynamic_cnn.memory_plan[2].byte_size == 16 &&
                        resolved_dynamic_cnn.memory_plan[5].byte_size == 8,
                    "expected resolved dynamic CNN memory plan byte sizes")) {
            return 1;
        }

        AiGraphPackage missing_dynamic_op_tensor = dynamic_cnn;
        missing_dynamic_op_tensor.dynamic_tensors.erase(
            missing_dynamic_op_tensor.dynamic_tensors.begin() + 2,
            missing_dynamic_op_tensor.dynamic_tensors.end());
        const std::vector<AiRuntimeShapeEntry> missing_op_tensor_shapes{
            dynamic_cnn_runtime_shapes[0],
            dynamic_cnn_runtime_shapes[1],
        };
        AiGraphPackage resolved_missing_dynamic_op_tensor{};
        if (!expect(!resolve_ai_runtime_shape_package(missing_dynamic_op_tensor,
                                                      missing_op_tensor_shapes,
                                                      resolved_missing_dynamic_op_tensor,
                                                      error) &&
                        error.find("shape") != std::string::npos,
                    "expected missing dynamic op tensor metadata to fail closed")) {
            return 1;
        }

        AiGraphPackage runtime_scratchpad_bound = dynamic_cnn;
        runtime_scratchpad_bound.scratchpad_budget_bytes = 16;
        runtime_scratchpad_bound.memory_plan[0].scratchpad_offset = 12;
        runtime_scratchpad_bound.memory_plan[0].scratchpad_bytes = 4;
        std::vector<AiRuntimeShapeEntry> scratchpad_bound_shapes = dynamic_cnn_runtime_shapes;
        scratchpad_bound_shapes[0].dims = {4, 4, 0, 0};
        scratchpad_bound_shapes[1].dims = {3, 3, 0, 0};
        scratchpad_bound_shapes[2].dims = {3, 3, 0, 0};
        scratchpad_bound_shapes[3].dims = {3, 3, 0, 0};
        scratchpad_bound_shapes[4].dims = {3, 0, 0, 0};
        if (!expect(!resolve_ai_runtime_shape_package(runtime_scratchpad_bound,
                                                      scratchpad_bound_shapes,
                                                      resolved_dynamic_cnn,
                                                      error) &&
                        error.find("scratchpad") != std::string::npos,
                    "expected runtime scratchpad bound rejection")) {
            return 1;
        }

        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
