# Phase 1 Hardening Regressions 执行记录（已完成）

> 该计划对应的工作已完成。本文档保留为本地执行记录，不再是待执行 checklist。

**目标：** 扩充 Phase 1 hardening 回归，覆盖更多非法整数编码、CPU 侧 MMIO 非法访问 fault 合同，以及更真实的 ELF 段布局。

**架构：** 保持 reference path 语义不变，优先补测试并把新增回归接入现有 `make test` 门禁。asm 回归负责验证 CPU/CSR 可观察合同，unit 回归负责验证 loader 的精细段布局行为。

**技术栈：** C++17、RISC-V asm、GNU Make

## 关联文档

- 来源设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/code_self_review_status.md](../status/code_self_review_status.md)

**实际结果：**
- 已扩展 `myCPU/tests/asm/illegal_integer_encodings.S`
- 已新增 `myCPU/tests/asm/mmio_access_faults.S`
- 已新增 `myCPU/tests/unit/elf_loader_segments.cpp`
- 已新增 `myCPU/tests/unit/elf_loader_rejects.cpp`
- 已新增 `myCPU/tests/unit/elf_loader_header_rejects.cpp`
- 已扩展 `myCPU/tests/unit/bus_device_guards.cpp`
- 已新增 `myCPU/tests/unit/mmio_contract_matrix.cpp`
- 已把以上回归接入 `myCPU/Makefile` 与 `make test`
- 计划完成后又继续追加了 `myCPU/tests/asm/csr_illegal_matrix.S`，把 CSR / 特权非法访问也补成第一轮矩阵

---

### 任务 1：非法整数编码矩阵回归

**文件：**
- 修改：`myCPU/tests/asm/illegal_integer_encodings.S`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：扩展非法编码样本**
- [x] **步骤 2：运行 `cd myCPU && make test-illegal_integer_encodings` 验证当前行为**
- [x] **步骤 3：若出现缺口，最小化修复整数语义判定**
- [x] **步骤 4：重新运行 `cd myCPU && make test-illegal_integer_encodings`**
- [x] **步骤 5：把该回归继续纳入 `make test` 聚合门禁**

### 任务 2：MMIO 非法访问 asm 回归

**文件：**
- 创建：`myCPU/tests/asm/mmio_access_faults.S`
- 修改：`myCPU/Makefile`
- 可能修改：`myCPU/src/mem/address_space.cpp`
- 可能修改：`myCPU/src/devices/*.cpp`

- [x] **步骤 1：编写覆盖 UART/CLINT/PLIC/storage 非法 width/offset 的 asm 回归**
- [x] **步骤 2：运行 `cd myCPU && make test-mmio_access_faults` 验证失败模式**
- [x] **步骤 3：若 trap cause / mtval / resume 行为不符，最小化修复实现**
- [x] **步骤 4：重新运行 `cd myCPU && make test-mmio_access_faults`**
- [x] **步骤 5：把该回归纳入 `make test` 聚合门禁**

### 任务 3：ELF 段布局 unit 回归

**文件：**
- 创建：`myCPU/tests/unit/elf_loader_segments.cpp`
- 修改：`myCPU/Makefile`
- 可能修改：`myCPU/src/loader/elf_loader.cpp`

- [x] **步骤 1：编写多 `PT_LOAD` / mixed data+BSS / sparse file offset 回归**
- [x] **步骤 2：运行 `cd myCPU && make test-unit-elf_loader_segments` 验证失败模式**
- [x] **步骤 3：若 loader 对段装载或 zero-fill 有缺口，最小化修复实现**
- [x] **步骤 4：重新运行 `cd myCPU && make test-unit-elf_loader_segments`**
- [x] **步骤 5：把该回归纳入 `make test` 聚合门禁**

### 任务 4：总体验证

**文件：**
- 修改：`myCPU/Makefile`
- 复查：`docs/status/mainline_status.md`
- 复查：`myCPU/AGENTS.md`

- [x] **步骤 1：运行新增单项回归**
- [x] **步骤 2：运行 `cd myCPU && make test`**
- [x] **步骤 3：若实现有改动，再评估是否需要同步文档**
- [x] **步骤 4：记录实际验证结果与剩余风险**
