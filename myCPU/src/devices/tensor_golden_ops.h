#pragma once

#include <cstdint>
#include <vector>

#include "ai_graph_package.h"

float tensor_golden_decode_fp16(uint16_t bits);
float tensor_golden_decode_bf16(uint16_t bits);
uint16_t tensor_golden_encode_fp16(float value);
uint16_t tensor_golden_encode_bf16(float value);

std::vector<int32_t> tensor_golden_gemm_i8_to_i32(
    const std::vector<int8_t>& lhs,
    const std::vector<int8_t>& rhs,
    uint32_t m,
    uint32_t k,
    uint32_t n);
std::vector<int32_t> tensor_golden_gemm_i16_to_i32(
    const std::vector<int16_t>& lhs,
    const std::vector<int16_t>& rhs,
    uint32_t m,
    uint32_t k,
    uint32_t n);
std::vector<float> tensor_golden_gemm_fp16_to_fp32(
    const std::vector<uint16_t>& lhs,
    const std::vector<uint16_t>& rhs,
    uint32_t m,
    uint32_t k,
    uint32_t n);
std::vector<float> tensor_golden_gemm_bf16_to_fp32(
    const std::vector<uint16_t>& lhs,
    const std::vector<uint16_t>& rhs,
    uint32_t m,
    uint32_t k,
    uint32_t n);

std::vector<int32_t> tensor_golden_conv2d_valid_i8_to_i32(
    const std::vector<int8_t>& input,
    uint32_t input_h,
    uint32_t input_w,
    const std::vector<int8_t>& kernel,
    uint32_t kernel_h,
    uint32_t kernel_w);
std::vector<int32_t> tensor_golden_conv2d_valid_i16_to_i32(
    const std::vector<int16_t>& input,
    uint32_t input_h,
    uint32_t input_w,
    const std::vector<int16_t>& kernel,
    uint32_t kernel_h,
    uint32_t kernel_w);

std::vector<int32_t> tensor_golden_relu_i32(const std::vector<int32_t>& input);
std::vector<float> tensor_golden_max_pool_2d_f32(
    const std::vector<float>& input,
    uint32_t input_h,
    uint32_t input_w,
    uint32_t window_h,
    uint32_t window_w,
    uint32_t stride_h,
    uint32_t stride_w);
std::vector<int32_t> tensor_golden_reduce_sum_rows_i32(
    const std::vector<int32_t>& input,
    uint32_t rows,
    uint32_t cols);
std::vector<int32_t> tensor_golden_transpose_2d_i32(
    const std::vector<int32_t>& input,
    uint32_t rows,
    uint32_t cols);

