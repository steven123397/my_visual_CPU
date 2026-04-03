# 当前项目修正版优先级路线图

## 文档定位

本文档用于基于当前 `main` 分支已落地状态，重新排序项目近期与中期优先级，明确“现在先稳什么、哪些方向暂缓扩、远期再推进什么”。

它不是执行 checklist，也不替代 [mainline_status.md](mainline_status.md)；它更适合作为下一轮拆计划前的优先级总览。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/cpp_refactor_design.md](../design/cpp_refactor_design.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
- 相关状态：
  - [status/mainline_status.md](mainline_status.md)
  - [status/kernel_alpha_status.md](kernel_alpha_status.md)
  - [status/code_self_review_status.md](code_self_review_status.md)
- 已完成计划归档：
  - [plan/history_plan.md#phase1-hardening-regressions-plan](../plan/history_plan.md#phase1-hardening-regressions-plan)
  - [plan/history_plan.md#pipeline-core-integration-plan](../plan/history_plan.md#pipeline-core-integration-plan)
  - [plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan)

## 目标 / 主题

当前仓库已经不是等待 bring-up 的设计稿，而是一个已可运行的模拟器原型。修正版路线图的目标，不是继续把所有可做方向并行铺开，而是按结构收益和风险顺序重新排队。

## 当前判断

- `Phase 1` 核心 bring-up、`kernel_alpha` 十条 guest 基线、`pipeline core` 正式接入、`debug/frontend` 教学演示链路，以及 `Phase 3-A/B/C` 首轮收口都已经落地。
- 因此，当前主线不应再把“接入 `pipeline / frontend`”“第一次进入 OoO”“再做一个新 demo”当作最高优先级问题。
- 当前最真实的缺口，主要集中在 4 类：reference path correctness 的持续补洞、guest runtime / kernel runtime 的结构债、`debug/frontend` 协议与长会话稳健性，以及 `pipeline` 新能力的 bug-driven hardening。
- 这意味着近期路线图应先稳主线基线，再稳结构边界，最后才考虑继续扩高级微架构和更大功能面。

## 关键历史节点

- `2026-03-26`：`Phase 1` hardening 回归矩阵完成第一轮系统扩充，illegal / MMIO / ELF / CSR 高风险 correctness 边界已压成持续门禁。
- `2026-03-27`：`Phase 3-A` 最小分支预测增强完成，`pipeline` 开始正式进入高级微架构阶段，但仍保持克制边界。
- `2026-04-03`：`Phase 3-B/C` 首轮总计划归档完成，`rename + ROB + LSQ +` 最小真实 `OoO execute` 已正式进入当前主线。

## 修正版优先级路线图

1. **P0：reference path correctness 与合同 hardening 继续收口。**
   当前最高优先级仍是 `functional + shared InstructionSemantics`、Sv39 / privilege / CSR / MMIO / ELF 等 reference 合同。新增 bug 出现时，优先补最小持久回归，而不是开启新的功能面。
2. **P1：`guest runtime / kernel runtime / kernel_alpha` 结构 hardening。**
   当前 guest 侧更值得继续做的是 `vm*`、`trap*`、`kernel_runtime`、`kernel_bringup`、`kernel_alpha` 共享 helper 的边界收口，而不是先把 guest 功能表继续拉长。
3. **P2：维护已落地的 `Phase 2` 能力，而不是继续扩 `Phase 2` 功能面。**
   `pipeline` 差分 / smoke、`debug_session/protocol`、Node debug server 和浏览器前端现在都属于已接入能力。近期目标是稳协议、稳快照、稳测试和长会话行为，而不是扩成通用调试器。
4. **P3：`Phase 3` 只做受控推进，不并行扩多个高风险方向。**
   只有在 `P0` 到 `P2` 没有新的红灯后，才建议继续推进下一轮微架构；而且每次只选一个方向，例如 issue / dispatch、`LSQ` memory-order hardening，或 predictor bug-driven refinement，不要同时推进 issue、replay、memory speculation 和更复杂 predictor。
5. **P4：平台与小内核能力在稳定边界上渐进扩展。**
   在主线稳定后，再考虑更完整的 storage / platform 合同、更系统的 kernel object / runtime 组织，以及必要的 guest bring-up 能力扩展；这些工作应服务于“小型 OS / kernel bring-up”，而不是转成另一个 UI 或 demo 项目。
6. **P5：更远期再进入 cache / DMA / multicore。**
   `Phase 4` 方向仍然成立，但它们必须建立在前面几级已经稳定的 reference、runtime 和 `pipeline` 边界之上，不能提前抢占近期主线。

## 当前不建议优先推进的事项

- 不建议现在就把 `debug/frontend` 扩成带断点、条件暂停、任意镜像加载的大调试器。
- 不建议现在就把 `interactive_os` 往图形桌面或大功能集继续扩。
- 不建议现在就把 `pipeline` 一口气推进到更激进的 MMIO speculation、复杂 partial merge、复杂 predictor 组合、cache hierarchy 或 superscalar。
- 不建议把更多设备模型、异步 completion 机制和宿主文件持久化混进当前 storage 主线，除非它们直接服务当前 bring-up / correctness 缺口。

## 下一步

1. 新一轮工作优先从 `P0` 或 `P1` 中各选一条单独建 plan，不再继续维护一个笼统的大 `Phase 3` 总计划。
2. 如果下一轮确实要继续推进 `Phase 3`，先在 `status/design` 里明确只选一个子方向，再创建对应计划文档。
3. 每次准备扩新能力时，都先回答它会不会污染 reference path、guest 基线、`debug/frontend` 协议边界和现有验证门禁。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
