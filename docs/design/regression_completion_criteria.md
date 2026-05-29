# 回归收口标准（2026-03-26）

## 文档定位

本文档用于定义当前 `Phase 1` / `Phase 2` 主线任务中，回归相关工作做到什么程度可以认为“阶段性完善”。

它的目的不是鼓励无限堆叠测试数量，而是给后续实现和收口提供统一判断口径：哪些回归必须补，哪些回归已经足够，什么时候应该从“继续扩回归”切回“维护已有门禁”。

## 关联文档

- 相关状态：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 已完成计划：
  - [plan/history_plan.md#phase1-hardening-regressions-plan](../plan/history_plan.md#phase1-hardening-regressions-plan)

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为当前 Phase 1 / Phase 2 回归是否达到阶段性收口的统一判断口径。
- 具体执行进展、当前缺口和近期任务以 [status/mainline_status.md](../status/mainline_status.md) 与相关 `status` 文档为准。

## 目标 / 主题

当前仓库已经不是纯设计稿，而是一个已可运行的模拟器原型。对这样一条主线来说，回归工作的目标不是追求穷举，而是对当前已经承诺支持的语义范围形成可持续的门禁闭环。

这份文档只讨论当前冻结范围内的“阶段性收口标准”。如果后续功能面扩大、合同变化或 Phase 2 语义继续外扩，回归范围应随之重新打开。

## 当前状态

当前主线已经具备以下前提：

- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成。
- `functional` reference path、`kernel_alpha` 正向和九条负向路径、`make test` 主门禁已经成立。
- `pipeline core`、`make test-pipeline`、本地 `debug_session/protocol + frontend` 教学演示链路已经正式接入。
- `2026-03-26` 已完成第一轮更系统的 Phase 1 hardening regression 扩充：非法编码、CPU 侧 MMIO access fault、ELF segment / reject / header、host-side MMIO contract 与 CSR illegal matrix 已接入现有门禁。

在这个前提下，当前对“补回归”的理解应明确为：

- 回归不是按 case 数量收口，而是按语义合同和边界风险收口。
- 不追求对全部排列组合做穷举，而是要求每类高风险边界至少具备代表性的正向、负向和边界用例。
- 每修掉一个真实 bug，都应留下一个能稳定复现该问题的持久回归。
- 当新增 case 已经主要是在重复已有覆盖，而不是覆盖新的语义缺口时，主线目标就不再是“继续堆回归”，而是“维护已有门禁”。

## 关键历史节点

- `2026-03-25` 已完成一轮 simulator-side correctness 修复，覆盖非法整数编码、`DIV/REM` 溢出边界、ELF pure-BSS `PT_LOAD` 与 bus / device 第一轮边界防御。
- `2026-03-26` 已把 illegal / MMIO / ELF / CSR 这几类高风险 reference-path 边界补成第一轮更系统的回归矩阵。
- `kernel_alpha_demo` 与九条负向回归已经形成 Phase 1 冻结后的稳定 bring-up 基线。
- `pipeline core` 与 `debug/frontend` 已完成正式接入，当前不再是“待合入功能”，而是需要继续稳定化的既有能力。

这些历史节点说明，当前仓库中的回归工作已经从“先把功能接通”转入“围绕既有语义合同补齐验证闭环”。

## 当前仍然有效的风险 / 限制

- 如果继续以“能想到的都补一点”为原则推进回归，测试数量会无限增长，但很难说明哪些风险已经真正被消掉。
- reference path 的 robustness 回归虽然已经完成第一轮系统扩充；当前 `privilege / Sv39` 代表性合同已基本形成闭环，后续更适合按新增 bug 或明确新语义缺口补最小持久回归。
- `pipeline differential` 的高风险主干矩阵已经基本形成闭环，但后续仍需控制低收益变体扩张，并在新增 bug 出现时补最小持久回归。
- `debug/frontend` 已接入，但它的目标仍应限定为“教学演示可用”，不应因为演示链路存在就无限扩张协议或 UI 功能面。

## 阶段性收口标准

### Phase 1

当满足以下条件时，可以认为当前冻结范围内的 Phase 1 回归已经“阶段性完善”：

- reference path 当前承诺支持的语义面，都已经有对应门禁，而不是只靠人工 smoke。
- 非法编码、MMIO 合法 / 非法访问、ELF 段布局、CSR / trap / privilege 基本路径都至少具备一组代表性的正向、负向和边界用例。
- `kernel_alpha` 十条回归和 `guest_supervisor_demo` 长期保持稳定输出，新改动通常会先被现有门禁拦住，而不是反复暴露“同一类漏测”。
- 新发现的问题开始更多来自功能新增或范围扩大，而不是来自当前冻结范围内明显缺失的基础合同。

### Phase 2

当满足以下条件时，可以认为当前冻结范围内的 Phase 2 回归已经“阶段性完善”：

- `pipeline` 在当前支持范围内，对 `functional` 的差分结果已经具备系统性门禁，而不只是 smoke。
- trap / interrupt / CSR / MMIO / privilege transition 等高风险交错路径已经有代表性一致性验证。
- `debug/frontend` 的快照结构、协议输出和教学 demo 链路具备稳定测试，不依赖人工临场操作才能确认可用。
- 后续新增 bug 大多能由既有差分或快照门禁直接拦截，而不是反复说明“当前验证面还没搭起来”。

## 何时不应再盲目扩回归

出现以下信号时，应把主线重点从“继续加更多 case”转回“维护、整理和局部补洞”：

- 新增测试大多只是已有场景的轻微变体，不能提供新的合同覆盖。
- 测试维护成本和运行时间增长，已经明显快于风险下降速度。
- 最近发现的问题主要来自新功能、新设备或新阶段目标，而不是当前冻结范围内的旧缺口。
- 现有门禁已经足以在日常改动中稳定发现大多数回归，继续加 case 的收益明显下降。

## 维护边界

当前回归体系的维护口径是：

- `privilege / Sv39`、illegal / MMIO / ELF / CSR 矩阵按新增 bug 或新合同补最小持久回归。
- `kernel_alpha` 十条基线和 `guest_supervisor_demo` 是 Phase 1 完成态的核心门禁。
- `pipeline` 差分矩阵只围绕新增 bug 和明确新合同扩展，避免低收益 case 盘点。
- `debug/frontend` 验证限定在快照、协议和 demo 可用性，不扩成通用调试器验收清单。
- 若新增测试只是重复已有覆盖，不把它扩成新的长期门禁。

## 验证基线

- `cd myCPU && make test-fast-smoke`
- `cd myCPU && make test-standard-regression`
- `cd myCPU && make test-slow-guest`
- `cd myCPU && make test-opt-in-external`
- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`

分层 target 语义：

- `test-fast-smoke`：host unit / host smoke 中不依赖外部资产、适合开发中快速回归的入口。
- `test-standard-regression`：当前标准回归，等价覆盖既有 `make test` 与关键 pipeline guardrail。
- `test-slow-guest`：guest demos、pipeline guest demos 与 `xv6` shell 类慢门禁。
- `test-opt-in-external`：真实 Linux Image / 外部发行版 rootfs / Spike 等显式外部资产入口；缺资产时必须 fail-closed。
