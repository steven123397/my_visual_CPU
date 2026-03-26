# Sv39 MPRV 数据访存语义实现计划

> **文档状态：** 已完成

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 补齐 `M-mode` 下 `mstatus.MPRV=1` 时的 Sv39 数据访存语义，使 `load/store` 按 `MPP` 指定的有效特权级走地址翻译与权限检查。

**架构：** 保持当前 `functional` reference path 与共享 `AddressSpace` 结构不变，只在地址翻译与权限判定路径上引入“数据访存有效特权级”概念。首轮通过一条最小 asm 回归稳定复现缺口，再做最小实现修复，并确保现有 `Sv39` 与 privilege 门禁不回归。

**技术栈：** C++17、RISC-V assembly、GNU Make

---

## 文档定位

这是一份围绕当前 `privilege / Sv39` 主线缺口的最小执行计划。实时状态以后续代码、测试结果和相关 `status` 文档为准。

## 关联文档

- 来源设计：
  - [docs/design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)
  - [docs/design/cpp_refactor_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/cpp_refactor_design.md)
- 目标状态：
  - [docs/status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)

## 文件结构

### 预计修改

- `myCPU/tests/asm/sv39_mprv.S`
  新增 asm 回归，覆盖 `MPRV + MPP=S/U` 对数据访存翻译与权限的影响。
- `myCPU/Makefile`
  接入新 asm 测试与 `make test` 聚合门禁。
- `myCPU/src/arch/csr_file.h`
  补 `MSTATUS_MPRV` 常量定义。
- `myCPU/src/mem/address_space.cpp`
  最小引入数据访存有效特权级判定，并在 `translate()` / `check_leaf_permissions()` 中消费。
- `readme.md`
  如本轮行为变化需要对外说明，再最小同步已实现特性描述。
- `myCPU/AGENTS.md`
  如本轮补齐后形成稳定支持声明，再最小同步实现基线。

## 完成定义

- `M-mode` 下 `load/store` 在 `MPRV=0` 时保持当前直通物理访问语义。
- `M-mode` 下 `load/store` 在 `MPRV=1` 时按 `MPP` 指定的有效特权级走 Sv39 翻译与权限检查。
- instruction fetch 不受本轮改动影响。
- 新 asm 回归先失败后通过，并纳入 `make test`。
- 现有 `Sv39`、privilege 与 guest 主门禁保持通过。

**实际结果：**
- 已新增 `myCPU/tests/asm/sv39_mprv.S`，覆盖 `MPRV=0` 直通物理访问、`MPRV=1 + MPP=S` 下的 `SUM` 约束，以及 `MPRV=1 + MPP=U` 下的 supervisor-only page fault。
- 已在 `myCPU/src/arch/csr_file.h` 补齐 `MSTATUS_MPRV` 常量，并在 `myCPU/src/mem/address_space.cpp` 落地数据访存有效特权级判定。
- 已把 `sv39_mprv` 接入 `make test` 与 `make test-pipeline` 的 asm 门禁。
- 已通过 `cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。

### 任务 1：新增失败的 `Sv39 + MPRV` asm 回归

**文件：**
- 创建：`myCPU/tests/asm/sv39_mprv.S`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：编写最小回归**

回归应至少覆盖：
- `M-mode + MPRV=0` 的数据访存继续 bypass translation
- `M-mode + MPRV=1 + MPP=S` 时，对 user page 的 load/store 仍受 `SUM` 约束
- `M-mode + MPRV=1 + MPP=U` 时，对 supervisor-only page 的 load 会触发 page fault

- [x] **步骤 2：运行单项回归验证失败**

运行：`cd myCPU && make test-sv39_mprv`

预期：FAIL，失败原因是当前实现未按 `MPRV` 改变数据访存的有效特权级。

- [x] **步骤 3：把回归纳入主门禁**

修改 `myCPU/Makefile`，将 `sv39_mprv` 纳入 asm 测试列表与 `make test` 聚合目标。

### 任务 2：最小实现 `MPRV` 数据访存有效特权级

**文件：**
- 修改：`myCPU/src/arch/csr_file.h`
- 修改：`myCPU/src/mem/address_space.cpp`

- [x] **步骤 1：补 `MSTATUS_MPRV` 常量**

在 `csr_file.h` 中补齐 `MSTATUS_MPRV` 位定义，避免在实现里散落 magic number。

- [x] **步骤 2：引入数据访存有效特权级判定**

在 `AddressSpace` 内部引入最小 helper：
- instruction fetch 继续使用当前真实 privilege mode
- `load/store` 在 `core_.privilege_mode() == Machine && (mstatus & MSTATUS_MPRV)` 时，按 `MPP` 解释有效 privilege mode

- [x] **步骤 3：让翻译和权限检查消费该判定**

最小修改 `translate()` / `check_leaf_permissions()`：
- bare/Sv39 是否 bypass translation 取决于有效 privilege mode，而不是单纯真实 mode
- user page、`SUM`、`MXR` 等检查按有效 privilege mode 与当前 `mstatus` 生效

- [x] **步骤 4：重新运行单项回归**

运行：`cd myCPU && make test-sv39_mprv`

预期：PASS。

### 任务 3：回归验证与文档同步

**文件：**
- 复查：`myCPU/tests/asm/sv39_sum_mxr.S`
- 复查：`myCPU/tests/asm/sv39_basic.S`
- 复查：`myCPU/tests/asm/privilege_transitions.S`
- 可能修改：`myCPU/AGENTS.md`
- 可能修改：`readme.md`

- [x] **步骤 1：运行针对性验证**

运行：
- `cd myCPU && make test-sv39_mprv`
- `cd myCPU && make test-sv39_sum_mxr`
- `cd myCPU && make test-sv39_edge_faults`
- `cd myCPU && make test-privilege_transitions`

预期：全部 PASS。

- [x] **步骤 2：运行主门禁**

运行：`cd myCPU && make test`

预期：PASS。

- [x] **步骤 3：如支持声明发生变化，同步文档**

如果本轮形成新的稳定实现声明，则最小更新 `myCPU/AGENTS.md` 和 `readme.md`。
