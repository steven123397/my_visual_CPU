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

当前 host-side 固定 workload 还额外保留一条 `guest_ai_accel_demo` 镜像路径：

- 它复用 guest `ai_accel_demo` 的最小 `int32 reduce_sum` submission contract。
- 作用不是新增新的用户任务类型，而是把 guest/host 共用的 submission 事实来源也接回
  `--ai-profile-manifest` 路径，方便在 host-only gate 里继续锁住 timing / DMA /
  per-op 摘要。

为了展示窗口前的 `Demo V1` 收口，当前推荐的固定展示矩阵也已经收窄成同一组正式入口：

- `bounded_dynamic_gemm_v1`
- `bounded_dynamic_cnn_v1`
- `bounded_dynamic_tiny_model_v1`
- `static_tiny_attention_v1`
- `guest_ai_accel_demo`

其中：

- 前 4 条是当前真正开放的 task-spec user-task 入口；
- `guest_ai_accel_demo` 不是新的 user-task kind，而是 guest/host bridge workload；
- 推荐演示顺序固定为 `guest_ai_accel_demo -> dynamic_gemm -> dynamic_cnn ->
  dynamic_tiny_model`，`static_tiny_attention_v1` 作为可选第五条正向样例保留；
- fail-closed 观察固定使用一条带未知 top-level key 的
  `bounded_dynamic_gemm_v1` 样例，让 host-side importer 直接拒绝，而不是把错误推迟到
  manifest / device。

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

同一路线当前也已补上一条受限的半精度小模型入口：

- `task spec` 现在还支持 `bounded_dynamic_tiny_model_v1`。
- 它固定映射到现有 `dynamic_tiny_model` 的
  `fp16 gemm -> fp32 relu -> fp32 pool_max` bounded dynamic graph package，
  只开放 batch=`1/2`、`3` 列输入 activations；权重、op family、runtime shape table 与
  memory plan 继续复用现有 host-side baseline。
- 这一步的意义仍然是把“受限用户任务入口”逐步扩到现有 guardrail 已覆盖的 workload family，
  而不是开放任意 FP16 小模型 authoring、任意 pooling 参数或第二套设备 ABI。

同一路线当前也已补上一条最窄的静态 attention-like 入口：

- `task spec` 现在还支持 `static_tiny_attention_v1`。
- 它固定映射到现有 `tiny_attention_static` 的
  `fp16 gemm -> fp32 softmax -> fp32 gemm` 静态 graph package，只开放
  `value_vector[2]` 这一组最小输入面；query/logits 路径、权重、memory plan 与 profile contract
  继续复用既有 stretch baseline。
- 这一步仍然不代表任意 attention authoring、动态 sequence length、KV-cache、多头注意力或
  Transformer runtime 已经打开；它只是把现有静态 attention-like guardrail 也纳入统一的
  host-side task-spec importer 路线。

这意味着当前已经真正打开了“用户定义任务 contract”，但仍明确不开放：

- 任意模型上传
- 完整 ONNX / PyTorch runtime
- 任意 op graph authoring
- 独立 frontend-side graph interpreter
- 新的 guest ABI 或新的设备 descriptor 语义

同样地，`Demo V1` 的完成定义也只到“固定样例可复现、固定 fail-closed 可观察”：

- 可以把 `task spec -> pack -> run -> summary` 走通成固定入口；
- 可以稳定展示 4 条推荐正向样例和 1 条 fail-closed 样例；
- 可以用 `guest_ai_accel_demo` 证明 guest/host bridge 仍然和 host profile contract 同源。

它不代表任意 task authoring、广义 NPU runtime、任意 attention family、动态 sequence、
KV-cache、multi-head attention、Linux-facing driver 或更真实的 overlap scheduler 已完成。

### 接口 / 数据 / 契约

- **用户任务合同**
  - 需要一套公开但受限的 graph schema / importer / DSL contract。
  - 当前固定为 host-side `task spec` 文件入口，只接受
    `bounded_dynamic_gemm_v1`、`bounded_dynamic_cnn_v1` 与
    `bounded_dynamic_tiny_model_v1`、`static_tiny_attention_v1`。
  - 用户输入先进入 host 侧校验、compile / lower 和资源预算，再转成统一 graph package。
  - 除 workload-specific shape / payload 约束外，当前所有 task kind 还统一要求
    `source_tag` 必须可表示为 `uint32`，`max_ticks` 必须落在 `[1, 4294967295]`；
    不允许把负数、零、`bool` 或超出 `uint32` 的值延后到 manifest / device 路径再处理。
  - 当前 `task spec` envelope 也固定 fail-closed：顶层必须是 JSON object，
    `format` 必须是 `ai_task_spec_v1`，`task_kind` 只能是当前已开放的受限入口，
    `name` 也必须先通过非空字符串校验，不能靠缺字段或空字符串绕过 host-side parser。
  - 当前 importer 也会对每个 task kind 的顶层 key 集合做白名单校验；
    未声明字段与重复顶层 key 都必须在 host-side 直接 fail-closed，不能被静默忽略或覆盖后继续 lower。
  - `task spec.name` 当前也必须是安全 basename，不能包含路径分隔符或 `.` / `..` 这类路径逃逸成分；
    host-side 打包产物不允许被写出 `--out-dir`。

- **compile / lower 合同**
  - lowering 结果必须继续落到统一 graph package、tensor table、memory plan 和 submission ABI。
  - 当前 host-side pack / importer 还会额外导出 `<name>.memory_plan.txt` 可读 sidecar，
    用同一份 graph package memory-plan 事实来源回显 `shape_mode`、scratchpad budget、
    tensor 数量和逐 tensor layout；它只用于 host smoke / compile-profile contract 校验，
    不是新的设备 ABI 或第二套 layout 来源。
  - 对 bounded dynamic workload，当前 host-side pack / importer 还会额外导出
    `<name>.resolved_memory_plan.txt`，把共享 runtime-shape resolve 之后的真实
    tensor byte_size / scratchpad_bytes 也收成可读 sidecar；它同样必须继续和共享
    graph-package + runtime-shape 合同共源，不能在 Python 打包侧发明第二套运行时 layout 语义。
  - 非法 shape、超预算 scratchpad、unsupported op / dtype / quantization 继续 fail-closed；
    当前 `int8` payload 也显式拒绝 JSON `bool` 伪装成整数。
  - 当前受限浮点输入面也继续 fail-closed：`bounded_dynamic_tiny_model_v1` 的
    `fp16` activations 必须可表示为 `fp16`，`static_tiny_attention_v1` 的
    `value_vector` 必须可表示为 `fp32`，而且这两类浮点输入都必须是 finite；
    不允许把宿主侧打包阶段的 overflow / non-finite 值留成 Python traceback。
  - task-spec 顶层标量字段也继续 fail-closed：当前 importer 会在 host-side 直接拒绝
    非 `uint32` 的 `source_tag`、`bool` 标量与零/越界 `max_ticks`，避免把歧义值写进 manifest /
    submission ABI 再依赖后续路径兜底。
  - task-spec 顶层 schema 同样继续 fail-closed：当前 importer 会直接拒绝未知 top-level key
    与重复顶层 key，避免拼写错误、未约定扩展字段或 JSON 重复键在 host-side 被静默吞掉或后值覆盖前值。
  - 当前 lowering 已覆盖：
    - `bounded_dynamic_gemm_v1 -> dynamic_bounded GEMM package`
    - `bounded_dynamic_cnn_v1 -> dynamic_bounded CNN package`
    - `bounded_dynamic_tiny_model_v1 -> dynamic_tiny_model package`
    - `static_tiny_attention_v1 -> tiny_attention_static package`
  - 对当前两个 bounded task kind，task-spec importer 生成的 `graph.bin` 与
    `runtime_shape.bin` 也应继续与现有 `dynamic_gemm / dynamic_cnn` 的同尺寸基线保持一致；
    允许变化的是 `name / source_tag / input/output payload`，而不是第二套 lowering /
    memory-plan 事实来源。
  - 更宽 op family 的 importer 仍要等后续专项切片逐步打开。

- **性能模型合同**
  - 新增 tile scheduler、overlap、queue depth 和 timeline 观察时，优先通过 host profile /
    manifest / debug 只读字段暴露，不随意扩大 guest ABI。
  - simulated cycles 仍是唯一正式性能口径，不引入“宿主机跑得更快就是设备更强”的表述。
  - 当前第一刀先把 `timed-simple no-overlap` 固定成稳定合同：
    `AiAcceleratorProfileSummary` 必须暴露 `timing_model=TimedSimpleNoOverlap`、
    `scheduler_ops_per_cycle=32`、`scheduler_tile_setup_cycles=1`、
    `allow_dma_compute_overlap=false`、`dma_setup_cycles=2` 与
    `dma_bytes_per_cycle=16`。
  - 同一份 `AiAcceleratorProfileSummary` 还应暴露最近一次成功 submission 的
    `queue / dma / compute / stall / completion / busy` aggregate timing delta，
    让 host-side smoke 能直接验证阶段边界，而不必先穿过共享 CLI 或更宽前端入口。
  - 同一合同还应保留最近一次 submission 的 outcome 摘要：
    `fault / retired_ops / bytes_moved`。这样 host-side smoke 可以同时锁住
    “阶段画像”和“本次完成结果”，而不需要把这类收口强行扩大成新的 guest ABI。
  - 同一份 `AiAcceleratorProfileSummary` 现在也应携带最近一次真正进入设备 submission
    contract 的 compile/runtime-shape 摘要：
    `shape_mode / runtime_shape_count / tensor_count / memory_plan_entries /
    dynamic_tensor_count / scratchpad_budget_bytes`。对 bounded dynamic workload，
    这里要保留“原始 graph package 仍是 dynamic_bounded，本次 submission 带了多少条
    runtime-shape entry”的事实，而不是被 resolved package 漂成另一套 host-only 口径。
    同一组摘要也可以继续补成 tensor-role breakdown，例如
    `input_tensor_count / output_tensor_count / weight_tensor_count /
    constant_tensor_count / intermediate_tensor_count`，把最近一次 submission 的
    graph package 角色分布收成设备自有 host-side contract，而不是让 direct device、
    guest bridge 和 manifest/profile harness 各自手抄一套角色猜测。
  - 同一份合同还可以继续保留不改变执行语义的 graph topology / transfer-plan 摘要：
    `op_count / dependency_count / root_op_count / leaf_op_count / dependency_depth /
    max_fanin / max_fanout / load_entry_count / store_entry_count`。这类字段的作用是把
    “本次 submission 究竟是单 op 还是多 op、dependency 链有多深、图结构扇入扇出是否发生变化、是否有
    dependency 链、系统 RAM <-> scratchpad 传输了几类 tensor”收成 host-side 可测事实来源，
    为后续 queue / overlap-ready staged metadata 继续留窄边界。
    如果还要把 transfer-plan contract 再细一层，也可以继续补
    `load_plan_bytes / store_plan_bytes`，把最近一次 submission 计划从系统 RAM 搬入 /
    搬出的 tensor byte budget 一起收成设备自有 host-side contract，同时继续和运行后
    `dma_load/store_bytes` 区分开：前者是 graph package 级计划摘要，后者是本次执行后的结果摘要。
    同一条 compile contract 也可以继续补成 memory-plan 总量摘要，例如
    `memory_plan_total_bytes / memory_plan_total_scratchpad_bytes /
    memory_plan_scratchpad_span_bytes`，让 direct device、guest bridge 与
    manifest/profile 路径都能直接读取最近一次有效 submission 的总 layout budget，
    而不必在各自 harness 里重复手算 memory-plan 汇总；其中
    `scratchpad_span_bytes` 明确表示 memory-plan 里的最大
    `scratchpad_offset + scratchpad_bytes`，故意与简单求和的
    `memory_plan_total_scratchpad_bytes`、以及运行期聚合的 `scratchpad_peak_bytes`
    区分开。
  - 如果还要继续往 queue-ready 方向收窄，可以优先补最近一次 submission 创建时的
    queue snapshot，例如 `submission_base / completion_base / queue_depth /
    submission_queue_size / completion_queue_size / queue_configured`。这类字段只回答
    “设备当时拿到的是哪种 ring 配置”，不回答 overlap、outstanding queue 调度或 timeline
    隐藏。
  - 如果还要把 queue-ready staged metadata 再细化一层，也可以继续补
    `submission_head / submission_tail / completion_head / completion_tail` snapshot。
    这类字段同样只复述设备在开始执行该 submission 时看到的 ring 游标状态，
    用来让 direct device、guest bridge 和 manifest/profile 路径共享同一份 queue lifecycle
    事实来源，而不是把完成后的 MMIO 终态误当成 submission 创建时的 contract。
  - 如果还要继续往 descriptor-ready 方向收窄，也可以优先补最近一次 submission 的
    descriptor header 摘要，例如 `token / flags / graph_package_bytes /
    runtime_shape_table_offset / runtime_shape_table_bytes / runtime_shape_table_addr /
    source_tag`，以及 `input_table_addr / output_table_addr` 对应的
    table access span bytes。这类字段只重复设备已经真实消费过的
    submission header 事实，
    用来让 direct device、guest bridge 和 manifest/profile 路径继续共用同一份
    device-owned host-side contract，而不是在 host harness 再发明第二套 descriptor 文本口径。
  - 如果当前切片还要继续细化 `timed-simple`，优先补最近一次 submission 的
    `dma_load/store cycles` 与 `dma_load/store bytes` 细分画像，而不是直接跨进 overlap
    timeline 或更宽 guest ABI。
  - 这组字段除了被直接设备 smoke 校验，也应能在
    `Machine::run_ai_profile_manifest()` 完成后由设备自有 `profile_summary()` 原样读回，
    这样 manifest/profile 路径就能复用同一份完整的 compile / timing / outcome /
    queue / descriptor host-side contract，而不必先扩 shared CLI 或 frontend 入口。
  - 这组 compile/runtime-shape 摘要继续只作为 host-side / manifest-side 可读合同，
    不新增 guest MMIO 字段，也不取代 `<name>.memory_plan.txt` /
    `<name>.resolved_memory_plan.txt` 两层 sidecar；三者必须继续共用同一份 graph package +
    runtime-shape resolve 事实来源。
  - 这条 manifest/profile readback 一致性至少要继续覆盖当前稳定 guardrail：
    `cnn`、`gemm`、`tiny_model`、`dynamic_gemm`、`dynamic_tiny_model`、`dynamic_cnn`、
    `custom_dynamic_gemm`、`custom_dynamic_cnn` 与 `tiny_attention_static`。
  - 同一 `Machine` 上连续跑不同 manifest 时，这份 contract 也必须被后一次 workload
    完整刷新；不能把 counters、per-op summary 或 doorbell/completion 状态按历史运行累加成
    伪结果。
  - 如果后一次 manifest 在 host 解析 / runtime-shape resolve 阶段就 fail-closed 抛错，
    由于 `run_ai_profile_manifest()` 已先发 reset，这份 contract 也必须回到默认空状态，
    不能继续保留上一轮成功摘要冒充“最新设备状态”。
  - 如果 manifest 因 `max_ticks` 太小在设备执行期超时，host summary 应返回
    `progress=timeout / fault_code=AI_ACCEL_FAULT_TIMEOUT`；设备 `profile_summary()` 不应
    伪造一次完成态 timing/outcome / per-op profile，但对于已经被设备接受并进入
    `active_submission` 的那次 submission，compile / descriptor / queue snapshot contract
    仍应保留最近一次 accepted submission 的事实来源。
  - 如果 manifest 走到了设备执行期并以 completion fault 结束，host summary 与设备
    `profile_summary()` 必须共享同一份失败 submission 摘要：fault code、DMA/queue/completion
    计数器要如实落下；同一次 accepted submission 的 compile / descriptor contract 也要
    保留下来，但 `tile_count / scratchpad_peak_bytes / op summaries` 仍保持空，不伪造成功
    compute 画像。
  - 这一步仍不改动执行语义；它只把现有保守模型的关键参数收口成 host-side 可测事实来源，
    供后续 tile scheduler / overlap / queue depth 切片在不破坏现有基线的前提下逐步展开。

- **系统集成合同**
  - host harness、当前 guest demo、未来 Linux-facing driver 必须共享同一套设备语义。
  - 当前已明确的共享 ABI 边界仍然只包括：
    submission descriptor、submit/completion ring、doorbell、completion/fault status、
    只读计数器，以及由同一次 submission 派生出来的设备 `profile_summary()`。
  - host manifest/profile harness 可以额外暴露文本 summary，但它只能读取并组织这套既有设备事实，
    不能引入第二套 guest completion 语义、第二套 queue 协议或独立 frontend-side 设备解释器。
  - 当前 guest `ai_accel_demo` 至少要继续与 host-side profile contract 对齐：guest 提交完成后，
    `debug_snapshot` 计数器与设备 `profile_summary()` 的 timing / outcome / DMA breakdown /
    per-op 摘要必须指向同一份最近一次 submission 事实来源，而不是只在 host manifest
    路径上成立。
  - 同一条 guest/host 合同也必须覆盖最小 reset 生命周期：guest `ai_accel_demo` 成功提交后，
    设备 MMIO reset 必须把 `doorbell / completion / last_fault`、只读计数器和
    `profile_summary()` 一起清回默认空状态，不能残留上一轮 guest submission 摘要。
  - 同一条 guest/host 合同在提交前也必须显式守住默认空状态：
    guest `ai_accel_demo` 开始运行前，设备 `debug_snapshot`、只读计数器和
    `profile_summary()` 都必须已经处于零值 / 空摘要默认态，不能依赖“第一次成功提交后再
    倒推设备初始状态正确”。
  - 这条默认态 / reset 合同还应继续覆盖 guest 直接可见的 MMIO 控制面：
    `status`、`queue_depth`、`irq_status`、`irq_mask`、`last_fault` 与 `fault_detail`
    在 pre-run 默认态和 reset 默认态都必须回到 `READY-only / queue empty / no IRQ /
    default mask / no fault / zero detail`，不能只通过 debug snapshot 间接推断。
  - 成功提交后的 guest 可见控制面合同也要按 guest runtime 的真实消费路径锁住：
    当前 `ai_accel_demo` 会在 completion IRQ 到达后读取并 ack `irq_status`，随后在
    external post-handler 里把 `irq_mask` 关到 `0`。因此 host smoke 需要验证的是
    “successful completion 之后控制面回到 `READY-only / queue empty / irq_status=0 /
    irq_mask=0 / no fault / zero detail`”，而不是假设 completion IRQ 仍然挂在设备上。
  - 同一条 guest lifecycle 合同还应继续覆盖 submit/completion queue 的 guest-visible
    ring state：pre-run 默认态下 `submit_queue_size / head / tail` 与
    `completion_queue_size / head / tail` 都必须为 `0`；成功提交后，当前单 entry
    `ai_accel_demo` 路径应稳定落到 `size=1 / head=1 / tail=1`；reset 后再统一清回 `0`。
  - queue 配置本身的 guest-visible base 地址也应纳入同一合同：
    pre-run 默认态和 reset 默认态下 `submit_queue_base / completion_queue_base` 必须为 `0`；
    当前 `ai_accel_demo` 成功路径则必须把这两组 base 配成非零、`64B` 对齐且彼此不同的 ring
    缓冲地址，而不是只通过 `size / head / tail` 间接推断 queue setup 已完成。
  - 这条 guest reset 合同也应覆盖 debug snapshot 只读观察面：
    `engine_busy`、`scratchpad_occupancy_bytes`、DMA/compute/stall/busy/queue/completion
    计数器、`effective_ops_per_cycle` 与 `utilization` 在 guest demo 成功后必须回到 idle，
    reset 后也必须回到零值默认态。
  - 同一轮 guest lifecycle guardrail 也应把 `submission_count / fault_count` 与既有
    `doorbell / completion` 一起纳入 MMIO 只读计数器合同：默认态为零，成功路径按当前
    `ai_accel_demo` submission 精确落值，reset 后再次回零。
  - 为了把这条 guest/host bridge 保持在 host-only 可回归范围内，`ai_proto` 现在还维护一个
    `guest_ai_accel_demo` 镜像 workload：它在 `--ai-profile-manifest` 路径上复用和 guest
    demo 同一类 `int32 reduce_sum` submission contract，并锁住相同的 timing / DMA /
    per-op 摘要。
  - 未来 Linux-facing driver 也必须继续消费这同一套 queue / completion / counter /
    profile-summary 设备事实；它可以增加更系统的 driver/runtime 包装，但不能分叉设备 ABI。
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
- 第一阶段验证矩阵：
  - 默认门禁：
    `git diff --check`、`cd myCPU && make test-host-ai_tensor_golden_ops_smoke`，以及按
    `myCPU/AGENTS.md` 在触及 `tests/host/*` / `src/devices/*` 时补跑
    `cd myCPU && make test-pipeline`。
  - host smoke：
    `test-host-ai_accelerator_gemm_smoke`、`test-host-ai_accelerator_cnn_smoke`、
    `test-host-ai_accelerator_profile_smoke` 负责 timing/outcome/DMA breakdown、
    manifest lifecycle、task-spec lowering guardrail 和代表性 workload consistency。
  - guest smoke：
    `test-host-ai_accel_guest_smoke`、`test-guest-ai_accel_demo`、
    `test-pipeline-guest-ai_accel_demo` 负责 guest queue/completion/counter ABI，以及 guest
    demo 与设备 `profile_summary()` 的同源 submission contract。
  - 未来 Linux 集成门禁：
    当前仍未启动；等 Linux-facing driver 真正打开后，再单独补更宽的 integration gate，
    不提前混入这一阶段的 host/guest smoke。

### Demo V1 展示路径

展示窗口前，当前固定的 `Demo V1` 入口是：

- `python3 workloads/ai_proto/pack_graph.py --demo-v1 --out-dir workloads/ai_proto/generated/demo_v1`
- `python3 workloads/ai_proto/run_demo_v1.py --out-dir workloads/ai_proto/generated/demo_v1`

其中 `run_demo_v1.py` 只固定做三件事：

1. 先生成 `Demo V1` 展示资产；
2. 再运行 4 条推荐正向样例：
   `guest_ai_accel_demo`、`custom_dynamic_gemm`、`custom_dynamic_cnn`、
   `custom_dynamic_tiny_model`；
3. 最后验证 1 条 fail-closed 样例：
   `custom_dynamic_gemm_fail_closed.task_spec.json`。

可选第五条正向样例 `custom_tiny_attention_static` 保留为单独 manifest 运行项，而不是默认脚本
步骤，以避免把 stretch workload 误表述成当前演示闭环的必要前提。

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
