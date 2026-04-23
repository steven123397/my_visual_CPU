#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <vector>

#include "../../src/devices/tensor_golden_ops.h"

namespace {

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool almost_equal(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 1e-4f;
}

bool test_quantized_gemm_and_conv() {
    const std::vector<int8_t> lhs = {1, 2, -1, 0, 3, 4};
    const std::vector<int8_t> rhs = {2, -1, 1, 3, -2, 4};
    const std::vector<int32_t> gemm = tensor_golden_gemm_i8_to_i32(lhs, rhs, 2, 3, 2);
    const std::vector<int32_t> expected_gemm = {6, 1, -5, 25};
    if (!expect(gemm == expected_gemm, "expected int8 GEMM golden output")) {
        return false;
    }

    const std::vector<int16_t> input = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16,
    };
    const std::vector<int16_t> kernel = {
        1, 0,
        -1, 2,
    };
    const std::vector<int32_t> conv =
        tensor_golden_conv2d_valid_i16_to_i32(input, 4, 4, kernel, 2, 2);
    const std::vector<int32_t> expected_conv = {
        8, 10, 12,
        16, 18, 20,
        24, 26, 28,
    };
    return expect(conv == expected_conv, "expected int16 conv golden output");
}

bool test_semi_precision_gemm() {
    const std::vector<uint16_t> fp16_lhs = {0x3C00, 0x4000, 0x3800, 0xBC00};
    const std::vector<uint16_t> fp16_rhs = {0x3C00, 0x4000, 0x3E00, 0x3800};
    const std::vector<float> fp16_gemm =
        tensor_golden_gemm_fp16_to_fp32(fp16_lhs, fp16_rhs, 2, 2, 2);
    if (!expect(fp16_gemm.size() == 4, "expected fp16 GEMM output shape")) {
        return false;
    }
    if (!expect(almost_equal(fp16_gemm[0], 4.0f) &&
                    almost_equal(fp16_gemm[1], 3.0f) &&
                    almost_equal(fp16_gemm[2], -1.0f) &&
                    almost_equal(fp16_gemm[3], 0.5f),
                "expected fp16 GEMM golden values")) {
        return false;
    }

    const std::vector<uint16_t> bf16_lhs = {0x3F80, 0x4000, 0x4040, 0x4080};
    const std::vector<uint16_t> bf16_rhs = {0x3F80, 0x3FC0, 0x4000, 0x4040};
    const std::vector<float> bf16_gemm =
        tensor_golden_gemm_bf16_to_fp32(bf16_lhs, bf16_rhs, 2, 2, 2);
    return expect(almost_equal(bf16_gemm[0], 5.0f) &&
                      almost_equal(bf16_gemm[1], 7.5f) &&
                      almost_equal(bf16_gemm[2], 11.0f) &&
                      almost_equal(bf16_gemm[3], 16.5f),
                  "expected bf16 GEMM golden values");
}

bool test_elementwise_pool_reduce_and_layout() {
    const std::vector<int32_t> relu =
        tensor_golden_relu_i32({-3, 0, 4, -1, 6});
    if (!expect(relu == std::vector<int32_t>({0, 0, 4, 0, 6}),
                "expected relu golden output")) {
        return false;
    }

    const std::vector<float> pooled = tensor_golden_max_pool_2d_f32(
        {
            1.0f, 4.0f, 3.0f, 2.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            0.0f, -1.0f, 9.0f, 10.0f,
            2.0f, 3.0f, 11.0f, 12.0f,
        },
        4,
        4,
        2,
        2,
        2,
        2);
    if (!expect(pooled.size() == 4, "expected max pool output shape") ||
        !expect(almost_equal(pooled[0], 6.0f) &&
                    almost_equal(pooled[1], 8.0f) &&
                    almost_equal(pooled[2], 3.0f) &&
                    almost_equal(pooled[3], 12.0f),
                "expected max pool golden values")) {
        return false;
    }

    const std::vector<int32_t> reduced =
        tensor_golden_reduce_sum_rows_i32({1, 2, 3, 4, 5, 6}, 2, 3);
    const std::vector<int32_t> transposed =
        tensor_golden_transpose_2d_i32({1, 2, 3, 4, 5, 6}, 2, 3);
    return expect(reduced == std::vector<int32_t>({6, 15}),
                  "expected row reduce golden output") &&
           expect(transposed == std::vector<int32_t>({1, 4, 2, 5, 3, 6}),
                  "expected transpose golden output");
}

}  // namespace

int main() {
    try {
        if (!test_quantized_gemm_and_conv()) {
            return 1;
        }
        if (!test_semi_precision_gemm()) {
            return 1;
        }
        if (!test_elementwise_pool_reduce_and_layout()) {
            return 1;
        }
        std::puts("ai_tensor_golden_ops_smoke: PASS");
        return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
}
