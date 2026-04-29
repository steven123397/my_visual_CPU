# 主线 Wave 6 Slice A / JIT DBT Hot-path Evidence 计划

> **文档状态：** 执行中

## 文档定位

本文档用于执行主线 `Wave 6 / JIT / DBT` 的第一刀：`Slice A / hot-path evidence`。

本计划只做 hot-path / translation candidate 观察合同，不实现 JIT engine、DBT
translator、IR、block cache、host code emission、multicore、coherence 或新的 memory
consistency 模型。

## 关联文档

- 来源设计：
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 目标状态：
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 基于现有 profile / debug / probe 基础，固定一条可重复的 hot-path candidate 观察面。
- 证明候选区间输出不会改变 guest 可见语义。
- 为后续是否进入 translation contract 或 opt-in DBT prototype 提供证据。

## 完成定义

- 明确 hot-path candidate 的输入来源、排序口径和 fallback 行为。
- 至少补一条稳定回归，覆盖 candidate 输出或空候选 fallback。
- 默认 `make test` / `make test-pipeline` 不因本计划启用 JIT 或改变 guest 语义。
- 相关 status / design 回写完成。
- 本计划完成后归档到 [history_plan.md](history_plan.md)，并删除本活跃计划文件。

## 任务

### 任务 1：盘点现有 profile 与 probe 信号

**文件：**
- 阅读：`myCPU/src/exec/execution_profile.*`
- 阅读：`myCPU/src/debug/*`
- 阅读：`myCPU/tests/host/*execution_profile*`
- 阅读：`myCPU/tests/host/run_debug_cli_probe_test.py`
- 修改：`docs/status/mainline_status.md`

- [ ] **步骤 1：** 确认现有 profile 是否已经有足够的 PC / branch / trap / memory 统计入口。
- [ ] **步骤 2：** 明确 hot-path candidate 第一刀的排序口径，优先使用现有字段，不新造大 schema。
- [ ] **步骤 3：** 把任何必须延期的信号缺口写回 `mainline_status`。

### 任务 2：固定 hot-path candidate 观察合同

**文件：**
- 修改：待任务 1 盘点后确定，优先落在 `execution_profile`、debug snapshot 或 probe 文本输出的现有边界。
- 测试：待任务 1 盘点后确定，优先补最窄 host smoke / probe unittest。

- [ ] **步骤 1：** 先写失败测试，固定 candidate 输出或空候选 fallback。
- [ ] **步骤 2：** 运行最窄测试确认失败。
- [ ] **步骤 3：** 用最少实现接入只读 candidate 摘要。
- [ ] **步骤 4：** 运行最窄测试确认通过。

### 任务 3：守住默认路径和非目标边界

**文件：**
- 修改：`docs/design/wave6_jit_dbt_readiness_design.md`
- 修改：`docs/status/mainline_status.md`
- 测试：`myCPU` 默认门禁

- [ ] **步骤 1：** 确认默认 CLI / debug / pipeline 不启用 JIT 或 DBT 执行路径。
- [ ] **步骤 2：** 明确本轮没有 block cache、host code emission、multicore 或 coherence。
- [ ] **步骤 3：** 运行 `cd myCPU && make test`。
- [ ] **步骤 4：** 运行 `cd myCPU && make test-pipeline`。

### 任务 4：完成归档

**文件：**
- 修改：`docs/plan/history_plan.md`
- 修改：`docs/status/mainline_status.md`
- 删除：`docs/plan/mainline_wave6_jit_dbt_hot_path_evidence_slice_a_plan.md`

- [ ] **步骤 1：** 回写完成结果和剩余风险。
- [ ] **步骤 2：** 归档本计划。
- [ ] **步骤 3：** 删除本活跃计划文件。
- [ ] **步骤 4：** 运行 `git diff --check`。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
