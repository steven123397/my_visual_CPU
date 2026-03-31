# Phase 3-A 分支预测增强实现计划

> **文档状态：** 已完成（2026-03-27）

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 在不改变当前 `functional + shared InstructionSemantics` ISA 真值来源、也不改变 `pipeline` in-order 提交模型的前提下，为 `PipelineBackend` 接入首轮最小分支预测能力，并补齐 predictor、pipeline、debug / protocol 的验证闭环。

**架构：** 首轮采用“独立 predictor 子模块 + pipeline stage 预测元数据 + snapshot / protocol 最小可观察性”的切片。取指阶段先做轻量 control-flow 分类并查询 predictor，执行阶段在真实分支决议点更新 predictor，并仅在 mispredict 时复用现有 flush / redirect 机制恢复；`frontend` 继续只消费 richer snapshot，不主动承担 UI 改造。

**技术栈：** C++17、GNU Make、host-side g++ smoke tests、Node `--test`、RISC-V 交叉工具链（用于完整 `make test` / `make test-pipeline`）。

## 文档定位

本文档用于把 [phase3_branch_prediction_design.md](../design/phase3_branch_prediction_design.md) 收口成可执行任务清单，明确 `Phase 3-A` 第一轮要改哪些文件、以什么顺序落地、由哪些测试验证，以及完成后如何回写主线状态文档。

本文档只回答“怎么落地”。实时进度、风险变化与完成结果摘要以 [mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 来源设计：
  - [design/phase3_branch_prediction_design.md](../design/phase3_branch_prediction_design.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

---

## 参考文档

- [AGENTS.md](../../AGENTS.md)
- [myCPU/AGENTS.md](../../myCPU/AGENTS.md)
- [readme.md](../../readme.md)
- [pipeline_core_integration_plan.md](pipeline_core_integration_plan.md)

## 目标

- 为当前 `PipelineBackend` 引入最小但真实的分支预测，而不是继续维持“永远顺序取指 + execute redirect”。
- 保持 `pipeline` 仍是 5-stage in-order backend，不引入 rename、`ROB`、`LSQ` 或新的提交模型。
- 保持既有 trap / interrupt / CSR / MMIO / privilege differential 主干不回归。
- 为 predictor 补最小可观察性，让 host-side smoke、`debug_cli_smoke` 和调试服务能够看到 predictor 核心状态。
- 把首轮实现 ownership 收口在 `myCPU/src/exec/*`、`tests/host/*` 与 snapshot / protocol 字段，不主动扩前端 UI 结构。

## 完成定义

- `pipeline` 拥有独立 predictor 子模块，且 predictor 状态不再四散耦合在 `pipeline_backend.cpp` 的局部布尔分支里。
- 取指阶段能够基于 predictor 改变 `fetch_pc_` 选择；执行阶段能够在分支决议后更新 predictor，并只在 mispredict 时触发 flush / redirect。
- `pipeline_backend_smoke`、`backend_differential_smoke`、`debug_cli_smoke` 均补上 predictor 相关场景，并通过。
- `make test`、`make test-pipeline`、`cd frontend && node --test` 继续通过。
- 文档完成同步：
  - [myCPU/AGENTS.md](../../myCPU/AGENTS.md)
  - [readme.md](../../readme.md)
  - [mainline_status.md](../status/mainline_status.md)

## 文件结构

### 新增文件

- `myCPU/src/exec/branch_predictor.h`
  `Phase 3-A` predictor 对外接口、查询结果、更新输入、统计信息与最小 debug state 定义。首轮把 predictor 的职责收口在这里，而不是把状态混回 `PipelineBackend`。
- `myCPU/src/exec/branch_predictor.cpp`
  predictor 实现。首轮推荐使用“PC 索引 + 2-bit saturating counter + 最小 target 记忆”的简单动态预测器，同时允许对 `jal` 使用静态 predict-taken，对 `jalr` 维持 not-predicted。
- `myCPU/tests/host/predictor_smoke.cpp`
  predictor 独立 smoke，覆盖 `query / update / reset / stats` 的最小闭环。

### 重点修改文件

- `myCPU/src/exec/pipeline_types.h`
  为 stage slot 增加预测元数据，例如“是否命中过 predictor”“是否预测 taken”“预测目标”“分支类别/分支 PC”。
- `myCPU/src/exec/pipeline_backend.h`
  接入 predictor 成员、必要的 helper 声明，以及与 debug snapshot 相关的最小状态缓存。
- `myCPU/src/exec/pipeline_backend.cpp`
  首轮核心改动面：`step_if()` 查询 predictor、`step_ex()` 做真实分支决议与 predictor 更新，并把“无条件 redirect”改成“仅 mispredict 才 redirect / flush”。
- `myCPU/src/debug/debug_snapshot.h`
  新增 predictor debug snapshot 字段，承载模式、最近一次预测、最近一次 mispredict 与统计计数。
- `myCPU/src/debug/debug_protocol.cpp`
  把 predictor 字段序列化到 `snapshot` JSON 中，维持现有字段兼容性。
- `myCPU/tests/host/pipeline_backend_smoke.cpp`
  增加 predictor 命中、mispredict flush、predictor reset 的 backend 行为 smoke。
- `myCPU/tests/host/backend_differential_smoke.cpp`
  增加 predictor 参与但 architected 结果仍与 `functional` 一致的差分场景。
- `myCPU/tests/host/debug_cli_smoke.cpp`
  增加 predictor JSON 字段存在性与关键值变化的 smoke。
- `myCPU/Makefile`
  接入 predictor 源文件、新 host smoke，以及 `test-pipeline` 的 predictor 门禁。
- `myCPU/AGENTS.md`
  回写 simulator 基线，明确 `Phase 3-A` predictor 的边界和验证要求。
- `readme.md`
  只补最小外部说明，强调 `pipeline` 已具备分支预测增强与最小可观察性；不展开实现流水账。
- `docs/status/mainline_status.md`
  回写 Phase 3-A 首轮完成结果、验证基线与仍有效风险。

### 本轮明确不改

- `myCPU/src/isa/*`
  predictor 不得成为新的 ISA 语义来源。
- `myCPU/src/trap.cpp`
  不顺手改 trap / interrupt 提交模型。
- `myCPU/guest/*`
  不在首轮预测器工作里引入 guest runtime 或 `kernel_alpha` 语义变化。
- `frontend/app/*`
  不主动重做前端界面；首轮只允许 richer snapshot 被动透传。
- `frontend/server/debug_server.mjs`
  调试服务只做透传，不承接 predictor 业务逻辑。

## 任务

### 任务 1：建立独立 predictor 子模块与最小 smoke

**文件：**
- 创建：`myCPU/src/exec/branch_predictor.h`
- 创建：`myCPU/src/exec/branch_predictor.cpp`
- 创建：`myCPU/tests/host/predictor_smoke.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：先写失败的 predictor smoke**

  在 `tests/host/predictor_smoke.cpp` 中先写最小断言，至少覆盖：

  - 冷启动时，条件分支默认不预测 taken。
  - 对同一 PC 连续两次 taken update 后，query 能返回 taken 与稳定 target。
  - taken-trained 的分支在收到 not-taken update 后，统计计数与下次查询结果按预期变化。
  - `reset()` 会清空表项与统计计数。

  建议先按下面的接口假设写测试：

  ```cpp
  BranchPredictor predictor;
  const auto first = predictor.query(pc, raw_branch);
  predictor.update({.pc = pc, .raw = raw_branch, .taken = true, .target = taken_pc});
  const auto second = predictor.query(pc, raw_branch);
  ```

- [x] **步骤 2：运行测试验证失败**

  运行：`cd myCPU && make test-host-predictor_smoke`

  预期：

  - FAIL
  - 报错指向缺少 `branch_predictor.*`、缺少 `query/update/reset`，或 Makefile 尚未接入该测试

- [x] **步骤 3：实现最小 predictor 模块**

  在 `branch_predictor.h/.cpp` 中实现首轮最小接口：

  ```cpp
  struct PredictorQueryResult {
      bool valid{false};
      bool predicted_taken{false};
      uint64_t predicted_target{0};
      bool table_hit{false};
  };

  struct PredictorUpdate {
      uint64_t pc{0};
      uint32_t raw{0};
      bool taken{false};
      uint64_t target{0};
  };

  class BranchPredictor {
  public:
      PredictorQueryResult query(uint64_t pc, uint32_t raw) const;
      void update(const PredictorUpdate& update);
      void reset();
      PredictorStats stats() const;
  };
  ```

  首轮约束明确写死在实现里：

  - 条件分支使用 PC 索引的 2-bit saturating counter。
  - `jal` 允许静态 predict-taken，并返回可计算的 PC-relative target。
  - `jalr` 首轮不预测，统一走顺序取指。
  - predictor 只关心 next-PC 建议和统计，不持有 architected 状态。

- [x] **步骤 4：运行独立 smoke 验证通过**

  运行：`cd myCPU && make test-host-predictor_smoke`

  预期：

  - PASS
  - predictor 的 `query / update / reset / stats` 行为可复现、可解释

- [x] **步骤 5：Commit**

  ```bash
  git add myCPU/src/exec/branch_predictor.h myCPU/src/exec/branch_predictor.cpp myCPU/tests/host/predictor_smoke.cpp myCPU/Makefile
  git commit -m "feat(预测器): 引入最小 branch predictor 子模块"
  ```

### 任务 2：把 predictor 接到 fetch / execute / flush 主路径

**文件：**
- 修改：`myCPU/src/exec/pipeline_types.h`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/tests/host/pipeline_backend_smoke.cpp`

- [x] **步骤 1：先写失败的 pipeline smoke**

  在 `pipeline_backend_smoke.cpp` 中新增 3 组最小场景：

  - 可训练的条件分支循环：steady-state taken 命中后，不应继续产生“每次都 flush”的旧路径行为。
  - 循环退出时的 not-taken：应触发一次可观察的 mispredict flush，并回到正确 fallthrough。
  - 新建 `PipelineBackend` 后 predictor 应回到初始状态，不能沿用旧会话训练结果。

  首轮 smoke 不追求性能计数，只检查：

  - `pc`
  - `instret`
  - `redirect / trap_flush`
  - predictor 统计和最近一次预测信息

- [x] **步骤 2：运行测试验证失败**

  运行：`cd myCPU && make test-host-pipeline_backend_smoke`

  预期：

  - FAIL
  - 旧实现仍会把所有 taken branch / jump 都当成 execute-time redirect，无法满足 predict-hit / mispredict 断言

- [x] **步骤 3：在 `PipelineBackend` 中接入 predictor**

  按以下切片修改：

  - `pipeline_types.h`
    为 `StageSlot` 新增预测元数据，例如：

    ```cpp
    struct PredictionMeta {
        bool valid{false};
        bool predicted_taken{false};
        uint64_t predicted_target{0};
        bool table_hit{false};
    };
    ```

  - `step_if()`
    取指成功后，对 `raw` 做轻量 control-flow 分类，查询 predictor，并在命中 taken 时直接推进 `fetch_pc_`。
  - `step_ex()`
    根据 `InsnEffects` 的真实分支结果与 `PredictionMeta` 做比较：
    - 预测正确：不触发 flush
    - 预测错误：沿现有 `redirect_pending_ + next_if_id_ / next_id_ex_` 机制恢复
  - `reset_stage_state()`
    只清空 stage / fetch-fault 状态，不重置 predictor；predictor reset 只能发生在 backend 新建或显式 `reset()`

- [x] **步骤 4：运行针对性验证**

  运行：`cd myCPU && make test-host-predictor_smoke test-host-pipeline_backend_smoke`

  预期：

  - PASS
  - predictor 命中、mispredict flush、backend 重建后 reset 三条最小路径都成立

- [x] **步骤 5：Commit**

  ```bash
  git add myCPU/src/exec/pipeline_types.h myCPU/src/exec/pipeline_backend.h myCPU/src/exec/pipeline_backend.cpp myCPU/tests/host/pipeline_backend_smoke.cpp
  git commit -m "feat(pipeline): 接入首轮分支预测取指与恢复路径"
  ```

### 任务 3：补齐 predictor 的差分验证与 debug / protocol 可观察性

**文件：**
- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/tests/host/backend_differential_smoke.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：先写失败的差分和 CLI smoke**

  先补两类失败测试：

  - `backend_differential_smoke.cpp`
    新增 `predictable_branch_loop` 或等价场景，验证 predictor 参与后，`functional` 与 `pipeline` 的最终 `pc / instret / gpr / csr / watched memory` 仍一致。
  - `debug_cli_smoke.cpp`
    新增 predictor 字段断言，至少覆盖：
    - `mode`
    - `last_prediction_pc`
    - `last_prediction_target`
    - `last_prediction_correct`
    - `total_predictions`
    - `correct_predictions`
    - `mispredictions`
    - 最近一次 mispredict redirect 信息

- [x] **步骤 2：运行测试验证失败**

  运行：`cd myCPU && make test-host-backend_differential_smoke test-host-debug_cli_smoke`

  预期：

  - FAIL
  - 旧 `PipelineDebugSnapshot` / JSON 中没有 predictor 字段，且差分场景还没接 predictor 路径

- [x] **步骤 3：实现 predictor debug snapshot / protocol**

  在 `debug_snapshot.h` 中新增最小字段，例如：

  ```cpp
  struct PredictorDebugSnapshot {
      std::string mode{};
      bool last_prediction_valid{false};
      bool last_prediction_taken{false};
      bool last_prediction_correct{false};
      uint64_t last_prediction_pc{0};
      uint64_t last_prediction_target{0};
      bool last_mispredict_valid{false};
      uint64_t last_mispredict_pc{0};
      uint64_t last_mispredict_target{0};
      uint64_t total_predictions{0};
      uint64_t correct_predictions{0};
      uint64_t mispredictions{0};
  };
  ```

  然后：

  - 在 `PipelineBackend::debug_snapshot()` 中填这些字段
  - 在 `debug_protocol.cpp` 的 `snapshot_json()` 中稳定序列化
  - 在 `Makefile` 中把 `test-host-predictor_smoke` 挂进 `test-pipeline`

- [x] **步骤 4：运行差分、CLI 和前端基线**

  运行：

  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd frontend && node --test`

  预期：

  - PASS
  - richer snapshot 不要求改 `frontend/app/*`，但调试服务和前端状态测试要继续兼容

- [x] **步骤 5：Commit**

  ```bash
  git add myCPU/src/debug/debug_snapshot.h myCPU/src/debug/debug_protocol.cpp myCPU/src/exec/pipeline_backend.h myCPU/src/exec/pipeline_backend.cpp myCPU/tests/host/backend_differential_smoke.cpp myCPU/tests/host/debug_cli_smoke.cpp myCPU/Makefile
  git commit -m "test(预测器): 补齐差分与 debug 可观察性门禁"
  ```

### 任务 4：同步文档、完成主线回写并守住全量基线

**文件：**
- 修改：`myCPU/AGENTS.md`
- 修改：`readme.md`
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/plan/phase3_branch_prediction_plan.md`

- [x] **步骤 1：更新对外与内部文档**

  文档同步只写“当前已落地事实”，不写实现流水账：

  - `myCPU/AGENTS.md`
    增补 `Phase 3-A` predictor 的边界、局部规则和新增验证要求。
  - `readme.md`
    只补最小外部描述，例如 `pipeline` 已具备首轮 branch prediction 与 predictor snapshot 字段。
  - `docs/status/mainline_status.md`
    回写本轮结果摘要、关键历史节点与剩余风险。

- [x] **步骤 2：运行全量验证基线**

  运行：

  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
  - `cd frontend && node --test`

  预期：

  - 全部 PASS
  - 新 predictor 不得破坏既有 guest、trap / interrupt、MMIO、privilege 主门禁

- [x] **步骤 3：回写计划完成态**

  完成本计划后，必须同步：

  - 把本文档头部改为“已完成”
  - 勾完全部 checklist
  - 在 `mainline_status.md` 中写入：
    - 完成结果摘要
    - 关键历史节点
    - 仍然有效的剩余风险

- [x] **步骤 4：Commit**

  ```bash
  git add myCPU/AGENTS.md readme.md docs/status/mainline_status.md docs/plan/phase3_branch_prediction_plan.md
  git commit -m "docs(phase3): 回写分支预测增强完成态"
  ```

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）

## 执行结果

- `2026-03-27` 已完成 `Phase 3-A` 第一轮分支预测增强：引入独立 `branch_predictor` 子模块，把 predictor 接到 `PipelineBackend` 的 fetch / execute / mispredict 恢复主路径，并补齐 `predictor_smoke`、`pipeline_backend_smoke`、`backend_differential_smoke`、`debug_cli_smoke` 与 `frontend` 兼容性门禁。
- 已完成主线文档回写：
  - [myCPU/AGENTS.md](../../myCPU/AGENTS.md)
  - [readme.md](../../readme.md)
  - [mainline_status.md](../status/mainline_status.md)
- 完成态验证基线：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
  - `cd frontend && node --test`
