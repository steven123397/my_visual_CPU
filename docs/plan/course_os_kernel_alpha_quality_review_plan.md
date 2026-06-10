# course-os kernel_alpha quality review 计划

> **文档状态：** 执行中

## 文档定位

本文档记录对 `kernel_alpha` 课程 OS 层代码做一次以
[../../.agents/skills/mycpu-quality-review/SKILL.md](../../.agents/skills/mycpu-quality-review/SKILL.md)
为核心的只读质量审查计划。

本计划的第一目标是审查和提出收敛建议，不直接实现 Stage 12、Stage 13、`rustc`
或更宽 Linux 用户态兼容层。

## 关联文档

- 来源设计 / 参考：
  - [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
  - [../design/course_os_kernel_alpha_linux_compat_plus_design.md](../design/course_os_kernel_alpha_linux_compat_plus_design.md)
  - [../../undefined_os_study_notes.md](../../undefined_os_study_notes.md)
  - [../../.agents/skills/mycpu-quality-review/SKILL.md](../../.agents/skills/mycpu-quality-review/SKILL.md)
- 目标状态：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)

## 目标

- 以 `mycpu-quality-review` 的 `review-only` 模式审查课程 OS 层代码。
- 从 OS 模型、架构和算法角度评估可收敛空间，而不是继续扩大 Linux syscall breadth。
- 区分课程 OS 主体、Linux compat 旁路和基础 guest runtime，不把三者揉成一个大框架。
- 输出分级 findings，并为后续 `safe-fixes` / `fix-and-validate` 切片提供候选。

## 完成定义

- 已完成只读审查，finding 使用 `[必须修复]`、`[建议修改]`、`[仅记录]`、`[问题]` 分级。
- 每条 finding 至少写清影响范围、为什么是问题、建议动作和建议验证。
- 已明确哪些建议来自 Undefined-OS 架构参考，哪些不适合当前项目照搬。
- 未修改生产代码，除非用户在审查后另行批准后续修复切片。
- 完成后已同步更新 `kernel_alpha_status.md` 的 review 结果摘要，并归档到 `history_plan.md`。

## 审查范围

### 主范围

- `myCPU/guest/kernel/course_scheduler.c`
- `myCPU/guest/kernel/course_memory.c`
- `myCPU/guest/kernel/course_fs.c`
- `myCPU/guest/kernel/procfs.c`
- `myCPU/guest/kernel/course_elf_loader.c`
- `myCPU/guest/kernel/course_libc.c`
- `myCPU/guest/kernel/course_sync.c`
- `myCPU/guest/kernel/course_os_stage1.c`
- `myCPU/guest/kernel/course_os_stage2.c`
- `myCPU/guest/kernel/course_os_stage3.c`
- `myCPU/guest/kernel/course_shell.c`
- `myCPU/guest/kernel/course_user_programs.c`
- `myCPU/guest/course_os_shell`
- `myCPU/guest/kernel_alpha`

### 边界阅读范围

- `myCPU/guest/kernel/linux_compat_*`
  - 只审查课程 OS 与 Linux compat 的边界是否清楚。
  - 不把 Linux compat syscall breadth 作为本计划的修复目标。
- `myCPU/guest/kernel/vm_*`
- `myCPU/guest/kernel/trap_*`
- `myCPU/guest/kernel/user_*`
  - 只在课程 OS review 需要理解共享 runtime 边界时阅读。

## 任务

### 任务 1：锁定 worktree 和规则

**文件：**
- 读取：`AGENTS.md`
- 读取：`docs/AGENTS.md`
- 读取：`myCPU/AGENTS.md`
- 读取：`myCPU/guest/AGENTS.md`
- 读取：`docs/status/kernel_alpha_status.md`
- 读取：`.agents/skills/mycpu-quality-review/SKILL.md`

- [ ] **步骤 1：进入课程 OS worktree**
  - 运行：`cd /home/liangjiaqi/projects/my_visual_CPU/.worktrees/course-os-kernel-alpha`
  - 运行：`git status --short --branch`
  - 预期：分支为 `course-os-kernel-alpha`；如有脏状态，先记录，不清理、不回滚。
- [ ] **步骤 2：确认 review-only 模式**
  - 本计划只读审查，不改生产代码。
  - 发现可修项后，先记录 finding；后续是否进入 `safe-fixes` 由用户确认。
- [ ] **步骤 3：确认非目标**
  - 不推进 Stage 12 / Stage 13。
  - 不扩 `rustc`。
  - 不把完整 signal / futex / mmap / ext4 作为近期实现目标。

### 任务 2：按质量 rubric 做第一轮只读审查

**文件：**
- 读取：审查范围中的 `course_*`、`procfs`、`kernel_alpha` 和 `course_os_shell` 文件。

- [ ] **步骤 1：contract safety**
  - 检查 Stage 1 / Stage 2 / Stage 3 marker 是否被清楚隔离。
  - 检查课程 OS 逻辑是否绕过共享 guest runtime 合同。
  - 检查 Linux compat 旁路是否污染课程级 `course_*` 模块。
- [ ] **步骤 2：structural convergence**
  - 检查单文件是否混合调度、VM、FS、shell、demo 编排等不相关职责。
  - 检查 stage orchestration 是否可以继续收口到更小的模型对象。
- [ ] **步骤 3：duplication and drift**
  - 检查 procfs / stage summary / shell / user program catalog 是否重复维护同一事实。
  - 检查 Undefined-OS 参考内容是否已经被文档误写成当前实现承诺。
- [ ] **步骤 4：validation signal quality**
  - 检查现有 guest unit / smoke 是否能证明建议修改不破坏 Stage 1-4 和 Stage 11 v0。
  - 标出每个 finding 的最窄建议验证命令。
- [ ] **步骤 5：实现收敛与诊断边界**
  - 检查课程 OS / Linux compat 实现是否存在臃肿、杂乱、文件过大或职责堆叠问题。
  - 检查红灯测试、临时诊断测试、host smoke 和长期 guardrail 是否放在合适层级，避免诊断噪音混入正常路径。
  - 检查功能是否在 stage 编排、shell、procfs、runtime 或 Linux compat 之间混乱重叠，并标出可收敛边界。

### 任务 3：从 OS 模型、架构、算法角度形成 findings

**文件：**
- 修改：`docs/plan/course_os_kernel_alpha_quality_review_plan.md`

- [ ] **步骤 1：按分级记录 findings**
  - `[必须修复]`：contract、安全、fail-closed 或状态口径污染。
  - `[建议修改]`：职责堆叠、重复模型、算法证据不清、验证噪音。
  - `[仅记录]`：值得后续单独立计划的模型升级，例如 process model、VFS metadata、procfs 观察面。
  - `[问题]`：需要用户确认课程目标或展示目标。
- [ ] **步骤 2：标注 Undefined-OS 参考的采用方式**
  - 可参考：进程生命周期对象、VFS inode / metadata、地址空间 backend 思路、stub 分层治理。
  - 谨慎参考：完整 futex / signal / mmap / ext4。
  - 不采用：换底座、多架构优先、继续追完整 Linux userland。
- [ ] **步骤 3：给出后续切片建议**
  - 只推荐 1 到 3 个 `safe-fixes` 或 `fix-and-validate` 候选。
  - 每个候选必须写明验证命令和不改变的 public marker。

### 任务 4：完成态同步与归档

**文件：**
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/course_os_kernel_alpha_quality_review_plan.md`

- [ ] **步骤 1：状态文档回写**
  - 在 `kernel_alpha_status.md` 中记录 review 完成摘要、是否存在必须修复项、下一步建议。
- [ ] **步骤 2：归档计划**
  - 向 `history_plan.md` 追加完成时间、审查范围、finding 概况和建议验证。
  - 删除本计划文件。
- [ ] **步骤 3：提交前验证**
  - review-only 默认不跑实现测试。
  - 至少运行：
    ```bash
    git diff --check
    git diff --cached --check
    ```

## 建议验证基线

- review-only：
  - `git diff --check`
  - `git diff --cached --check`
- 若后续进入课程 OS 代码 `safe-fixes`：
  - `cd myCPU && make test-unit-course_os_stage1`
  - `cd myCPU && make test-unit-course_os_stage2`
  - `cd myCPU && make test-unit-course_os_stage3`
  - `cd myCPU && make test-guest-kernel_alpha_demo`
  - `cd myCPU && make test-guest-course_os_shell_demo`
- 若改动触及 Stage 11 v0 旁路边界：
  - `cd myCPU && make test-unit-course_os_stage11_linux_compat`
  - `cd myCPU && make test-host-course_os_linux_compat_external_workflow_smoke`

## 完成态回写要求

- 全部 checklist 必须勾完。
- `kernel_alpha_status.md` 必须记录 review 完成摘要和下一步建议。
- 如果审查结论改变课程 OS 设计口径，必须同步更新
  `docs/design/course_os_kernel_alpha_course_os_baseline_design.md` 或
  `docs/design/course_os_kernel_alpha_linux_compat_plus_design.md` 的对应部分。
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
