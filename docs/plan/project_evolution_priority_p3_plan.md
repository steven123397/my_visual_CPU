# PROJECT_EVOLUTION P3 实现计划

> **文档状态：** 执行中

## 文档定位

本文档承接 [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
中 `P3` 优先级事项，用于把远期形式化占位、pipeline 诚实标注、状态文档治理和
showcase 冻结归档拆成可执行 checklist。本文主要是治理和设计边界计划，不启动新的
模拟器执行后端或大规模功能实现。

## 关联文档

- 来源设计：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/wave7_productization_and_showcase_design.md](../design/wave7_productization_and_showcase_design.md)
- 目标状态：
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 给 `InstructionSemantics` 的表驱动 / DSL / 半结构化方向留下清晰占位。
- 明确 pipeline 后端是教学级 OoO 观察面、性能估算沙盘，还是未来可信微架构模型候选。
- 建立专项 status 的边界自审规则，避免维护态文档继续承担实时主线职责。
- 将课程结题 showcase 冻结为 v1 归档，避免展示材料持续侵入主线 status / design。

## 完成定义

- `InstructionSemantics` 未来形式在设计文档中有明确位置，且不会改变当前
  `InstructionSemantics + functional backend` 作为 ISA 真值来源的口径。
- pipeline 设计文档明确说明当前可信边界、不可作为性能真值的范围，以及如果要参数化
  需要先补哪些差分门禁。
- status 文档有季度自审规则；进入维护态的专项 status 有归档或降频维护路径。
- showcase 文档明确 v1 课程结题归档边界；新的工程 / 研究展示线需要另起设计或计划。

## 任务

### 任务 1：`InstructionSemantics` 形式演进占位

**文件：**
- 创建：按结论需要新增 ISA semantics future design
- 修改：
  - `docs/design/regression_completion_criteria.md`
  - `docs/status/mainline_status.md`
  - `myCPU/src/isa/`
  - `myCPU/tests/host/`

- [ ] **步骤 1：** 盘点当前 `InstructionSemantics`、functional backend、pipeline 和
      JIT/DBT dry-run 如何消费 ISA 语义。
- [ ] **步骤 2：** 写入长期占位：表驱动、DSL、半结构化 record 或外部形式化导出的候选
      形态，只记录边界和迁移前置条件。
- [ ] **步骤 3：** 明确短期不改变 ISA 真值来源，不为了占位改写现有 decoder 或执行路径。
- [ ] **步骤 4：** 如新增设计文档，同步 `docs/index.md` 和 mainline status 链接。

### 任务 2：pipeline 后端诚实标注

**文件：**
- 创建：无
- 修改：
  - `docs/design/phase3_ooo_execution_model_design.md`
  - `docs/design/pipeline_speculation_contracts.md`
  - `docs/status/mainline_status.md`
  - `README.md`

- [ ] **步骤 1：** 查清 README、design、status 和 showcase 中对 pipeline 的公开描述。
- [ ] **步骤 2：** 统一措辞：当前 pipeline 不作为微架构性能真值；如果描述为参数化沙盘，
      必须列出尚未完成的参数和 guardrail。
- [ ] **步骤 3：** 若选择推进性能估算方向，另起 P2/P3 后续计划；本文只完成标注和边界。
- [ ] **步骤 4：** 运行文档链接和 diff 检查，避免 stale anchor 或重复事实来源。

### 任务 3：专项 status 边界治理

**文件：**
- 创建：无
- 修改：
  - `AGENTS.md`
  - `docs/AGENTS.md`
  - `docs/status/mainline_status.md`
  - `docs/status/kernel_alpha_status.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `docs/status/linux_distribution_platform_status.md`
  - `docs/status/code_reself_status.md`

- [ ] **步骤 1：** 列出所有专项 status 的当前状态：活跃、维护、历史归档、只保留长期观察。
- [ ] **步骤 2：** 写入季度自审规则：维护态 status 应降频更新，完成态 checklist 必须归档到
      `history_plan.md`，不继续堆在 status。
- [ ] **步骤 3：** 对每份专项 status 做最小整理，只删除或移动已明显过期的执行流水账，
      不改写仍有效的当前事实。
- [ ] **步骤 4：** 更新 `docs/index.md` 导航，保证 status 入口与实际维护状态一致。

### 任务 4：showcase v1 冻结归档

**文件：**
- 创建：按结论需要新增 showcase v1 归档说明
- 修改：
  - `docs/showcase/README.md`
  - `docs/design/wave7_productization_and_showcase_design.md`
  - `docs/status/mainline_status.md`
  - `docs/index.md`
  - `README.md`

- [ ] **步骤 1：** 盘点课程结题 PPT、讲稿、截图、HTML 预览页和展示脚本当前入口。
- [ ] **步骤 2：** 在 showcase README 中标注 v1 课程结题归档边界和最后维护口径。
- [ ] **步骤 3：** 如果需要新的工程 / 研究 showcase，先写独立 design 或 plan，不复用
      课程结题 showcase 作为实时状态入口。
- [ ] **步骤 4：** 检查 README / docs index，确保读者能区分课程归档材料和当前工程路线。

## 验证基线

- `git diff --check`
- 文档新增、重命名或删除后，检查 [../index.md](../index.md) 是否包含入口。
- 若触碰 README，人工检查 README 是否仍把项目描述为已可运行的模拟器原型。
- 若触碰代码路径，仅允许为梳理引用而做只读检查；任何实现改动必须另起实现计划和测试。

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/mainline_status.md](../status/mainline_status.md) 必须记录 P3 治理完成摘要、
  仍然有效的远期占位和不启动的范围。
- 如新增设计文档，必须同步 [../index.md](../index.md)。
- 需要把“完成时间 + 完成内容 + 必要过程摘要”追加到
  [history_plan.md](history_plan.md)。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
