# `P4-prep-1`：`bus / memory region` 合同收口实现计划

> **面向 AI 代理的工作者：** 推荐使用 `superpowers:executing-plans` 或 `superpowers:subagent-driven-development` 按任务执行本计划。步骤使用复选框（`- [ ]`）语法跟踪进度。
>
> **文档状态：** 执行中

**目标：** 把 `Phase 4` 的第一刀收窄为统一的 `bus / memory region` 查询合同，并让现有向量访存、`pipeline` memory 判断与后续 memory 观察都复用这一事实来源。

**架构：** 在物理地址层新增最小 `region` 类型与 span 查询接口，由 `Bus` 统一暴露 `RAM / MMIO / unmapped` 与 `cacheable / dma_visible / has_side_effect / supports_burst` 等属性；现有执行路径不再直接硬编码 `MEM_BASE / MEM_SIZE` 判定，而是通过 `Bus` 查询结果完成保守决策。

**技术栈：** C++17、GNU Make、host unit / smoke tests、现有 `functional` / `pipeline` 回归

---

## 文档定位

本文档用于把 [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md) 中已经冻结的 `P4-prep-1`，细化成一份可直接执行的实现计划。

本计划只覆盖 `bus / memory region` 合同收口，不包含真正 `cache / DMA / multicore / coherence` 实现。

## 关联文档

- 来源设计： [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
- 相关设计：
  - [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
- 目标状态：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)

## 目标

- 在 `Bus` 层形成统一、可复用的物理 `region` 查询接口。
- 用最小数据结构表达当前仓库已经稳定存在的 region 属性：`kind / cacheable / dma_visible / has_side_effect / supports_burst / label`。
- 让现有向量访存预校验、`pipeline` RAM / MMIO 判断和后续 memory 观察都不再散落硬编码 `MEM_BASE / MEM_SIZE` 或手写 MMIO 地址范围判断。
- 保持当前 guest 可见语义不变：fault 类型、MMIO side effect、防御式 fail-closed 边界都不能回退。

## 完成定义

- `myCPU/src/mem/` 下存在统一的 region 类型定义，且 `Bus` 能对单地址和 span 返回一致结论。
- `Bus` 至少能区分：`ram`、`mmio`、`unmapped`，并提供当前需要的属性位。
- `vector_ops.cpp`、`pipeline_backend_execute.cpp` 和 `load_store_queue.cpp` 不再直接依赖本地 RAM 地址范围硬编码完成关键判断。
- 新增或扩充的 unit test 能直接验证 region 查询合同，现有 `vector` / `pipeline` smoke 能守住行为不变。
- 计划完成后，结果回写 `status` 与相关 `AGENTS.md`，并归档到 `history_plan`。

## 执行约束

- 本计划不默认自动提交；提交由开发者在阶段完成后决定。
- 继续优先维护 reference path 的正确性与可观察性，不为了“更像 `Phase 4`”扩大 guest 语义面。
- 若实施过程中发现 `Bus / Device / AddressSpace` 的边界与预期明显冲突，应先回写设计或状态，再继续动实现。

## 文件边界

### 计划内预计创建

- `myCPU/src/mem/memory_region.h`
  - 定义统一的物理 region 类型、属性和 span 查询结果。
- `myCPU/tests/unit/bus_region_contract.cpp`
  - 直接验证 `Bus` 的 region / span 查询合同。

### 计划内预计修改

- `myCPU/src/devices/device.h`
  - 为设备补最小 region 属性入口，提供默认 MMIO 语义。
- `myCPU/src/mem/ram.h`
  - 为 RAM 明确 region 属性覆盖。
- `myCPU/src/mem/bus.h`
  - 暴露单地址 / span 查询接口。
- `myCPU/src/mem/bus.cpp`
  - 实现 region 查询、span 校验与默认属性装配。
- `myCPU/src/exec/vector_ops.cpp`
  - 把向量访存 span 预校验迁移到统一 region contract。
- `myCPU/src/exec/pipeline_backend_execute.cpp`
  - 把 `RAM-only` / `known MMIO` 相关判断改为统一查询。
- `myCPU/src/exec/load_store_queue.cpp`
  - 把 LSQ 中的 RAM 范围判断改为统一查询。
- `myCPU/Makefile`
  - 接入新的 unit test 目标。
- `docs/status/mainline_status.md`
  - 完成态回写实现结果与剩余风险。
- `docs/status/project_priority_roadmap.md`
  - 完成态回写优先级变化与下一步建议。
- `AGENTS.md`
  - 如 `Phase 4` 准备入口或当前焦点发生实质变化，则同步摘要。
- `myCPU/AGENTS.md`
  - 如 `Bus / memory region` 成为新的局部边界，则同步局部规则与当前下一步。
- `docs/plan/history_plan.md`
  - 归档完成结果。

### 计划内主要验证入口

- `cd myCPU && make test-unit-bus_region_contract`
- `cd myCPU && make test-unit-bus_device_guards`
- `cd myCPU && make test-unit-mmio_contract_matrix`
- `cd myCPU && make test-host-vector_vlite_smoke`
- `cd myCPU && make test-host-vector_pipeline_smoke`
- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

## 任务

### 任务 1：建立 `bus region` 的红灯测试面

**文件：**
- 创建：`myCPU/tests/unit/bus_region_contract.cpp`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：编写失败的 unit test，锁定当前需要的 region 语义**

  测试至少覆盖：

  - RAM 地址返回 `kind = ram`
  - UART / CLINT / PLIC / Storage 返回 `kind = mmio`
  - 未映射地址返回 `kind = unmapped`
  - RAM `cacheable = true`，MMIO `cacheable = false`
  - MMIO `has_side_effect = true`
  - `span` 跨设备边界或跨到 unmapped 时必须返回失败 / 不一致结果

  参考骨架：

  ```cpp
  const auto ram = bus.describe_region(MEM_BASE, 4);
  if (ram.kind != PhysicalRegionKind::Ram || !ram.cacheable || ram.has_side_effect) {
      return fail("expected RAM region properties");
  }

  const auto uart = bus.describe_region(UART_BASE + UART_REG_THR, 1);
  if (uart.kind != PhysicalRegionKind::Mmio || uart.cacheable || !uart.has_side_effect) {
      return fail("expected UART MMIO properties");
  }

  const auto cross = bus.describe_span(UART_BASE + UART_REG_THR, 2);
  if (cross.ok) {
      return fail("expected span crossing live MMIO window to be rejected");
  }
  ```

- [ ] **步骤 2：把新测试接入 `Makefile`**

  在 `UNIT_TEST_NAMES` 中加入 `bus_region_contract`，保持现有命名和自动目标生成方式一致。

- [ ] **步骤 3：运行测试，确认当前仍是红灯**

  运行：`cd myCPU && make test-unit-bus_region_contract`

  预期：编译失败，报 `Bus` 缺少新的 region 查询类型 / 方法，或链接失败；此时不要绕开失败，直接进入接口实现。

### 任务 2：实现统一的 region 类型与 `Bus` 查询接口

**文件：**
- 创建：`myCPU/src/mem/memory_region.h`
- 修改：`myCPU/src/devices/device.h`
- 修改：`myCPU/src/mem/ram.h`
- 修改：`myCPU/src/mem/bus.h`
- 修改：`myCPU/src/mem/bus.cpp`
- 测试：`myCPU/tests/unit/bus_region_contract.cpp`

- [ ] **步骤 1：定义最小 region 数据结构**

  在 `myCPU/src/mem/memory_region.h` 中新增最小公共类型，建议保持 header-only：

  ```cpp
  enum class PhysicalRegionKind : uint8_t {
      Ram,
      Mmio,
      Unmapped,
  };

  struct PhysicalRegionInfo {
      PhysicalRegionKind kind{PhysicalRegionKind::Unmapped};
      bool cacheable{false};
      bool dma_visible{false};
      bool has_side_effect{false};
      bool supports_burst{false};
      const char* label{"unmapped"};
  };

  struct PhysicalSpanInfo {
      bool ok{false};
      PhysicalRegionInfo region{};
      uint64_t first_addr{0};
      uint64_t size{0};
  };
  ```

- [ ] **步骤 2：给 `Device` / `Ram` 增加最小属性入口**

  建议在 `Device` 基类提供默认 `region_info()`，保持“普通设备默认就是 live MMIO”；在 `Ram` 中覆盖为普通 RAM 属性。

  关键点：

  - 不改现有 `load / store / contains / is_mmio / debug_name` 合同
  - 只补充统一事实来源，不制造第二套设备分类系统

- [ ] **步骤 3：在 `Bus` 中实现单地址与 span 查询**

  目标接口建议类似：

  ```cpp
  PhysicalRegionInfo describe_region(uint64_t addr, int size) const;
  PhysicalSpanInfo describe_span(uint64_t addr, uint64_t bytes) const;
  ```

  关键要求：

  - 单地址查询必须与 `find_device()` 的映射结果一致
  - `span` 查询必须能识别跨 region、跨边界和 unmapped 情况
  - 当前不需要做复杂聚合，只需回答“整段是否属于同一种可接受 region”

- [ ] **步骤 4：运行 unit test，确认绿灯**

  运行：

  - `cd myCPU && make test-unit-bus_region_contract`
  - `cd myCPU && make test-unit-bus_device_guards`
  - `cd myCPU && make test-unit-mmio_contract_matrix`

  预期：全部通过；若 `bus_device_guards` 或 `mmio_contract_matrix` 回退，先修 contract，再继续迁移调用点。

### 任务 3：迁移现有执行路径到统一 region contract

**文件：**
- 修改：`myCPU/src/exec/vector_ops.cpp`
- 修改：`myCPU/src/exec/pipeline_backend_execute.cpp`
- 修改：`myCPU/src/exec/load_store_queue.cpp`
- 测试：`myCPU/tests/host/vector_vlite_smoke.cpp`
- 测试：`myCPU/tests/host/vector_pipeline_smoke.cpp`

- [ ] **步骤 1：迁移向量访存 span 预校验**

  把 `vector_ops.cpp` 中基于 `MEM_BASE / MEM_SIZE` 的本地判断，改为统一查询 `Bus` 的 span / region 结果。

  目标是继续保持当前 fail-closed 语义：

  - live MMIO 或非 RAM span 直接 `access fault`
  - 不留下 UART side effect 或 RAM partial write

- [ ] **步骤 2：迁移 `pipeline` 中的 RAM-only / known-MMIO 判断**

  把 `pipeline_backend_execute.cpp` 与 `load_store_queue.cpp` 中的本地范围判断收口到统一接口。

  关键点：

  - 保持当前 `RAM-only` 路径与非 RAM / MMIO 的保守处理不变
  - 不趁机扩大 memory speculation 或 LSQ 合同
  - 如果现有逻辑依赖“已知 MMIO”和“非 RAM”之间的区别，必须用统一查询结果显式表达，不再留手写地址表

- [ ] **步骤 3：运行定向 smoke，确认行为未变**

  运行：

  - `cd myCPU && make test-host-vector_vlite_smoke`
  - `cd myCPU && make test-host-vector_pipeline_smoke`
  - `cd myCPU && make test-pipeline`

  预期：全部通过；若 `vector_vlite_smoke` 回退，优先检查 span 查询是否把 RAM / MMIO 边界判错。

### 任务 4：做总门禁验证并完成文档回写

**文件：**
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/status/project_priority_roadmap.md`
- 修改：`AGENTS.md`
- 修改：`myCPU/AGENTS.md`
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/phase4_prep1_bus_memory_region_plan.md`

- [ ] **步骤 1：运行总门禁**

  运行：

  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`

  预期：全部通过；若失败，不回写文档，先把实现收口到绿灯。

- [ ] **步骤 2：回写状态与局部规则**

  至少同步：

  - `mainline_status`：完成结果、关键历史节点、仍然有效的限制
  - `project_priority_roadmap`：`Phase 4` 下一步从“设计已冻结”转为“`P4-prep-1` 已落地，下一步是否值得继续 `P4-prep-2`”
  - `AGENTS.md` / `myCPU/AGENTS.md`：若 `bus / memory region` 已成为当前稳定边界，补最小摘要

- [ ] **步骤 3：归档计划并删除活跃计划文件**

  完成后把“完成时间 + 完成内容 + 一两句过程摘要”追加到 `docs/plan/history_plan.md`，随后删除本计划文件，不长期保留完成态 checklist。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
