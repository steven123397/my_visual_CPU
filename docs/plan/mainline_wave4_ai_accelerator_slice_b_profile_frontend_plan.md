# 主线 Wave 4 / AI accelerator 切片 B：profile 与 frontend 观察面实现计划

> **文档状态：** 执行中

## 文档定位

本文档记录主线 `Wave 4` 中 AI accelerator 部分的切片 B 实现计划。它在切片 A 动态 shape / workload 基础上，收口 profile / timing attribution，并把 AI accelerator 的只读状态接入浏览器前端观察面。

这里的 `Wave 4` 指 [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md) 中的主线 wave，不是 AI accelerator 局部历史里的 Wave 1 / 2 / 3 后再延续出的局部 Wave 4。

## 关联文档

- 来源设计：
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
- 前置计划：
  - [mainline_wave4_ai_accelerator_slice_a_dynamic_shape_workload_plan.md](mainline_wave4_ai_accelerator_slice_a_dynamic_shape_workload_plan.md)
- 目标状态：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 细化 `timed-simple` profile：让 stall / tile / DMA / scratchpad attribution 能解释 workload 行为。
- 暴露前端可消费的 AI accelerator 只读状态，不让 frontend 成为执行语义来源。
- 给 `guest_ai_accel_demo` 增加 workload presentation，并新增最小 AI accelerator panel。

## 非目标

- 不做 Wave 7 产品化重设计。
- 不把 host-side itemized profile 强行外推成复杂 MMIO ABI；如需进入 debug JSON，先冻结最小只读 schema。
- 不实现真实 DMA overlap、多 outstanding queue 或 cache coherence。

## 完成定义

- debug snapshot / JSON 至少稳定暴露 AI accelerator aggregate counters；如新增 op summary，也必须有后向兼容的只读 schema。
- 前端能在 `guest_ai_accel_demo` 下展示 queue、busy、scratchpad、DMA bytes、device / dma / compute / stall cycles、utilization 等真实字段。
- `frontend` 测试覆盖 workload presentation 和 AI accelerator panel。
- `debug_cli_smoke`、`test-guest-ai_accel_demo`、`test-pipeline-guest-ai_accel_demo`、`frontend node --test` 通过。

## 任务

### 任务 1：profile / timing attribution hardening

**文件：**

- 修改：`myCPU/src/devices/ai_graph_scheduler.{h,cpp}`
- 修改：`myCPU/src/devices/ai_accelerator.{h,cpp}`
- 修改：`myCPU/tests/host/ai_accelerator_cnn_smoke.cpp`
- 修改：`myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`
- 修改：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [ ] **步骤 1：** 先补 smoke，明确当前 `stall_cycles=0` 的弱点和需要稳定的 attribution 输出。
- [ ] **步骤 2：** 在 `timed-simple` 层补最小 stall / wait 归因，例如 memory-plan/tile setup 等可解释等待，不引入真实并行执行。
- [ ] **步骤 3：** 保持 success 更新 summary、fault 不污染上一轮 success、reset 清零的 lifecycle。
- [ ] **步骤 4：** 运行 AI accelerator host 窄门禁。

### 任务 2：debug snapshot schema 收口

**文件：**

- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol_response.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`

- [ ] **步骤 1：** 确定前端需要的最小字段：aggregate counters 必须稳定；op summary 只有在 schema 足够小且有测试时才加入。
- [ ] **步骤 2：** 更新 JSON response 与 debug smoke，锁住 `guest_ai_accel_demo` 完成后的最终可见性。
- [ ] **步骤 3：** 运行 `cd myCPU && make test-host-debug_cli_smoke test-host-ai_accel_guest_smoke`。

### 任务 3：frontend AI accelerator panel

**文件：**

- 修改：`frontend/server/tests_manifest.mjs`
- 修改：`frontend/app/components/panels/workload.js`
- 修改：`frontend/app/components/panels/platform_arch.js`
- 视需要修改：`frontend/app/styles.css`
- 修改：`frontend/tests/render.test.mjs`
- 修改：`frontend/tests/panels.test.mjs`

- [ ] **步骤 1：** 给 `guest_ai_accel_demo` 增加 presentation：badge、expected marker `KMVAI`、workload ops、当前边界说明。
- [ ] **步骤 2：** 在平台或独立 panel 中展示 AI accelerator counters；没有字段时优雅降级。
- [ ] **步骤 3：** 补前端测试，覆盖 AI panel、workload card 和真实字段渲染。
- [ ] **步骤 4：** 运行 `cd frontend && node --test`。

### 任务 4：阶段收口

**文件：**

- 修改：`docs/status/npu_tpu_accelerator_status.md`
- 修改：`docs/status/mainline_status.md`

- [ ] **步骤 1：** 回写主线 Wave 4 切片 B 完成结果和切片 C stretch 是否可启动。
- [ ] **步骤 2：** 运行 `cd myCPU && make test-host-debug_cli_smoke test-host-ai_accel_guest_smoke test-guest-ai_accel_demo test-pipeline-guest-ai_accel_demo`。
- [ ] **步骤 3：** 视触达范围补跑 `cd myCPU && make test-pipeline`。

## 完成态回写要求

- 全部 checklist 勾完后，在专项状态和主线状态记录 profile/frontend 结果。
- 等主线 Wave 4 的 AI accelerator 部分整体完成后，与切片 A / 切片 C 一起归档到 [history_plan.md](history_plan.md)。
