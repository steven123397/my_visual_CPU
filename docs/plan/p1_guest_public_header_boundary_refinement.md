# guest 公共头文件边界收口计划

> **文档状态：** 执行中

## 文档定位

本文档用于记录 `P1-5` guest 公共头文件边界收口如何落地、当前做到哪一步，以及完成后需要如何回写状态文档并归档。

## 关联文档

- 来源设计：无，本计划直接落实已确认任务，并对齐 [status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 中仍未完成的 `P1-5`
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/project_priority_roadmap.md](../status/project_priority_roadmap.md)

## 目标

- 收紧 `kernel_runtime.h`、`supervisor_runtime.h` 与 `user_program_smoke.h` 对可变内部布局的暴露，优先减少跨模块直接读写字段的场景。
- 让生产代码和非同模块单测更多通过窄 helper / accessor 观察状态，而不是把结构边界继续等同于 `struct` layout。
- 本轮不把这些 public struct 全部改成 opaque handle，也不顺手扩到新的功能面或更大范围的 guest 生命周期重构。

## 完成定义

- `kernel_runtime` / `supervisor_runtime` 的跨模块访问改成以窄 helper 为主，外部代码不再直接读写 `trap_context`、`address_space`、`interrupts` 以及 interrupt counter / handler 配置细节。
- `user_program_smoke` 的对外使用改成以窄状态查询或组合 helper 为主，外部代码和对应单测不再依赖 `smoke.program`、`remap_region`、`invalid_region`、`remap_object` 这些内部布局。
- 相关单测与 guest smoke 保持通过，`status` 文档回写当前收口结果、验证范围与剩余风险。

## 任务

### 任务 1：收口 `kernel_runtime` / `supervisor_runtime` 的状态访问面

**文件：**
- 修改：`myCPU/guest/include/kernel_runtime.h`
- 修改：`myCPU/guest/include/supervisor_runtime.h`
- 修改：`myCPU/guest/kernel/kernel_runtime.c`
- 修改：`myCPU/guest/kernel/supervisor_runtime.c`
- 修改：`myCPU/guest/kernel_alpha/common.c`
- 修改：`myCPU/guest/kernel_alpha/interrupt_contract.c`
- 修改：`myCPU/guest/kernel/monitor_commands.c`
- 修改：`myCPU/tests/unit/kernel_runtime.c`
- 修改：`myCPU/tests/unit/supervisor_runtime.c`
- 修改：`myCPU/tests/unit/kernel_alpha_common.c`
- 修改：`myCPU/tests/unit/kernel_alpha_interrupt.c`
- 修改：`myCPU/tests/unit/monitor_commands.c`

- [ ] **步骤 1：** 盘点 `kernel_runtime_t` 与 `supervisor_runtime_interrupt_state_t` 当前仍被跨模块直接读写的字段，区分“生产调用必须保留的观察面”和“只是在单测里偷看内部布局”的场景。
- [ ] **步骤 2：** 在不把 struct 彻底 opaque 化的前提下，补最小 accessor / reset / bind / counter helper，并把 guest 生产代码迁到这些窄接口上。
- [ ] **步骤 3：** 重写相关单测，让它们优先通过 helper 或行为合同观察状态；只有同模块测试确实需要时，才保留最小直接布局检查。

### 任务 2：收口 `user_program_smoke` 的公共状态面

**文件：**
- 修改：`myCPU/guest/include/user_program_smoke.h`
- 修改：`myCPU/guest/kernel/user_program_smoke.c`
- 修改：`myCPU/guest/kernel/supervisor_demo_smoke.c`
- 修改：`myCPU/tests/unit/user_program_smoke.c`

- [ ] **步骤 1：** 明确 `user_program_smoke_t` 当前对外真实需要暴露的只有哪些观察面，区分 `supervisor_demo_smoke` 与单测分别依赖了什么。
- [ ] **步骤 2：** 为 `user_program_smoke` 增加最小状态查询 / 组合 helper，把 `supervisor_demo_smoke` 及相关调用从 `smoke.program` 等直接字段访问迁走。
- [ ] **步骤 3：** 调整 `user_program_smoke` 单测，让 init、prepare rollback 和后续新增状态检查优先走 helper 可见合同，而不是继续把测试绑死在内部布局上。

### 任务 3：验证与文档回写

**文件：**
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/status/project_priority_roadmap.md`

- [ ] **步骤 1：** 运行本轮直接相关门禁，至少覆盖 `test-unit-supervisor_runtime`、`test-unit-kernel_runtime`、`test-unit-kernel_alpha_common`、`test-unit-kernel_alpha_interrupt`、`test-unit-monitor_commands`、`test-unit-user_program_smoke`、`test-guest-supervisor_demo`、`test-guest-kernel_alpha_demo` 与 `test-guest-kernel_alpha_fault_demo`。
- [ ] **步骤 2：** 在 `mainline_status.md` 回写本轮 `P1-5` 的收口结果、验证范围与仍然有效的剩余风险。
- [ ] **步骤 3：** 在 `project_priority_roadmap.md` 同步更新 `P1-5` 状态；若本计划完成，追加归档到 `history_plan.md` 并删除本文件。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
