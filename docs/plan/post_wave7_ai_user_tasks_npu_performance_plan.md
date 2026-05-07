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

- [x] **步骤 1：** 把“用户自定义 AI 任务”和“更接近商用 NPU 的性能模型”从 `Wave 7`
  白名单展示形态中拆出来，形成正式新主线边界。
- [x] **步骤 2：** 明确这条线的非目标，例如任意浏览器上传、完整 ONNX / PyTorch runtime、
  训练全栈和 wall-clock 性能承诺。
- [x] **步骤 3：** 在 AI 专项状态、主线状态和索引中建立正式入口。

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

- [x] **步骤 1：** 为 `pack_graph.py` 新增 `--task-spec` 入口，并固定 `ai_task_spec_v1 /
  bounded_dynamic_gemm_v1` contract。
- [x] **步骤 2：** 在 importer 内自动 lower 到现有 dynamic GEMM graph package、runtime shape
  table、expected output 和 manifest。
- [x] **步骤 3：** 在 importer 内新增最小 automatic memory plan helper，并用 host smoke 锁住
  正向与 fail-closed 负向合同。
  当前已完成 `task_spec_lowering.py` 共享 host-side lower 模块抽离，`pack_graph.py`
  只保留 CLI 与固定 workload 入口，前端继续复用同一条 host 打包路径。
  当前 pack / task-spec 路径也会额外导出 `<name>.memory_plan.txt` 可读 sidecar，并由
  `ai_accelerator_profile_smoke` 直接对齐 graph package 的 scratchpad budget 与逐 tensor
  memory-plan entry，避免 compile 资源摘要漂成第二套 layout 来源。
  同一条 bounded dynamic 路径现在也会额外导出 `<name>.resolved_memory_plan.txt`，
  并由 host smoke 继续复用共享 runtime-shape resolve 合同验证真实运行时 byte_size /
  scratchpad_bytes，避免 Python 打包侧漂出第二套 resolved layout 语义。
  `ai_accelerator_profile_smoke` 现也会直接比较 task-spec `custom_dynamic_gemm`
  与内建 `dynamic_gemm` 的 `graph.bin / runtime_shape.bin`，锁住它们继续共用同一套
  lowering / memory-plan 事实来源。

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
  当前同一路径的 `<name>.memory_plan.txt` sidecar 也会一起回显 `dynamic_cnn` /
  `custom_dynamic_cnn` 的 compile 资源布局，并由 host smoke 与 graph package memory-plan
  逐项对齐。
  `ai_accelerator_profile_smoke` 现也会直接比较 task-spec `custom_dynamic_cnn`
  与内建 `dynamic_cnn` 的 `graph.bin / runtime_shape.bin`，锁住它们继续共用同一套
  lowering / memory-plan 事实来源。

### 任务 2C：沿同一 importer 路线扩到 `bounded_dynamic_tiny_model_v1`

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/workloads/ai_proto/task_spec_lowering.py`
  - `myCPU/workloads/ai_proto/README.md`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [x] **步骤 1：** 固定 `ai_task_spec_v1 / bounded_dynamic_tiny_model_v1` contract，保持
  batch=`1/2`、`3` 列 `fp16` input activations 的最窄受限入口。
- [x] **步骤 2：** 在 importer 内自动 lower 到现有 `dynamic_tiny_model` graph package、
  runtime shape table、expected output 和 manifest，不引入第二套设备 ABI。
- [x] **步骤 3：** 用 host smoke 锁住 task-spec 与内建 `dynamic_tiny_model`
  继续共用同一套 lowering / memory-plan / profile-summary 事实来源。
  当前 `dynamic_tiny_model / custom_dynamic_tiny_model` 的 `<name>.memory_plan.txt`
  sidecar 也已纳入同一套 host-side compile/profile contract。

### 任务 2D：沿同一 importer 路线扩到 `static_tiny_attention_v1`

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/workloads/ai_proto/task_spec_lowering.py`
  - `myCPU/workloads/ai_proto/README.md`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [x] **步骤 1：** 固定 `ai_task_spec_v1 / static_tiny_attention_v1` contract，保持
  `value_vector[2]` 的最窄静态 attention-like 入口。
- [x] **步骤 2：** 在 importer 内自动 lower 到现有 `tiny_attention_static`
  graph package、expected output 和 manifest，不引入第二套设备 ABI。
- [x] **步骤 3：** 用 host smoke 锁住 task-spec 与内建 `tiny_attention_static`
  继续共用同一套 graph / memory-plan / profile-summary 事实来源。
  当前 `tiny_attention_static / custom_tiny_attention_static` 的 `<name>.memory_plan.txt`
  sidecar 也已纳入同一套 host-side compile/profile contract。

### 任务 2E：统一 importer 顶层标量字段的 fail-closed 约束

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/workloads/ai_proto/task_spec_lowering.py`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [x] **步骤 1：** 固定现有 task kind 共享的 `source_tag / max_ticks` 标量合同：
  `source_tag` 必须 fit in `uint32`，`max_ticks` 必须为非零 `uint32`，并且这两类字段都不能接受
  JSON `bool` 伪装成整数。
- [x] **步骤 2：** 在 importer 内统一 fail-closed 拒绝负数、零和越界值，不把歧义值延后到
  manifest / device 路径。
- [x] **步骤 3：** 用 host smoke 锁住 GEMM/CNN/dynamic tiny/attention 四条 task-spec
  路径上的负向合同。
  同一轮也把共享 parser 的 envelope 合同显式锁进 host smoke：`task spec` 顶层必须是
  JSON object，`format` 必须是 `ai_task_spec_v1`，`task_kind` 只能是当前已开放入口，
  `name` 也必须是非空字符串。
  同一轮继续把标量/输入 hygiene 收紧到 host-side importer：`source_tag / max_ticks` 不接受
  JSON `bool`，`int8` payload 不接受 JSON `bool`，`bounded_dynamic_tiny_model_v1` 的
  `fp16` 输入与 `static_tiny_attention_v1` 的 `fp32` 输入都必须先通过 representable-range
  与 finite 校验，不能以 Python traceback 形式泄漏实现细节。

### 任务 2F：统一 importer 顶层 schema 的 fail-closed 约束

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/workloads/ai_proto/task_spec_lowering.py`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [x] **步骤 1：** 固定现有 task kind 的顶层 key 白名单，避免 `task spec` 顶层 schema 漂移成
  “未知字段静默忽略”的 fail-open 行为。
- [x] **步骤 2：** 在 importer 内统一 fail-closed 拒绝未知 top-level key，不把拼写错误或未约定
  扩展字段延后到 manifest / device 路径；同一份 parser 也要拒绝重复顶层 key。
- [x] **步骤 3：** 用 host smoke 锁住 GEMM task-spec 路径上的未知 top-level key 和 duplicate
  top-level key 负向合同，作为现有共享 parser 的最小 guardrail。
  同一轮也继续把共享 importer 的补洞范围收窄到了三类明确 fail-closed 合同：
  `source_tag / max_ticks` 不接受 JSON `bool`，`int8` payload 不接受 JSON `bool`，
  `task_spec.name` 必须是安全 basename，不能把产物写出 `--out-dir`。

### 任务 3：规划 NPU-like 性能模型第一刀

**文件：**
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `myCPU/src/devices/ai_dma_engine.{h,cpp}`
  - `myCPU/src/devices/ai_accelerator.{h,cpp}`
  - `myCPU/src/devices/ai_submission_queue.{h,cpp}`
  - `myCPU/tests/host/ai_accelerator_*`

- [x] **步骤 1：** 把当前 `timed-simple no-overlap` 与后续 tile scheduler / overlap /
  multi outstanding queue 的阶段边界写清楚。
- [x] **步骤 2：** 明确第一刀应该先补哪类 profile / timing 字段，而不是一次性引入完整 timeline。
- [x] **步骤 3：** 保持 simulated cycles 作为唯一正式性能口径，不把宿主机 wall-clock
  混进设备性能模型。

当前说明：

- 任务 3 的第一刀已经落到 host-side 可测 contract：
  先把当前 `timed-simple no-overlap` 的阶段边界、最小 timing/outcome 字段与
  manifest/guest readback 一致性收口，再把 overlap / timeline / multi-outstanding queue
  继续后移。
- 当前优先级仍保持收窄：继续在现有 guardrail 不回退的前提下推进性能模型，不把它提前扩大成
  新 guest ABI、shared CLI 或 frontend timeline schema。
- 当前已新增最小性能模型合同第一刀：
  `AiAcceleratorProfileSummary` 固定暴露 `timing_model=TimedSimpleNoOverlap`、
  `scheduler_ops_per_cycle=32`、`scheduler_tile_setup_cycles=1`、
  `allow_dma_compute_overlap=false`、`dma_setup_cycles=2`、
  `dma_bytes_per_cycle=16`，并由 `ai_accelerator_gemm_smoke` /
  `ai_accelerator_cnn_smoke` 锁住成功、fault-stable 与 reset 后默认值。
- 当前同一合同已进一步补上最近一次成功 submission 的 aggregate timing delta：
  `last_submission_device_cycles / dma_cycles / compute_cycles / stall_cycles /
  queue_cycles / completion_cycles / busy_cycles`，继续只停留在 host-side 事实来源，
  不扩大成新的 guest ABI 或 timeline schema。
- 当前同一合同还已补上最近一次 submission 的 outcome 摘要：
  `last_submission_fault / retired_ops / bytes_moved`，让成功与 fault-stable 路径
  都能在 AI host smoke 里按同一份 profile contract 收口。
- 当前同一合同已继续细化到 DMA 子阶段：
  `last_submission_dma_load/store_cycles` 与 `last_submission_dma_load/store_bytes`
  也纳入 host-side profile contract，用来锁住当前 `timed-simple` 模型里的
  DMA 读写拆分，而不提前引入 overlap/timeline。
- 当前同一合同也已补上最近一次 submission 的 compile/runtime-shape 摘要：
  `last_submission_shape_mode / runtime_shape_count / tensor_count /
  memory_plan_entries / dynamic_tensor_count / scratchpad_budget_bytes`。
  `ai_accelerator_gemm_smoke`、`ai_accelerator_cnn_smoke` 与
  `ai_accelerator_profile_smoke` 现在会同时锁住 static / dynamic / fault-stable / reset /
  manifest readback 路径，确保 workload sidecar、runtime-shape resolve 与设备自有
  `profile_summary()` 继续共用同一份 graph-package 事实来源。
- 当前同一合同也已继续补上 graph topology / transfer-plan 摘要：
  `last_submission_op_count / dependency_count / root_op_count / leaf_op_count /
  load_entry_count / store_entry_count`。对应 `ai_accelerator_gemm_smoke`、
  `ai_accelerator_cnn_smoke`、`ai_accel_guest_smoke` 与
  `ai_accelerator_profile_smoke` 现在会把 single-op、multi-op、guest bridge 和 manifest
  readback 四类代表路径一起锁住，继续把 queue/overlap 之前的结构摘要收口成设备自有
  host-side contract。
- 当前同一合同也已继续把 transfer-plan 摘要细化到 planned bytes：
  `last_submission_load_plan_bytes / store_plan_bytes`。对应
  `ai_accelerator_gemm_smoke`、`ai_accelerator_cnn_smoke`、`ai_accel_guest_smoke`
  与 `ai_accelerator_profile_smoke` 现在会把 static、bounded dynamic、guest bridge
  和 manifest readback 四类代表路径一起锁住，并明确区分“graph package 计划搬运字节数”
  与执行结果里的 `dma_load/store_bytes`。
- 当前同一合同也已继续补上 tensor-role breakdown：
  `last_submission_input_tensor_count / output_tensor_count / weight_tensor_count /
  constant_tensor_count / intermediate_tensor_count`。对应
  `ai_accelerator_gemm_smoke`、`ai_accelerator_cnn_smoke`、`ai_accel_guest_smoke`
  与 `ai_accelerator_profile_smoke` 现在会把 static、bounded dynamic、guest bridge
  和 manifest readback 四类代表路径一起锁住，确保 direct device、guest bridge 和
  manifest/profile harness 继续共用同一份 graph package 角色分布事实来源。
- 当前同一合同还已继续补上 queue snapshot：
  `submission_base_snapshot / completion_base_snapshot / queue_depth_snapshot /
  submission_queue_size_snapshot / completion_queue_size_snapshot /
  queue_configured_snapshot`。这组字段继续只记录“最近一次 submission 创建时的 ring 配置与
  pending depth”，不引入 overlap / multi-outstanding queue 语义；direct device / guest
  bridge / manifest readback 三条路径都已纳入 AI smoke。
- 当前同一合同也已把 queue snapshot 继续细化到 ring 游标层：
  `submission_head_snapshot / submission_tail_snapshot /
  completion_head_snapshot / completion_tail_snapshot`。这组字段继续只复述设备开始执行该
  submission 时看到的 queue lifecycle 状态，不把完成后的 MMIO 终态误当成 submission
  创建时的 contract；direct device / guest bridge / manifest readback 三条路径都已纳入 AI smoke。
- 当前同一合同也已继续补上 descriptor header 摘要：
  `last_submission_token / flags / graph_package_bytes / runtime_shape_table_offset /
  runtime_shape_table_addr / source_tag`。对应 `ai_accelerator_gemm_smoke`、`ai_accelerator_cnn_smoke`、
  `ai_accel_guest_smoke` 与 `ai_accelerator_profile_smoke` 现在会把 direct device、
  guest bridge 和 manifest readback 三条路径的真实 descriptor header 合同一起锁住。
  这组字段继续只复述设备已经真实消费过的 submission header 事实，不引入第二套 host-only
  descriptor 口径；direct device / guest bridge / manifest readback 三条路径都已纳入 AI smoke。
- 当前 `ai_accelerator_profile_smoke` 也已补上 manifest 路径的设备侧回读校验：
  代表性 `cnn / gemm / tiny_model / dynamic_gemm / dynamic_tiny_model / dynamic_cnn /
  custom_dynamic_gemm / custom_dynamic_cnn / tiny_attention_static` manifest
  在 `Machine::run_ai_profile_manifest()` 结束后，必须把同一份
  `AiAcceleratorProfileSummary` compile / timing / outcome / queue / descriptor
  合同重新填充出来。
- 当前同一 smoke 还已补上 rerun 刷新合同：
  同一 `Machine` 连续执行不同 manifest 后，后一次 workload 的
  `AiProfileRunResult / profile_summary / completion_count / doorbell_count / last_fault`
  都必须切到最新状态，不允许沿用或累加前一次运行的摘要；最近一次 workload 的
  compile / topology / queue / descriptor 摘要也必须整份刷新到第二次 manifest。
- 当前同一 smoke 还补上了“成功后接 host-side fail-closed 抛错”的 reset 合同：
  如果第二次 manifest 在 parser / runtime-shape resolve 阶段就抛异常，
  设备 `profile_summary / completion_count / doorbell_count / last_fault` 也必须回到默认空状态。
- 当前同一 smoke 还补上了 `max_ticks` 超时合同：
  当 manifest 在设备执行阶段超时返回 `AI_ACCEL_FAULT_TIMEOUT` 时，
  `AiProfileRunResult` 必须带回 timeout 计数器，而设备 `profile_summary` 仍保持空摘要。
- 当前同一 smoke 还补上了 completion-fault 合同：
  当 manifest 在设备执行阶段返回 completion fault 时，
  `AiProfileRunResult` 与设备 `profile_summary` 必须共享同一份失败 submission 摘要，
  并保持空的 aggregate / per-op compute 画像。

### 任务 4：规划系统集成与验证矩阵

**文件：**
- 修改：
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/guest/include/ai_accel.h`
  - `myCPU/guest/kernel/ai_accel.c`
  - `frontend/tests/debug_server*.test.mjs`
  - `myCPU/tests/host/ai_accel_guest_smoke.cpp`

- [x] **步骤 1：** 明确 host harness、当前 guest demo 和未来 Linux-facing driver 的共享 ABI 边界。
- [x] **步骤 2：** 规划第一阶段验证矩阵，区分默认门禁、host smoke、guest smoke 和未来 Linux 集成门禁。
- [ ] **步骤 3：** 整条计划完成后，把结果回写到 AI 专项状态文档并归档到 `history_plan.md`。

当前说明：

- 当前共享 ABI 边界已明确收窄为：
  submission descriptor、submit/completion ring、doorbell、completion/fault status、
  只读 counters，以及同一次 submission 派生出的设备 `profile_summary()`。
  host manifest/profile harness 可以增加文本出口，但不能另起第二套 guest 设备语义。
- 当前第一阶段验证矩阵已明确为：
  默认门禁=`git diff --check` + `test-host-ai_tensor_golden_ops_smoke` + 按触达范围补跑
  `make test-pipeline`；host smoke=`test-host-ai_accelerator_gemm_smoke /
  cnn_smoke / profile_smoke`；guest smoke=`test-host-ai_accel_guest_smoke /
  test-guest-ai_accel_demo / test-pipeline-guest-ai_accel_demo`；Linux 集成门禁继续后移。
- `ai_accel_guest_smoke` 现阶段应先守住最小 guest/host 对齐合同：
  guest `ai_accel_demo` 成功提交后，不只要锁住 `doorbell / completion / counter` ABI，
  还要继续验证设备 `profile_summary()` 会暴露同一份最近一次 submission 的
  timing / outcome / DMA breakdown / per-op 摘要。
- 同一 smoke 当前也要继续锁住 guest reset 生命周期：
  guest `ai_accel_demo` 成功后执行设备 MMIO reset，必须同时清空
  `doorbell / completion / last_fault`、只读计数器和 `profile_summary()`。
- 同一 smoke 现在还要把 guest 生命周期补成完整三段：
  `pre-run 默认态 -> 成功提交 -> reset 默认态`。也就是在 `machine.run()` 之前先显式验证
  `debug_snapshot`、MMIO 只读计数器和 `profile_summary()` 都处于零值 / 空摘要默认态，
  而不是只在成功提交或 reset 之后检查。
- 同一 smoke 现在还要把 guest 直接可见的 MMIO 控制面一并锁住：
  `status / queue_depth / irq_status / irq_mask / last_fault / fault_detail`
  在 pre-run 默认态和 reset 默认态都必须恢复到 `READY-only / queue empty / no IRQ /
  default mask / no fault / zero detail`。
- 同一 smoke 也要锁住成功提交之后 guest 已消费 completion IRQ 的最终控制面：
  当前 `ai_accel_demo` 会先 ack `irq_status`，再在 external post-handler 里把 `irq_mask`
  关到 `0`，所以成功路径应验证
  `READY-only / queue empty / irq_status=0 / irq_mask=0 / no fault / zero detail`，
  而不是假设 completion IRQ 仍停留在设备寄存器里。
- 同一 smoke 还要锁住 queue lifecycle 的 guest-visible ring state：
  pre-run 默认态下 submit/completion queue 的 `size / head / tail` 都必须是 `0`；
  当前单 entry `ai_accel_demo` 成功路径必须稳定落到 `size=1 / head=1 / tail=1`；
  reset 后这组 queue state 再统一清回 `0`。
- 同一 smoke 现在也要把 queue base 地址合同锁住：
  pre-run 默认态和 reset 默认态下 `submit_queue_base / completion_queue_base` 都必须为 `0`；
  成功路径则必须验证这两组 base 已被 guest demo 配成非零、`64B` 对齐且彼此不同的 ring 地址。
- 同一轮也把 guest debug snapshot 的 idle/reset 口径锁进 host smoke：
  成功提交后 `engine_busy=false`、`scratchpad_occupancy_bytes=0`，reset 后 DMA/compute/stall/
  busy/queue/completion 计数器与 `effective_ops_per_cycle / utilization` 也都必须回零。
- 同一轮 guest MMIO counter 口径也继续补齐到
  `doorbell / submission / completion / fault` 四类只读计数器：
  默认态为零，成功路径锁住 `1 / 1 / 1 / 0`，reset 后再次回零。
- 当前 `ai_accelerator_profile_smoke` 也已新增 `guest_ai_accel_demo` host-side 镜像 workload，
  用来在 `--ai-profile-manifest` 路径上继续锁住这条 guest/host bridge，而不必先扩大到
  Linux-facing driver 或 shared CLI。
- 这一步仍然不扩到 Linux-facing driver，也不引入新的 guest descriptor / completion ABI；
  它只是把现有 guest demo 也接回当前 host-side profile contract。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
