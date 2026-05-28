# 全量代码审查并行 Agent 计划

> **文档状态：** 待启动

## 文档定位

本文档用于规划一次面向 `my_visual_CPU` 全仓库的只读全量代码审查。它不是修复计划，也不在
Linux 发行版主线仍处于活跃收口时执行；它用于在 Linux 主线形成清晰 checkpoint 后，组织
4 到 5 个 agent 并行审查，并由一个 coordinator 汇总成分级 findings。

本文档只记录审查组织方式、agent 分工、输入输出格式、完成定义和后续回写要求。审查发现的
active findings 统一回写到 [../status/code_reself_status.md](../status/code_reself_status.md)。

## 关联文档

- 来源设计 / 规划：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
  - [../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
- 目标状态：
  - [../status/code_reself_status.md](../status/code_reself_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)

## 启动前提

- Linux 发行版主线已接近收口，并至少形成一个可描述的 checkpoint。
- 启动审查前必须重新执行并记录：
  - `git status --short --branch`
  - `git log -1 --oneline`
- 若工作区仍有 Linux 主线脏改，应明确哪些文件属于 Linux 并行进程；审查 agent 可以读取这些文件，
  但不得把尚未收口的中间态当成最终设计事实。
- 本计划默认只读执行：不改代码、不格式化、不提交、不清理 worktree。

## 目标

- 对当前已可运行的 RISC-V 系统模拟器原型做一次全量代码审查。
- 找出会影响后续整体升级计划的真实风险，而不是泛泛罗列风格问题。
- 以 `reference-first + observability + evidence chain` 为核心审查口径，检查代码、测试和文档是否仍然同源。
- 把审查结果分成：
  - `必须修复`
  - `建议修改`
  - `仅记录`
- 将 findings 转换成后续整改 backlog，而不是在审查阶段直接混入修复。

## 非目标

- 不在本轮实现任何修复。
- 不把 Linux 发行版主线未完成的中间态写成最终缺陷。
- 不重新设计项目路线；路线判断以 `PROJECT_EVOLUTION_PLAN.md` 和后续整改计划为输入。
- 不用代码审查替代真实验证；每条 finding 只能说明风险和证据，不能声称修复已完成。
- 不把 JIT 未默认启用、外部 Linux 资产 fail-closed、AI 未开放任意模型上传这类已知非目标写成 bug。

## 审查总控角色

本轮需要一个 coordinator，负责：

- 读取根 `AGENTS.md`、`myCPU/AGENTS.md`、`myCPU/guest/AGENTS.md`、`docs/AGENTS.md`。
- 读取 `README.md`、`docs/status/mainline_status.md`、`docs/status/code_reself_status.md`。
- 确认启动基线 commit、当前脏文件和 Linux 主线 checkpoint。
- 并行分派 5 个只读审查 agent。
- 合并、去重、分级、交叉验证各 agent 的 findings。
- 把最终结果回写到 `docs/status/code_reself_status.md`。

Coordinator 不应把 agent 原始结论原样堆进状态文档；必须先做判断：

- 是否有明确文件 / 行号 / 函数证据。
- 是否违反现有 design / status / AGENTS 边界。
- 是否已经被其他 agent 发现，或只是同一问题的不同表述。
- 是否属于已知非目标。
- 是否需要真实验证后才能升级为 `必须修复`。

## 严重级别定义

### 必须修复

满足任一条件即可归入：

- 可能导致 guest 可见行为错误、状态破坏、数据破坏或错误 trap / interrupt / MMIO 行为。
- 可能让 reference truth 与 pipeline / JIT / AI / frontend 观察面产生未声明分叉。
- 可能让测试或 probe 声称支持了实际未支持的能力。
- 可能破坏 fail-closed 合同，例如缺外部资产时静默回落到 repo 内假资产。
- 可能导致安全边界问题，例如 frontend / server 路径穿越、任意文件读取、任意模型或镜像未经校验进入执行路径。

### 建议修改

满足任一条件可归入：

- 结构性重复、事实来源分散、schema 漂移或长期维护成本明显上升。
- 测试分层不清、门禁命名不清、slow / opt-in 测试误入默认路径。
- 文档与实现口径不一致，但暂未造成错误支持声明。
- 抽象边界已经模糊，影响后续整改计划落地。

### 仅记录

满足任一条件可归入：

- 与长期路线有关，但当前没有立即行为风险。
- 已知技术债，需要在后续整改计划中排期。
- 需要更多真实 workload 或外部资产验证后才能升级。

## 并行 Agent 分工

### Agent 1：ISA / reference truth / privilege / memory

**审查范围：**

- `myCPU/src/decode.c`
- `myCPU/src/cpu.*`
- `myCPU/src/arch/*`
- `myCPU/src/exec/integer_ops.*`
- `myCPU/src/exec/floating_ops.*`
- `myCPU/src/exec/system_ops.*`
- `myCPU/src/exec/control_flow_ops.*`
- `myCPU/src/exec/memory_ops.*`
- `myCPU/src/exec/functional_backend.*`
- `myCPU/src/trap.cpp`
- `myCPU/src/mem/*`
- `myCPU/src/loader/*`
- `myCPU/tests/asm/*`
- `myCPU/tests/unit/*loader*`
- `myCPU/tests/host/instruction_semantics_*`
- `myCPU/tests/host/atomic_semantics_smoke.cpp`
- `myCPU/tests/host/backend_differential_smoke.cpp`
- `myCPU/tests/host/spike_differential_smoke.cpp`

**重点问题：**

- 共享 `InstructionSemantics + functional backend` 是否仍是唯一 ISA 真值来源。
- decode、semantic helper、pipeline hazard 分类之间是否存在重复且可能漂移的指令事实。
- F/D/C/A/CSR/Sv39/privilege 新增路径是否有 host / asm / pipeline 对应证据。
- `fcsr`、rounding mode、exception flags、FS / SD、CSR alias 是否存在只在某条路径生效的隐式差异。
- memory access、page fault、access fault、translation fault、MMIO side-effect 是否区分清楚。
- loader reset / payload load / primary image load 是否会破坏 cache / JIT / debug snapshot 生命周期合同。

**输出要求：**

- 最多列出 12 条 finding。
- 每条必须带文件路径和函数 / 类型名；有行号更好。
- 对每条 finding 给出建议验证命令，例如：
  - `cd myCPU && make test-host-instruction_semantics_smoke`
  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-host-spike_differential_smoke`
  - `cd myCPU && make test`

### Agent 2：pipeline / JIT / DBT / cache runtime

**审查范围：**

- `myCPU/src/exec/pipeline*`
- `myCPU/src/exec/reorder_buffer.*`
- `myCPU/src/exec/load_store_queue.*`
- `myCPU/src/exec/rename_map.*`
- `myCPU/src/exec/physical_register_file.*`
- `myCPU/src/exec/branch_predictor.*`
- `myCPU/src/exec/execution_profile.*`
- `myCPU/src/exec/dbt_*`
- `myCPU/src/exec/interpreter_dbt_prototype.*`
- `myCPU/src/platform/machine.*` 中与 backend / profile / cache / JIT dispatch 有关的部分
- `myCPU/tests/host/*pipeline*`
- `myCPU/tests/host/dbt_*`
- `myCPU/tests/host/execution_profile_smoke.cpp`
- `myCPU/tests/host/vector_pipeline_smoke.cpp`

**重点问题：**

- pipeline 是否只消费共享语义，没有复制或偷修 ISA 行为。
- ROB / LSQ / rename / forwarding / hazard 对整数、浮点、CSR、memory helper 的分类是否和 shared semantics 对齐。
- JIT / DBT 是否仍保持 opt-in、host-smoke-only 或明确非默认边界。
- executable cache invalidation 是否覆盖 guest store、payload load、primary image load、reset、`satp`、`sfence.vma` 和 region 属性变化。
- `execution_profile`、`shadow_cache`、L1D observation、JIT dispatch summary 是否存在 schema 漂移。
- pipeline 文档是否诚实标注其性能含义，是否会被误读成可信微架构性能真值。

**输出要求：**

- 最多列出 12 条 finding。
- 明确区分：
  - correctness risk
  - observability / schema risk
  - future-route decision risk
- 对每条 finding 给出建议验证命令，例如：
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-host-dbt_runtime_harness_smoke`
  - `cd myCPU && make test-host-execution_profile_smoke`

### Agent 3：platform / Linux distro / guest runtime / kernel_alpha

**审查范围：**

- `myCPU/src/platform/*`
- `myCPU/src/devices/uart16550.*`
- `myCPU/src/devices/clint.*`
- `myCPU/src/devices/plic.*`
- `myCPU/src/devices/simple_storage.*`
- `myCPU/src/devices/virtio_*`
- `myCPU/src/devices/virtqueue.*`
- `myCPU/workloads/linux_proto/*`
- `myCPU/workloads/boards/*`
- `myCPU/workloads/run_debug_cli_probe.py`
- `myCPU/tests/host/run_debug_cli_probe_test.py`
- `myCPU/tests/host/virtio_blk_smoke.cpp`
- `myCPU/tests/host/xv6_*`
- `myCPU/guest/*`
- `myCPU/guest/AGENTS.md`
- `docs/status/linux_distribution_platform_status.md`
- `docs/plan/post_wave7_linux_distribution_platform_longterm_plan.md`
- `docs/status/kernel_alpha_status.md`

**重点问题：**

- Linux distro runtime 是否继续 fail-closed，不把 repo `linux_proto/rootfs.ext4` 当作标准发行版证据。
- external `Image/rootfs`、bootargs、prompt、profile、expected 的 env 合同是否清晰且可验证。
- `virtio-blk`、UART、CLINT、PLIC、DTB ISA 广告、HWCAP、procfs 视图之间是否存在能力声明漂移。
- `kernel_alpha` 课程 OS 接管边界是否清楚，旧 Phase 1 `KMVPETDS` / 负向 demo guardrail 是否被误写成当前课程 OS 行为承诺。
- guest runtime 的基础设施层和入口编排层是否保持清楚，是否有 demo-only 逻辑回流到通用 kernel helper。
- Linux fourth-stage `timerfd-one-shot-readback-ok` 与标准发行版平台主线是否存在叙事冲突。

**输出要求：**

- 最多列出 12 条 finding。
- 每条 finding 必须标明它影响的是：
  - default repo gate
  - opt-in external rootfs gate
  - documentation / support claim
  - future cleanup decision
- 对每条 finding 给出建议验证命令，例如：
  - `cd myCPU && make test-host-run_debug_cli_probe`
  - `cd myCPU && make test-host-virtio_blk_smoke`
  - `cd myCPU && make test-host-xv6_shell_smoke`
  - `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_curated_matrix`

### Agent 4：AI accelerator / task-spec / NPU performance model

**审查范围：**

- `myCPU/src/devices/ai_*`
- `myCPU/src/devices/tensor_golden_ops.*`
- `myCPU/guest/include/ai_accel.h`
- `myCPU/guest/kernel/ai_accel.c`
- `myCPU/guest/ai_accel_demo/*`
- `myCPU/workloads/ai_proto/*`
- `myCPU/tests/unit/ai_*`
- `myCPU/tests/host/ai_*`
- `docs/design/npu_tpu_accelerator_direction_design.md`
- `docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
- `docs/status/npu_tpu_accelerator_status.md`

**重点问题：**

- host manifest、guest MMIO、device descriptor、queue、doorbell、completion、profile summary 是否共享同一设备事实来源。
- task-spec importer 是否真正 fail-closed，避免未知字段、重复 key、路径逃逸、bool-as-int、non-finite 浮点输入进入后续路径。
- bounded dynamic shape、memory plan、resolved memory plan、runtime shape table 是否存在第二套事实来源。
- `timed-simple no-overlap` 是否被清楚标注为当前性能模型，而不是误写成真实 NPU overlap。
- AI 设备契约是否已经具备未来 Linux-facing driver 的可演化边界，还是仍偏 host smoke API。
- profile summary、aggregate timing、DMA breakdown、queue snapshot、topology summary 是否存在 schema 膨胀或缺少版本边界。

**输出要求：**

- 最多列出 12 条 finding。
- 每条 finding 必须标明它影响的是：
  - guest-visible ABI
  - host-only importer / manifest
  - profile / observability schema
  - future Linux-facing driver route
- 对每条 finding 给出建议验证命令，例如：
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - `cd myCPU && make test-host-ai_accel_guest_smoke`
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && python3 workloads/ai_proto/run_demo_v1.py --out-dir workloads/ai_proto/generated/demo_v1`

### Agent 5：frontend / debug protocol / docs governance / test matrix

**审查范围：**

- `frontend/*`
- `myCPU/src/debug/*`
- `myCPU/src/debug/debug_snapshot.h`
- `frontend/server/tests_manifest.mjs`
- `frontend/server/debug_server*.mjs`
- `frontend/app/*`
- `docs/AGENTS.md`
- `docs/index.md`
- `docs/status/mainline_status.md`
- `docs/status/code_reself_status.md`
- `README.md`
- `myCPU/Makefile`
- `deploy/*`

**重点问题：**

- frontend 是否只消费真实 manifest / snapshot / diagnostics / session API，不自造执行事实。
- `/console` Lab workbench 的 scenario catalog 是否只补展示元数据，不替代后端 truth。
- debug protocol / frontend server 是否存在路径穿越、任意文件暴露、session 生命周期泄漏或 timeout 误报。
- docs 是否继续保持 `design / plan / status` 分离，没有多个主线事实来源。
- 测试矩阵是否能分成 fast smoke、standard regression、slow guest、opt-in differential / external assets。
- showcase 是否已冻结为课程展示资产，没有继续侵入实时状态或工程设计。

**输出要求：**

- 最多列出 12 条 finding。
- 每条 finding 必须标明它影响的是：
  - frontend runtime behavior
  - debug protocol / server boundary
  - documentation governance
  - test matrix / developer workflow
- 对每条 finding 给出建议验证命令，例如：
  - `cd frontend && node --test`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-interactive_terminal_smoke`
  - `git diff --check`

## Agent 通用审查提示词模板

每个 agent 启动时使用下面模板，并替换 `<AGENT_NAME>`、`<SCOPE>`、`<FOCUS>` 和
`<COMMANDS>`。

```text
你正在参与 my_visual_CPU 的全量只读代码审查。

仓库路径：/home/liangjiaqi/projects/my_visual_CPU
审查模式：只读，不改文件，不格式化，不提交，不清理 worktree。

启动后先阅读：
- AGENTS.md
- myCPU/AGENTS.md
- docs/AGENTS.md
- docs/status/mainline_status.md
- docs/status/code_reself_status.md
- 与你审查范围直接相关的 design / status 文档

你的角色：<AGENT_NAME>
你的范围：<SCOPE>
你的重点：<FOCUS>

输出要求：
- findings 优先，不写泛泛总结。
- 最多 12 条 finding。
- 按 必须修复 / 建议修改 / 仅记录 分组。
- 每条 finding 必须包含：
  1. 标题
  2. 严重级别
  3. 文件路径和函数 / 类型 / 测试名
  4. 证据
  5. 风险
  6. 建议动作
  7. 建议验证命令
- 如果没有发现问题，明确说“本范围未发现必须修复项”，并列出残余风险或测试空白。

不要把以下已知非目标写成 bug：
- JIT / DBT 当前不是默认 backend。
- 标准 Linux 发行版 runtime 依赖外部 Image/rootfs，默认 fail-closed。
- AI 当前不开放任意模型上传、ONNX/PyTorch runtime 或 Linux-facing driver。
- frontend 不应伪造执行事实。

建议验证命令参考：
<COMMANDS>
```

## Coordinator 合并流程

### 任务 1：启动基线确认

**文件：**
- 读取：`AGENTS.md`
- 读取：`myCPU/AGENTS.md`
- 读取：`myCPU/guest/AGENTS.md`
- 读取：`docs/AGENTS.md`
- 读取：`docs/status/mainline_status.md`
- 读取：`docs/status/code_reself_status.md`

- [ ] **步骤 1：记录启动基线**
  运行：
  ```bash
  git status --short --branch
  git log -1 --oneline
  ```
- [ ] **步骤 2：确认 Linux 主线状态**
  阅读：
  ```bash
  sed -n '1,260p' docs/status/linux_distribution_platform_status.md
  sed -n '1,220p' docs/plan/post_wave7_linux_distribution_platform_longterm_plan.md
  ```
- [ ] **步骤 3：确认本轮审查只读边界**
  在 coordinator 工作记录中写明：
  - 审查基线 commit
  - 当前 dirty files
  - 哪些 dirty files 属于 Linux 主线并行进程
  - 本轮不改代码、不提交

### 任务 2：并行分派 5 个审查 agent

**文件：**
- 读取：本计划的 `并行 Agent 分工` 与 `Agent 通用审查提示词模板`

- [ ] **步骤 1：按本计划生成 5 条自包含 agent prompt**
- [ ] **步骤 2：并行启动 5 个只读 agent**
- [ ] **步骤 3：等待全部 agent 完成**
- [ ] **步骤 4：如果任一 agent 超范围改文件，立即丢弃其改动并只保留可验证发现**

### 任务 3：去重与分级

**文件：**
- 修改：`docs/status/code_reself_status.md`

- [ ] **步骤 1：合并 finding**
  按以下字段归并：
  - 文件 / 函数 / 测试名
  - 风险类型
  - 建议动作
- [ ] **步骤 2：删除重复或无证据 finding**
- [ ] **步骤 3：把每条 finding 归入 `必须修复`、`建议修改` 或 `仅记录`**
- [ ] **步骤 4：标记需要后续真实验证才能升级的问题**
- [ ] **步骤 5：把最终 active findings 写入 `docs/status/code_reself_status.md`**

### 任务 4：形成整改 backlog

**文件：**
- 修改：`docs/status/code_reself_status.md`
- 可能新增：后续整改计划文件，只有当用户明确要求启动修复时才新增

- [ ] **步骤 1：将 `必须修复` finding 排成第一批整改候选**
- [ ] **步骤 2：将跨多个子系统的 finding 标记为设计层整改，不直接派修复 agent**
- [ ] **步骤 3：将只影响文档口径或测试分层的问题标记为治理整改**
- [ ] **步骤 4：保留 `仅记录` 作为后续整体升级计划输入，不立即执行**

### 任务 5：完成态回写与归档

**文件：**
- 修改：`docs/status/code_reself_status.md`
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/full_code_review_parallel_agent_plan.md`

- [ ] **步骤 1：确认 `docs/status/code_reself_status.md` 已包含本轮最终结论**
- [ ] **步骤 2：追加归档条目到 `docs/plan/history_plan.md`**
- [ ] **步骤 3：删除本计划文件**
- [ ] **步骤 4：运行文档检查**
  ```bash
  git diff --check
  ```

## 完成定义

- 5 个 agent 全部完成只读审查，或明确记录某个 agent 未完成及原因。
- 每个 agent 至少覆盖自己的范围和关联设计 / 状态文档。
- Coordinator 已完成去重、分级和边界判断。
- `docs/status/code_reself_status.md` 已写入本轮 active findings、无问题结论或残余风险。
- findings 不包含无文件证据的泛泛建议。
- findings 不把已知非目标误写成 bug。
- 后续整改 backlog 已按 `必须修复 / 建议修改 / 仅记录` 分层。

## 验证要求

本轮审查默认不要求跑完整测试矩阵；但所有结论都必须给出建议验证命令。Coordinator 在完成回写前至少运行：

```bash
git diff --check
```

如果审查过程中需要确认当前 baseline 是否仍可运行，可按影响面选跑：

```bash
cd myCPU && make test-host-instruction_semantics_smoke
cd myCPU && make test-host-pipeline_backend_smoke
cd myCPU && make test-host-run_debug_cli_probe
cd frontend && node --test
```

只有在用户明确要求“验证当前 baseline”时，才把更重的命令作为本轮必跑项：

```bash
cd myCPU && make test-pipeline
cd myCPU && make test
```

真实外部 Linux / Spike / frontend e2e / AI demo v1 等 opt-in 验证，不作为本轮默认门禁；如果某条
finding 依赖它们，必须在 finding 里写清外部资产和环境变量要求。

## 完成态回写要求

- `docs/status/code_reself_status.md` 增加本轮审查摘要、active findings 和残余风险。
- 如果本轮没有发现必须修复项，也必须在 `docs/status/code_reself_status.md` 明确记录审查范围和剩余测试空白。
- 将“完成时间 + 审查范围 + findings 数量 + 后续整改方向”追加到 `docs/plan/history_plan.md`。
- 归档完成后删除本计划文件，不长期保留完成态 checklist。
