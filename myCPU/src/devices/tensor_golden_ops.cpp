#include "tensor_golden_ops.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

float bits_to_float(uint32_t bits) {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

uint32_t float_to_bits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

template <typename T>
void expect_size(const std::vector<T>& values, uint64_t expected, const char* label) {
    if (values.size() != expected) {
        throw std::invalid_argument(std::string(label) + " has unexpected element count");
    }
}

template <typename T>
std::vector<int32_t> golden_gemm_integer_to_i32(const std::vector<T>& lhs,
                                                const std::vector<T>& rhs,
                                                uint32_t m,
                                                uint32_t k,
                                                uint32_t n) {
    if (m == 0 || k == 0 || n == 0) {
        throw std::invalid_argument("GEMM shape must be non-zero");
    }
    expect_size(lhs, static_cast<uint64_t>(m) * k, "GEMM lhs");
    expect_size(rhs, static_cast<uint64_t>(k) * n, "GEMM rhs");
    std::vector<int32_t> out(static_cast<size_t>(m) * n, 0);
    for (uint32_t row = 0; row < m; ++row) {
        for (uint32_t col = 0; col < n; ++col) {
            int32_t acc = 0;
            for (uint32_t depth = 0; depth < k; ++depth) {
                acc += static_cast<int32_t>(lhs[row * k + depth]) *
                       static_cast<int32_t>(rhs[depth * n + col]);
            }
            out[row * n + col] = acc;
        }
    }
    return out;
}

float decode_low_precision(uint16_t bits, AiDataType dtype) {
    switch (dtype) {
    case AiDataType::Fp16:
        return tensor_golden_decode_fp16(bits);
    case AiDataType::Bf16:
        return tensor_golden_decode_bf16(bits);
    default:
        throw std::invalid_argument("unsupported low-precision dtype");
    }
}

std::vector<float> golden_gemm_low_precision_to_fp32(const std::vector<uint16_t>& lhs,
                                                     const std::vector<uint16_t>& rhs,
                                                     uint32_t m,
                                                     uint32_t k,
                                                     uint32_t n,
                                                     AiDataType dtype) {
    if (m == 0 || k == 0 || n == 0) {
        throw std::invalid_argument("GEMM shape must be non-zero");
    }
    expect_size(lhs, static_cast<uint64_t>(m) * k, "GEMM lhs");
    expect_size(rhs, static_cast<uint64_t>(k) * n, "GEMM rhs");
    std::vector<float> out(static_cast<size_t>(m) * n, 0.0f);
    for (uint32_t row = 0; row < m; ++row) {
        for (uint32_t col = 0; col < n; ++col) {
            float acc = 0.0f;
            for (uint32_t depth = 0; depth < k; ++depth) {
                acc += decode_low_precision(lhs[row * k + depth], dtype) *
                       decode_low_precision(rhs[depth * n + col], dtype);
            }
            out[row * n + col] = acc;
        }
    }
    return out;
}

template <typename T>
std::vector<int32_t> golden_conv2d_valid_to_i32(const std::vector<T>& input,
                                                uint32_t input_h,
                                                uint32_t input_w,
                                                const std::vector<T>& kernel,
                                                uint32_t kernel_h,
                                                uint32_t kernel_w) {
    if (input_h == 0 || input_w == 0 || kernel_h == 0 || kernel_w == 0) {
        throw std::invalid_argument("conv shape must be non-zero");
    }
    if (kernel_h > input_h || kernel_w > input_w) {
        throw std::invalid_argument("conv kernel exceeds input");
    }
    expect_size(input, static_cast<uint64_t>(input_h) * input_w, "conv input");
    expect_size(kernel, static_cast<uint64_t>(kernel_h) * kernel_w, "conv kernel");

    const uint32_t output_h = input_h - kernel_h + 1;
    const uint32_t output_w = input_w - kernel_w + 1;
    std::vector<int32_t> out(static_cast<size_t>(output_h) * output_w, 0);
    for (uint32_t oy = 0; oy < output_h; ++oy) {
        for (uint32_t ox = 0; ox < output_w; ++ox) {
            int32_t acc = 0;
            for (uint32_t ky = 0; ky < kernel_h; ++ky) {
                for (uint32_t kx = 0; kx < kernel_w; ++kx) {
                    acc += static_cast<int32_t>(input[(oy + ky) * input_w + (ox + kx)]) *
                           static_cast<int32_t>(kernel[ky * kernel_w + kx]);
                }
            }
            out[oy * output_w + ox] = acc;
        }
    }
    return out;
}

}  // namespace

float tensor_golden_decode_fp16(uint16_t bits) {
    const uint32_t sign = static_cast<uint32_t>(bits & 0x8000U) << 16;
    uint32_t exponent = (bits >> 10) & 0x1FU;
    uint32_t mantissa = bits & 0x03FFU;

    if (exponent == 0) {
        if (mantissa == 0) {
            return bits_to_float(sign);
        }
        while ((mantissa & 0x0400U) == 0) {
            mantissa <<= 1;
            --exponent;
        }
        ++exponent;
        mantissa &= 0x03FFU;
    } else if (exponent == 0x1FU) {
        return bits_to_float(sign | 0x7F800000U | (mantissa << 13));
    }

    exponent = exponent + (127 - 15);
    return bits_to_float(sign | (exponent << 23) | (mantissa << 13));
}

float tensor_golden_decode_bf16(uint16_t bits) {
    return bits_to_float(static_cast<uint32_t>(bits) << 16);
}

uint16_t tensor_golden_encode_fp16(float value) {
    const uint32_t bits = float_to_bits(value);
    const uint32_t sign = (bits >> 16) & 0x8000U;
    int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xFFU) - 127 + 15;
    uint32_t mantissa = bits & 0x007FFFFFU;

    if (((bits >> 23) & 0xFFU) == 0xFFU) {
        return static_cast<uint16_t>(sign | 0x7C00U | (mantissa ? 0x0200U : 0));
    }
    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<uint16_t>(sign);
        }
        mantissa |= 0x00800000U;
        const uint32_t shifted = mantissa >> static_cast<uint32_t>(1 - exponent + 13);
        const uint32_t round_bit = (mantissa >> static_cast<uint32_t>(1 - exponent + 12)) & 1U;
        return static_cast<uint16_t>(sign | (shifted + round_bit));
    }
    if (exponent >= 31) {
        return static_cast<uint16_t>(sign | 0x7C00U);
    }
    mantissa += 0x00001000U;
    if (mantissa & 0x00800000U) {
        mantissa = 0;
        ++exponent;
    }
    if (exponent >= 31) {
        return static_cast<uint16_t>(sign | 0x7C00U);
    }
    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13));
}

uint16_t tensor_golden_encode_bf16(float value) {
    const uint32_t bits = float_to_bits(value);
    const uint32_t lsb = (bits >> 16) & 1U;
    const uint32_t rounded = bits + 0x7FFFU + lsb;
    return static_cast<uint16_t>(rounded >> 16);
}

std::vector<int32_t> tensor_golden_gemm_i8_to_i32(const std::vector<int8_t>& lhs,
                                                  const std::vector<int8_t>& rhs,
                                                  uint32_t m,
                                                  uint32_t k,
                                                  uint32_t n) {
    return golden_gemm_integer_to_i32(lhs, rhs, m, k, n);
}

std::vector<int32_t> tensor_golden_gemm_i16_to_i32(const std::vector<int16_t>& lhs,
                                                   const std::vector<int16_t>& rhs,
                                                   uint32_t m,
                                                   uint32_t k,
                                                   uint32_t n) {
    return golden_gemm_integer_to_i32(lhs, rhs, m, k, n);
}

std::vector<float> tensor_golden_gemm_fp16_to_fp32(const std::vector<uint16_t>& lhs,
                                                   const std::vector<uint16_t>& rhs,
                                                   uint32_t m,
                                                   uint32_t k,
                                                   uint32_t n) {
    return golden_gemm_low_precision_to_fp32(lhs, rhs, m, k, n, AiDataType::Fp16);
}

std::vector<float> tensor_golden_gemm_bf16_to_fp32(const std::vector<uint16_t>& lhs,
                                                   const std::vector<uint16_t>& rhs,
                                                   uint32_t m,
                                                   uint32_t k,
                                                   uint32_t n) {
    return golden_gemm_low_precision_to_fp32(lhs, rhs, m, k, n, AiDataType::Bf16);
}

std::vector<float> tensor_golden_gemm_f32(const std::vector<float>& lhs,
                                          const std::vector<float>& rhs,
                                          uint32_t m,
                                          uint32_t k,
                                          uint32_t n) {
    if (m == 0 || k == 0 || n == 0) {
        throw std::invalid_argument("GEMM shape must be non-zero");
    }
    expect_size(lhs, static_cast<uint64_t>(m) * k, "GEMM lhs");
    expect_size(rhs, static_cast<uint64_t>(k) * n, "GEMM rhs");
    std::vector<float> out(static_cast<size_t>(m) * n, 0.0f);
    for (uint32_t row = 0; row < m; ++row) {
        for (uint32_t col = 0; col < n; ++col) {
            float acc = 0.0f;
            for (uint32_t depth = 0; depth < k; ++depth) {
                acc += lhs[row * k + depth] * rhs[depth * n + col];
            }
            out[row * n + col] = acc;
        }
    }
    return out;
}

std::vector<int32_t> tensor_golden_conv2d_valid_i8_to_i32(const std::vector<int8_t>& input,
                                                          uint32_t input_h,
                                                          uint32_t input_w,
                                                          const std::vector<int8_t>& kernel,
                                                          uint32_t kernel_h,
                                                          uint32_t kernel_w) {
    return golden_conv2d_valid_to_i32(input, input_h, input_w, kernel, kernel_h, kernel_w);
}

std::vector<int32_t> tensor_golden_conv2d_valid_i16_to_i32(const std::vector<int16_t>& input,
                                                           uint32_t input_h,
                                                           uint32_t input_w,
                                                           const std::vector<int16_t>& kernel,
                                                           uint32_t kernel_h,
                                                           uint32_t kernel_w) {
    return golden_conv2d_valid_to_i32(input, input_h, input_w, kernel, kernel_h, kernel_w);
}

std::vector<int32_t> tensor_golden_relu_i32(const std::vector<int32_t>& input) {
    std::vector<int32_t> out = input;
    for (int32_t& value : out) {
        value = std::max<int32_t>(0, value);
    }
    return out;
}

std::vector<float> tensor_golden_max_pool_2d_f32(const std::vector<float>& input,
                                                 uint32_t input_h,
                                                 uint32_t input_w,
                                                 uint32_t window_h,
                                                 uint32_t window_w,
                                                 uint32_t stride_h,
                                                 uint32_t stride_w) {
    if (input_h == 0 || input_w == 0 || window_h == 0 || window_w == 0 ||
        stride_h == 0 || stride_w == 0) {
        throw std::invalid_argument("max pool shape must be non-zero");
    }
    expect_size(input, static_cast<uint64_t>(input_h) * input_w, "max pool input");
    if (window_h > input_h || window_w > input_w) {
        throw std::invalid_argument("max pool window exceeds input");
    }

    const uint32_t output_h = (input_h - window_h) / stride_h + 1;
    const uint32_t output_w = (input_w - window_w) / stride_w + 1;
    std::vector<float> out(static_cast<size_t>(output_h) * output_w, 0.0f);
    for (uint32_t oy = 0; oy < output_h; ++oy) {
        for (uint32_t ox = 0; ox < output_w; ++ox) {
            float best = -std::numeric_limits<float>::infinity();
            for (uint32_t wy = 0; wy < window_h; ++wy) {
                for (uint32_t wx = 0; wx < window_w; ++wx) {
                    const uint32_t iy = oy * stride_h + wy;
                    const uint32_t ix = ox * stride_w + wx;
                    best = std::max(best, input[iy * input_w + ix]);
                }
            }
            out[oy * output_w + ox] = best;
        }
    }
    return out;
}

std::vector<int32_t> tensor_golden_reduce_sum_rows_i32(const std::vector<int32_t>& input,
                                                       uint32_t rows,
                                                       uint32_t cols) {
    if (rows == 0 || cols == 0) {
        throw std::invalid_argument("reduce shape must be non-zero");
    }
    expect_size(input, static_cast<uint64_t>(rows) * cols, "reduce input");
    std::vector<int32_t> out(rows, 0);
    for (uint32_t row = 0; row < rows; ++row) {
        int32_t acc = 0;
        for (uint32_t col = 0; col < cols; ++col) {
            acc += input[row * cols + col];
        }
        out[row] = acc;
    }
    return out;
}

std::vector<int32_t> tensor_golden_transpose_2d_i32(const std::vector<int32_t>& input,
                                                    uint32_t rows,
                                                    uint32_t cols) {
    if (rows == 0 || cols == 0) {
        throw std::invalid_argument("transpose shape must be non-zero");
    }
    expect_size(input, static_cast<uint64_t>(rows) * cols, "transpose input");
    std::vector<int32_t> out(static_cast<size_t>(rows) * cols, 0);
    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < cols; ++col) {
            out[col * rows + row] = input[row * cols + col];
        }
    }
    return out;
}

std::vector<float> tensor_golden_softmax_rows_f32(const std::vector<float>& input,
                                                  uint32_t rows,
                                                  uint32_t cols) {
    if (rows == 0 || cols == 0) {
        throw std::invalid_argument("softmax shape must be non-zero");
    }
    expect_size(input, static_cast<uint64_t>(rows) * cols, "softmax input");

    std::vector<float> out(input.size(), 0.0f);
    for (uint32_t row = 0; row < rows; ++row) {
        const size_t base = static_cast<size_t>(row) * cols;
        float row_max = input[base];
        for (uint32_t col = 1; col < cols; ++col) {
            row_max = std::max(row_max, input[base + col]);
        }

        float denominator = 0.0f;
        for (uint32_t col = 0; col < cols; ++col) {
            const float value = std::exp(input[base + col] - row_max);
            out[base + col] = value;
            denominator += value;
        }

        if (denominator == 0.0f) {
            throw std::invalid_argument("softmax denominator is zero");
        }
        for (uint32_t col = 0; col < cols; ++col) {
            out[base + col] /= denominator;
        }
    }
    return out;
}
