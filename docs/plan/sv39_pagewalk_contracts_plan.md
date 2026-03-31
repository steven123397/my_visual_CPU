# Sv39 Page-Walk 合同补洞实现计划

> **文档状态：** 已完成

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 补齐 `Sv39` page-walk 对 superpage 对齐和 non-leaf PTE 保留位的合同验证，并把对应回归接入现有 asm 门禁。

**架构：** 保持当前 `AddressSpace` 的页表遍历框架不变，先用一条最小 asm 回归覆盖“misaligned superpage fault”和“non-leaf reserved bits fault”，再最小修正 page-walk 校验逻辑。优先把真实缺口补成稳定合同，不扩大功能面。

**技术栈：** C++17、RISC-V assembly、GNU Make

---

## 关联文档

- 来源设计：
  - [docs/design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
- 目标状态：
  - [docs/status/mainline_status.md](../status/mainline_status.md)

## 文件结构

### 预计修改

- `myCPU/tests/asm/sv39_pagewalk_contracts.S`
  新增 asm 回归，覆盖 superpage 对齐与 non-leaf reserved bits fault。
- `myCPU/Makefile`
  接入新 asm 测试与 pipeline 对称门禁。
- `myCPU/src/mem/address_space.cpp`
  最小补充 non-leaf PTE 保留位检查。
- `docs/status/mainline_status.md`
  如本轮形成新的稳定 hardening 结果，补充状态摘要。

## 完成定义

- misaligned 2MiB / 1GiB superpage 继续稳定触发 page fault。
- non-leaf PTE 上的保留位组合不会被误当成有效下级页表指针。
- 新 asm 回归接入 `make test` 和 `make test-pipeline`。
- `cd myCPU && make test` 与 `cd myCPU && make test-pipeline` 通过。

**实际结果：**
- 已新增 `myCPU/tests/asm/sv39_pagewalk_contracts.S`，覆盖 misaligned 2MiB superpage fault 和 non-leaf `U` 保留位 fault。
- 已在 `myCPU/src/mem/address_space.cpp` 补齐 non-leaf `U/A/D` 保留位检查，避免被误当成有效下级页表。
- 已把 `sv39_pagewalk_contracts` 接入 `make test` 和 `make test-pipeline`。
- 已通过 `cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。

### 任务 1：新增失败的 page-walk 合同回归

**文件：**
- 创建：`myCPU/tests/asm/sv39_pagewalk_contracts.S`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：编写最小回归**

回归至少覆盖：
- misaligned superpage 叶子 PTE 触发 page fault
- non-leaf PTE 携带保留位时触发 page fault

- [x] **步骤 2：运行单项回归验证失败**

运行：`cd myCPU && make test-sv39_pagewalk_contracts`

预期：FAIL，失败点来自当前缺失的 page-walk 合同。

- [x] **步骤 3：接入主门禁**

将 `sv39_pagewalk_contracts` 纳入 `make test` 和 `make test-pipeline`。

### 任务 2：最小修复 page-walk 校验

**文件：**
- 修改：`myCPU/src/mem/address_space.cpp`

- [x] **步骤 1：补 non-leaf PTE 保留位检查**

在页表遍历下降前，最小检查 non-leaf PTE 当前不支持的保留位组合，避免被误当成下级页表。

- [x] **步骤 2：重新运行单项回归**

运行：`cd myCPU && make test-sv39_pagewalk_contracts`

预期：PASS。

### 任务 3：验证与状态回写

**文件：**
- 可能修改：`docs/status/mainline_status.md`

- [x] **步骤 1：运行针对性验证**

运行：
- `cd myCPU && make test-sv39_pagewalk_contracts`
- `cd myCPU && make test-pipeline-sv39_pagewalk_contracts`
- `cd myCPU && make test-sv39_edge_faults`

- [x] **步骤 2：运行主门禁**

运行：
- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

- [x] **步骤 3：如需要，回写状态文档**

如果这轮形成新的 hardening 结果，则最小更新主线状态文档。
