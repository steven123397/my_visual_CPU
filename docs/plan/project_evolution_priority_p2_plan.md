# PROJECT_EVOLUTION P2 实现计划

> **文档状态：** 执行中

## 文档定位

本文档承接 [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
中 `P2` 优先级事项，用于把中期协议化、路线清晰化和可配置性工作拆成可执行
checklist。本文只记录执行步骤与验收，不作为 Linux、AI、frontend 或 machine config 的
长期设计来源。

## 关联文档

- 来源设计：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
  - [../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)
  - [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
- 目标状态：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)

## 目标

- 处理 Linux 第四阶段冻结点，让 Linux 路线状态不再停留在半冻不冻的历史措辞。
- 将 bounded-dynamic shape 抽象提升为独立、可引用的设计契约。
- 给 Frontend ↔ Simulator 协议建立版本化边界。
- 降低新增 workload 的工程门槛。
- 让 RAM / MMU / 设备等机器参数从硬编码走向受控配置。

## 完成定义

- Linux 第四阶段冻结点有明确处理结果：重新定义下一阶段目标，或声明已被
  Alpine/Debian distro platform 路线替代。
- bounded-dynamic shape 有独立设计文档或等价设计章节，明确 schema、扩展点、
  兼容策略和 AI op 注册关系。
- Frontend ↔ Simulator 协议有版本字段、兼容策略、错误码规范和至少一个 server/client
  兼容测试。
- `workloads/custom/` 或等价机制可自动发现受控 workload；不再要求普通用户手工改
  Makefile 和 manifest。
- CLI 和前端至少支持 RAM size 的受控配置；MMU / device 配置有设计边界和测试覆盖。

## 任务

### 任务 1：Linux 第四阶段冻结点处理

**文件：**
- 创建：按结论需要新增 Linux 下一阶段计划
- 修改：
  - `docs/design/post_wave7_linux_distribution_platform_design.md`
  - `docs/status/linux_distribution_platform_status.md`
  - `docs/status/mainline_status.md`
  - `docs/plan/post_wave7_linux_distribution_platform_longterm_plan.md`

- [ ] **步骤 1：** 盘点 `timerfd-one-shot-readback-ok` 冻结点、当前 Alpine/Debian
      rootfs 证据和 Linux serial console opt-in guardrail。
- [ ] **步骤 2：** 二选一写入设计：解冻并定义下一阶段 capability，或声明第四阶段冻结点
      作为历史锚点并由 distro platform 路线承接。
- [ ] **步骤 3：** 同步 status 和 long-term plan，只保留当前有效风险，不复制旧历史清单。
- [ ] **步骤 4：** 运行相关默认或 opt-in Linux smoke，记录跳过条件和资产要求。

### 任务 2：bounded-dynamic shape 独立文档化

**文件：**
- 创建：
  - `docs/design/bounded_dynamic_shape_contract.md`
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `docs/index.md`
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `frontend/server/ai_tiny_model_service.mjs`

- [ ] **步骤 1：** 从现有 task-spec、runtime shape table、AI profile 和 frontend
      parameterized tiny model 中提取实际 schema。
- [ ] **步骤 2：** 写独立设计文档，固定 shape 维度、动态范围、dtype、op 白名单、
      version、兼容策略和拒绝原因。
- [ ] **步骤 3：** 更新 AI 设计文档，把 bounded-dynamic shape 作为引用契约而不是埋在
      导入器实现里。
- [ ] **步骤 4：** 补测试或 schema 校验，确认非法 shape 以稳定错误码 fail closed。

### 任务 3：Frontend ↔ Simulator 协议版本化

**文件：**
- 创建：
  - `docs/design/frontend_simulator_protocol.md`
- 修改：
  - `docs/design/post_wave7_frontend_lab_product_design.md`
  - `frontend/server/debug_server.mjs`
  - `frontend/server/debug_server_runtime.mjs`
  - `frontend/app/app.js`
  - `frontend/tests/debug_server.test.mjs`
  - `frontend/tests/debug_server_runtime.test.mjs`
  - `docs/index.md`

- [ ] **步骤 1：** 盘点当前 `/api/tests`、`/api/session/load`、run/pause/reset/terminate、
      AI profile 和 debug command 的 response shape。
- [ ] **步骤 2：** 写协议设计文档，固定 `protocol_version`、capability discovery、
      error code、feature gating 和 backward compatibility。
- [ ] **步骤 3：** 先补 server/client 测试，断言旧客户端缺 version 时的兼容响应和新客户端
      能按 capability 显隐功能。
- [ ] **步骤 4：** 实现版本字段和 capability discovery，不改变现有 workload 运行语义。

### 任务 4：Workload 系统自动发现和模板化

**文件：**
- 创建：
  - `myCPU/workloads/custom/README.md`
  - 按实现需要新增 `profile.toml` 示例
- 修改：
  - `frontend/server/tests_manifest.mjs`
  - `frontend/server/debug_server.mjs`
  - `frontend/tests/debug_server.test.mjs`
  - `docs/design/post_wave7_frontend_lab_product_design.md`
  - `docs/status/mainline_status.md`

- [ ] **步骤 1：** 定义 `workloads/custom/<name>/profile.toml + program.elf` 的最小目录
      contract。
- [ ] **步骤 2：** 先补 manifest 生成测试，覆盖合法 workload、缺 ELF、缺 profile、
      重名、非法路径和 disabled workload。
- [ ] **步骤 3：** 实现自动扫描并合并到 manifest，同时保留现有手写 manifest 条目。
- [ ] **步骤 4：** 前端在 workload 卡片中标记 custom workload 来源和不可用原因。

### 任务 5：机器参数可配置

**文件：**
- 创建：按实现需要新增 machine config test fixture
- 修改：
  - `myCPU/src/main.cpp`
  - `myCPU/src/platform/machine.cpp`
  - `myCPU/src/platform/machine.h`
  - `myCPU/workloads/boards/`
  - `frontend/server/debug_server.mjs`
  - `frontend/app/app.js`
  - `frontend/tests/debug_server.test.mjs`
  - `docs/design/post_wave7_frontend_lab_product_design.md`
  - `docs/status/mainline_status.md`

- [ ] **步骤 1：** 增加 `--ram-size` CLI 参数并固定解析规则，支持十进制和 `0x` 十六进制。
- [ ] **步骤 2：** 先补 host/unit 测试，覆盖默认 RAM、合法覆盖、过小、非对齐、溢出和
      与 board 默认值的优先级。
- [ ] **步骤 3：** 让 debug server 和前端 load 配置传递 RAM size；UI 使用受控输入，
      不允许无界字符串直传 CLI。
- [ ] **步骤 4：** 设计 MMU / device 勾选的后续边界；只有已有测试能覆盖的参数进入实现。

## 验证基线

- `cd myCPU && make test-unit-machine_loader_reset test-fast-smoke`
- `cd myCPU && make test-standard-regression`
- `cd frontend && node --test`
- Linux distro 相关验证必须区分默认 smoke 与真实 rootfs opt-in smoke。
- `git diff --check`

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/mainline_status.md](../status/mainline_status.md) 必须记录 P2 完成摘要、
  新增协议/配置入口和剩余风险。
- [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  必须记录 Linux 冻结点处理结果。
- [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  必须记录 bounded-dynamic shape 契约状态。
- 新增设计文档必须同步 [../index.md](../index.md)。
- 需要把“完成时间 + 完成内容 + 必要过程摘要”追加到
  [history_plan.md](history_plan.md)。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
