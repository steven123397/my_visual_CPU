# 课程 OS 架构后续增强计划

> **文档状态：** 候选后续计划，不是展示前门禁

## 文档定位

本文档承接原单体缺口收口计划中风险较高、会影响 trap / timer / scheduler / console / ELF 执行链路的任务。它们是课程 OS 的后续增强，不应作为展示前 P0 补洞。

## 关联文档

- 边界设计：[../design/course_os_gap_closure_boundary_design.md](../design/course_os_gap_closure_boundary_design.md)
- 课程 OS 基线设计：[../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
- 调度 timing 合同：[../design/course_os_scheduler_timing_contract.md](../design/course_os_scheduler_timing_contract.md)
- 课程 OS 真实用户 ELF 来源设计：[../design/course_os_real_user_elf_design.md](../design/course_os_real_user_elf_design.md)
- 平台 MMIO 合同：[../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
- 当前状态：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)

## 目标

在展示前缺口收口完成后，按独立设计和窄验证推进在线抢占调度、UART 中断驱动输入、真实课程 ELF、多级管道和 context switch cost 证据。

## 验证层级

- 默认门禁：
  - 与具体任务相关的新增 `test-unit-course_os_*`
  - `cd myCPU && make test-guest-kernel_alpha_demo`
  - `cd myCPU && make test-guest-course_os_shell_demo`
  - `git diff --check`
- Slow guest 门禁：
  - `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
  - `cd myCPU && make test-pipeline-guest-course_os_shell_demo`
  - 仅在 trap / timer / pipeline-visible guest 行为被修改时启用。
- Opt-in external 门禁：
  - 真实课程 ELF 如果依赖本机交叉编译器，必须显式声明工具链变量；缺工具链时不能影响默认回归。

## 完成定义

- 每个架构项先有窄设计或合同，再进入实现。
- 不破坏 Stage 1/2/3 marker、Stage 4 `course-os> ` prompt 和 Linux compat Plus 旁路边界。
- 每个任务有 host unit 或 guest smoke 证明，不以“能手动演示”为唯一验收。

## 任务

### 任务 1：在线抢占调度设计与实现（G3）

**文件：**
- 新增或修改：`docs/design/course_os_preemptive_scheduler_design.md`
- 修改：`myCPU/guest/include/course_scheduler.h`
- 修改：`myCPU/guest/kernel/course_scheduler.c`
- 修改：`myCPU/guest/kernel/trap_dispatch.c`
- 新增：`myCPU/tests/unit/course_os_preemptive_sched.c`

- [ ] **步骤 1：先补设计。** 明确离线统计调度器与在线调度器的职责边界、timer tick 输入、进程状态转换、context switch 统计和 Stage marker 兼容策略。
- [ ] **步骤 2：补红灯回归。** 新增在线 RR 时间片到期切换、FCFS 不抢占、CFS-lite 权重调度的 host 单测。
- [ ] **步骤 3：实现在线调度器。** 新增独立 online scheduler 状态，不改坏现有 `course_scheduler_run()` 离线统计证据。
- [ ] **步骤 4：接入 timer path。** 只在明确启用 online scheduler 时让 supervisor timer handler 驱动 tick。
- [ ] **步骤 5：验证。** 运行 `cd myCPU && make test-unit-course_os_preemptive_sched test-guest-kernel_alpha_demo`。

### 任务 2：UART 中断驱动输入（G7）

**文件：**
- 新增或修改：`docs/design/course_os_uart_interrupt_input_design.md`
- 修改：`myCPU/guest/kernel/console_input.c`
- 修改：`myCPU/guest/kernel/trap_dispatch.c`
- 修改：`myCPU/tests/unit/course_os_stage2_shell.c`

- [ ] **步骤 1：先补设计。** 定义轮询 fallback、中断 buffer、PLIC external interrupt 接入点和前端 terminal 合同。
- [ ] **步骤 2：补红灯回归。** 用 host/unit 层验证 interrupt-mode buffer 收到字符后能唤醒 shell 输入路径。
- [ ] **步骤 3：实现 interrupt mode。** 在 external interrupt handler 中读取 UART RX 字符并写入 console input buffer。
- [ ] **步骤 4：验证。** 运行 `cd myCPU && make test-guest-course_os_shell_demo`；若改动 PLIC / trap 共享路径，再运行 `cd myCPU && make test-guest-kernel_alpha_demo`。

### 任务 3：真实课程 ELF 用户程序（G5） - 已完成

**文件：**
- 新增：`docs/design/course_os_real_user_elf_design.md`
- 修改：`docs/plan/project_evolution_priority_p1_plan.md`
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`docs/index.md`
- 修改：`myCPU/guest/include/course_process.h`
- 修改：`myCPU/guest/kernel/course_process.c`
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/guest/kernel/course_user_programs.c`
- 修改：`myCPU/tests/unit/course_os_stage2_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage3_elf.c`

- [x] **步骤 1：先补设计。** 默认路径选择仓库内手写最小 RV64 `ET_EXEC`，不让默认回归依赖本机未声明交叉工具链；同时把 P1 的 `exec /path` 外部 ELF 合同合并到同一份设计中。
- [x] **步骤 2：补红灯回归。** 断言 5 个课程用户程序不是共用同一段占位 ELF，且 loader 能看到真实 `PT_LOAD` / entry 差异；同时覆盖 `course_process_exec_image()` 和 shell `exec /path` 从课程 FS 读取 ELF。
- [x] **步骤 3：替换课程用户程序资产。** 内置 Stage 3 catalog 改为 5 份不同的手写最小 RV64 ELF；`course_process_exec()` 变成 catalog wrapper，内置 ELF 和课程 FS 文件 ELF 统一进入 `course_process_exec_image()`。
- [x] **步骤 4：验证。** 已运行并通过 `cd myCPU && make test-unit-course_os_stage2_shell test-unit-course_os_stage3_elf`、`cd myCPU && make test`、`cd myCPU && make test-pipeline-guest-course_os_shell_demo test-pipeline-guest-kernel_alpha_demo`、`cd myCPU && make test-pipeline` 和 `git diff --check`。

完成标注（2026-06-16）：本项完成课程 ELF 来源统一化第一刀。5 个 Stage 3 课程程序现在有不同
entry / code segment 的最小 RV64 `ET_EXEC` bytes；`course-os> exec /path/to/prog [arg]`
能从课程 FS 中读取受控 ELF 文件并复用 `course_elf_loader` / `course_process` 装载路径。
该能力不执行 host 任意路径，不依赖外部 rootfs 或交叉编译器，也不把 Linux compat 语义倒灌进
课程 `course_*` 模块；坏 ELF、缺文件和目录路径按设计 fail-closed。

### 任务 4：多级管道（G8） - 已完成

**文件：**
- 修改：`myCPU/guest/include/course_shell.h`
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage2_shell.c`

- [x] **步骤 1：确认需求边界。** 当前课程基线已满足单级管道；多级管道是 shell 能力增强，不阻塞展示前验收。
- [x] **步骤 2：补红灯回归。** 新增 `echo hello | cat | cat` 和空 stage 错误输入测试。
- [x] **步骤 3：改 parser 和执行模型。** 将 `left/right` 管道结构扩展为有上限的 stage 数组，逐级传递 stdout 到下一级 stdin。
- [x] **步骤 4：验证。** 运行 `cd myCPU && make test-unit-course_os_stage2_shell test-guest-course_os_shell_demo`。

完成标注（2026-06-15）：shell parser 已切换为最多 8 级的 `pipeline[]`，多级管道执行通过双 scratch buffer
逐级传递 stdout；单 stage 命令保留原有直接写 caller output 的语义，避免脚本模式递归执行时破坏输出汇总。
本项只完成 G8；当前本计划仍未完成的任务是任务 1 在线抢占调度和任务 2 UART 中断驱动输入。

### 任务 5：context switch cost 证据（G9） - 已完成

**文件：**
- 新增或修改：`docs/design/course_os_scheduler_timing_contract.md`
- 修改：`myCPU/guest/kernel/course_scheduler.c`
- 修改：`myCPU/guest/kernel/procfs.c`
- 修改：`myCPU/tests/unit/course_os_preemptive_sched.c`

- [x] **步骤 1：先补 timing 合同。** 明确模拟器 cycle 到时间的换算来源；没有该合同前不写死“QEMU < 1ms”。
- [x] **步骤 2：补红灯回归。** 单测验证 scheduler 能记录最近一次 switch cycle cost，procfs 能输出该字段。
- [x] **步骤 3：实现统计。** 在当前离线 scheduler run path 记录 cycle 差值；`/proc/schedstat` 输出 cycle 字段，时间字段只在换算合同存在时输出。
- [x] **步骤 4：验证。** 运行 `cd myCPU && make test-unit-course_os_preemptive_sched`。

完成标注（2026-06-16）：当前任务未实现任务 1 的在线调度器，也未接 trap / timer path；本轮先把合同限定为
离线 `course_scheduler_run()` 的 scheduler-local cycle delta。`course_scheduler_summary_t` 新增
`last_switch_cycle_cost` / `total_switch_cycle_cost`，`/proc/schedstat` 输出 cycle-only 字段，不输出
ns / us / ms 时间换算字段。后续在线调度器完成后可在保持字段名稳定的前提下替换 cycle 来源。

## 完成态回写要求

- 单项完成即可回写 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 的当前能力与剩余风险。
- 全部完成后追加 [history_plan.md](history_plan.md)，再删除本计划。
