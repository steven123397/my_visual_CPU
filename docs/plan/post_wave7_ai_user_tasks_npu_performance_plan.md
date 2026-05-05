# Post-Wave 7 用户 AI 任务与 NPU 性能模型计划

> **文档状态：** 执行中

## 文档定位

本文档用于记录 `Post-Wave 7 用户 AI 任务 + NPU 性能模型` 这条新主线如何落地、当前做到哪一步，以及完成后需要如何回写状态文档并归档。

## 关联文档

- 来源设计：
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
- 目标状态：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 把当前白名单 demo / 参数化小模型基线升级为正式的 `用户 AI 任务入口 + 更接近商用 NPU 的性能模型` 新主线。
- 明确第一阶段要先补哪些 contract：用户任务 schema / importer、compile / lower、
  automatic memory plan、以及性能模型分层。

## 完成定义

- 仓库内有正式 design / plan / status / index 入口。
- 当前白名单 baseline、剩余 gap、第一阶段用户任务 contract 和性能模型目标已经文档化。
- 第一刀代码实施的主要文件边界和验证矩阵已经明确，并已开始落第一批 host-side 代码。
- `npu_tpu_accelerator_status.md` 与 `mainline_status.md` 已反映这条新主线已正式启动。

## 任务

### 任务 1：收口 Post-Wave 7 AI 主线边界

**文件：**
- 创建：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/plan/post_wave7_ai_user_tasks_npu_performance_plan.md`
- 修改：
  - `docs/status/npu_tpu_accelerator_status.md`
  - `docs/status/mainline_status.md`
  - `docs/index.md`

- [ ] **步骤 1：** 把“用户自定义 AI 任务”和“更接近商用 NPU 的性能模型”从 `Wave 7`
  白名单展示形态中拆出来，形成正式新主线边界。
- [ ] **步骤 2：** 明确这条线的非目标，例如任意浏览器上传、完整 ONNX / PyTorch runtime、
  训练全栈和 wall-clock 性能承诺。
- [ ] **步骤 3：** 在 AI 专项状态、主线状态和索引中建立正式入口。

### 任务 2：规划用户任务入口与 compile / memory plan 第一刀

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `myCPU/src/devices/ai_graph_package.{h,cpp}`
  - `myCPU/src/devices/ai_graph_scheduler.{h,cpp}`
  - `frontend/server/ai_tiny_model_service.mjs`

- [x] **步骤 1：** 确定第一阶段采用哪种受限入口：公开 graph schema、importer 还是 DSL。
  当前结论：先做 host-side 受限 `task spec importer`，第一刀固定为
  `bounded_dynamic_gemm_v1`，不先开任意模型上传或完整 ONNX/PyTorch runtime。
- [x] **步骤 2：** 设计 compile / lower 到统一 graph package 的最小路径，并保留 fail-closed reject matrix。
  当前结论：`task spec -> dynamic_bounded GEMM graph package + runtime shape table + manifest`
  继续走现有 `--ai-profile-manifest` 路径。
- [x] **步骤 3：** 明确 automatic memory plan 和资源预算如何接到现有 `scratchpad` /
  DMA / compute 路径，而不是另起第二套设备行为。
  当前结论：第一刀只在 importer 内实现顺序 scratchpad 分配 helper，不新建第二套设备行为。

### 任务 2A：实施第一刀 `bounded_dynamic_gemm_v1` task spec importer

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `myCPU/workloads/ai_proto/README.md`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [ ] **步骤 1：** 为 `pack_graph.py` 新增 `--task-spec` 入口，并固定 `ai_task_spec_v1 /
  bounded_dynamic_gemm_v1` contract。
- [ ] **步骤 2：** 在 importer 内自动 lower 到现有 dynamic GEMM graph package、runtime shape
  table、expected output 和 manifest。
- [ ] **步骤 3：** 在 importer 内新增最小 automatic memory plan helper，并用 host smoke 锁住
  正向与 fail-closed 负向合同。
  当前已完成 `task_spec_lowering.py` 共享 host-side lower 模块抽离，`pack_graph.py`
  只保留 CLI 与固定 workload 入口，前端继续复用同一条 host 打包路径。

### 任务 2B：沿同一 importer 路线扩到 `bounded_dynamic_cnn_v1`

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `myCPU/workloads/ai_proto/README.md`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
  - `frontend/server/ai_tiny_model_service.mjs`
  - `frontend/tests/debug_server.test.mjs`

- [x] **步骤 1：** 固定 `ai_task_spec_v1 / bounded_dynamic_cnn_v1` contract，保持
  `3x3/4x4 int8 input + 2x2 int8 kernel + conv2d -> relu -> transpose -> reduce`
  的最窄受限入口。
- [x] **步骤 2：** 在 importer 内自动 lower 到现有 dynamic CNN graph package、
  runtime shape table、expected output 和 manifest，不引入第二套设备 ABI。
- [x] **步骤 3：** 把 automatic memory plan helper 升级为 `16B` 对齐顺序 scratchpad
  分配，并用 host/frontend 最窄 smoke 锁住正向与 fail-closed 合同。

### 任务 3：规划 NPU-like 性能模型第一刀

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `myCPU/src/devices/ai_dma_engine.{h,cpp}`
  - `myCPU/src/devices/ai_accelerator.{h,cpp}`
  - `myCPU/src/devices/ai_submission_queue.{h,cpp}`
  - `myCPU/tests/host/ai_accelerator_*`

- [ ] **步骤 1：** 把当前 `timed-simple no-overlap` 与后续 tile scheduler / overlap /
  multi outstanding queue 的阶段边界写清楚。
- [ ] **步骤 2：** 明确第一刀应该先补哪类 profile / timing 字段，而不是一次性引入完整 timeline。
- [ ] **步骤 3：** 保持 simulated cycles 作为唯一正式性能口径，不把宿主机 wall-clock
  混进设备性能模型。

当前说明：

- 任务 3 仍保留，但不作为这轮第一刀的优先实现对象。
- 本轮优先级已经收窄为“先打开正式用户任务 contract，再在现有 guardrail 不回退的前提下继续推进性能模型”。

### 任务 4：规划系统集成与验证矩阵

**文件：**
- 修改：
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/guest/include/ai_accel.h`
  - `myCPU/guest/kernel/ai_accel.c`
  - `frontend/tests/debug_server*.test.mjs`
  - `myCPU/tests/host/ai_accel_guest_smoke.cpp`

- [ ] **步骤 1：** 明确 host harness、当前 guest demo 和未来 Linux-facing driver 的共享 ABI 边界。
- [ ] **步骤 2：** 规划第一阶段验证矩阵，区分默认门禁、host smoke、guest smoke 和未来 Linux 集成门禁。
- [ ] **步骤 3：** 整条计划完成后，把结果回写到 AI 专项状态文档并归档到 `history_plan.md`。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
