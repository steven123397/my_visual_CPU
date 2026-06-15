# 课程 OS 展示前缺口收口计划

> **文档状态：** 执行中

## 文档定位

本文档只管理展示前可低风险补齐的课程 OS 证据面。原单体缺口收口计划中涉及 trap / timer / scheduler / ELF 执行链路、OSComp 或外部资产的内容，已拆到：

- [course_os_arch_followup_plan.md](course_os_arch_followup_plan.md)
- [course_os_plus_external_validation_plan.md](course_os_plus_external_validation_plan.md)

## 关联文档

- 边界设计：[../design/course_os_gap_closure_boundary_design.md](../design/course_os_gap_closure_boundary_design.md)
- 课程要求：[../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)
- 课程 OS 基线设计：[../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
- 当前状态：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)

## 目标

让 Stage 1-4 已有课程 OS 能力在课程汇报、前端 shell 和 guest smoke 中更直接可见，优先收口目录枚举、`ls`、`kill`、同步演示、`mkfs` 和时钟频率证据。

## 验证层级

- 默认门禁：
  - `cd myCPU && make test-unit-course_os_stage1`
  - `cd myCPU && make test-unit-course_os_stage2_shell`
  - `cd myCPU && make test-unit-course_os_stage2_process`
  - `cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - `cd myCPU && make test-unit-course_os_stage3_sched_sync`
  - `cd myCPU && make test-guest-course_os_shell_demo`
  - `git diff --check`
- Slow guest 门禁：
  - `cd myCPU && make test-guest-kernel_alpha_demo`，仅在 Stage 1/2/3 marker 或 demo 编排被触及时运行。
- Opt-in external 门禁：
  - 本计划不引入外部资产依赖。

## 完成定义

- `ls`、`kill` 不再是固定字符串 stub。
- `course_fs_listdir` 合同明确，且覆盖空目录、普通目录、非目录路径、缺失路径和缓冲区不足。
- 同步机制、`mkfs` 和 `timer_hz` 有 shell / procfs 可展示证据。
- Stage 1/2/3 marker 与 Stage 4 `course-os> ` prompt 不变。
- 完成后更新 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)、[history_plan.md](history_plan.md) 和 [../index.md](../index.md)，再删除本计划。

## 任务

### 任务 1：FS 层目录枚举接口（G4）

**文件：**
- 修改：`myCPU/guest/include/course_fs.h`
- 修改：`myCPU/guest/kernel/course_fs.c`
- 修改：`myCPU/tests/unit/course_os_stage1.c`

- [x] **步骤 1：补红灯回归。** 在 `test_course_fs_listdir` 中增加精确输出断言和小缓冲区失败断言，确认旧实现会在缓冲区不足时静默返回成功。
- [x] **步骤 2：固定接口合同。** 在 `course_fs.h` 记录直接子节点、空格分隔、换行结尾、完整输出才成功的合同。
- [x] **步骤 3：改实现。** 在 `course_fs_listdir` 中先计算完整输出长度，确认 `out_size` 足够后再写入，避免部分输出被当作成功。
- [x] **步骤 4：验证。** 运行 `cd myCPU && make test-unit-course_os_stage1`。

### 任务 2：真实 `ls` 命令（G1）

**文件：**
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage2_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage3_fs_shell.c`

- [x] **步骤 1：补红灯回归。** 新增 `ls` 单测：创建文件后运行 `ls` 应输出文件名，`ls /home` 应输出目标目录内容，缺失路径应输出诊断而不是固定 `".\n"`。
- [x] **步骤 2：替换 stub。** 将 `course_shell.c` 中的 `ls` 改为调用 `course_fs_listdir(&shell->fs, target_path, out, remaining_size)`；无参数使用 cwd，有参数解析为绝对或相对路径。
- [x] **步骤 3：保持输出稳定。** 目录项顺序沿用 `course_fs_listdir` 的文件名升序；输出保留末尾换行。
- [x] **步骤 4：验证。** 运行 `cd myCPU && make test-unit-course_os_stage2_shell test-unit-course_os_stage3_fs_shell`。

### 任务 3：真实 `kill` 命令（G2）

**文件：**
- 修改：`myCPU/guest/include/course_process.h`
- 修改：`myCPU/guest/kernel/course_process.c`
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage2_process.c`
- 修改：`myCPU/tests/unit/course_os_stage2_shell.c`

- [x] **步骤 1：补红灯回归。** 新增 process 单测覆盖目标进程被置为 zombie / killed 状态、缺失 pid 失败、不能 kill init 或当前 shell 进程。
- [x] **步骤 2：实现进程层 API。** 新增 `course_process_kill(...)`，只修改课程 process table 状态，不引入 Linux signal 语义。
- [x] **步骤 3：替换 shell stub。** `kill <pid>` 解析目标 pid，成功输出 `killed <pid>`，失败输出可诊断原因。
- [x] **步骤 4：验证。** 运行 `cd myCPU && make test-unit-course_os_stage2_process test-unit-course_os_stage2_shell`。

### 任务 4：同步机制 shell 演示（G6）

**文件：**
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage3_sched_sync.c`

- [ ] **步骤 1：补红灯回归。** 新增 `sem` / `mutex` 命令单测，覆盖 init、wait/post、lock/unlock 和错误顺序。
- [ ] **步骤 2：新增 shell 命令。** 增加 `sem`、`mutex`、`concurrency_demo`，只展示已有课程同步对象的状态变化。
- [ ] **步骤 3：验证。** 运行 `cd myCPU && make test-unit-course_os_stage3_sched_sync`。

### 任务 5：`mkfs` shell 命令（G10）

**文件：**
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage3_fs_shell.c`

- [ ] **步骤 1：补红灯回归。** 创建文件后运行 `mkfs`，再运行 `ls` 应返回空目录输出。
- [ ] **步骤 2：新增命令。** `mkfs` 调用 `course_fs_mkfs(&shell->fs)` 并输出 `mkfs: filesystem initialized`。
- [ ] **步骤 3：验证。** 运行 `cd myCPU && make test-unit-course_os_stage3_fs_shell`。

### 任务 6：时钟频率证据（G11）

**文件：**
- 修改：`myCPU/guest/include/timer.h`
- 修改：`myCPU/guest/kernel/timer.c`
- 修改：`myCPU/guest/kernel/procfs.c`
- 修改：`myCPU/tests/unit/course_os_stage3_proc.c`

- [ ] **步骤 1：补红灯回归。** 在 procfs 单测中断言 `/proc/cpuinfo` 或等价输出包含 `timer_hz=100`。
- [ ] **步骤 2：固定课程证据字段。** 暴露 `TIMER_HZ 100` 或等价只读字段；不借此重写平台 timer 合同。
- [ ] **步骤 3：验证。** 运行 `cd myCPU && make test-unit-course_os_stage3_proc`。

## 完成态回写要求

- 本计划的 checklist 全部完成后，向 [history_plan.md](history_plan.md) 追加完成摘要。
- [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 只保留当前完成态、剩余风险和下一步，不抄完整 checklist。
- [../index.md](../index.md) 删除已完成计划链接，保留对应 history anchor。
