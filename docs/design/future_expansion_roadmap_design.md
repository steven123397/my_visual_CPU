# 未来扩展路线图统一设计

## 文档定位

本文档记录的是从当前稳定基线继续往前看的“未来候选路线菜单”，而不是当前主线的即时推进指令。

与当前 design / status 体系的分工如下：

- 当前模块边界、当前实现方式、当前正式 contract
  - 以对应模块 design 文档为准。
- 当前主线状态、当前风险、当前默认下一步
  - 以 `docs/status/` 为准。
- 本文档
  - 只回答“如果未来要扩功能或做升级，可以考虑哪些方向、它们的依赖关系是什么、哪些路线值得作为候选重主线”。

因此，本文档提供的是未来路线菜单与依赖图，不自动覆盖当前 `status` 文档里的默认主线判断。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/xv6_linux_jit_status.md](../status/xv6_linux_jit_status.md)
- 相关设计：
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [minimal_interactive_os_design.md](minimal_interactive_os_design.md)
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [npu_tpu_accelerator_direction_design.md](npu_tpu_accelerator_direction_design.md)
  - [phase4_preparation_design.md](phase4_preparation_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
  - [spike_differential_validation_design.md](spike_differential_validation_design.md)
  - [xv6_linux_jit_mainline_design.md](xv6_linux_jit_mainline_design.md)
- 当前计划：
  - [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。当前 design 文档体系已经把“现在是什么、各模块边界是什么”收口成了一组当前参考资料，因此未来路线图文档不再需要重复承载这些现状说明的细节。

当前主线已经完成或基本完成的基线包括：

- `Phase 1` 冻结：RV64IM reference path、M/S/U 特权级、Sv39、最小平台设备、`kernel_alpha` bring-up
- `Phase 2` 收口：`functional / pipeline` 双后端、验证补洞两轮完成
- `Phase 3` 首轮：最小 predictor、`rename + ROB + LSQ +` 最小 `OoO execute`、decode 边界收窄与当前后续取舍判断
- `Phase 4` 准备：`P4-prep-1`（`bus / memory region` 合同）已完成
- 向量扩展：`V-lite` `V0 ~ V4` + 当前前端教学可视化已接通
- 验证体系：现有 `make test` / `make test-pipeline`、guest 正负回归与 Spike 外部差分

在这个基础上，真正需要回答的问题已经变成：未来如果继续扩功能或做升级，哪些方向值得保留，哪些是当前默认延续线，哪些则应理解为“当目标切换时再启动的候选重主线”。

## 目标

- 给出从近期到远期的候选扩展路线，覆盖 ISA、平台、workload、微架构、系统级跃迁 5 个维度。
- 明确各方向之间的依赖链，避免在前置条件缺失时抢跑。
- 为每个方向提供收益判断、技术风险评估和最小可行切片。
- 与当前 `reference-first`、`workload-driven` 的仓库方法论保持一致。

## 非目标

- 不在本文档中定义具体实施 plan；每个方向启动时仍应单独在 `docs/plan/` 建计划。
- 不改写当前 `Phase 1 ~ 3` 已完成的正式判断。
- 不把本文档理解成“所有方向都必须做完”的路线承诺。
- 不自动覆盖 `docs/status/` 里对“当前下一步”的即时判断。

## 如何使用本文档

当前建议把本文档理解为 3 层：

1. **默认延续线**
   - 与当前主线方法论最连续、当前更值得优先评估的方向。
2. **候选切换线**
   - 一旦未来目标从“继续当前主线”切换到“标准 OS bring-up”或“更重系统模拟”，就值得启动的新重主线。
3. **远期激进线**
   - 工程量大、前置条件多、只有在目标和资源都明确时才应考虑的方向。

## 总体依赖关系

```text
当前基线
  ├─ 默认延续线
  │    ├─ 向量 / ML workload 继续深化
  │    ├─ 独立 `NPU / TPU-like` tensor accelerator 建模
  │    └─ P4-prep-2 / memory observation
  ├─ 候选切换线
  │    └─ 标准 OS bring-up（xv6-riscv）
  └─ 远期激进线
       ├─ 多核 / coherence
       ├─ Linux
       └─ JIT / DBT
```

这张依赖图的重点不是“画完整路线树”，而是强调：当前默认延续线、候选切换线和远期激进线不应混在同一轮里一起推进。

---

## Phase A：ISA 补全与平台成熟

定位：把模拟器从“当前最小可用 + 已可 bring-up”推进到“更标准工具链 / 更标准 guest 环境可用”。这组工作里，有些是默认延续线可直接吸收的准备项，有些则主要服务于标准 OS bring-up 路线。

### A1：RV64A 原子指令扩展

**优先级**：高。它是标准 OS bring-up、多核同步原语和更标准 RISC-V 软件栈的关键前置。

| 维度 | 说明 |
|------|------|
| 范围 | `lr.w/d`、`sc.w/d`、`amo{swap,add,and,or,xor,min,max}[u].{w,d}` |
| reference path | `functional` 后端先提供正确语义 |
| pipeline path | 后续再按 `LSQ` 与提交合同补最小原子语义 |
| 验证 | 先 host asm smoke；更重 contention 场景放到未来多核语境 |
| 收益 | 打开更标准的 `rv64ima` 软件栈与 OS bring-up 路线 |

最小可行切片：先只做 `functional` 后端 + host smoke；`pipeline` 侧语义延后到更明确需求出现时再补。

### A2：F / D 浮点扩展

**优先级**：中。它对 RV64GC、标准数学库和更完整软件栈有价值，但不是当前默认主线或 `xv6-riscv` 的近端硬前置。

| 维度 | 说明 |
|------|------|
| 范围 | `f0 ~ f31`、`fcsr`、基础浮点算术 / 转换 / load/store |
| 当前收益 | 更标准的 RV64GC 兼容性 |
| 主要服务方向 | 更完整工具链、Linux 路线、浮点 workload |
| 风险 | IEEE 754 corner case 很多，验证面明显变宽 |

最小可行切片：先只做 `functional` 路径与最小 host smoke，不把浮点 pipeline 通道一起混入当前微架构主线。

### A3：C 压缩指令扩展

**优先级**：中。它主要提升工具链兼容性和代码密度，但不是当前默认主线的阻塞项。

| 维度 | 说明 |
|------|------|
| 范围 | 16-bit 压缩指令解码与 16 / 32 bit 变长边界 |
| 收益 | 更自然地承接标准 GCC 默认输出 |
| 主要服务方向 | RV64GC / Linux 路线 |
| 风险 | fetch / decode 边界会更复杂 |

最小可行切片：先在 decode 层做 `16-bit -> 32-bit` 的等价展开，继续复用已有执行路径。

### A4：virtio 设备生态

**优先级**：高。它是标准 OS bring-up 路线里最值得保留的平台候选方向。

| 维度 | 说明 |
|------|------|
| 第一刀 | `virtio-blk` |
| 当前价值 | 标准块设备接口，可为 xv6 / Linux 路线提供更真实平台 |
| 与现有平台关系 | UART / CLINT / PLIC 继续保留；virtio 是新增设备层 |
| 风险 | 需要引入 vring、transport 和更标准的中断路由 |

最小可行切片：先实现 `virtio-blk MMIO transport +` 最小 vring + host smoke，不急于同时扩 `virtio-console` 或 `virtio-net`。

### A5：CSR / 特权级补全

**优先级**：中。更适合作为 workload / guest 驱动的按需补全，而不是预先做一轮“大一统补齐”。

| 维度 | 说明 |
|------|------|
| 当前策略 | 按 guest / OS 实际需求补字段和行为 |
| 验证 | 优先复用现有 Spike 差分和最小 host / guest 回归 |
| 适用方向 | xv6 / Linux 路线、更多 privilege contract |

---

## Phase B：Workload 升级

定位：在现有基线之上，引入更高信号的 workload。当前仓库主要有三条候选方向：

- 继续沿向量 / ML workload 线深化
- 继续把 AI workload 向独立 `NPU / TPU-like` 设备方向推进
- 切到标准 OS bring-up 线

### B1：更完整的向量扩展

**优先级**：中高，但当前不应直接理解成“下一步就是完整 RVV 1.0 一口气落地”。

| 维度 | 说明 |
|------|------|
| 当前基线 | `V-lite` + 固定 `conv -> relu` CNN demo + 最小 vector-aware pipeline |
| 更健康的推进方式 | 继续先围绕当前 `V4` 边界做 hardening / observation，再决定是否补更完整的向量语义面 |
| 候选增量 | LMUL、尾部策略、更广向量 load/store、reduction、permutation |
| 风险 | 状态空间和验证面会明显放大 |

建议理解为：这是一条长期候选升级线，但近端仍应先以 `V4` 现有边界为落脚点，而不是立刻跳完整 RVV。

### B2：跑起 `xv6-riscv`

**优先级**：中高。如果未来明确把主线切到“标准 OS bring-up”，它就是最有说服力的中期牵引目标；但它不是当前默认主线的自动下一步。

| 维度 | 说明 |
|------|------|
| 目标 | 启动 `xv6-riscv` 到 shell 提示符 |
| 关键前置 | `RV64A`、`virtio-blk`、按需 CSR / 平台补全 |
| 收益 | 展示价值极高，也是 trap / VM / interrupt / atomic 组合路径的强压力测试 |
| 当前定位 | 候选切换主线，而不是当前默认延续线 |

最小可行切片：先在 `functional` 后端跑通最小适配，再决定是否把这条线升级为正式主线。

### B3：更丰富的 ML workload

**优先级**：中。它与当前默认延续线更连续。

| 维度 | 说明 |
|------|------|
| 当前基线 | 固定 `conv -> relu` 单层 CNN demo |
| 候选增量 | Pooling、FC、多层网络串联、量化推理 |
| 收益 | 为向量 pipeline、后续 memory observation 和教学演示提供更真实 workload 信号 |
| 风险 | 若底层向量边界还不稳，workload 一扩就会把问题一起放大 |

### B4：独立 `NPU / TPU-like` tensor accelerator

**优先级**：中。它直接面向更专用的 AI 加速能力，但不应误读成“当前下一步立刻实现”。

| 维度 | 说明 |
|------|------|
| 当前基线 | 当前已有 `V-lite`、固定 `CNN` demo、`Bus / Device / MMIO` 边界，以及 `P4-prep-1` 的 `memory_region` 合同 |
| 目标 | 形成独立 `MMIO` AI 加速器：静态子图执行器、`scratchpad + DMA`、host / guest 共用 `descriptor / queue / completion` ABI |
| 主要收益 | 更贴近真实 `NPU / TPU` 的结构价值，能同时承接 `CNN` 与 `GEMM / Transformer-like` 推理 workload |
| 主要风险 | 需要更明确的 `DMA-ready` memory contract、图包格式、数值 golden model 与最小 driver / runtime 边界 |
| 当前定位 | 未来候选方向；已经有正式设计边界，但不是当前已激活主线 |

建议理解为：这条线不是替代 `V-lite`，而是把现有 `vector / ML` 语料继续向独立 AI 设备方向推进；如果未来真的启动，应按单独 design / plan / status 执行，而不是混在当前主线里顺手扩大。

---

## Phase C：微架构深化

定位：由 workload 信号驱动，不单独为了“更像高级微架构”而抢跑。

### C1：`P4-prep-2` / memory observation / shadow cache

**优先级**：中高。它是当前最健康的结构性候选切片之一。

| 维度 | 说明 |
|------|------|
| 当前基线 | `P4-prep-1` 已收口 `bus / memory region` 合同 |
| 目标 | 在不改变 architected 语义的前提下，获取 cache 行为数据 |
| 收益 | 为后续是否值得实现真实 cache 提供 workload 证据 |
| 风险 | 如果没有稳定 workload，观测结果价值会偏低 |

### C2：L1 cache 模型

**优先级**：中低。只有在 `C1` 明确给出稳定信号后才值得投入。

| 维度 | 说明 |
|------|------|
| 当前定位 | 候选后继项，而不是当前应该直接做的第一刀 |
| 风险 | 一旦引入真实 cache，就会显著放大一致性、`sfence.vma`、self-modifying code、DMA 等边界 |

### C3：`Phase 3` issue decoupling

**优先级**：低，且是条件触发项。

| 维度 | 说明 |
|------|------|
| 当前判断 | 现有 stall 证据仍主要指向 `memory_path_busy` 和 `source_operands_not_ready` |
| 触发条件 | 真实 workload 出现稳定的 decode 级 load/store 串行化或 replay hotspot |
| 最小切片 | 先做 issue decoupling，而不是直接放宽 unknown-address speculation |

### C4：分支预测器升级

**优先级**：低，且同样是条件触发项。

| 维度 | 说明 |
|------|------|
| 当前基线 | 最小 predictor 已够用 |
| 触发条件 | workload 中出现显著 branch mispredict flush |
| 收益 | 更高的教学可视化价值与更明确的前端观察信号 |

---

## Phase D：系统级跃迁（远期激进方向）

定位：把模拟器从“已可运行的教学原型”继续推向更重的系统模拟器。这些方向工程量大、前置条件多，应只在目标和资源都明确时启动。

### D1：多核 SMP + 缓存一致性

**优先级**：远期。

| 维度 | 说明 |
|------|------|
| 前置 | `RV64A`、更明确的 cache 路线、更多 memory-order 验证能力 |
| 当前建议 | 先做“多 hart + 串行化共享内存”的更窄切片，再评估真实 cache + coherence |
| 风险 | 状态空间爆炸，验证成本极高 |

### D2：跑 Linux 最小配置

**优先级**：远期。

| 维度 | 说明 |
|------|------|
| 前置 | 更完整的 ISA、平台、CSR、设备生态，必要时还要有更高性能执行路径 |
| 当前建议 | 若未来真走这条线，先经由 `xv6-riscv` 形成更健康的中间台阶 |
| 风险 | 合规性要求极高，任何一个 CSR / 中断 / 页表细节都可能导致 panic |

### D3：JIT / 动态二进制翻译

**优先级**：远期。

| 维度 | 说明 |
|------|------|
| 当前价值 | 主要服务更重 guest 与 Linux 路线的性能需求 |
| 前置 | ISA 面、平台 contract 和 guest workload 至少要比现在更稳定 |
| 风险 | 工程量大、平台依赖强、调试和安全性都更复杂 |

---

## 推荐执行方式

### 路径 A：默认延续线（更贴合当前主线）

当前更贴合仓库现有方法论的默认延续线是：

1. 常态维护 + bug-driven hardening
2. 继续围绕 `V4` 现有边界做 observation / 补洞
3. 在有更稳定 workload 之后，再评估 `C1` / `P4-prep-2`
4. 由 workload 证据决定是否值得继续推进更完整的向量语义面、独立 `NPU / TPU-like` tensor accelerator，或更后的 cache 路线

这条线的优点是：与当前 design / status 文档的默认判断最一致，风险也最低。

### 路径 B：候选切换线（标准 OS bring-up）

如果未来项目明确要把主线切到“标准 OS bring-up / 更标准平台生态”，更合理的候选切换线是：

1. `A1`：`RV64A`
2. `A4`：`virtio-blk`
3. `A5`：按需 CSR / 平台补全
4. `B2`：跑起 `xv6-riscv`
5. 视结果再评估 `A2 / A3` 与更远的 Linux 路线

这条线并非不值得做，而是它应作为“切主线”来启动，而不是混在当前默认延续线里顺手推进。

## 并行建议

以下方向可以相对独立并行：

- 常态维护与 `V4` hardening
- `RV64A` 与更克制的前端 / guest bug-driven hardening
- `RV64A` 与 `virtio-blk` 的前期准备

以下方向存在明显串行依赖，不应抢跑：

- `xv6-riscv` 之前不要假设 `RV64A + virtio-blk` 已经可用
- `L1 cache` 之前不要跳过 `memory observation / shadow cache`
- 多核 / coherence 之前不要假设当前 cache 路线和 memory-order 验证已经够用
- Linux 与 JIT 都不适合在 ISA / 平台 contract 仍频繁变化时提前启动

## 风险与取舍

| 风险 | 影响 | 缓解 |
|------|------|------|
| 未来路线菜单被误读成当前默认主线 | 与 `status` 口径冲突 | 明确由 `status` 决定当前下一步，本文件只给候选路线 |
| ISA 补全面过大，分散精力 | reference path 质量下降 | 严格保持先 `functional` 后 `pipeline`；每个扩展独立门控 |
| `xv6-riscv` 适配暴露大量 CSR / 平台缺口 | 工作量不可预测 | 作为候选切换主线单独启动，不混入当前默认延续线 |
| 完整向量语义面扩得过快 | 验证与实现复杂度失控 | 继续先围绕已落地 `V4` 边界做更窄推进 |
| 独立 `NPU / TPU-like` 方向过早实现 | 会把 DMA、graph package、driver ABI 和 profile 一次性放大 | 先把它收口成正式设计，只在优先级真正切换后单开专项 |
| 各方向同时推进导致 reference path 不稳定 | 回归爆炸 | 每轮最多并行 2 ~ 3 个低交叉依赖方向 |

## 约束与边界

- `reference-first` 原则不变：任何 ISA / 平台扩展，都必须先在 `functional` 后端跑通且有门禁，再考虑 `pipeline` 侧。
- 现有 `Phase 1 ~ 3` 判断和冻结边界不因未来路线菜单而自动改变。
- 不要一次性启动所有方向；每次最多并行 2 ~ 3 个无高交叉依赖的方向。
- 每个方向真正启动时，仍应单独写 design / plan / status，而不是直接把本文档当实施文档使用。
- 激进方向（`Phase D`）只有在前置条件充分满足、且有明确目标驱动时才启动。

## 当前有效性说明

- 当前有效：本文档作为从当前稳定基线出发的未来候选路线菜单。
- 当前默认延续线、当前风险和“下一轮最该做什么”，以 [../status/mainline_status.md](../status/mainline_status.md) 与 [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。
- 自 `2026-04-21` 起，标准 OS bring-up 切换线已经被正式激活；当前执行口径见 [xv6_linux_jit_mainline_design.md](xv6_linux_jit_mainline_design.md)、[../status/xv6_linux_jit_status.md](../status/xv6_linux_jit_status.md) 和 [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)。
- 如果后续某条候选路线真正启动，其实时状态应回写到对应 `docs/status/` 文档，而不是在本文档里继续堆实时进度。
