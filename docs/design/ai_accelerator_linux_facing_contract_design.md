# AI 加速器 Linux-facing 设备契约设计

## 文档定位

本文档记录独立 `MMIO NPU / TPU-like` AI accelerator 面向未来 Linux driver 的最小设备契约。

它回答：

- 当前 host/profile、guest demo、graph package 和前端 tiny model 已经固定了哪些设备事实。
- Linux-facing driver 后续必须消费哪些已有 ABI，而不是另起一套设备协议。
- `PROJECT_EVOLUTION` P1 第一刀为什么选择 `host-facade`，而不是直接声明 Linux driver 已完成。

本文档不记录执行 checklist。当前状态以
[../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
和 [../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 当前活跃计划：
  - [../plan/project_evolution_priority_p1_plan.md](../plan/project_evolution_priority_p1_plan.md)
- 相关设计：
  - [post_wave7_ai_user_tasks_npu_performance_design.md](post_wave7_ai_user_tasks_npu_performance_design.md)
  - [npu_tpu_accelerator_direction_design.md](npu_tpu_accelerator_direction_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)

## 背景与问题

当前 AI accelerator 已经有独立 MMIO 设备、submission / completion queue、DMA
load/store、静态和 bounded-dynamic graph package、guest `ai_accel_demo`、host
`--ai-profile-manifest` 和前端参数化 tiny model。它已经不是只有 host-only demo 的方向。

但这些能力此前主要散落在设备头文件、host smoke、workload packer 和状态文档里。后续如果直接写
Linux driver，很容易出现两类漂移：Linux path 自己定义一套 descriptor / profile 口径，或前端把
模板字段误当成设备 ABI。因此 P1 第一刀先把 Linux-facing 最小合同收成可测试的只读 facade。

## 目标

- 固定未来 Linux driver 的最小设备发现、队列、DMA、IRQ 和 profile 回读合同。
- 让 host CLI、前端 service 和设计文档共享同一条 Linux-facing 口径。
- 明确第一刀只做 contract / host facade，不声明 Linux kernel module、devfs 节点或 ioctl 已实现。
- 保持现有 host profile、guest demo、graph package 和 bounded task-spec guardrail 不变。

## 非目标

- 不实现 Linux kernel driver。
- 不新增真实 `/dev/mycpu-ai0` 节点。
- 不新增真实 ioctl 调用路径。
- 不开放任意 graph package 上传、任意模型上传、ONNX / PyTorch runtime 或浏览器侧 graph interpreter。
- 不改变 guest 可见 execution semantics、MMIO 寄存器语义、completion ABI 或 simulated-cycle profile 口径。

## 约束与边界

- AI accelerator 仍是独立 MMIO 设备，不回退为 CPU 紧耦合向量扩展。
- 共享设备 ABI 事实来源仍是 `ai_submission_queue`、`ai_accelerator`、`Machine::run_ai_profile_manifest()`、
  graph package parser / serializer 和现有 host / guest smoke。
- Linux-facing driver 后续只能包装既有 queue / descriptor / completion / counter / profile
  事实，不能分叉第二套 queue 协议或 completion 语义。
- 当前前端 `linuxFacingContract` 只是只读展示 / API contract summary，不是设备 ABI 的新事实来源。

## 方案

### 当前边界盘点

- **Host smoke API**
  - `./mycpu --ai-profile-manifest <manifest>` 运行受控 graph package profile。
  - `Machine::run_ai_profile_manifest()` 负责 manifest 解析、graph / runtime shape / tensor
    payload 装载、queue submission、completion wait、expected output gate 和 summary readback。
  - `./mycpu --ai-linux-contract` 输出 `schema=ai_linux_contract_v1` 的只读 Linux-facing
    contract summary，作为 P1 第一刀 host facade。

- **MMIO / queue / IRQ**
  - MMIO window：`base=0x10002000`，`size=0x1000`。
  - PLIC source：`9`。
  - Capability：queue、quantized、semi-precision、static graph、profile。
  - Submission descriptor：`48` bytes。
  - Completion entry：`40` bytes。
  - Queue 上限：`1024` entries。
  - IRQ 语义：completion / fault 两类状态，通过 PLIC source 9 进入平台中断路径。

- **Graph package / DMA**
  - graph package 最大尺寸：`1048576` bytes。
  - DMA 仍通过 guest physical contiguous system RAM buffer、input table 和 output table
    进入设备，不引入 host pointer 或 browser-side payload。
  - `pack_graph.py` 和 `task_spec_lowering.py` 继续是受控 workload / task-spec 降低到
    graph package、runtime shape table、memory plan 和 manifest 的事实来源。

- **Profile summary**
  - profile schema：`1`。
  - timing schema：`1`。
  - timing model：`TimedSimpleNoOverlap`。
  - 当前稳定回读字段包括 device / DMA / compute / stall / busy / queue / completion cycles、
    retired ops、bytes moved、tile count、scratchpad peak、per-op summary，以及最近一次
    submission 的 compile / runtime-shape / descriptor / queue snapshot。

- **前端 tiny model**
  - `/api/ai/tiny-model/templates` 暴露白名单模板和 `linuxFacingContract`。
  - 浏览器仍只能提交模板参数或 task-spec-backed preset；服务端重新生成 manifest 后调用
    `mycpu --ai-profile-manifest`。
  - 前端不上传任意 graph package，也不解释设备 ABI。

### Linux-facing 最小 contract

- **Device Tree node**
  - `compatible = "mycpu,ai-accelerator"`
  - `reg = <0x10002000 0x1000>`
  - `interrupts = <9>`
  - 后续如要进入真实 Linux DTB，应先以这三个字段作为最小 discoverable contract。

- **Descriptor / completion**
  - submission descriptor 继续采用现有 `48-byte` layout：
    `token`、`graph_package_addr`、`graph_package_bytes`、`flags`、`input_table_addr`、
    `output_table_addr`、`source_tag`、`runtime_shape_table_offset`。
  - completion entry 继续采用现有 `40-byte` layout：
    `token`、`status`、`fault_code`、`retired_ops`、`bytes_moved`、`source_tag`。
  - `AI_ACCEL_SUBMISSION_FLAG_PROFILE` 继续表示本次 submission 需要 profile readback。

- **DMA buffers**
  - Linux driver 后续应 pin / 分配 guest physical contiguous buffers，再把 graph package、
    input table、output table、runtime shape table 交给设备。
  - 第一版不声明 IOMMU、scatter-gather list、cache-coherent DMA 或 host pointer ABI。

- **IRQ**
  - 设备完成或 fault 后通过 PLIC source 9 通知。
  - driver 可以读取 `irq_status` 并写 `irq_ack`，但不得假设 completion IRQ 会在 guest handler
    读取后继续保持 pending。

- **devfs / ioctl facade**
  - 预留未来 devfs 名称：`/dev/mycpu-ai0`。
  - 预留 ioctl surface：`submit`、`wait`、`read_profile`。
  - 当前状态是 `linux_driver=not-implemented`；这些名字只是后续 driver stub 的 contract
    约束，不是已存在 API。

- **Profile readback**
  - driver 读回的 profile 字段必须来自设备 summary / MMIO counters / completion entry。
  - 文本或 JSON 包装可以变化，但不能制造第二套 timing model、第二套 bytes moved 或第二套
    per-op summary 事实来源。

### 第一刀决断

P1 第一刀选择 `host-facade`：

- 增加 `./mycpu --ai-linux-contract`，输出稳定文本摘要，供 host smoke 锁住。
- 在 frontend AI tiny model service 的 templates 响应中增加 `linuxFacingContract`，供前端只读展示和
  API 合同测试消费。
- 新增本文档，把 DT node、descriptor、DMA buffer、IRQ、devfs / ioctl 预留面和 profile 回读字段写成
  长期边界。

这一步不做 guest driver stub，也不做真实 Linux driver smoke。原因是当前仓库默认不携带真实 Linux
Image / rootfs 资产，且 AI 设备 Linux driver 还需要单独定义 kernel module、DTB 接线、用户态测试程序
和 opt-in 外部门禁；把这些放进本切片会混淆“合同定调”和“driver 实现”两个阶段。

### 验证思路

- Host contract facade：
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
- Frontend API contract：
  - `cd frontend && node --test tests/debug_server.test.mjs`
- AI 相关回归补充：
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accel_guest_smoke`
- 收口检查：
  - `git diff --check`

## 风险与取舍

- 只做 host facade 会延后真实 Linux driver 反馈；后续必须用单独计划补 driver stub / smoke。
- 过早实现 Linux driver 会把 DTB、kernel module、rootfs 资产和 userland smoke 一次性拉进来，
  不利于保持 P1 第一刀可验证。
- 让 frontend 也暴露 contract summary 会带来字段同步成本，因此它必须保持只读摘要，并由
  host smoke / 设计文档 / 状态文档共同约束。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效。
- 当前结果以
  [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  和 [../status/mainline_status.md](../status/mainline_status.md) 为准。
