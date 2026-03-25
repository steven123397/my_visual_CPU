# Pipeline Core 主线集成实现计划

> 归档说明：本文档对应的接入工作已经完成，保留为历史计划记录；当前结果以 [pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)、[debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md) 和当前状态文档为准。下文中的“本轮”“不迁移”“待办”等表述均按当时计划语境理解，不代表当前状态。

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 在不破坏当前 `phase1-stable` Phase 1 基线和 guest / `kernel_alpha` 回归的前提下，把你同学分支中的 Phase 2 pipeline core 重接到主线，并正式接入 `--backend pipeline`。

**架构：** 先把 `Machine` 迁移到 `ExecutionBackend` 抽象，再接入共享语义层 `InstructionSemantics + InsnEffects + ExecutionContext`，随后把 `AddressSpace` 改成 result-based fault API，最后接入精确异常的 `PipelineBackend` 与 host-side pipeline 门禁。`functional` 继续作为默认 backend 和统一 ISA 语义真值来源；按当时计划语境，`debug/frontend` 留到后续单独一轮处理。

**技术栈：** C11、C++17、GNU Make、RISC-V 交叉工具链（用于完整 `make test`）、host-side g++ smoke tests。

---

## 参考文档

- [pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)
- [pipeline_integration_prep.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_integration_prep.md)
- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
- [readme.md](/home/liangjiaqi/projects/my_visual_CPU/readme.md)

## 文件结构

### 新增文件

- `myCPU/src/exec/backend.h`
  `ExecutionBackend` 最小接口。首轮只承载 backend 驱动所需的最小公共表面，不引入 `debug_snapshot()`。
- `myCPU/src/exec/functional_backend.h`
  `functional` backend 声明。
- `myCPU/src/exec/functional_backend.cpp`
  现有 reference path 的 backend 包装。
- `myCPU/src/exec/pipeline_backend.h`
  `PipelineBackend` 声明。
- `myCPU/src/exec/pipeline_backend.cpp`
  IF/ID/EX/MEM/WB、forwarding、load-use interlock、redirect、flush、commit-boundary trap / interrupt。
- `myCPU/src/exec/pipeline_types.h`
  pipeline stage register、slot、内部控制状态。
- `myCPU/src/isa/effects.h`
  `TrapRequest`、`MemoryRequest`、`ControlEffect`、`InsnEffects` 等共享值对象。
- `myCPU/src/isa/execution_context.h`
  共享语义层读取 `CPU` / `CSR` / `AddressSpace` / `Bus` 的受控入口声明。
- `myCPU/src/isa/execution_context.cpp`
  `ExecutionContext` 实现。
- `myCPU/src/isa/instruction_semantics.h`
  统一 ISA 语义入口。
- `myCPU/src/isa/instruction_semantics.cpp`
  指令族到 `build_*_effects()` 的总分发。
- `myCPU/tests/host/instruction_semantics_smoke.cpp`
  共享语义层 smoke。
- `myCPU/tests/host/address_space_faults_smoke.cpp`
  `AddressSpace::AccessResult` smoke。
- `myCPU/tests/host/pipeline_backend_smoke.cpp`
  pipeline 行为 smoke。
- `myCPU/tests/host/backend_differential_smoke.cpp`
  `functional` / `pipeline` 提交态差分 smoke。
- `myCPU/tests/host/backend_cli.sh`
  CLI backend 选择 smoke。

### 重点修改文件

- `myCPU/src/platform/machine.h`
  持有 `ExecutionBackend`、backend kind、`step()` / `set_backend_kind()` 等入口。
- `myCPU/src/platform/machine.cpp`
  backend 重建、镜像重载、`step()` / `run()` 编排。
- `myCPU/src/main.cpp`
  新增 `--backend functional|pipeline`，同时保留当前 `--disk`、`--disk-not-ready`、`--disk-bad-magic`。
- `myCPU/src/cpu.cpp`
  参考路径迁到共享语义层与 `apply_instruction_effects()`。
- `myCPU/src/mem/address_space.h`
  新增 `AccessResult` 与 `fetch32_result/load_result/store_result`。
- `myCPU/src/mem/address_space.cpp`
  fault-result 实现，并保留 legacy wrapper 以兼容 `functional`。
- `myCPU/src/exec/integer_ops.h`
  暴露 `build_integer_effects()`。
- `myCPU/src/exec/integer_ops.cpp`
  从“直接执行”抽出整数类 effects 构造。
- `myCPU/src/exec/control_flow_ops.h`
  暴露 `build_control_flow_effects()`。
- `myCPU/src/exec/control_flow_ops.cpp`
  从“直接执行”抽出跳转 / 分支 effects 构造。
- `myCPU/src/exec/memory_ops.h`
  暴露 `build_memory_effects()`、`apply_memory_effects()`、`extend_loaded_value()`。
- `myCPU/src/exec/memory_ops.cpp`
  统一 load/store effects 构造与提交。
- `myCPU/src/exec/system_ops.h`
  暴露 `build_system_effects()`。
- `myCPU/src/exec/system_ops.cpp`
  统一 CSR / system / trap-return 语义构造。
- `myCPU/src/trap.h`
  如有必要，补最小接口以支持 commit-boundary return / interrupt 提交。
- `myCPU/src/trap.cpp`
  保持唯一 trap 路由，不引入第二套 trap 语义。
- `myCPU/src/devices/clint.h`
  为 pipeline 提供需要的最小只读接口。
- `myCPU/src/devices/clint.cpp`
  配合 `time` / timer interrupt 在两种 backend 下保持一致行为。
- `myCPU/src/devices/plic.h`
  为 pipeline 提供需要的最小只读或 helper 接口。
- `myCPU/src/devices/plic.cpp`
  保持 machine / supervisor external interrupt 行为一致。
- `myCPU/src/devices/simple_storage.h`
  保持当前 storage 合同，对 pipeline 所需 helper 做最小补充。
- `myCPU/src/devices/simple_storage.cpp`
  配合 backend 重置 / platform reload，不破坏当前 readiness / bad-magic 行为。
- `myCPU/src/devices/uart16550.h`
  为 pipeline 最小兼容性补接口。
- `myCPU/src/devices/uart16550.cpp`
  保持 UART 输出与 THRE interrupt 行为一致。
- `myCPU/src/mem/bus.h`
  为 pipeline 和 host tests 保留最小必要接口。
- `myCPU/src/mem/bus.cpp`
  处理 backend reload、非法访问与最后一次访问辅助时，保留当前 guard 行为。
- `myCPU/src/mem/ram.h`
  视 pipeline tests 需要补最小 helper。
- `myCPU/src/mem/ram.cpp`
  与 host smoke 对齐。
- `myCPU/Makefile`
  接入新增源文件、host tests、`test-pipeline` 聚合目标；不引入 `debug/frontend` 相关项。
- `myCPU/AGENTS.md`
  更新 simulator 主体实现基线与验证门禁。
- `readme.md`
  补充 `--backend pipeline` 的最小对外说明。

### 本轮明确不改

- `myCPU/src/debug/*`
- `frontend/*`
- `myCPU/guest/*` 的语义、readiness 合同与 bring-up 编排

## 任务 1：接入 backend 抽象与 `functional` 包装

**文件：**

- 创建：`myCPU/src/exec/backend.h`
- 创建：`myCPU/src/exec/functional_backend.h`
- 创建：`myCPU/src/exec/functional_backend.cpp`
- 修改：`myCPU/src/platform/machine.h`
- 修改：`myCPU/src/platform/machine.cpp`
- 修改：`myCPU/Makefile`
- 测试：现有 `cd myCPU && make test`（回归守门）

- [ ] **步骤 1：先让 `Machine` 具备 backend 编排骨架**

  修改 [machine.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/platform/machine.h) 和 [machine.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/platform/machine.cpp)，引入：

  - `BackendKind`
  - `std::unique_ptr<ExecutionBackend>`
  - `Machine::step()`
  - `Machine::set_backend_kind()`
  - `Machine::rebuild_backend()`

  第一版只要求 `FunctionalBackend` 可工作，不要求 `pipeline` 已接入。

- [ ] **步骤 2：实现 `ExecutionBackend` 与 `FunctionalBackend`**

  在 [backend.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/exec/backend.h) 中定义最小接口：

  ```cpp
  class ExecutionBackend {
  public:
      virtual ~ExecutionBackend() = default;
      virtual void step() = 0;
      virtual const char* name() const = 0;
  };
  ```

  在 [functional_backend.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/exec/functional_backend.cpp) 中先直接包装：

  ```cpp
  void FunctionalBackend::step() {
      cpu_step(cpu_, bus_);
  }
  ```

- [ ] **步骤 3：把新源文件接进构建系统**

  修改 [Makefile](/home/liangjiaqi/projects/my_visual_CPU/myCPU/Makefile)，加入：

  - `src/exec/functional_backend.cpp`
  - 未来 `pipeline` / `isa` 文件的占位变量结构

  这一阶段不要把 `debug_session`、`debug_protocol` 或 `frontend` 相关项带进来。

- [ ] **步骤 4：运行现有完整回归验证不回归**

  运行：`cd myCPU && make test`

  预期：

  - 构建通过
  - 现有 Phase 1 / guest / `kernel_alpha` 回归仍通过

- [ ] **步骤 5：Commit**

  ```bash
  git add myCPU/src/exec/backend.h myCPU/src/exec/functional_backend.h myCPU/src/exec/functional_backend.cpp myCPU/src/platform/machine.h myCPU/src/platform/machine.cpp myCPU/Makefile
  git commit -m "refactor(执行后端): 引入 functional backend 骨架"
  ```

## 任务 2：引入共享语义层并让 `functional` 走统一语义

**文件：**

- 创建：`myCPU/src/isa/effects.h`
- 创建：`myCPU/src/isa/execution_context.h`
- 创建：`myCPU/src/isa/execution_context.cpp`
- 创建：`myCPU/src/isa/instruction_semantics.h`
- 创建：`myCPU/src/isa/instruction_semantics.cpp`
- 创建：`myCPU/tests/host/instruction_semantics_smoke.cpp`
- 修改：`myCPU/src/cpu.cpp`
- 修改：`myCPU/src/exec/integer_ops.h`
- 修改：`myCPU/src/exec/integer_ops.cpp`
- 修改：`myCPU/src/exec/control_flow_ops.h`
- 修改：`myCPU/src/exec/control_flow_ops.cpp`
- 修改：`myCPU/src/exec/memory_ops.h`
- 修改：`myCPU/src/exec/memory_ops.cpp`
- 修改：`myCPU/src/exec/system_ops.h`
- 修改：`myCPU/src/exec/system_ops.cpp`
- 修改：`myCPU/Makefile`
- 测试：`myCPU/tests/host/instruction_semantics_smoke.cpp`

- [ ] **步骤 1：先写共享语义层 smoke**

  新增 [instruction_semantics_smoke.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/host/instruction_semantics_smoke.cpp)，覆盖至少这些场景：

  - `addi` 生成正确的 `rd_write`
  - `jal` 生成正确的 link write 和 redirect
  - 非法 branch 编码生成 illegal-instruction trap effect
  - `lw` 生成正确的 `MemoryRequest`
  - `sw` 生成正确的 `MemoryRequest`

- [ ] **步骤 2：运行新测试，确认它先失败**

  运行：`cd myCPU && make tests/host/instruction_semantics_smoke`

  预期：FAIL，报缺少 `isa/*`、`InsnEffects`、`InstructionSemantics` 或 `build_*_effects()`。

- [ ] **步骤 3：实现共享语义值对象与上下文**

  在 [effects.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/isa/effects.h) 中添加：

  - `TrapRequest`
  - `RegWrite`
  - `CsrWrite`
  - `MemoryRequest`
  - `ControlEffect`
  - `InsnEffects`

  在 [execution_context.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/isa/execution_context.cpp) 中提供 `CPU` / `CoreState` / `CsrFile` / `AddressSpace` / `Bus` 的受控访问。

- [ ] **步骤 4：把 `exec/*` 迁成 `build_*_effects()`**

  在 `integer/control-flow/memory/system` 四个族中新增 `build_*_effects()`，保留现有 `execute_*()` 兼容层，避免一次性打散当前 functional 路径。

  目标状态：

  - `InstructionSemantics::execute()` 统一分发到 `build_*_effects()`
  - [cpu.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/cpu.cpp) 通过 `apply_instruction_effects()` 消费 effects
  - 共享语义层成为 `functional` 与后续 `pipeline` 的统一 ISA 语义源

- [ ] **步骤 5：运行共享语义 smoke 并再跑完整回归**

  运行：`cd myCPU && make tests/host/instruction_semantics_smoke && ./tests/host/instruction_semantics_smoke`

  预期：PASS

  运行：`cd myCPU && make test`

  预期：PASS

- [ ] **步骤 6：Commit**

  ```bash
  git add myCPU/src/isa/effects.h myCPU/src/isa/execution_context.h myCPU/src/isa/execution_context.cpp myCPU/src/isa/instruction_semantics.h myCPU/src/isa/instruction_semantics.cpp myCPU/src/cpu.cpp myCPU/src/exec/integer_ops.h myCPU/src/exec/integer_ops.cpp myCPU/src/exec/control_flow_ops.h myCPU/src/exec/control_flow_ops.cpp myCPU/src/exec/memory_ops.h myCPU/src/exec/memory_ops.cpp myCPU/src/exec/system_ops.h myCPU/src/exec/system_ops.cpp myCPU/tests/host/instruction_semantics_smoke.cpp myCPU/Makefile
  git commit -m "refactor(语义层): 接入共享 instruction semantics"
  ```

## 任务 3：把 `AddressSpace` 改成 result-based fault API

**文件：**

- 创建：`myCPU/tests/host/address_space_faults_smoke.cpp`
- 修改：`myCPU/src/mem/address_space.h`
- 修改：`myCPU/src/mem/address_space.cpp`
- 修改：`myCPU/src/exec/memory_ops.h`
- 修改：`myCPU/src/exec/memory_ops.cpp`
- 测试：`myCPU/tests/host/address_space_faults_smoke.cpp`

- [ ] **步骤 1：先写 `AddressSpace::AccessResult` smoke**

  新增 [address_space_faults_smoke.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/host/address_space_faults_smoke.cpp)，覆盖：

  - bare-mode unmapped load 的 access fault result
  - legacy `load()` wrapper 仍然会真正写 trap CSR
  - Sv39 non-canonical 地址返回 page-fault result
  - result API 不会直接写 trap CSR

- [ ] **步骤 2：运行新测试，确认它先失败**

  运行：`cd myCPU && make tests/host/address_space_faults_smoke`

  预期：FAIL，报缺少 `AccessResult`、`load_result()` 等接口。

- [ ] **步骤 3：实现 `AccessResult` 和 result API**

  在 [address_space.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/mem/address_space.h) 中新增：

  ```cpp
  struct AccessResult {
      bool ok{false};
      uint64_t value{0};
      TrapRequest fault{};
  };
  ```

  并实现：

  - `fetch32_result()`
  - `load_result()`
  - `store_result()`

  legacy wrapper `fetch32/load/store` 继续保留，用于当前 `functional` 兼容消费。

- [ ] **步骤 4：调整 `memory_ops` 与 `cpu.cpp` 的 fault 消费路径**

  确保：

  - 共享语义层和后续 `pipeline` 能消费 result API
  - 当前 `functional` 仍能通过 legacy wrapper 获得与旧行为一致的 trap 进入

- [ ] **步骤 5：运行专用 smoke 与完整回归**

  运行：`cd myCPU && make tests/host/address_space_faults_smoke && ./tests/host/address_space_faults_smoke`

  预期：PASS

  运行：`cd myCPU && make test`

  预期：PASS

- [ ] **步骤 6：Commit**

  ```bash
  git add myCPU/src/mem/address_space.h myCPU/src/mem/address_space.cpp myCPU/src/exec/memory_ops.h myCPU/src/exec/memory_ops.cpp myCPU/tests/host/address_space_faults_smoke.cpp
  git commit -m "refactor(地址空间): 引入 fault result 接口"
  ```

## 任务 4：接入 `PipelineBackend` 核心执行模型

**文件：**

- 创建：`myCPU/src/exec/pipeline_types.h`
- 创建：`myCPU/src/exec/pipeline_backend.h`
- 创建：`myCPU/src/exec/pipeline_backend.cpp`
- 创建：`myCPU/tests/host/pipeline_backend_smoke.cpp`
- 修改：`myCPU/src/platform/machine.h`
- 修改：`myCPU/src/platform/machine.cpp`
- 修改：`myCPU/src/trap.h`
- 修改：`myCPU/src/trap.cpp`
- 修改：`myCPU/src/devices/clint.h`
- 修改：`myCPU/src/devices/clint.cpp`
- 修改：`myCPU/src/devices/plic.h`
- 修改：`myCPU/src/devices/plic.cpp`
- 修改：`myCPU/src/devices/simple_storage.h`
- 修改：`myCPU/src/devices/simple_storage.cpp`
- 修改：`myCPU/src/devices/uart16550.h`
- 修改：`myCPU/src/devices/uart16550.cpp`
- 修改：`myCPU/src/mem/bus.h`
- 修改：`myCPU/src/mem/bus.cpp`
- 修改：`myCPU/src/mem/ram.h`
- 修改：`myCPU/src/mem/ram.cpp`
- 修改：`myCPU/Makefile`
- 测试：`myCPU/tests/host/pipeline_backend_smoke.cpp`

- [ ] **步骤 1：先写 pipeline backend smoke**

  新增 [pipeline_backend_smoke.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/host/pipeline_backend_smoke.cpp)，覆盖至少这些关键行为：

  - commit-boundary timer interrupt
  - illegal instruction 不允许 younger write 提交
  - fetch fault 不允许 wrong-path commit
  - `mret` 之后的 fetch fault 处理
  - forwarding、load-use interlock、redirect、flush 的基础路径

- [ ] **步骤 2：运行新测试，确认它先失败**

  运行：`cd myCPU && make tests/host/pipeline_backend_smoke`

  预期：FAIL，报缺少 `PipelineBackend` 或其依赖接口。

- [ ] **步骤 3：实现 `PipelineBackend`，但去掉 debug 依赖**

  从远端 `pipeline_backend.*` 迁移核心逻辑时，明确删除或不引入：

  - `BackendDebugSnapshot`
  - `debug_snapshot()`
  - `make_stage_snapshot()` 等纯调试输出拼装
  - 对 `myCPU/src/debug/*` 的任何 include 依赖

  保留的只有 pipeline core：

  - IF/ID/EX/MEM/WB
  - forwarding
  - load-use interlock
  - redirect / flush
  - commit-boundary trap / interrupt

- [ ] **步骤 4：把 `Machine` 扩成可持有 `PipelineBackend`**

  在 [machine.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/platform/machine.cpp) 的 `rebuild_backend()` 中新增 `Pipeline` 分支。

  如远端实现需要 `prepare_for_load()`、`reset_loaded_image()`、`clear_storage_image()` 等辅助入口，则按当前主线现有语义做最小重接，不得回退当前 storage 选项行为。

- [ ] **步骤 5：运行 pipeline smoke 与完整 `functional` 回归**

  运行：`cd myCPU && make tests/host/pipeline_backend_smoke && ./tests/host/pipeline_backend_smoke`

  预期：PASS

  运行：`cd myCPU && make test`

  预期：PASS，说明 pipeline core 的引入没有破坏默认 `functional` Phase 1 基线。

- [ ] **步骤 6：Commit**

  ```bash
  git add myCPU/src/exec/pipeline_types.h myCPU/src/exec/pipeline_backend.h myCPU/src/exec/pipeline_backend.cpp myCPU/src/platform/machine.h myCPU/src/platform/machine.cpp myCPU/src/trap.h myCPU/src/trap.cpp myCPU/src/devices/clint.h myCPU/src/devices/clint.cpp myCPU/src/devices/plic.h myCPU/src/devices/plic.cpp myCPU/src/devices/simple_storage.h myCPU/src/devices/simple_storage.cpp myCPU/src/devices/uart16550.h myCPU/src/devices/uart16550.cpp myCPU/src/mem/bus.h myCPU/src/mem/bus.cpp myCPU/src/mem/ram.h myCPU/src/mem/ram.cpp myCPU/tests/host/pipeline_backend_smoke.cpp myCPU/Makefile
  git commit -m "feat(流水线): 接入五级 pipeline backend 核心"
  ```

## 任务 5：正式接入 `--backend pipeline` 与 host-side 差分门禁

**文件：**

- 创建：`myCPU/tests/host/backend_differential_smoke.cpp`
- 创建：`myCPU/tests/host/backend_cli.sh`
- 修改：`myCPU/src/main.cpp`
- 修改：`myCPU/src/platform/machine.h`
- 修改：`myCPU/src/platform/machine.cpp`
- 修改：`myCPU/Makefile`
- 测试：`backend_differential_smoke.cpp`、`backend_cli.sh`

- [ ] **步骤 1：先写差分 smoke 和 CLI smoke**

  新增 [backend_differential_smoke.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/host/backend_differential_smoke.cpp)，比较 `functional` 与 `pipeline` 在提交态上的一致性，至少覆盖：

  - 基础整数 / 跳转 / CSR / 访存路径
  - illegal instruction
  - `ecall`
  - `mret`

  新增 [backend_cli.sh](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/host/backend_cli.sh)，覆盖：

  - backend 缺省值仍为 `functional`
  - `--backend functional` 输出正确
  - `--backend pipeline` 输出正确
  - 非法 backend 名称报 `unknown backend`

- [ ] **步骤 2：运行新测试，确认它们先失败**

  运行：`cd myCPU && make tests/host/backend_differential_smoke`

  预期：FAIL，报 `functional/pipeline` 差分不一致或缺少 CLI backend 入口。

  运行：`cd myCPU && sh tests/host/backend_cli.sh mycpu tests/asm/hello.elf`

  预期：FAIL，报 `--backend` 未实现或 `pipeline` 不可选。

- [ ] **步骤 3：在当前主线语义上实现 CLI backend 切换**

  修改 [main.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/main.cpp)：

  - 增加 `--backend functional|pipeline`
  - 保留当前 `-b`、`--disk`、`--disk-not-ready`、`--disk-bad-magic`
  - 不引入 `--debug-cli`

  修改 [machine.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/platform/machine.h) / [machine.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/platform/machine.cpp)，确保 backend 切换、镜像加载与 storage 附加在当前主线语义下正确工作。

- [ ] **步骤 4：把 host-side pipeline 门禁接进 `Makefile`**

  在 [Makefile](/home/liangjiaqi/projects/my_visual_CPU/myCPU/Makefile) 中新增：

  - `tests/host/*` 的构建规则
  - `test-host-instruction_semantics`
  - `test-host-address_space_faults`
  - `test-host-pipeline_backend`
  - `test-host-backend_differential`
  - `test-backend-cli`
  - `test-pipeline`

  **注意：**

  - 本轮 `test-pipeline` 只聚合 host-side pipeline 测试与 CLI smoke
  - 不照搬远端对全部 asm 样例运行 `--backend pipeline` 的 `test-pipeline-%` 方案
  - 不把 guest/runtime 回归迁移到 `pipeline` 门禁

- [ ] **步骤 5：运行本轮完整验收**

  运行：`cd myCPU && make test-pipeline`

  预期：PASS

  运行：`cd myCPU && make test`

  预期：PASS

- [ ] **步骤 6：Commit**

  ```bash
  git add myCPU/src/main.cpp myCPU/src/platform/machine.h myCPU/src/platform/machine.cpp myCPU/tests/host/backend_differential_smoke.cpp myCPU/tests/host/backend_cli.sh myCPU/Makefile
  git commit -m "feat(流水线): 正式接入 pipeline backend 选择与门禁"
  ```

## 任务 6：同步文档并做最终验证

**文件：**

- 修改：`myCPU/AGENTS.md`
- 修改：`readme.md`
- 修改：`docs/status/code_self_review_2026-03-24.md`（如需要）
- 修改：`docs/status/kernel_alpha_bringup_status.md`（仅在需要说明 pipeline 集成边界时）
- 测试：最终完整验证

- [ ] **步骤 1：更新对外与对内说明**

  在 [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md) 中补充：

  - `ExecutionBackend` / `PipelineBackend` 已接入
  - `functional` 仍是默认 reference backend
  - `pipeline` 当前正式门禁只到 host-side

  在 [readme.md](/home/liangjiaqi/projects/my_visual_CPU/readme.md) 中补充：

  - `--backend pipeline` 的使用方式
  - `test-pipeline` 的定位

  如果需要，在状态文档中补一条“pipeline core 已接入、`debug/frontend` 留待下一轮”的当时阶段状态。

- [ ] **步骤 2：运行最终验证**

  运行：`cd myCPU && make test-pipeline`

  运行：`cd myCPU && make test`

  预期：两者均 PASS

- [ ] **步骤 3：检查工作区与变更边界**

  运行：`git status --short`

  预期：

  - 只包含本轮 pipeline core、host tests 和文档同步改动
  - 不应出现 `myCPU/src/debug/*` 或 `frontend/*`
  - 不应出现对 `myCPU/guest/*` 的语义性改动

- [ ] **步骤 4：Commit**

  ```bash
  git add myCPU/AGENTS.md readme.md docs/status/code_self_review_2026-03-24.md docs/status/kernel_alpha_bringup_status.md
  git commit -m "docs(流水线): 同步 pipeline core 集成状态"
  ```

## 执行备注

- 本计划默认按小补丁推进，每个任务完成后都应先做局部验证，再做一次完整门禁。
- 如果在任务 4 或任务 5 中发现 `pipeline` 为了通过 host-side smoke 需要修改 guest/runtime 合同，应立即停止并回到 [pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md) 重新评估，不得直接改 guest 语义绕过问题。
- 如果在移植远端 `PipelineBackend` 时发现核心逻辑与 `debug_snapshot()` 强耦合，应优先删除调试输出拼装代码，而不是把 `debug/*` 一起带入本轮。
