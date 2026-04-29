# 主线 Wave 4 / AI accelerator 切片 C：Softmax 与 tiny attention stretch 实现计划

> **文档状态：** 执行中

## 文档定位

本文档记录主线 `Wave 4` 中 AI accelerator 部分的切片 C stretch 计划。它只有在切片 A 和切片 B 的核心目标已经收口后才启动；失败或延期时，不阻塞主线 Wave 4 的 AI accelerator 核心完成。

这里的 `Wave 4` 指 [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md) 中的主线 wave，不是 AI accelerator 局部历史里的 Wave 1 / 2 / 3 后再延续出的局部 Wave 4。

## 关联文档

- 来源设计：
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
- 前置计划：
  - [mainline_wave4_ai_accelerator_slice_a_dynamic_shape_workload_plan.md](mainline_wave4_ai_accelerator_slice_a_dynamic_shape_workload_plan.md)
  - [mainline_wave4_ai_accelerator_slice_b_profile_frontend_plan.md](mainline_wave4_ai_accelerator_slice_b_profile_frontend_plan.md)
- 目标状态：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)

## 目标

- 新增最小 `Softmax` op，第一刀只支持静态 `fp32 -> fp32` row-wise softmax。
- 新增 `tiny_attention_static` host workload，固定为小规模 `gemm -> softmax -> gemm` 闭环。
- 证明 attention 方向能被当前 graph package / scheduler / profile path 表达，但不承诺完整 Transformer runtime。

## 非目标

- 不支持动态 sequence length、KV-cache、多 head attention 或 causal mask。
- 不支持 INT4、GELU / Sigmoid、MobileNet、训练前向 / 反向。
- 不把 Softmax 直接扩成通用数学库；只为 tiny attention 的最小闭环服务。

## 启动条件

- 切片 A 已完成，`dynamic_tiny_model` 或等价动态 workload 通过。
- 切片 B 已完成，profile/frontend aggregate 观察面稳定。
- `make test-host-ai_accelerator_profile_smoke`、`make test-host-debug_cli_smoke` 和 `cd frontend && node --test` 当前为绿灯。

## 完成定义

- `AiOpCode::Softmax` 进入 graph package parser / validator / serializer，并有非法 dtype / rank / memory-plan reject 测试。
- `tensor_golden_ops` 或 compute path 有 deterministic `fp32` softmax 样本。
- `tiny_attention_static` 通过 host profile，输出使用固定小样本，避免非确定性近似造成字节漂移。
- `ai_profile_op` 能稳定显示 `gemm / softmax / gemm` 的 op summary。

## 任务

### 任务 1：Softmax op 合同

**文件：**

- 修改：`myCPU/src/devices/ai_graph_package.{h,cpp}`
- 修改：`myCPU/src/devices/tensor_golden_ops.{h,cpp}`
- 修改：`myCPU/tests/unit/ai_graph_package.cpp`
- 修改：`myCPU/tests/host/ai_tensor_golden_ops_smoke.cpp`

- [ ] **步骤 1：** 先补 parser / validator 红灯：`Softmax` 只接受 `fp32 -> fp32`、rank 2 或可解释的 row-wise layout。
- [ ] **步骤 2：** 补 deterministic softmax golden 样本，优先使用全 0 或等值 logits，保证 expected output 字节稳定。
- [ ] **步骤 3：** 运行 `cd myCPU && make test-unit-ai_graph_package test-host-ai_tensor_golden_ops_smoke`。

### 任务 2：Softmax compute path

**文件：**

- 修改：`myCPU/src/devices/ai_compute_elementwise.{h,cpp}`
- 修改：`myCPU/src/devices/ai_graph_scheduler.cpp`
- 修改：`myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`

- [ ] **步骤 1：** 构造最小 static softmax graph，先锁住 output、retired_ops、compute_cycles 和 fault 行为。
- [ ] **步骤 2：** 实现 row-wise softmax compute，不改已有 op 的 dtype 合同。
- [ ] **步骤 3：** 运行 `cd myCPU && make test-host-ai_accelerator_gemm_smoke`。

### 任务 3：tiny static attention workload

**文件：**

- 修改：`myCPU/workloads/ai_proto/pack_graph.py`
- 修改：`myCPU/workloads/ai_proto/README.md`
- 修改：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [ ] **步骤 1：** 新增 `tiny_attention_static` packer，固定 `gemm -> softmax -> gemm`。
- [ ] **步骤 2：** 选择等值 logits 或小整数可精确验证样本，保持 expected output 可字节比较。
- [ ] **步骤 3：** 锁住 `ai_profile_aggregate` 和三段 `ai_profile_op`。
- [ ] **步骤 4：** 运行 `cd myCPU && make test-host-ai_accelerator_profile_smoke` 与 `cd myCPU && make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_attention_static`。

### 任务 4：stretch 收口或降级

**文件：**

- 修改：`docs/status/npu_tpu_accelerator_status.md`
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/plan/history_plan.md`

- [ ] **步骤 1：** 如果完成，记录切片 C 作为 stretch 完成结果。
- [ ] **步骤 2：** 如果发现数值或 op 合同扩大过快，停止本计划并把它降级到 AI accelerator 后续专项阶段，不阻塞切片 A / 切片 B 收口。
- [ ] **步骤 3：** 主线 Wave 4 的 AI accelerator 部分整体完成后，把切片 A / B / C 统一归档并删除活跃计划文件。

## 完成态回写要求

- 如果本 stretch 完成，专项状态必须明确它不是完整 attention 支持。
- 如果本 stretch 延期，专项状态必须保留已完成的主线 Wave 4 AI accelerator 核心部分，不把延期写成主线 Wave 4 未完成。
