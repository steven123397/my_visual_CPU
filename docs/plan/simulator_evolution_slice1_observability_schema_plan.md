# simulator-evolution Slice 1 observability schema 计划

> **文档状态：** 执行中

## 文档定位

本文档记录 `simulator-evolution` 第一切片如何落地。第一切片只处理统一
observability schema 的设计边界、现状盘点和最小迁移入口，不把所有 profile /
trace / Evidence Drawer 一次性改完。

每一个 `simulator-evolution` 切片完成时，都必须额外执行“设计文档口径同步”：
把本切片落地后的真实边界、非目标和验证证据，回写到对应 `docs/design/` 文档中。
不能只更新 status / history，也不能让设计文档继续保留旧口径。

## 关联文档

- 来源设计 / 路线：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/wave5_cache_memory_system_design.md](../design/wave5_cache_memory_system_design.md)
  - [../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
- 目标状态：
  - [../status/simulator_evolution_status.md](../status/simulator_evolution_status.md)

## 目标

- 建立统一 observability schema 的长期设计文档入口。
- 盘点当前已有 observation producer / consumer，明确哪些字段是稳定合同、哪些只是局部调试输出。
- 选择第一批迁移候选，但不在本切片里强行迁移所有模块。
- 固定后续每个 `simulator-evolution` 切片的收尾纪律：实现 / 文档 / status / history 必须同步。

## 完成定义

- 新增或更新 `docs/design/simulator_evolution_observability_schema_design.md`。
- 设计文档至少覆盖事件身份、来源、时间顺序、subject、effect、payload、schema version、
  与 Evidence Drawer / debug probe / profile 的关系，以及明确非目标。
- 当前 `ExecutionProfile`、memory observation、shadow cache、JIT / DBT dispatch、AI profile
  和 frontend Evidence Drawer 的现状映射已经记录。
- 已选出第一批最多 2 个低耦合迁移候选，并说明验证命令。
- [../status/simulator_evolution_status.md](../status/simulator_evolution_status.md) 已回写完成结果和下一切片。
- 完成后已把摘要追加到 [history_plan.md](history_plan.md)，并删除本计划文件。

## 任务

### 任务 1：锁定上下文和工作区

**文件：**
- 读取：`AGENTS.md`
- 读取：`docs/AGENTS.md`
- 读取：`myCPU/AGENTS.md`
- 读取：`docs/status/mainline_status.md`
- 读取：`docs/status/simulator_evolution_status.md`

- [ ] **步骤 1：确认 worktree / branch**
  - 运行：`git status --short --branch`
  - 预期：位于 `.worktrees/simulator-evolution`，分支为 `simulator-evolution`。
- [ ] **步骤 2：确认本切片非目标**
  - 不改默认 backend。
  - 不改 guest 可见语义。
  - 不把 `kernel_alpha` review 或 Linux syscall breadth 纳入本切片。
- [ ] **步骤 3：确认文档角色**
  - 长期边界写入 `docs/design/`。
  - 当前事实写入 `docs/status/simulator_evolution_status.md`。
  - 执行 checklist 只保留在本文档，完成后归档到 `history_plan.md`。

### 任务 2：盘点现有 observation producer / consumer

**文件：**
- 读取：`myCPU/src`
- 读取：`frontend`
- 读取：`docs/design/wave6_jit_dbt_readiness_design.md`
- 读取：`docs/design/wave5_cache_memory_system_design.md`
- 读取：`docs/design/post_wave7_frontend_lab_product_design.md`
- 读取：`docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`

- [ ] **步骤 1：搜索现有可观察性关键词**
  - 运行：
    ```bash
    rg -n "ExecutionProfile|memory observation|shadow_cache|jit_dispatch|dispatch summary|AiAcceleratorProfile|Evidence Drawer|profile" myCPU frontend docs/design
    ```
  - 预期：得到 producer / consumer 候选清单。
- [ ] **步骤 2：按来源分类**
  - `execution-profile`
  - `memory-observation`
  - `cache-shadow`
  - `jit-dbt-dispatch`
  - `ai-accelerator-profile`
  - `frontend-evidence`
- [ ] **步骤 3：标注合同等级**
  - `stable-contract`：已有测试或文档直接依赖的字段。
  - `diagnostic-output`：只用于人读的调试输出。
  - `candidate-for-schema`：适合迁入统一 schema 的字段。

### 任务 3：建立 observability schema 设计文档

**文件：**
- 创建：`docs/design/simulator_evolution_observability_schema_design.md`
- 修改：`docs/index.md`

- [ ] **步骤 1：创建设计文档骨架**
  - 必须包含：
    - 文档定位
    - 关联文档
    - 背景与问题
    - 目标
    - 非目标
    - 事件模型
    - 版本与兼容策略
    - 现有 producer / consumer 映射
    - 首批迁移候选
    - 验证策略
- [ ] **步骤 2：定义统一事件最小字段**
  - 至少包括：
    - `schema_version`
    - `event_id`
    - `source`
    - `phase`
    - `subject`
    - `timestamp_or_step`
    - `effect`
    - `payload`
    - `evidence_ref`
- [ ] **步骤 3：写清非目标**
  - 不把统一 schema 变成 guest ABI。
  - 不要求所有模块一次性迁移。
  - 不让 frontend 反向成为执行语义来源。
  - 不用 schema 抹平不同模块真实语义差异。
- [ ] **步骤 4：同步索引**
  - 在 `docs/index.md` 的 `simulator-evolution` 专题入口中加入新设计文档链接。

### 任务 4：选择第一批迁移候选

**文件：**
- 修改：`docs/design/simulator_evolution_observability_schema_design.md`
- 修改：`docs/status/simulator_evolution_status.md`

- [ ] **步骤 1：只选择最多 2 个低耦合候选**
  - 推荐候选顺序：
    1. debug / probe 只读事件摘要。
    2. JIT / DBT dispatch summary。
    3. memory observation 读侧映射。
- [ ] **步骤 2：为每个候选写明验证命令**
  - debug / probe 候选优先：
    `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`
  - JIT / DBT 候选优先：
    `cd myCPU && make test-host-dbt_runtime_harness_smoke`，若目标不存在则先改用当前 Makefile 中最窄 DBT host smoke。
- [ ] **步骤 3：写明暂不迁移项**
  - AI profile、frontend Evidence Drawer 或 pipeline 深层统计如果涉及范围过宽，保留为后续切片。

### 任务 5：完成态同步与归档

**文件：**
- 修改：`docs/design/simulator_evolution_observability_schema_design.md`
- 修改：`docs/status/simulator_evolution_status.md`
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/simulator_evolution_slice1_observability_schema_plan.md`

- [ ] **步骤 1：设计文档口径同步**
  - 把本切片实际完成的边界、非目标、已验证命令和剩余风险写回设计文档。
- [ ] **步骤 2：状态文档回写**
  - 在 `simulator_evolution_status.md` 中记录完成结果、关键历史节点和下一切片。
- [ ] **步骤 3：归档计划**
  - 向 `history_plan.md` 追加完成时间、完成内容和验证摘要。
  - 删除本计划文件。
- [ ] **步骤 4：提交前验证**
  - 运行：
    ```bash
    git diff --check
    git diff --cached --check
    ```

## 完成态回写要求

- 全部 checklist 必须勾完。
- 必须同步更新对应 `docs/design/` 文档的描述和口径。
- 对应 `status` 文档必须增加完成结果摘要、关键历史节点和仍然有效的剩余风险。
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
