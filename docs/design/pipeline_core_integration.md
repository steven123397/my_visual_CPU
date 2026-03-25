# Pipeline Core 集成设计

## 文档定位

本文档用于说明如何把你同学在 `my-project-3-24` 分支上完成的 Phase 2 pipeline core 工作，按当前主线边界重新接回仓库。

它是 [pipeline_integration_prep.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_integration_prep.md) 的具体化设计，重点回答以下问题：

- 本轮到底集成什么，不集成什么
- 哪些改动可以直接迁移，哪些必须基于当前主线重接
- `pipeline` 后端在当前阶段的正式门禁是什么
- 如何在不破坏 `phase1-stable` 基线的前提下，把 `--backend pipeline` 正式接入

当前状态补充：

- 本文档对应的第一轮 `pipeline core` 集成已经完成。
- 当前主线已经在此基础上继续完成了第二轮 `debug/frontend` 接入。
- 第二轮设计见 [debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)。
- 下文中的“本轮目标/非目标”仅指第一轮 `pipeline core` 集成语境，不代表当前主线的全部已接入范围。

## 背景

截至 `2026-03-25`，当前主线 `main` 位于提交 `14d0c6c`，其冻结的 Phase 1 基线为 tag `phase1-stable`（提交 `283aee6`）。

你同学的 Phase 2 工作分支为：

- 仓库：`https://github.com/steven123397/my_visual_CPU.git`
- 分支：`my-project-3-24`
- 当前分支头：`3c89598`

当前已确认：

- `main` 与 `my-project-3-24` 的共同祖先是 `3817572`
- `my-project-3-24` 不是只包含 pipeline，还额外包含 `debug session/protocol` 与 `frontend` 可视化调试器
- 当前主线在 `3817572` 之后，已经完成一轮 guest runtime 收口、`kernel_alpha` Phase 1 hardening、文档体系重组，以及一批 simulator-side correctness 修复

因此，本轮不能把远端分支整体 merge 或整体 rebase 到当前主线，也不能为了接 pipeline 而回退当前 `phase1-stable` 之后的结构收口。

## 本轮目标

本轮只集成 Phase 2 的 pipeline core，目标如下：

1. 保留当前 `functional` reference path，继续作为统一 ISA 语义真值来源。
2. 正式接入 `ExecutionBackend` 抽象，以及 `functional` / `pipeline` 两种后端。
3. 正式接入 CLI backend 选择，使 `--backend pipeline` 可运行。
4. 把 pipeline 所需的共享语义层、fault-result 模型和 host-side 验证接到当前主线。
5. 继续让 Phase 1 的 guest / `kernel_alpha` 回归由 `functional` 后端守住。

## 本轮非目标

本轮明确不做以下事情：

1. 不合入 `myCPU/src/debug/*`。
2. 不合入仓库根目录 `frontend/`。
3. 不要求 `pipeline` 后端首轮就通过全部 guest / `kernel_alpha` 回归。
4. 不为了适配 pipeline 而修改 guest runtime 语义、readiness 合同或 Phase 1 既有回归输出。
5. 不把当前 reference path 打散成多份 ISA 语义来源。

## 集成原则

本轮继续遵守 [pipeline_integration_prep.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_integration_prep.md) 已经确定的原则，并进一步收口为下面几条：

1. `functional` 继续保留，且其行为定义优先于 `pipeline`。
2. `pipeline` 是新 backend，不是新的语义来源。
3. 所有 ISA 语义只保留一份共享实现，后端差异只体现在调度、提交和 hazard 处理。
4. guest/runtime 不为 `pipeline` 改语义；若后续 `pipeline` 跑 guest 失败，优先检查 simulator 的 trap / interrupt / MMIO / commit 时序。
5. 首轮集成必须优先保证：
   - `functional` 不回归
   - `pipeline` 有清晰的 host-side 门禁
   - `--backend pipeline` 对用户可见且可运行

## 远端分支改动分类

对 `3817572..3c89598` 的远端改动，当前按三类处理。

### 一、可直接迁移或小改后迁移

这部分是本轮的主体：

- `myCPU/src/exec/backend.h`
- `myCPU/src/exec/functional_backend.*`
- `myCPU/src/exec/pipeline_backend.*`
- `myCPU/src/exec/pipeline_types.h`
- `myCPU/src/isa/*`
- `myCPU/tests/host/*`

这些文件的职责与当前主线目标一致，基本符合“新增 backend、共享语义层和 host-side pipeline 验证”的边界。

### 二、需要基于当前主线重接

这部分在远端和当前主线都发生过变更，不能直接覆盖：

- `myCPU/Makefile`
- `myCPU/src/main.cpp`
- `myCPU/src/platform/machine.cpp`
- `myCPU/src/platform/machine.h`
- `myCPU/src/cpu.cpp`
- `myCPU/src/trap.cpp`
- `myCPU/src/trap.h`
- `myCPU/src/mem/address_space.cpp`
- `myCPU/src/mem/address_space.h`
- `myCPU/src/mem/bus.cpp`
- `myCPU/src/mem/bus.h`
- `myCPU/src/mem/ram.cpp`
- `myCPU/src/mem/ram.h`
- `myCPU/src/devices/clint.*`
- `myCPU/src/devices/plic.*`
- `myCPU/src/devices/simple_storage.*`
- `myCPU/src/devices/uart16550.*`
- `myCPU/src/exec/control_flow_ops.*`
- `myCPU/src/exec/integer_ops.*`
- `myCPU/src/exec/memory_ops.*`
- `myCPU/src/exec/system_ops.*`

这些路径需要以当前主线为基准，手工吸收 pipeline 所需的最小差异，避免把旧分支对 simulator / platform 的隐含假设一起带回来。

### 三、本轮默认不迁移

- `myCPU/src/debug/*`
- `frontend/*`
- 远端新增的前端设计 / 实现文档
- 与调试 UI 或实时可视化直接相关的脚本和协议层

这部分将在 pipeline core 稳定后，再作为单独的第二轮集成处理。

## 目标架构

本轮集成后的执行路径应为：

```text
main.cpp
  -> Machine
      -> ExecutionBackend
           -> FunctionalBackend
           -> PipelineBackend
      -> CPU
      -> AddressSpace
      -> TrapController
      -> Bus
      -> Ram / UART / CLINT / PLIC / SimpleStorage
```

其中边界如下：

- `Machine` 负责平台对象组装、镜像加载、backend 选择与执行循环。
- `ExecutionBackend` 负责定义统一 `step()` 接口。
- `FunctionalBackend` 负责包装当前 reference path。
- `PipelineBackend` 负责 IF/ID/EX/MEM/WB、forwarding、interlock、redirect、flush 与 commit-boundary trap / interrupt。
- `InstructionSemantics` 负责输出共享的架构效果，不直接决定 backend 调度。
- `AddressSpace` 负责返回 fetch / load / store 的结果对象，而不是直接把 fault 即时注入成架构提交。

## 详细设计

### 1. backend 正式接入

需要在当前主线上正式完成以下改造：

- 在 `Machine` 中持有 `ExecutionBackend`
- 默认 backend 仍为 `functional`
- CLI 新增 `--backend functional|pipeline`
- `Machine::run()` 与 `Machine::step()` 通过 backend 驱动执行

首轮目标不是把 `pipeline` 设成默认值，而是让它成为一个正式、可选、可测试的执行模型。

### 2. 共享语义层

远端分支把 `cpu.cpp` 中的直接执行逻辑改造成：

- `ExecutionContext`
- `InsnEffects`
- `InstructionSemantics`

这一方向与当前主线设计目标一致，应予以保留。

本轮集成时需要满足：

1. `functional` 与 `pipeline` 共享同一份语义实现。
2. 旧的 `exec/*` 不再各自直接成为 backend 的最终语义来源。
3. 任何后续 privileged / CSR / MMIO 行为修复，只改共享语义层或其底层公共支撑，而不是分别修两套后端。

### 3. `AddressSpace` fault-result 模型

为了满足 pipeline 的精确异常与提交边界要求，当前主线中的 CPU 访存 / 取指错误处理需要改成“先返回结果，再由 backend 决定何时提交 trap”。

本轮要求：

1. `fetch32`、`load`、`store` 提供结果对象。
2. 结果对象能表达：
   - 成功
   - fault
   - cause
   - tval
   - 读取得到的值
3. `functional` 仍可在单步路径中近似立即消费这些结果。
4. `pipeline` 必须只在 commit boundary 真正进入 trap。

### 4. trap / interrupt 提交边界

本轮 pipeline 不追求复杂乱序，只追求一个正确、可验证的 5-stage backend。

因此要明确：

1. 指令按程序顺序提交。
2. older fault 之前，younger 指令不得污染架构状态。
3. interrupt 在 commit boundary 才能被正式递送。
4. `mret/sret`、fetch fault、load/store fault、illegal instruction、`ecall`、CSR/system 子集，都必须沿着共享语义和统一 trap 提交路径工作。

### 5. host-side pipeline 验证

首轮 `pipeline` 的正式门禁只放在 host-side，不直接要求 guest 全量回归。

本轮应接入并维护：

- `instruction_semantics_smoke`
- `address_space_faults_smoke`
- `pipeline_backend_smoke`
- `backend_differential_smoke`
- backend CLI smoke

如有必要，应在 [Makefile](/home/liangjiaqi/projects/my_visual_CPU/myCPU/Makefile) 中新增明确的 `pipeline` 测试入口，例如：

- `make test-pipeline`

这样可以把 Phase 2 backend 验证和 Phase 1 guest 回归明确分层。

## 实施顺序

本轮按以下顺序推进：

1. 新增 backend 抽象与 `FunctionalBackend`，先把 `Machine -> backend_->step()` 接通。
2. 接入 CLI backend 选择，但默认值保持 `functional`。
3. 接入共享语义层与 `InsnEffects`。
4. 把 `AddressSpace` 改成 fault-result 模型。
5. 接入 `PipelineBackend`。
6. 接入 host-side pipeline 测试与差分 smoke。
7. 最后补齐当前主线需要的最小正式文档，不迁移远端 `debug/frontend` 过程性文档。

这个顺序的目的，是让每一步都能单独验证，不把 backend、语义层、CLI 和测试一起揉成一次性大改。

## 验收标准

本轮通过标准分成两档。

### `functional` 必守

以下门禁继续要求通过：

- `cd myCPU && make test`

这意味着当前 Phase 1、guest runtime、`kernel_alpha` 与 correctness 基线不能被 pipeline 集成破坏。

### `pipeline` 首轮新增门禁

以下 host-side 门禁应新增并要求通过：

- `cd myCPU && make test-pipeline`

若当前实现采用分散目标，也至少要守住：

- `pipeline_backend_smoke`
- `backend_differential_smoke`
- `instruction_semantics_smoke`
- `address_space_faults_smoke`
- backend CLI smoke

## 风险与对策

### 风险 1：共享语义层接入时破坏当前 functional 路径

对策：

- 先保留 `functional` 为默认 backend
- 每引入一层共享语义改动都立即跑 `make test`

### 风险 2：远端 pipeline 假设的 platform 行为与当前主线不一致

对策：

- 对设备、总线、`AddressSpace`、`TrapController` 相关路径一律基于当前主线手工重接
- 不直接覆盖当前 `phase1-stable` 之后的 simulator-side correctness 修复

### 风险 3：把 debug/frontend 一起带入，放大集成面

对策：

- 本轮明确禁止迁移 `myCPU/src/debug/*` 与 `frontend/*`
- 等 pipeline core 稳定后，再单独做第二轮集成

### 风险 4：验证门禁不清，导致 pipeline 迟迟无法落地

对策：

- 明确本轮 `pipeline` 只守 host-side 专用测试
- Phase 1 guest 回归继续由 `functional` 负责

## 本轮完成后的下一步

当本轮 pipeline core 已稳定接入主线后，下一步再考虑两件事：

1. 单独评估并接入 `debug session/protocol`
2. 单独评估并接入 `frontend` 可视化调试器

这两部分都应建立在“共享语义层、backend 抽象、`--backend pipeline`、host-side pipeline 门禁”已经稳定存在的前提上，而不是反过来驱动 pipeline core 的设计。
