# 课程 OS kernel_alpha Stage 1-4 基线设计

## 文档定位

本文档合并并替代原先分散的四份 Stage 1 / Stage 2 / Stage 3 / Stage 4 设计文档，记录
《操作系统课程设计》`kernel_alpha` 分线的课程 OS 基线边界。

Stage 1-3 定义 `kernel_alpha_demo` 的一次性课程 OS summary smoke；Stage 4 定义
`guest_course_os_shell_demo` 的常驻 `course-os> ` 交互 shell 和浏览器 `/console` Lab 接入。
本文档只说明长期有效的边界、取舍、公共合同和验证口径；实时状态以
[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准，执行过程归档到
[../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan](../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage3-plan](../plan/history_plan.md#course-os-kernel-alpha-stage3-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage2-plan](../plan/history_plan.md#course-os-kernel-alpha-stage2-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
- 后续 plus 设计：
  - [course_os_kernel_alpha_linux_compat_plus_design.md](course_os_kernel_alpha_linux_compat_plus_design.md)
- 前端 / 调试链路：
  - [post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md)
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [minimal_interactive_os_design.md](minimal_interactive_os_design.md)
- 背景与创新来源：
  - [../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)
  - [../../OS_innovative_ideas.md](../../OS_innovative_ideas.md)

## 总体边界

`kernel_alpha` 当前是课程 OS 主线入口，不再以旧 Phase 1 `KMVPETDS` 作为当前正向能力承诺。
课程 OS 基线分成两类公共入口：

- `kernel_alpha_demo`：一次性正向 smoke，输出 `K/M/V/P/E/T` 基础 bring-up 后串联
  Stage 1 / Stage 2 / Stage 3 summary，然后关机，适合作为 functional / pipeline 门禁。
- `guest_course_os_shell_demo`：Stage 4 常驻交互入口，启动后进入 `course-os> `，适合作为
  浏览器 `/console`、UART terminal 和课程 OS shell 展示入口。

Stage 1-4 的设计不声明完整 POSIX、完整 Linux 用户态兼容、通用发行版运行、完整动态链接器、
完整 signal/futex、完整 TTY / job control、AI/NPU、JIT/DBT、Pipeline-aware scheduling 或微内核化。
这些方向如果继续推进，必须作为独立 plus / 后续阶段设计，不能回头扩大课程 OS 基线完成定义。

## 公共结构

课程 OS 基线采用“基础设施复用 + 阶段编排层 + 独立展示入口”的结构：

| 层 | 职责 | 约束 |
|---|---|---|
| guest runtime 基础设施 | PMM、Sv39、trap、user runtime、VM、storage、console、procfs | 作为共享能力维护，不把 stage demo 逻辑塞回基础设施 |
| 课程 OS 模块 | scheduler、memory、FS、syscall、process、FD、shell、ELF/libc、sync、procfs | 使用课程 ABI 和教学级合同，不声明 Linux ABI |
| `course_os_stageN` 编排 | 输出 Stage 1 / 2 / 3 marker 和固定证据摘要 | `kernel_alpha/main.c` 只串联编排，不堆业务逻辑 |
| `course_os_shell` 入口 | 常驻 `course-os> ` REPL、proc alias、浏览器 terminal 展示 | 不改变 `kernel_alpha_demo` 的一次性 marker 合同 |
| Linux compat plus | Stage 4 后的旁路 Linux ABI / ELF / rootfs / syscall 扩展 | 必须走 `linux_compat_*` 和 ABI 分流，不污染课程模块 |

## Stage 1：三模块九功能点与 `/proc` 证据面

Stage 1 把 `kernel_alpha` 从 bring-up demo 切换为课程 OS 主线入口，范围冻结为课程基本要求中的
3 个模块、9 个功能点：

| 模块 | 功能点 | 证据面 |
|---|---|---|
| 进程管理 | FCFS、RR、CFS-lite | 等待时间、周转时间、上下文切换次数、当前调度策略 |
| 内存管理 | Demand Paging、Clock 页面置换、`kmalloc` / `kfree` | 物理页统计、缺页次数、回收次数、释放后复用 |
| 文件系统 | 文件 / 目录 CRUD、`seek`、简化 B 树目录索引 | 文件/目录操作计数、路径解析次数、目录索引查找步数 |
| 证据面 | 只读 `/proc` | `/proc/ps`、`/proc/meminfo`、`/proc/schedstat`、`/proc/fsstat` |

Stage 1 的目标是稳定课程 OS 第一阶段答辩证据，不追求六大模块全覆盖。CFS-lite、Clock 和
B 树目录索引采用教学级实现：优先保证概念清晰、统计可观察、smoke 可验证，而不是复制完整
Linux CFS、完整页面回收或真实磁盘文件系统。

Stage 1 正向 summary 固定在 `kernel_alpha_demo` 的 `K/M/V/P/E/T` 基础 bring-up 之后输出：

```text
course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat
```

## Stage 2：A 方案核心闭环与 COW / crash 创新线

Stage 2 从“算法证据面”推进到“用户程序 -> syscall -> 进程 / FS / shell -> 可观测证据”的
课程 OS 闭环，覆盖：

- 课程 syscall ABI：`read`、`write`、`open`、`close`、`seek`、`exit`、`fork`、`exec`、
  `wait` / `waitpid`、`getpid`、`ps`、`kill` 等课程范围接口。
- 真实进程生命周期：PCB、父子关系、ready / running / blocked / zombie / dead 状态、
  `fork` / `exec` / `exit` / `waitpid`。
- 进程级 FD 表：`0/1/2`、普通文件、管道端点和只读 procfs 节点统一通过 FD 读写。
- 课程 FS 指标：至少 128 个文件、单文件 64KB、至少 3 层目录、绝对 / 相对路径、`seek`、
  `mkfs` 或等价可重复初始化。
- 课程 shell：`help`、`ls`、`cat`、`echo`、`ps`、`kill`、`cd`、`pwd`、`exit`、外部程序、
  参数、单级管道和基础重定向。
- 创新线：COW Fork、用户态崩溃隔离、crash report，以及 `/proc/syscalls`、`/proc/cow`、
  `/proc/crashlog`。

Stage 2 的硬边界是用户态错误必须被隔离为进程级退出或 zombie，不能升级为 kernel panic。
用户指针、坏 fd、坏路径、非法 syscall 和 procfs 写入都必须 fail-closed 并留下可诊断结果。

Stage 2 summary 固定为：

```text
course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog
```

## Stage 3：课程满分基线真实化

Stage 3 补齐 A 方案中答辩最容易被追问的硬指标，把 Stage 2 中仍偏 summary 的部分推进到更接近真实
OS 的可验证能力：

| 子面 | 合同 |
|---|---|
| ELF / libc / 用户程序 | RV64 little-endian 静态 ELF、`PT_LOAD` 装载、entry pc、用户栈 `argc/argv/envp`、简化 libc syscall wrapper、`hello` / `echo` / `cat` / `forktest` / `crashdemo` |
| 调度 / 同步 | FCFS、RR、CFS-lite 指标化；RR time slice / preempt count；课程级 semaphore / mutex 阻塞、唤醒、owner 和 misuse guard |
| VM / COW | Sv39 / `vm_fault` store fault 证据链、refcount、saved pages、fault count、copy count、leak summary |
| FS / shell | `mkfs`、`seek`、`unlink`、`rmdir`、128 文件、64KB 单文件、3 层目录、`sh /demo.sh` 脚本模式 |
| `/proc` | `/proc/cpuinfo`、`/proc/uptime`、`/proc/<pid>/status`、`/proc/<pid>/fd`、`/proc/<pid>/maps` |

Stage 3 不重写 Stage 1 / Stage 2 的核心模型，而是补证据链和固定门禁。ELF loader 只承诺课程级
静态 ELF；shell 只承诺课程级命令、单级 pipe、基础重定向和脚本逐行执行；`/proc/<pid>` 只承诺
status / fd / maps 三个只读节点。

Stage 3 summary 固定为：

```text
course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid
```

完整 `kernel_alpha_demo` 正向输出在 Stage 3 后保持：

```text
KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog|course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid
```

## Stage 4：前端交互 shell 与 `/console` Lab

Stage 4 不扩大 `kernel_alpha_demo` 的一次性 marker，而是新增独立 `guest_course_os_shell_demo`：

- 启动后进入 `course-os> ` prompt。
- 浏览器 `/console` 通过现有 manifest、debug session、terminal、reset 和 command wait 合同加载。
- terminal 输入命令后等待当前 offset 之后的新 `course-os> ` prompt，避免旧 prompt 提前 settle。
- 复用 Stage 3 的 `course_shell`、FD / FS、procfs、ELF / libc、COW 和 crash isolation。
- Course OS Shell Lab 说明它证明的是课程 OS shell，不是 Linux shell。

Stage 4 固定的 guest-side proc 快捷命令：

- `meminfo`
- `schedstat`
- `fsstat`
- `syscalls`
- `cow`
- `crashlog`
- `cpuinfo`
- `uptime`
- `status [pid]`
- `fd [pid]`
- `maps [pid]`

Stage 4 入口默认不调用 `platform_shutdown()`。浏览器 session 生命周期由 frontend `Terminate` 管理；
guest 内部 `exit` 只输出结果并回到 prompt。前端不应通过文本推断额外 OS 状态，也不应把 proc
快捷命令做成 UI 改写，所有命令必须由 guest shell 自己理解。

## Linux compat plus 分界

Stage 1-4 是课程 OS 基线，使用课程 ABI、课程 FS、课程 shell 和课程 ELF/libc 合同。Stage 4 后的
Linux 用户态兼容 plus 必须作为旁路扩展：

- Linux 兼容程序通过显式 `linux ...` launcher 或稳定后的 fallback 启动。
- Linux 进程使用独立 ABI 标记和 `linux_compat_*` syscall / rootfs / ELF / VM / trace 模块。
- 课程 shell 内置命令和 Stage 3 课程用户程序优先级高于 Linux rootfs PATH fallback。
- Linux compat 不能修改 Stage 1 / Stage 2 / Stage 3 marker，也不能改变 Stage 4 `course-os> ` prompt。

该分界保证课程 OS 答辩证据和 Linux compat 探索可以同时演进，但不会互相伪装完成态。

## 验证口径

Stage 1-4 基线至少守住：

- `cd myCPU && make test-unit-course_os_stage1`
- `cd myCPU && make test-unit-course_os_stage2_syscall`
- `cd myCPU && make test-unit-course_os_stage2_process`
- `cd myCPU && make test-unit-course_os_stage2_fd_fs`
- `cd myCPU && make test-unit-course_os_stage2_shell`
- `cd myCPU && make test-unit-course_os_stage2_cow_crash`
- `cd myCPU && make test-unit-course_os_stage2`
- `cd myCPU && make test-unit-course_os_stage3_elf`
- `cd myCPU && make test-unit-course_os_stage3_sched_sync`
- `cd myCPU && make test-unit-course_os_stage3_vm`
- `cd myCPU && make test-unit-course_os_stage3_fs_shell`
- `cd myCPU && make test-unit-course_os_stage3_proc`
- `cd myCPU && make test-unit-course_os_stage3`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
- `cd myCPU && make test-host-course_os_shell_terminal_smoke`
- `cd myCPU && make test-guest-course_os_shell_demo`
- `cd myCPU && make test-pipeline-guest-course_os_shell_demo`
- `cd frontend && node --test`
- `git diff --check`

触及 shared guest runtime 时，还应按根 `AGENTS.md` 和 `myCPU/guest/AGENTS.md` 补跑
kernel runtime、VM、trap、user task / program 和旧 9 条 `kernel_alpha` 负向 demo 门禁。

## 风险与取舍

- 如果把 `kernel_alpha_demo` 改成长驻 shell，会破坏一次性 marker 门禁；Stage 4 因此使用独立 guest entry。
- 如果让 frontend 改写命令或推断 OS 状态，会制造 UI 假象；Stage 4 只扩 manifest 和 terminal 合同。
- 如果课程 syscall / FS / shell 被扩成 Linux ABI，会污染课程 OS 基线；Linux compat 必须走旁路模块。
- 如果 `/proc` 扩成写控制面，会混淆证据面和控制面；Stage 1-4 只承诺只读证据。
- 如果把 AI/NPU、JIT/DBT、Pipeline-aware 调度提前塞进 Stage 1-4，会扩大课程基线，削弱可验收性。

## 当前有效性说明

- 当前有效：本文档是 Stage 1-4 课程 OS 基线的统一设计口径。
- 原 Stage 1 / Stage 2 / Stage 3 / Stage 4 四份独立设计文档已合并到本文档，不再作为独立设计入口维护。
- Stage 1 / Stage 2 / Stage 3 / Stage 4 均已完成，完成态以
  [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 和
  [../plan/history_plan.md](../plan/history_plan.md) 中对应归档为准。
