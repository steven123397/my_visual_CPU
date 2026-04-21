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

## 当前优先级

### 1. 常态维护仍是默认前提

- 继续维护 reference correctness 矩阵，不让 illegal / MMIO / ELF / CSR / Sv39 合同回退。
- 继续守住 `kernel_alpha` 十条 guest 基线、`guest_supervisor_demo` 与当前 debug / frontend 链路。
- 继续把新增 bug 的最小持久回归补到已有门禁中，而不是重新打开低收益的大规模回归扩面。

### 2. `RV64A + CSR / privilege foundation` 是当前第一优先

- `xv6-riscv`、更后续 `Linux`，以及未来 `JIT / DBT` 都需要更稳的 architected contract。
- 当前第一刀不应再是“一次性最小跑通 `xv6`”，而是先把可复用的 atomic / reservation / privilege foundation 立住。
- 即便 `pipeline` 先采用保守消费方式，也必须复用同一份共享 contract，而不是另起一套最小特判语义。

### 3. `virtio / platform foundation` 是当前第二优先

- 当前这轮不应只把 `virtio-blk` 写成一个设备特例，而是直接建立 `virtio-mmio + virtqueue + virtio_device` 分层。
- `xv6` 只是第一个消费者；这层结构后续还要服务 `Linux` 和更多 `virtio` 设备。
- 这条线与 Workstream A 可并行推进，但 ownership 必须保持独立。

### 4. `xv6-riscv` workload harness / bring-up 是当前第三优先

- `xv6-riscv` 当前已经不是“未来可以考虑”，而是当前主线的近端牵引目标。
- 但这条线更适合先做外部 workload harness、boot path 盘点和 gap audit，再在 A/B 第一轮 contract 站稳后推进真实 smoke。
- 不要把它写成一次性 `xv6_demo`；优先建立后续可容纳 `Linux` 的 external guest workload 入口。

### 5. observation / profile foundation + 默认延续线 guardrail 是并行必须项

- 当前主线虽然切到标准 OS bring-up，但 `V4`、`P4-prep-1`、`kernel_alpha`、`interactive_os` 和 debug / pipeline workload 仍然要继续守住。
- 当前更值得并行保留的，不只是单纯的 `V4` hardening，还包括面向 `Linux / JIT / DBT` 的 execution profile / observation foundation。
- 这条线的目标是为后续 hot-path 定位、memory behavior 观察、cache 评估和 JIT 候选路径选择提供证据，而不是现在就抢跑真实 JIT。

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

## 如需新开计划

1. 当前主线的总计划以 [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md) 为准。
2. 如果 Workstream A / B / C / D 任一条线后续需要再细分成第二层专项，新增计划也应继续挂到 [xv6_linux_jit_status.md](xv6_linux_jit_status.md)，不要重新分裂事实来源。
3. 只有在 `xv6-riscv` foundation、workload harness 和观测证据都更稳定之后，才考虑新开 `Linux` bring-up 专项计划。
4. 只有在 profile / hot-path / workload 证据足够明确之后，才考虑新开 `JIT / DBT` 专项计划。
5. 如果未来出现真实 `Phase 3` stall hotspot、`debug/frontend` bug 或 Spike correctness 缺口，再围绕具体问题单开最小专项计划。
