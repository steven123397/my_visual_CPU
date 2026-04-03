# 已完成计划归档

## 文档定位

本文档用于归档 `docs/plan/` 下已经完成的计划文档。

`docs/plan/` 当前只保留：

- 仍在执行的计划文档
- [template.md](template.md)
- 本归档文件

计划完成后，不再长期保留原始 checklist 文档；只在这里保留“什么时候做了什么”的摘要记录，必要时补一两句实现过程说明。

## 当前归档规则

- 完成态计划先在对应 `status` 文档回写结果，再归档到本文档。
- 归档条目至少保留：完成时间、完成内容。
- 如有必要，可额外补一段很短的实现过程摘要，但不回灌完整 checklist 或逐步执行细节。
- 原计划文档在归档完成后删除，不再保留在 `docs/plan/`。
- `design`、`status` 与后续活跃计划引用历史计划时，统一链接到本文档对应条目。
- 当前如果没有活跃计划，`docs/plan/` 只保留 [template.md](template.md) 和本文档。

## 完成时间线

### 2026-03-25

#### pipeline-core-integration-plan

- 原文件：`pipeline_core_integration_plan.md`
- 完成内容：完成 `pipeline core` 第一轮主线接入，正式引入 `ExecutionBackend`、`FunctionalBackend`、`PipelineBackend`、共享 `InstructionSemantics`、CLI `--backend pipeline` 与 `make test-pipeline` 主入口。
- 实现过程摘要：先把 backend 抽象和 fault-result 访存接口接回主线，再补 host-side smoke / differential；`debug/frontend` 留到后续第二轮单独接入。
- 结果参考：[pipeline_core_integration.md](../design/pipeline_core_integration.md)、[mainline_status.md](../status/mainline_status.md)

#### kernel-alpha-storage-error-contract-plan

- 原文件：`kernel_alpha_storage_error_contract_plan.md`
- 完成内容：补齐独立 `kernel_alpha` 对 storage 错误合同的最小消费能力，新增 `BAD_BLOCK_COUNT` 负向路径，并把后续 storage 错误扩展纳入稳定 bring-up 基线。
- 实现过程摘要：保持 `SimpleStorage` 设备语义不变，主要在 guest platform / storage helper 和独立 demo 入口上扩最小合同。
- 结果参考：[kernel_alpha_storage_error_contract.md](../design/kernel_alpha_storage_error_contract.md)、[kernel_alpha_status.md](../status/kernel_alpha_status.md)

### 2026-03-26

#### phase1-hardening-regressions-plan

- 原文件：`phase1-hardening-regressions_plan.md`
- 完成内容：完成第一轮更系统的 Phase 1 hardening 回归扩充，把 illegal 编码、CPU 侧 MMIO access fault、ELF segment / reject / header、bus / device guard、MMIO contract matrix 与 CSR illegal matrix 接入现有门禁。
- 实现过程摘要：整体策略是优先补回归、只做最小修复，把 reference path 的高风险 correctness 边界压成持续门禁。
- 结果参考：[regression_completion_criteria.md](../design/regression_completion_criteria.md)、[mainline_status.md](../status/mainline_status.md)、[code_self_review_status.md](../status/code_self_review_status.md)

#### sv39-mprv-semantics-plan

- 原文件：`sv39_mprv_semantics_plan.md`
- 完成内容：补齐 `mstatus.MPRV` 驱动的 Sv39 数据访存语义，使 `M-mode` 下的 `load/store` 在 `MPRV=1` 时按 `MPP` 指定的有效特权级执行地址翻译与权限检查。
- 实现过程摘要：保持现有 `AddressSpace` 结构不变，只在有效特权级判定和权限检查路径上补最小修复，并接入 asm 门禁。
- 结果参考：[regression_completion_criteria.md](../design/regression_completion_criteria.md)、[mainline_status.md](../status/mainline_status.md)

#### sv39-pagewalk-contracts-plan

- 原文件：`sv39_pagewalk_contracts_plan.md`
- 完成内容：补齐 Sv39 page-walk 对 misaligned superpage 和 non-leaf PTE 保留位的合同校验，并把对应 asm 回归纳入 `make test` 与 `make test-pipeline`。
- 实现过程摘要：先用最小 asm 样本稳定复现缺口，再只修 page-walk 校验逻辑，不扩大功能面。
- 结果参考：[regression_completion_criteria.md](../design/regression_completion_criteria.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-03-27

#### phase3-branch-prediction-plan

- 原文件：`phase3_branch_prediction_plan.md`
- 完成内容：完成 `Phase 3-A` 第一轮分支预测增强，给 `pipeline` 接上最小 predictor 子模块、预测相关快照字段以及对应的 host-side / frontend 验证闭环。
- 实现过程摘要：保持 `pipeline` 仍为 in-order 后端，只让预测影响取指方向和 mispredict 恢复，不改变 architected 语义来源与提交模型。
- 结果参考：[phase3_branch_prediction_design.md](../design/phase3_branch_prediction_design.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-02

#### phase3-ooo-readiness-plan

- 原文件：`phase3_ooo_readiness_plan.md`
- 完成内容：完成 `Phase 3-B/C` OoO 接线前置准备，补齐执行模型设计、投机执行契约、sequence / retire trace、commit boundary helper，以及未接线但可单测的 `rename_map / reorder_buffer / load_store_queue` 基础模块。
- 实现过程摘要：这一步不直接做 OoO 接线，而是先把结构边界、观测面和验证基建补齐，让后续大块接线不再混着改模型和改实现。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-03

#### phase3-phys-free-list-plan

- 原文件：`phase3_phys_free_list_plan.md`
- 完成内容：为当前 `rename + ROB` 主路径补齐最小 `phys free-list / recycle`，让 phys tag 不再只依赖线性增长，并把 commit 回收与 rollback 恢复纳入稳定合同。
- 实现过程摘要：free-list 直接收口在 `RenameMap` 内，由 ROB head commit 回收 old committed phys，flush / rollback 恢复 checkpoint 快照。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-lsq-replay-contract-plan

- 原文件：`phase3_lsq_replay_contract_plan.md`
- 完成内容：为 `LSQ` 建立最小 `replay-needed` 合同与观测面，能显式区分 unresolved store 阻塞、overlap 阻塞和 `replay_required` 中间态。
- 实现过程摘要：这一轮只把 memory-order 风险变成稳定接口和 smoke，不直接引入自动 replay machinery。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-lsq-automatic-replay-plan

- 原文件：`phase3_lsq_automatic_replay_plan.md`
- 完成内容：把 `replay_required` 从观测状态推进成最小可执行的 automatic replay recovery，让 backend 在安全边界执行 coarse、`RAM-only` 的 replay flush。
- 实现过程摘要：直接复用现有 committed rollback + flush 主路径，不另起新的局部 recovery 机制。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-lsq-store-to-load-forwarding-plan

- 原文件：`phase3_lsq_store_to_load_forwarding_plan.md`
- 完成内容：补齐最小 `RAM-only store-to-load forwarding`，让 younger RAM load 可从 older ready store 前递结果，而不是只能回 RAM 读旧值。
- 实现过程摘要：forwarding 只支持 full-cover RAM 场景，不扩到 MMIO、partial merge 或更激进的 memory speculation。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-minimal-ooo-execute-plan

- 原文件：`phase3_minimal_ooo_execute_plan.md`
- 完成内容：把当前 `pipeline` 从“近似顺序 execute”推进到“最小真实 OoO execute”，让 younger ALU 可在 older memory op 未完成时先完成，但 architected side effect 仍只在顺序 commit 时生效。
- 实现过程摘要：核心收口是让 `ROB head` 成为真实退休源头，并把 memory 路径整理成最小独立执行单元，同时继续守住 MMIO non-speculative 与 precise 边界。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[mainline_status.md](../status/mainline_status.md)

#### phase3-ooo-execution-plan

- 原文件：`phase3_ooo_execution_plan.md`
- 完成内容：完成 `Phase 3-B/C` 首轮总收口，把 `rename + ROB + LSQ`、最小 phys 生命周期、`LSQ replay-needed` 合同、coarse automatic replay、`RAM-only` store-to-load forwarding、最小真实 `OoO execute` 和最小 `ROB / LSQ` 观测面一起收成当前主线路径。
- 实现过程摘要：整体采用“先 readiness、再分块子合同、最后回到总计划收口”的推进节奏；当前 `pipeline` 已稳定在“单发射、顺序退休、最小 OoO 完成窗口”的克制形态，后续重点转向 bug-driven hardening 与下一轮微架构取舍。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)、[mainline_status.md](../status/mainline_status.md)
