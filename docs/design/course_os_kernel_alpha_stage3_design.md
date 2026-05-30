# 课程 OS kernel_alpha 第三阶段设计

## 文档定位

本文档记录《操作系统课程设计》第三阶段在 `kernel_alpha` 上的当前有效设计边界。第三阶段定位为“课程满分基线真实化”：在 Stage 2 已经打通 syscall、进程生命周期、FD / FS、shell、COW 和 crash isolation 闭环之后，把仍偏教学 summary 的部分补成更接近真实 OS 的可验证能力。

本文档只说明长期有效的范围、接口边界和取舍；执行 checklist 完成后归档到 `history_plan.md`，实时状态以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage3-plan](../plan/history_plan.md#course-os-kernel-alpha-stage3-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage2-plan](../plan/history_plan.md#course-os-kernel-alpha-stage2-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
- 背景与创新来源：
  - [../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)
  - [../../OS_innovative_ideas.md](../../OS_innovative_ideas.md)

## 背景与问题

Stage 1 已经完成课程 OS 的进程、内存、文件系统三模块基础证据面；Stage 2 进一步完成 A 方案核心闭环和创新线：syscall ABI、真实进程生命周期、FD / FS 统一 I/O、5 个课程用户程序、shell、单级管道、重定向、COW Fork、用户态崩溃隔离，以及 `/proc/syscalls`、`/proc/cow`、`/proc/crashlog`。

A 方案仍然明确要求 ELF 加载、用户栈参数、简化 libc、FCFS + RR 两种调度算法、进程间同步、Demand Paging 的真实证据、文件系统 `mkfs` 和脚本化 shell 演示。第三阶段不继续扩大 Stage 2 的完成范围，而是补齐这些答辩中最容易被追问的硬指标。

创新清单中 AI/NPU、JIT/DBT、Pipeline-aware scheduling 和前端 Lab 都有辨识度，但它们会把当前主线带离“课程硬基线”。第三阶段选择先把课程要求落稳，再把全栈可观测、AI/NPU 或 Pipeline 方向留给后续独立阶段。

## 目标

- 建立教学级 ELF 用户程序加载路径，覆盖 ELF header / program header 校验、loadable segment 映射、entry pc 记录和用户栈 `argc` / `argv` / `envp` 初始化。
- 提供简化 libc syscall wrapper，让课程用户程序通过统一 syscall ABI 访问 `read` / `write` / `open` / `close` / `seek` / `exit` / `fork` / `exec` / `waitpid`。
- 固定至少 5 个真实用户程序：`hello`、`echo`、`cat`、`forktest`、`crashdemo`。
- 把 FCFS 和 RR 明确作为课程基线调度算法，保留 CFS-lite 作为 Stage 1 已落地创新算法。
- 扩展调度统计：average wait time、turnaround time、context switch count、preempt count、当前 policy 和 time slice。
- 实现课程级 semaphore / mutex 同步模型，覆盖阻塞、唤醒、owner 和 misuse guard。
- 把 COW Fork 的证据链从 Stage 2 教学匿名页模型推进到 Sv39 / `vm_fault` store fault 可验证路径，并暴露 saved pages、fault count、page refcount 和 leak summary。
- 固化文件系统课程指标：`mkfs`、`seek`、`unlink`、`rmdir`、绝对 / 相对路径、128 文件、单文件 64KB 和 3 层目录。
- 为 shell 增加 `sh /demo.sh` 脚本模式，支持 shebang、注释和逐行执行。
- 扩展 `/proc` 证据面：`/proc/cpuinfo`、`/proc/uptime`、`/proc/<pid>/status`、`/proc/<pid>/fd`、`/proc/<pid>/maps`。
- 新增 Stage 3 稳定 marker，并让 functional / pipeline `kernel_alpha_demo` 都验证 Stage 1 + Stage 2 + Stage 3 summary。

## 非目标

- 第三阶段不实现 AI/NPU syscall、AI Shell、NPU 中断驱动调度或 NPU 资源调度。
- 第三阶段不实现 JIT/DBT syscall、JIT profile、JIT 沙箱或自适应 JIT 调度。
- 第三阶段不实现 Pipeline-aware scheduling、`sys_perf` 或前端 Lab 面板。
- 第三阶段不做微内核化、FS 服务进程化、L4-like IPC、capability 文件描述符或容器隔离。
- 第三阶段不承诺完整 POSIX、完整 Unix signal、job control、动态链接、通用 Linux 用户态兼容或完整 bash。
- 第三阶段不把多级 pipe、`>>`、`2>`、复杂 quoting 或后台任务放进完成 marker；这些只作为后续 stretch。
- 第三阶段不扩展 COW 到文件系统 snapshot 或完整文件页 COW。
- 第三阶段不把 `/proc` 写接口作为控制面；`/proc` 仍保持只读证据面。

## 约束与边界

- `kernel_alpha` 继续作为课程 OS 主线入口，`interactive_os` monitor 不替代课程 shell。
- Stage 3 继续采用 `course_os_stageN` 总编排层；`kernel_alpha/main.c` 只负责 bring-up 后串联 Stage 1 / Stage 2 / Stage 3 summary。
- 新增能力优先落在 `myCPU/guest/kernel/` 和 `myCPU/guest/include/` 下的课程 OS 专用模块，不把课程逻辑散落到 simulator 主体或入口文件。
- syscall 编号、错误码和 libc wrapper 必须共享同一组 contract，避免用户程序和内核漂移。
- ELF loader 只承诺课程级静态 ELF；任何 malformed ELF 必须 fail-closed 并留下可读错误。
- 同步模型以课程进程表和调度器状态为事实来源，不引入完整 POSIX pthread 语义。
- VM / COW 证据优先复用现有 `vm_address_space`、`vm_process`、`vm_object` 和 `vm_fault` 合同；如果真实路径成本过高，必须至少用单测证明 fault-driven COW 的可观测数据流。
- 文件系统保持教学级 RAMFS / 简化 FS，不引入真实磁盘持久化一致性或 journaling。

## 方案

### 结构设计

第三阶段采用“硬基线补齐 + 证据面统一”的结构：

| 层 | 职责 | 主要证据 |
|---|---|---|
| ELF / libc / 用户程序 | ELF image catalog、静态装载、用户栈参数、libc wrapper、5 个用户程序 | `test-unit-course_os_stage3_elf`、`exec hello`、`exec crashdemo` |
| 调度 / 同步 | FCFS / RR / CFS-lite 指标、time slice、semaphore、mutex、blocked / wakeup | `/proc/schedstat`、`test-unit-course_os_stage3_sched_sync` |
| VM / COW | Sv39 store fault 触发 COW、refcount、saved pages、leak summary | `/proc/cow`、`/proc/meminfo`、`test-unit-course_os_stage3_vm` |
| FS / shell | `mkfs`、`seek`、`unlink`、`rmdir`、容量指标、`sh /demo.sh` | `test-unit-course_os_stage3_fs_shell`、shell transcript |
| `/proc` | CPU、uptime、per-pid status / fd / maps | `test-unit-course_os_stage3_proc` |
| demo / guardrail | 稳定 marker、正向课程场景、旧负向 demo 兼容 | `test-guest-kernel_alpha_demo`、`test-pipeline-guest-kernel_alpha_demo` |

### 稳定 Marker

第三阶段正向 marker 固定为：

```text
course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid
```

完整 `kernel_alpha_demo` 正向输出在 Stage 3 完成后应保持：

```text
KMVPET|course-os-stage1 ...|course-os-stage2 ...|course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid
```

Stage 3 marker 不携带 AI/NPU、JIT/DBT、Pipeline-aware 或 frontend 字段。

### ELF、Libc 与用户程序

ELF loader 的课程级最小合同：

- 只接受 RV64 little-endian static executable。
- 校验 ELF magic、class、endianness、version、machine、type、program header bounds。
- 只装载 `PT_LOAD` segment。
- segment 映射必须记录 code / data / stack 区间，供 `/proc/<pid>/maps` 输出。
- `argc` / `argv` / `envp` 写入用户栈或等价课程栈模型。
- bad ELF 返回错误，不破坏当前 shell、进程表或文件系统状态。

libc wrapper 的合同：

- wrapper 名称与 syscall 表一一对应。
- wrapper 只做用户态参数打包，不绕过 syscall 访问内核对象。
- `exit`、`fork`、`exec`、`waitpid`、`open`、`read`、`write`、`seek` 必须有单测覆盖。

五个固定用户程序：

- `hello`：验证 ELF + libc `write`。
- `echo`：验证 argv 传递。
- `cat`：验证 libc `open` / `read` / `write`。
- `forktest`：验证 `fork` / `waitpid` 与 COW 统计。
- `crashdemo`：验证用户态崩溃隔离和 crash report。

### 调度与同步

Stage 1 已经存在 FCFS / RR / CFS-lite。Stage 3 不重写调度器，而是补齐课程硬指标证据：

- FCFS 和 RR 必须在 Stage 3 demo 中各运行一次固定 workload。
- RR 必须记录 time slice 和 preempt count。
- CFS-lite 保留为第三种可观测 policy。
- `/proc/schedstat` 输出当前 policy、time slice、context switches、preempts、average wait time 和 average turnaround time。

同步模型：

- semaphore 支持 `init`、`wait`、`post`。
- mutex 支持 `lock`、`unlock`、owner 记录和非法 unlock guard。
- `wait` / `lock` 失败时进程进入 `blocked`，释放后回到 `ready`。
- Stage 3 不实现 priority inheritance；可在后续创新阶段单独设计。

### VM / COW 证据链

Stage 2 的 COW 已覆盖课程匿名用户页。Stage 3 的升级点是把证据链接近真实 VM：

- fork 后父子共享用户页并保持只读 / COW 标记。
- store fault 进入 `vm_fault` 或等价 fault policy 后执行 copy。
- copy 后父子数据隔离，refcount 更新。
- 当共享页只剩一个引用时，统计可显示已解除共享。
- `/proc/cow` 或 `/proc/meminfo` 至少暴露 `cow_faults`、`saved_pages`、`shared_pages`、`copied_pages`、`refcount_peak`、`leak_free=yes|no`。

### FS、Shell 与 Demo Script

Stage 3 固化文件系统课程指标：

- `course_fs_mkfs` 或等价入口可重复初始化根目录和默认演示文件。
- `seek` 对普通文件有效，对 bad fd / proc / 只读路径返回明确错误。
- `unlink` 拒绝删除目录，`rmdir` 拒绝删除非空目录。
- 128 文件、64KB 单文件和 3 层目录必须有单测固定。

shell 脚本模式：

- 支持 `sh /path/to/script.sh`。
- 支持忽略 shebang 和 `#` 注释行。
- 按行复用现有 `course_shell_run_line`。
- 脚本失败时返回失败行号和命令摘要；已执行行的输出保留在 transcript 中。

Stage 3 推荐固定演示脚本：

```sh
#!/bin/sh
echo "=== stage3 profile ==="
cat /proc/cpuinfo
cat /proc/uptime
cat /proc/meminfo
ps
exec hello
exec forktest
exec crashdemo
cat /proc/crashlog
```

### `/proc` 扩展

新增只读节点：

- `/proc/cpuinfo`：ISA、backend、kernel alpha stage 字段；没有 simulator 只读快照时可输出 guest 已知静态字段。
- `/proc/uptime`：tick 或 demo runtime 计数。
- `/proc/<pid>/status`：pid、ppid、state、name、exit_code、crash flag。
- `/proc/<pid>/fd`：进程 FD 表摘要。
- `/proc/<pid>/maps`：ELF code / data / stack / heap / COW 区间摘要。

所有未知 pid、未知节点、写入 `/proc` 的行为必须 fail-closed。

## 验证思路

第三阶段新增窄门禁：

- `cd myCPU && make test-unit-course_os_stage3_elf`
- `cd myCPU && make test-unit-course_os_stage3_sched_sync`
- `cd myCPU && make test-unit-course_os_stage3_vm`
- `cd myCPU && make test-unit-course_os_stage3_fs_shell`
- `cd myCPU && make test-unit-course_os_stage3_proc`
- `cd myCPU && make test-unit-course_os_stage3`

第三阶段完成门禁：

- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `git diff --check`

旧 Stage 1 / Stage 2 unit targets 和 9 条 `kernel_alpha` 负向 guest demo 继续作为回归 guardrail。

## 风险与取舍

- ELF loader 容易扩成通用 Linux ABI。第三阶段只做课程级静态 ELF，不接动态链接和完整 POSIX。
- syscall wrapper、shell 和用户程序可能绕过同一套 ABI。实现时必须让用户程序通过 libc wrapper，shell 也尽量走同一 FD / process contract。
- `vm_fault` 与 Stage 2 COW 教学模型可能存在抽象差异。第三阶段优先补证据链和测试，不重写已有 VM 主路径。
- `/proc/<pid>` 容易膨胀成完整 procfs。第三阶段只做 status / fd / maps 三个节点。
- 文档、status、plan 必须保持分工：design 固定边界，plan 记录 checklist，status 只记录当前状态和下一步。

## 当前有效性说明

- 当前有效：本文档作为课程 OS `kernel_alpha` 第三阶段的设计口径。
- 第三阶段当前已经实现并归档；完成态以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 和 [../plan/history_plan.md#course-os-kernel-alpha-stage3-plan](../plan/history_plan.md#course-os-kernel-alpha-stage3-plan) 为准。
- 第一阶段完成态以 [../design/course_os_kernel_alpha_stage1_design.md](course_os_kernel_alpha_stage1_design.md) 和 [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan) 为准。
- 第二阶段完成态以 [../design/course_os_kernel_alpha_stage2_design.md](course_os_kernel_alpha_stage2_design.md)、[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 和 [../plan/history_plan.md#course-os-kernel-alpha-stage2-plan](../plan/history_plan.md#course-os-kernel-alpha-stage2-plan) 为准。
