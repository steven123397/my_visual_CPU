# Post-Wave 7 用户 AI 任务与 NPU 性能模型设计

## 文档定位

本文档记录 `Wave 7` 阶段性收口之后，本地工作区重新打开的
`用户自定义 AI 任务 + 更接近商用 NPU 的性能模型` 新主线的当前有效设计边界。

它回答：

- 为什么当前白名单 demo / 参数化小模型已经不足以承接 AI accelerator 后续主线。
- 后续如何在不破坏现有 `MMIO NPU / TPU-like` 设备路线的前提下，打开用户 AI 任务入口。
- 哪些能力属于这条新主线，哪些仍不应被误写成“已经开放任意模型上传或完整 NPU runtime”。

本文档不记录执行 checklist。具体实施步骤写入 `docs/plan/`，当前状态以
[../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
和 [../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 当前活跃计划：
  - [../plan/post_wave7_ai_user_tasks_npu_performance_plan.md](../plan/post_wave7_ai_user_tasks_npu_performance_plan.md)
- 相关设计：
  - [npu_tpu_accelerator_direction_design.md](npu_tpu_accelerator_direction_design.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [wave7_productization_and_showcase_design.md](wave7_productization_and_showcase_design.md)

## 背景与问题

当前 AI accelerator 方向已经完成了独立 `MMIO` 设备路线、`DMA-ready` memory contract、
静态 graph package、bounded dynamic shape、profile 生命周期、`Softmax + tiny static attention`
stretch，以及 `Wave 7` 前端里的白名单 demo 和参数化小模型体验。

但这套能力仍然建立在一条明确保守的边界上：浏览器只能选择服务器端白名单模板，host 侧重新生成
graph package 和输入，设备时序模型仍是 `timed-simple no-overlap`，没有公开 graph schema、
没有用户自定义 task 入口、没有自动 memory plan、没有 Linux-facing driver，也没有更接近商用
NPU 的 tile scheduler、DMA + compute overlap 或 multi outstanding queue。

因此，`Wave 7` 之后的 AI 主线不应继续被理解为“再扩几个白名单模板”，而应正式切换为
两件更重的系统工作：一是打开受控的用户 AI 任务入口，二是把性能模型从当前教学/演示级
`timed-simple` 推向更可信的 NPU-like simulated performance model。

## 目标

- 在保持现有独立 `MMIO NPU / TPU-like` 路线的前提下，开放受控的用户 AI 任务入口。
- 为 graph schema / importer / lowering / compiler / automatic memory plan 建立正式设计边界。
- 把设备时序模型从当前 `timed-simple no-overlap` 分阶段推进到 tile scheduler、
  DMA + compute overlap、multi outstanding queue、buffer ownership 和更细颗粒度 profile。
- 继续保持 host harness、guest driver 和未来 Linux-facing driver 共享同一套设备 ABI。

## 非目标

- 不在第一刀开放任意浏览器上传任意模型文件。
- 不在第一刀直接接入完整 ONNX / PyTorch runtime。
- 不在第一刀承诺训练前向 / 反向、optimizer、分布式同步或完整 Transformer runtime。
- 不用宿主机 wall-clock 取代 simulated cycles 作为性能比较依据。

## 约束与边界

- AI accelerator 仍然是独立 `MMIO` 设备路线，不回退成 CPU 紧耦合向量扩展附属项。
- 共享 host/guest 设备 ABI 仍是事实来源；未来 Linux-facing driver 也必须消费同一套队列、
  doorbell、completion 和 fault 合同。
- 用户任务入口必须是受控、可验证、fail-closed 的，不接受跳过 host 侧校验的裸设备喂数。
- 当前 `dynamic_tiny_model`、`dynamic_gemm`、`dynamic_cnn` 和 `tiny_attention_static`
  仍是稳定 guardrail，不因新主线启动而退化。
- 更重的时序模型不能反向修改 guest 可见语义；它只能影响 profile / timing 观察和未来性能比较。

## 方案

### 结构设计

这条新主线按 5 层推进：

1. **任务入口层**
   - 把当前白名单模板之上的“用户任务入口”收口成正式 contract。
   - 第一阶段优先考虑受限 graph schema、importer 或 DSL，而不是任意大模型直接上传。

2. **编译与 memory plan 层**
   - 把现有手工/离线 graph package 生成入口推进到可复用的 lowering / compile pipeline。
   - 新增 automatic memory plan、shape / dtype / quantization 校验和 fail-closed reject matrix。

3. **设备执行层**
   - 继续复用 `ai_graph_package`、`ai_submission_queue`、`ai_dma_engine` 和
     `ai_graph_scheduler` 的统一事实来源。
   - 对外暴露的是更宽的任务入口，而不是第二套设备行为。

4. **性能模型层**
   - 从当前 `timed-simple no-overlap` 分阶段推进到 tile scheduler、DMA + compute overlap、
     multi outstanding queue、buffer ownership、per-op timeline 和 utilization 模型。
   - 仍以 simulated cycles 为统一性能口径。

5. **系统集成层**
   - 在 host harness 和前端体验稳定后，再推进 guest runtime 更宽的 submission path、
     更后续 Linux-facing driver，以及更系统化的 workload 资产管理。

### 当前用户任务入口合同

当前已打开的是受限 graph schema / importer / DSL 入口，而不是完整模型上传、
完整编译器或性能模型大切片。现有 `dynamic_tiny_model`、`dynamic_gemm`、`dynamic_cnn`
和 `tiny_attention_static` 继续作为功能与 profile guardrail；importer / lower 必须继续落到现有
统一 graph package、runtime shape table 和 submission ABI，不能引入第二套设备行为。

当前已经收口的用户任务入口是：

- 先在 host-side `pack_graph.py` 打开一个受限 `task spec` 入口。
- `task spec` 当前只支持 `bounded_dynamic_gemm_v1` 这一类最小 matmul-family 用户任务。
- importer 负责把它 lower 到现有 `dynamic_bounded GEMM` graph package。
- automatic memory plan 只作为 importer 内部的顺序 scratchpad 分配 helper 落地，
  还不是独立的通用 allocator 子系统。
- runtime shape table、expected output、manifest 与现有 `--ai-profile-manifest`
  路径继续保持同一套事实来源。
- 当前这套 lower/serialize 逻辑已经抽成 host-side 共享模块 `task_spec_lowering.py`，
  `pack_graph.py` 保留 CLI / 固定 workload 入口，前端仍通过同一条 host 打包路径消费它。

同一路线还支持：

- `task spec` 现在已额外支持 `bounded_dynamic_cnn_v1`。
- 它固定映射到现有 `conv2d -> eltwise_relu -> layout_transpose -> reduce_sum`
  的 bounded dynamic CNN graph package，不新增第二套设备 ABI。
- automatic memory plan 仍然只是 importer 内部 helper，但已经升级为
  `16B` 对齐顺序 scratchpad 分配，以匹配现有 `dynamic_cnn` contract。
- 这一步不是开放任意 CNN graph authoring，而是把“多 op + dependency + runtime shape +
  profile”正式纳入受限用户任务入口。

这意味着当前已经真正打开了“用户定义任务 contract”，但仍明确不开放：

- 任意模型上传
- 完整 ONNX / PyTorch runtime
- 任意 op graph authoring
- 独立 frontend-side graph interpreter
- 新的 guest ABI 或新的设备 descriptor 语义

### 接口 / 数据 / 契约

- **用户任务合同**
  - 需要一套公开但受限的 graph schema / importer / DSL contract。
  - 当前固定为 host-side `task spec` 文件入口，只接受
    `bounded_dynamic_gemm_v1` 与 `bounded_dynamic_cnn_v1`。
  - 用户输入先进入 host 侧校验、compile / lower 和资源预算，再转成统一 graph package。

- **compile / lower 合同**
  - lowering 结果必须继续落到统一 graph package、tensor table、memory plan 和 submission ABI。
  - 非法 shape、超预算 scratchpad、unsupported op / dtype / quantization 继续 fail-closed。
  - 当前 lowering 已覆盖：
    - `bounded_dynamic_gemm_v1 -> dynamic_bounded GEMM package`
    - `bounded_dynamic_cnn_v1 -> dynamic_bounded CNN package`
  - 更宽 op family 的 importer 仍要等后续专项切片逐步打开。

- **性能模型合同**
  - 新增 tile scheduler、overlap、queue depth 和 timeline 观察时，优先通过 host profile /
    manifest / debug 只读字段暴露，不随意扩大 guest ABI。
  - simulated cycles 仍是唯一正式性能口径，不引入“宿主机跑得更快就是设备更强”的表述。
  - 当前不改动 `timed-simple no-overlap`；性能模型仍以后续切片继续推进。

- **系统集成合同**
  - host harness、当前 guest demo、未来 Linux-facing driver 必须共享同一套设备语义。
  - 前端产品入口继续是受控展示面，不成为编译器或设备 ABI 的事实来源。

### 验证思路

- 文档层：
  - `git diff --check`
- 默认回归：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
- AI accelerator 定向门禁：
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accel_guest_smoke`
  - `cd frontend && node --test`

## 风险与取舍

- 如果直接开放任意模型上传而不先定义受限 contract，会把编译、资源限制、安全和验证一次性混在一起。
- 如果过早把 Linux-facing driver 拉进当前用户任务入口，会把 host compile / performance 模型尚未收口的问题转移到系统集成层。
- 如果性能模型改动直接侵入 guest 可见 ABI，会破坏当前已形成的 host/guest 共享合同。
- 同时扩“用户任务入口”和“更真实性能模型”会显著放大工作面，因此必须通过计划文档把阶段切清楚。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效。
- 当前结果以
  [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  和 [../status/mainline_status.md](../status/mainline_status.md) 为准。
