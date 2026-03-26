# 流水线集成准备清单

## 文档定位

本文档用于记录在 `Phase 1` 基线已经冻结之后，如何把后续拿到的“基于旧提交实现的流水线代码”接回当前主线。

它不是实现计划的执行流水账，而是一个长期有效的集成准备说明。

当前状态补充：

- 本文档描述的准备阶段已经完成。
- `pipeline core` 的正式接入结果见 [pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)。
- `debug/frontend` 的第二轮接入结果见 [debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)。
- 下文中的“后续接回”与类似未来时态，均按集成前的准备阶段语境理解。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
- 相关计划：
  - [plan/pipeline_core_integration_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/pipeline_core_integration_plan.md)

## 当前有效性说明

- 当前有效 / 历史语境：历史语境，保留当时的接回原则与准备判断。
- 当前已落地结果以 [pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md) 和 [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md) 为准。

## 当前冻结基线

- 稳定 tag：`phase1-stable`
- 对应提交：`283aee6`
- 基线含义：
  - `Phase 1` 核心目标已达成
  - 第一次真正的小型 OS / kernel bring-up 的基础障碍已清完
  - 后续 guest/runtime 工作默认视为 post-Phase1 hardening，而不是继续阻塞流水线集成的前置条件

## 为什么不能直接接旧分支

已知你同学的流水线工作基于：

- `3817572`

而当前冻结基线相对它已经多出一轮大幅 guest/runtime 收口，重点变化集中在：

- `myCPU/guest/kernel/`
- `myCPU/guest/kernel_alpha/`
- `myCPU/guest/include/`
- `myCPU/tests/unit/`
- 一部分 simulator device / platform / loader 路径

因此后续拿到那份代码后，不应直接把旧分支整体 rebase 到当前 `main`，也不应把 `guest/` 当前结构回退到旧形态来适配流水线。

## 集成原则

后续流水线集成时，默认遵守以下原则：

1. 当前 fetch-decode-execute reference path 继续保留，作为统一 ISA 语义基线。
2. 流水线路径作为新的执行模型或 backend 引入，不覆盖当前 reference core。
3. guest/runtime 不为流水线“改语义”；若 guest 回归失效，优先检查 simulator 的 trap / interrupt / MMIO / 提交时序。
4. `Phase 1` 基线上的 `kernel_alpha` 与 `guest_supervisor_demo` 回归继续作为流水线集成门禁。
5. 优先迁移 simulator 内部结构和算法；guest 侧若与当前结构冲突，默认按当前主线边界重接，而不是硬搬旧代码。

## 后续接入步骤

等你同学的分支可见之后，推荐按下面顺序推进：

1. 从 `phase1-stable` 开一条独立集成分支，例如：
   - `phase2-pipeline-integration`
2. 先做目录级 diff 审查，而不是急着 merge。
3. 把你同学的改动按三类划分：
   - 可直接迁移的 simulator core 结构改动
   - 需要基于当前代码重做的 simulator 逻辑
   - 不应直接迁移的 guest/runtime 改动
4. 先接 simulator 内核最外层骨架，再逐步接具体流水级、冒险处理、flush/redirect。
5. 每接一小步都跑完整回归，不允许“等全接完再统一修”。

## 三类改动判断标准

### 可直接迁移

通常包括：

- 明确局限在 `myCPU/src/` 内部、且不改 architected 行为的结构拆分
- 对 `cpu.cpp` / `exec/*` / `platform/*` 的 backend 组织性调整
- 与 fetch / decode / execute stage 划分有关、但不改变现有 trap / CSR / MMIO 外部语义的代码

### 需要基于当前主线重做

通常包括：

- 同时碰到 `src/` 与 `guest/` 的“端到端”调整
- 假设旧版 `kernel_alpha`/`supervisor_demo` 编排形态的 glue code
- 与当前 `kernel_bringup` / `kernel_runtime` / `interrupt_contract` / `storage_contract` 冲突的适配代码

### 默认不要直接迁移

通常包括：

- 大面积改动 `myCPU/guest/`
- 为了配合流水线而修改 guest timeout/readiness 合同
- 把当前已完成的 runtime / VM / trap 收口重新打散的代码

## 首批优先核对文件

等分支拿到之后，第一轮建议优先看这些路径：

- `myCPU/src/cpu.cpp`
- `myCPU/src/exec/*`
- `myCPU/src/trap.cpp`
- `myCPU/src/platform/machine.cpp`
- `myCPU/src/mem/address_space.cpp`
- `myCPU/src/devices/*`

如果旧分支还大幅修改了下面这些路径，先单独评估，不要直接并：

- `myCPU/guest/kernel/*`
- `myCPU/guest/kernel_alpha/*`
- `myCPU/guest/include/*`
- `myCPU/tests/unit/*`

## 集成时必须守住的验证

最低门禁：

- `cd myCPU && make -j2 test`

如果第一轮只改 simulator core，也至少要特别关注：

- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_magic_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_lba_range_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_command_demo`

## 当前建议

在你同学分支还不可见之前，当前阶段不继续提前设计流水线内部细节。

当前最合适的动作只有两件：

- 保持 `phase1-stable` 这个冻结点不被重写
- 等分支拿到后，先做 diff 分类，再决定哪些内容 cherry-pick，哪些内容重做
