# 当前项目优先级路线图

## 文档定位

本文档只保留当前仍然开放的优先级判断，不重复记录已经完成的整段执行过程。

它不替代 [mainline_status.md](mainline_status.md)。`mainline_status` 负责回答“现在主线是什么状态”，本文档负责回答“下一轮最值得做什么，以及什么不该抢跑”。

## 关联文档

- 相关设计：
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [../design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [xv6_linux_jit_status.md](xv6_linux_jit_status.md)
- 当前计划：
  - [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)
- 已完成计划归档：
  - [../plan/history_plan.md#phase4-prep1-bus-memory-region-plan](../plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)
  - [../plan/history_plan.md#vector-frontend-visualization-plan](../plan/history_plan.md#vector-frontend-visualization-plan)
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)

## 当前判断

- `P0` correctness 修补、`P1` 结构收口和 `P2` 首轮验证补洞都已经完成；自 `2026-04-21` 起，当前已经不再停留在“继续评估要不要切主线”，而是正式激活了标准 OS bring-up 切换线。
- 当前主线的优先级判断已经改为：先落 `RV64A + virtio + CSR / privilege 补全 + xv6-riscv workload harness / bring-up`，并让这轮结构决策直接服务后续 `Linux` 与 `JIT / DBT`。
- 默认延续线没有被丢弃：`V4`、`P4-prep-1`、`kernel_alpha`、`debug/frontend` 与既有回归矩阵继续作为当前主线的 guardrail、观测基础与回归支架。
- `future_expansion_roadmap_design.md` 仍然是路线菜单；当前真正已经激活的执行方案，以 [xv6_linux_jit_status.md](xv6_linux_jit_status.md)、[mainline_status.md](mainline_status.md) 和 [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md) 为准。
- `2026-04-22` 第一轮 A / B / C / D foundation 已按 ownership 整合进主工作树，且首轮 post-integration correctness findings 已关闭；因此“下一轮最值得做什么”的答案继续稳定在先做 B 类平台 follow-up，再推进 C 类 `xv6` board profile / bring-up。

## 当前优先级

### 1. 常态维护仍是默认前提

- 继续维护 reference correctness 矩阵，不让 illegal / MMIO / ELF / CSR / Sv39 合同回退。
- 继续守住 `kernel_alpha` 十条 guest 基线、`guest_supervisor_demo` 与当前 debug / frontend 链路。
- 继续把新增 bug 的最小持久回归补到已有门禁中，而不是重新打开低收益的大规模回归扩面。

### 2. `virtio / platform` follow-up 是当前第一优先

- 第一轮 `virtio-mmio + virtqueue + virtio_device + virtio-blk` foundation 已经进入主线，当前最窄、最值钱的缺口不再是 transport / queue / backend 本身。
- 真实 `xv6` board profile 之前，必须先把 UART 与 `virtio` 的 IRQ source 拆开，并把 `Machine` / board profile 接到实际 `virtio` 路径。
- 这是当前从“foundation 已集成”走向“真实 `xv6` 消费这套平台合同”的直接前置条件。
- 没做完这一步之前，不值得急着补更多 `virtio` 设备或额外 platform 特性。

### 3. `xv6-riscv` board profile / bring-up 是当前第二优先

- `xv6-riscv` 当前已经通过 external workload harness 接进主线，并且 `xv6_boot_smoke` 已从旧的 `mhartid` illegal trap 刷新到 post-A 的 early-boot checkpoint。
- 但这条线还没有真正消费 `virtio` contract：当前 board profile 仍记录 `simple_storage`，所以 bring-up 侧的下一跳是切 profile、刷新 smoke，并继续向更后面的稳定 checkpoint 推进。
- 这条线仍然应该优先保持“可复用的 external workload 入口 + 稳定 smoke”，而不是为了快跑演示去写一次性特判。
- `Linux` 后续会直接复用这层 harness / profile 结构，因此当前这里的抽象边界仍要保持克制和通用。

### 4. A 类 `CSR / privilege / timer` hardening 是当前第三优先

- A 线的第一轮 `RV64A + CSR / privilege` foundation 已经进入主线，因此它不再是“先要不要合”的问题，而是进入 bug-driven hardening 阶段。
- 当前最值得补的 A 类工作，不是再主动扩大 ISA 面，而是随着 `xv6` 暴露新的真实缺口，最小化地补上 `pmp*`、`menvcfg`、`stimecmp` 等后续 contract。
- 这条线要继续保证 `InstructionSemantics` 是单一语义来源，避免为了 bring-up 临时再造一套短寿命语义分支。
- 一旦新缺口能被更窄的 asm / host smoke 固化，就应继续沿当前 hardening 路线补最小回归。

### 5. observation / profile foundation + 默认延续线 guardrail 仍是并行必须项

- 当前主线虽然切到标准 OS bring-up，但 `V4`、`P4-prep-1`、`kernel_alpha`、`interactive_os` 和 debug / pipeline workload 仍然要继续守住。
- 当前更值得并行保留的，不只是单纯的 `V4` hardening，还包括面向 `Linux / JIT / DBT` 的 execution profile / observation foundation。
- 这条线的目标是为后续 hot-path 定位、memory behavior 观察、cache 评估和 JIT 候选路径选择提供证据，而不是现在就抢跑真实 JIT。
- 当前 D 线已经整合进主线，并把 `execution_profile_smoke` 接进默认 `make test` / `make test-pipeline`；下一步更应该用它锁住新出现的 `xv6 / virtio` 路径，而不是反向抢占 A/B/C 的 contract ownership。

### 6. Spike 外部差分与更激进 `Phase 3` 继续维持条件触发

- 当前 Spike 线最有价值的角色，仍然是在 reference correctness 出现疑点时提供外部 oracle。
- 当前没有证据支持优先重开更激进的 `issue / replay / speculation`；即使以后重开，也应先有真实 workload hotspot，再看是否值得先做 issue decoupling。

## 当前明确不优先做的事

1. 不直接抢跑 `Linux` 完整 bring-up。
2. 不在当前 foundation 还没站稳前，直接实现 `JIT / 动态二进制翻译`。
3. 不为 `xv6` 先造一批短寿命、只服务单一 demo 的 special case 或 Makefile 特判。
4. 不在当前单发射 + coarse replay 基线上，继续主动扩大更激进的 `Phase 3` issue / replay / memory disambiguation。
5. 不在 `xv6` foundation 与 workload 观测都还不稳定之前，直接抢跑更重的 `Phase 4` cache / DMA / multicore / coherence。
6. 不把 `debug/frontend` 顺势扩成更大的产品功能面或通用调试器。
7. 不把“foundation 已集成”误判成“`xv6` 已经跑在真实 `virtio` board profile 上”；当前尤其不能跳过 B 类 IRQ source follow-up 和 C 类 board profile 切换。

## 如需新开计划

1. 当前主线的总计划以 [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md) 为准。
2. 如果 Workstream A / B / C / D 任一条线后续需要再细分成第二层专项，新增计划也应继续挂到 [xv6_linux_jit_status.md](xv6_linux_jit_status.md)，不要重新分裂事实来源。
3. 只有在 `xv6-riscv` foundation、workload harness 和观测证据都更稳定之后，才考虑新开 `Linux` bring-up 专项计划。
4. 只有在 profile / hot-path / workload 证据足够明确之后，才考虑新开 `JIT / DBT` 专项计划。
5. 如果未来出现真实 `Phase 3` stall hotspot、`debug/frontend` bug 或 Spike correctness 缺口，再围绕具体问题单开最小专项计划。
