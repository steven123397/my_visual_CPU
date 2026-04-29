# 主线 Wave 4 / AI accelerator 切片 A：动态 shape 与 workload 实现计划

> **文档状态：** 执行中

## 文档定位

本文档记录主线 `Wave 4` 中 AI accelerator 部分的切片 A 实现计划。它只负责核心前置：把 `bounded dynamic shape` 从当前 `dynamic GEMM / FC-like` 第一刀扩到现有 op family 的受控合同，并补 1 条更像模型的动态 workload。

这里的 `Wave 4` 指 [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md) 中的主线 wave，不是 AI accelerator 局部历史里的 Wave 1 / 2 / 3 后再延续出的局部 Wave 4。

## 关联文档

- 来源设计：
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
- 目标状态：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 把 `bounded dynamic shape` 从单点 `GEMM / FC-like` 扩到现有 op family 的正向或 fail-closed 合同。
- 新增 `ai_proto` 动态 workload，优先 `dynamic_tiny_model`；若不引入新 dtype / op 语义即可完成，再补 `dynamic_cnn_block`。
- 继续保持 host harness 与 guest driver 共用同一套 graph package / descriptor / completion ABI，不新增并行协议。

## 非目标

- 不实现 `Softmax / attention`；它由后续主线 Wave 4 切片 C stretch 计划承接。
- 不实现 `INT4`、训练前向 / 反向、Linux-facing NPU driver 或真实 DMA overlap。
- 不把 CPU `F/D` 浮点扩展作为本计划前置。

## 完成定义

- `conv2d / relu / pool / reduce / layout_transpose` 的 dynamic tensor 组合有明确 positive 或 fail-closed 回归。
- runtime shape 与 memory plan 的 byte-size mismatch、scratchpad overflow、rank / dims 超界、动态 tensor 缺失都有稳定 fault 归因。
- `workloads/ai_proto/pack_graph.py` 新增至少 1 条动态小模型 workload，并由 `make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=<name>` 跑通。
- `test-unit-ai_graph_package`、`test-host-ai_accelerator_cnn_smoke`、`test-host-ai_accelerator_gemm_smoke`、`test-host-ai_accelerator_profile_smoke` 通过。

## 任务

### 任务 1：扩 dynamic shape reject / resolve 矩阵

**文件：**

- 修改：`myCPU/src/devices/ai_graph_package.{h,cpp}`
- 修改：`myCPU/tests/unit/ai_graph_package.cpp`

- [ ] **步骤 1：** 先补 `test-unit-ai_graph_package` 红灯，覆盖非 GEMM dynamic tensor、runtime shape byte mismatch、dynamic metadata 缺失和 scratchpad bound。
- [ ] **步骤 2：** 调整 `resolve_ai_runtime_shape_package()`，确保动态 tensor 的 `dims / byte_size / scratchpad_bytes` 解析后仍重新经过完整 package validator。
- [ ] **步骤 3：** 运行 `cd myCPU && make test-unit-ai_graph_package`，确认新增矩阵通过。

### 任务 2：补设备执行侧 dynamic op family 回归

**文件：**

- 修改：`myCPU/tests/host/ai_accelerator_cnn_smoke.cpp`
- 修改：`myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`
- 视需要修改：`myCPU/src/devices/ai_graph_scheduler.cpp`
- 视需要修改：`myCPU/src/devices/ai_compute_conv.cpp`
- 视需要修改：`myCPU/src/devices/ai_compute_elementwise.cpp`

- [ ] **步骤 1：** 在 host smoke 中构造动态 `conv2d -> relu -> layout_transpose -> reduce_sum` 或等价最小链路，先确认当前失败点。
- [ ] **步骤 2：** 仅修复通用 dynamic package resolve / memory-plan 语义，不为单个 workload 写特判。
- [ ] **步骤 3：** 补 memory-plan mismatch、scratchpad overflow 和 fault 后 profile lifecycle 不漂移回归。
- [ ] **步骤 4：** 运行 `cd myCPU && make test-host-ai_accelerator_cnn_smoke test-host-ai_accelerator_gemm_smoke`。

### 任务 3：新增动态小模型 workload

**文件：**

- 修改：`myCPU/workloads/ai_proto/pack_graph.py`
- 修改：`myCPU/workloads/ai_proto/README.md`
- 修改：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
- 修改：`myCPU/workloads/ai_proto/profile.mk`

- [ ] **步骤 1：** 新增 `dynamic_tiny_model` packer，优先采用 `dynamic GEMM -> relu -> pool`，避免打开新 dtype / op 合同。
- [ ] **步骤 2：** 生成 runtime shape table、input、expected output 与 manifest，并让 `make -n run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_tiny_model` 指向正确 manifest。
- [ ] **步骤 3：** 在 `ai_accelerator_profile_smoke` 中锁住 pack / profile / expected-output / `ai_profile_aggregate` / `ai_profile_op`。
- [ ] **步骤 4：** 运行 `cd myCPU && make test-host-ai_accelerator_profile_smoke` 与 `cd myCPU && make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_tiny_model`。

### 任务 4：阶段收口

**文件：**

- 修改：`docs/status/npu_tpu_accelerator_status.md`
- 修改：`docs/status/mainline_status.md`

- [ ] **步骤 1：** 回写主线 Wave 4 切片 A 完成结果、剩余风险和下一段计划入口。
- [ ] **步骤 2：** 运行 `cd myCPU && make test-unit-ai_graph_package test-host-ai_accelerator_cnn_smoke test-host-ai_accelerator_gemm_smoke test-host-ai_accelerator_profile_smoke`。
- [ ] **步骤 3：** 视触达范围补跑 `cd myCPU && make test`。

## 完成态回写要求

- 全部 checklist 勾完后，在 [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md) 增加主线 Wave 4 切片 A 完成结果。
- 如果本计划完成后继续执行切片 B / 切片 C，不删除本计划；等主线 Wave 4 的 AI accelerator 部分整体完成后统一归档到 [history_plan.md](history_plan.md) 并删除活跃计划文件。
