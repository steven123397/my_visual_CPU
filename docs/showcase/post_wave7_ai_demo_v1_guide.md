# Post-Wave 7 AI Demo V1 Guide

## 文档定位

本文档只服务于 `Post-Wave 7 AI demo v1` 的最短演示路径，不承担长期设计或实时状态职责。

长期边界看：

- [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)

实时状态看：

- [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)

## 展示边界

本次 `Demo V1` 只展示 5 条稳定入口：

- `bounded_dynamic_gemm_v1`
- `bounded_dynamic_cnn_v1`
- `bounded_dynamic_tiny_model_v1`
- `static_tiny_attention_v1`
- `guest_ai_accel_demo`

其中默认脚本固定运行 4 条正向样例：

- `guest_ai_accel_demo`
- `custom_dynamic_gemm`
- `custom_dynamic_cnn`
- `custom_dynamic_tiny_model`

并固定验证 1 条 fail-closed 样例：

- `custom_dynamic_gemm_fail_closed.task_spec.json`

`custom_tiny_attention_static` 会被打包，但默认不在脚本里执行，只作为可选第五条正向样例。

## 最短命令

在仓库根执行：

```bash
cd myCPU
python3 workloads/ai_proto/run_demo_v1.py --out-dir workloads/ai_proto/generated/demo_v1
```

如果要单独运行可选 attention 样例：

```bash
cd myCPU
./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/custom_tiny_attention_static.manifest
```

## 预期输出

默认脚本输出里应至少出现这些段落：

- `== demo_v1 pack ==`
- `== guest_ai_accel_demo summary ==`
- `== custom_dynamic_gemm summary ==`
- `== custom_dynamic_cnn summary ==`
- `== custom_dynamic_tiny_model summary ==`
- `== demo_v1 fail-closed ==`
- `demo_v1 summary: packed fixed assets, ran 4 positive samples, and verified 1 fail-closed sample`

推荐观察点：

- `guest_ai_accel_demo`
  看 `name=guest_ai_accel_demo` 与 `ai_profile_op ... opcode=reduce_sum`。
- `custom_dynamic_gemm`
  看 `shape_mode=dynamic_bounded`、`runtime_shapes=t0:2x8,t2:2x4`。
- `custom_dynamic_cnn`
  看 `conv2d / eltwise_relu / layout_transpose / reduce_sum` 四段 itemized summary。
- `custom_dynamic_tiny_model`
  看 `gemm / eltwise_relu / pool_max` 三段 itemized summary。
- fail-closed
  看 host-side 直接报
  `bounded_dynamic_gemm_v1 task spec has unexpected top-level key: unexpected_extra`。

## 最小验证矩阵

- `git diff --check`
- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-host-ai_accelerator_profile_smoke`

## 不要误报

本次 `Demo V1` 不代表：

- 任意模型上传
- 更宽 op family 或通用 compiler
- `DMA + compute overlap`、tile scheduler、multi outstanding queue
- Linux-facing NPU driver
- 大范围 frontend 产品化改造
