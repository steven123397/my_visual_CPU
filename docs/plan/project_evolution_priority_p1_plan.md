# PROJECT_EVOLUTION P1 实现计划

> **文档状态：** 执行中

## 文档定位

本文档承接 [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
中 `P1` 优先级事项，用于把需要近期定调的路线和交互能力拆成可执行 checklist。
本文不取代 AI、JIT、course OS 或前端 Lab 的设计文档，只记录落地顺序、验收门禁和
完成态回写要求。

## 关联文档

- 来源设计：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
  - [../design/ai_accelerator_linux_facing_contract_design.md](../design/ai_accelerator_linux_facing_contract_design.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
  - [../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)
- 目标状态：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)

## 目标

- 给 AI 加速器设备契约做 Linux-facing 方向定调。
- 给 JIT / DBT dry-run 做继续推进或归档的工程决断。
- 把测试矩阵分层入口升级为每条计划都必须声明的执行纪律。
- 让 course OS 和 `kernel_alpha` 从一次性 smoke 走向更真实的交互使用。
- 让前端 AI 面板在 bounded-dynamic shape 边界内支持自定义 workload。

## 完成定义

- AI 加速器形成明确的设备契约路线：继续 host-profile API、叠 Linux-facing driver
  facade，或拆阶段推进；对应设计文档和状态文档口径一致。
- JIT / DBT dry-run 有结论：推进为可选默认后端候选，或归档为方法论 demo；结论带
  验证证据和停止扩张边界。
- 新增或修改的计划文档都声明默认、slow guest、opt-in external 的验证层级。
- `course_os_shell` 能从受控文件源加载外部 RV64 ELF，或在设计文档中明确不可行原因
  和替代路径。
- `kernel_alpha` 有一个可持续查询 procfs / 调度 / COW / crashlog 证据的交互入口。
- 前端 AI 自定义入口只接受 bounded-dynamic shape 安全壳内的 JSON/op 组合，不接受任意
  graph package 或任意模型上传。

## 任务

### 任务 1：AI 加速器设备契约定调

**文件：**
- 创建：
  - `docs/design/ai_accelerator_linux_facing_contract_design.md`
- 修改：
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`
  - `myCPU/src/main.cpp`
  - `frontend/server/ai_tiny_model_service.mjs`

- [x] **步骤 1：** 盘点当前 host smoke API、MMIO 寄存器、profile summary、graph package
      和前端参数化 tiny model 的真实边界。
- [x] **步骤 2：** 写清 Linux-facing 路线的最小 contract：DT node、descriptor、DMA
      buffer、IRQ、ioctl/devfs 入口和 profile 回读字段。
- [x] **步骤 3：** 选择第一刀：只做 contract doc、host facade、guest driver stub，或
      最小真实 Linux driver smoke；选择结果写入设计文档。
- [x] **步骤 4：** 为选定第一刀补最窄测试，再实施必要代码或文档变更。

**结果：** 第一刀选择 `host-facade`。`./mycpu --ai-linux-contract` 输出
`schema=ai_linux_contract_v1` 的只读文本摘要，frontend
`/api/ai/tiny-model/templates` 同步返回 `linuxFacingContract`；二者固定 DT node、
MMIO / PLIC、descriptor / completion queue、DMA buffer、devfs / ioctl 预留面和
profile schema 边界。真实 Linux driver、`/dev/mycpu-ai0`、ioctl 与 Linux integration
smoke 仍未启动，后续需要独立窄计划。

### 任务 2：JIT / DBT dry-run 去留决断

**文件：**
- 创建：按结论需要新增 closeout 或 backend-candidate 计划
- 修改：
  - `docs/design/wave6_jit_dbt_readiness_design.md`
  - `docs/status/mainline_status.md`
  - `myCPU/src/exec/`
  - `myCPU/tests/host/`

- [ ] **步骤 1：** 汇总现有 dry-run 能力：translator、IR eval、lowering、host emitter、
      executable cache、runtime harness、fallback 和 invalidation。
- [ ] **步骤 2：** 固定决断标准：默认 backend 候选必须具备的 guest 范围、差分门禁、
      fallback coverage、性能证据和不可接受风险。
- [ ] **步骤 3：** 运行当前 host smoke / pipeline 相关门禁，记录哪些边界仍只能
      method-demo。
- [ ] **步骤 4：** 若推进，写出下一份窄实现计划；若归档，更新设计和 status，停止新增
      dry-run 接口面。

### 任务 3：测试矩阵分层执行纪律

**文件：**
- 创建：无
- 修改：
  - `docs/design/regression_completion_criteria.md`
  - `docs/status/mainline_status.md`
  - `docs/plan/template.md`
  - `AGENTS.md`
  - `myCPU/Makefile`

- [ ] **步骤 1：** 检查 `test-fast-smoke`、`test-standard-regression`、
      `test-slow-guest`、`test-opt-in-external` 当前覆盖和触发条件。
- [ ] **步骤 2：** 更新计划模板，要求新计划显式声明验证层级和 opt-in 资产条件。
- [ ] **步骤 3：** 把 AGENTS / regression 文档中的验证规则收敛到同一口径，避免
      status 复制完整测试矩阵。
- [ ] **步骤 4：** 为测试分层入口运行一次 smoke，确认命令名和目标仍然有效。

### 任务 4：`course_os_shell` 外部 ELF 加载

**文件：**
- 创建：按实现需要新增 guest unit fixture
- 修改：
  - `myCPU/guest/kernel/course_user_programs.c`
  - `myCPU/guest/include/course_user_programs.h`
  - `myCPU/guest/kernel/course_shell.c`
  - `myCPU/guest/include/course_shell.h`
  - `myCPU/tests/unit/user_program.c`
  - `myCPU/tests/unit/kernel_alpha_common.c`
  - `docs/status/kernel_alpha_status.md`

- [ ] **步骤 1：** 固定 `exec builtin-name` 旧行为和 `exec /path/to/prog` 新行为的解析
      合同。
- [ ] **步骤 2：** 先补 guest/unit 红灯，覆盖合法 ELF、缺文件、非 ELF、权限/格式错误和
      参数传递。
- [ ] **步骤 3：** 实现从课程 OS rootfs 或内置 FS 读取 ELF 的最小 loader 路径，不破坏
      现有 7 个内置程序。
- [ ] **步骤 4：** 运行 course OS 相关单元测试和 guest demo，确认 summary marker 未漂移。

### 任务 5：`kernel_alpha` 交互式观察面

**文件：**
- 创建：按实现需要新增 `kernel_alpha_monitor` guest 入口
- 修改：
  - `myCPU/guest/kernel_alpha/main.c`
  - `myCPU/guest/kernel/procfs.c`
  - `myCPU/guest/include/procfs.h`
  - `myCPU/Makefile`
  - `myCPU/tests/unit/kernel_alpha_common.c`
  - `docs/status/kernel_alpha_status.md`

- [ ] **步骤 1：** 选择入口形态：独立 `kernel_alpha_monitor` 或 `kernel_alpha_demo`
      summary 后进入最小 prompt。
- [ ] **步骤 2：** 先补测试，固定 `cat /proc/syscalls`、`cat /proc/cow`、
      `cat /proc/crashlog` 或等价命令的输出合同。
- [ ] **步骤 3：** 实现最小 monitor loop，禁止引入会改变 Stage 2 demo marker 的默认路径。
- [ ] **步骤 4：** 将 monitor 作为新交互能力记录到 status，而不是替换原 demo。

### 任务 6：AI 前端自定义 workload

**文件：**
- 创建：按实现需要新增 AI custom graph fixture
- 修改：
  - `frontend/server/ai_tiny_model_service.mjs`
  - `frontend/app/app.js`
  - `frontend/tests/debug_server.test.mjs`
  - `frontend/tests/debug_server_e2e.test.mjs`
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
  - `docs/status/npu_tpu_accelerator_status.md`

- [ ] **步骤 1：** 定义 `POST /api/ai/custom-graph` 的 JSON schema：op 白名单、
      dtype、shape、batch、input preset、输出 profile 字段和拒绝原因。
- [ ] **步骤 2：** 先补 server 测试，覆盖合法 op 序列、非法 op、越界 shape、dtype
      mismatch、超限 batch 和 profile 返回。
- [ ] **步骤 3：** 服务端复用 `pack_graph.py` 生成受控 graph package，前端只提交
      bounded-dynamic shape 描述。
- [ ] **步骤 4：** 前端提供 JSON editor 或结构化表单；用户可修改 op 序列和 shape，
      但不能上传任意 graph package。

## 验证基线

- 每个切片必须在计划内声明默认 / slow / opt-in 验证层级。
- `cd myCPU && make test-fast-smoke test-standard-regression`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- AI 或 Linux-facing driver 相关切片如依赖外部 rootfs / Spike / 真实 Image，必须使用
  明确 opt-in 环境变量记录跳过或通过原因。
- `git diff --check`

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/mainline_status.md](../status/mainline_status.md) 必须记录 P1 决断结果、
  哪些路线继续推进、哪些路线停止扩张。
- [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  必须记录 AI 设备契约和自定义 workload 的当前边界。
- [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 必须记录外部 ELF
  和交互观察面的完成范围。
- 需要把“完成时间 + 完成内容 + 必要过程摘要”追加到
  [history_plan.md](history_plan.md)。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
