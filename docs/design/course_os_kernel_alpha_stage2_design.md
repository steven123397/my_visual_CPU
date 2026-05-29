# 课程 OS kernel_alpha 第二阶段设计

## 文档定位

本文档记录《操作系统课程设计》第二阶段在 `kernel_alpha` 上的当前有效设计边界。第二阶段从第一阶段的“3 个模块、9 个功能点”推进到“尽量补齐 A 方案全文要求”，并把创新线锁定为 COW Fork、用户态崩溃隔离和 `/proc` 可观测。

本文档只说明长期有效的范围、接口边界和取舍；执行 checklist 写入对应 `plan` 文档，实时状态以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
- 背景与创新来源：
  - [../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)
  - [../../OS_innovative_ideas.md](../../OS_innovative_ideas.md)

## 背景与问题

第一阶段已经把 `kernel_alpha` 从 bring-up demo 切换为课程 OS 主线入口，并完成进程、内存、文件系统 3 个模块的 9 个功能点：FCFS / RR / CFS-lite，Demand Paging / Clock / `kmalloc` / `kfree`，文件 / 目录 CRUD / `seek` / B 树目录索引，以及只读 `/proc` 指标面。

但 A 方案全文还要求系统调用、真实进程生命周期、文件描述符、用户程序加载、shell、管道、重定向和用户程序崩溃隔离。第二阶段的目标不是继续横向堆 stage1 的教学模型，而是打通“用户程序 -> syscall -> 进程 / FS / shell -> 可观测证据”的课程 OS 闭环。

创新清单中 AI/NPU、JIT/DBT 和 Pipeline-aware 调度辨识度很高，但它们会把主线带离 A 方案核心缺口。第二阶段选择 COW Fork、崩溃隔离和 `/proc` 可观测作为主创新线，因为它们直接依附于 `fork`、page fault、进程退出、shell 和 `/proc`，能在补齐课程要求的同时形成展示亮点。

## 目标

- 建立课程 syscall ABI，覆盖打印、文件读写、进程创建、进程退出和等待。
- 补齐真实进程生命周期：PCB、父子关系、`fork` / `exec` / `exit` / `wait` / `waitpid`、就绪 / 运行 / 阻塞 / 僵尸状态转换。
- 建立进程级文件描述符表，支持 `open` / `close` / `read` / `write` / `seek`、标准输入输出和 `/proc` 只读节点。
- 扩展课程文件系统到 A 方案指标：至少 128 个文件、单文件 64KB、至少 3 层目录、绝对 / 相对路径、重定向和 `mkfs` 或等价初始化路径。
- 提供用户程序加载和执行闭环，至少能运行 5 个不同用户程序。
- 提供课程 shell，支持命令解析、参数、内置命令、外部程序、单级管道和基础重定向。
- 实现 COW Fork、用户态崩溃隔离和 crash report，并通过 `/proc` 暴露可观测证据。
- 保留第一阶段正向和负向 guardrail，不把第二阶段实现破坏基础 bring-up、storage、interrupt、fault 合同。

## 非目标

- 第二阶段不实现 AI/NPU syscall、AI Shell 或 NPU 资源调度。
- 第二阶段不实现 JIT/DBT syscall、JIT 沙箱或自适应 JIT 调度。
- 第二阶段不实现 Pipeline-aware scheduling 或前端 Lab 大面板。
- 第二阶段不实现完整 POSIX shell、job control、多级复杂 quoting 或后台任务。
- 第二阶段不实现完整 Unix signal 语义；`kill` 可先作为课程级进程终止请求。
- 第二阶段不承诺持久化磁盘一致性、journaling、snapshot、block cache 预读或完整 EXT2。
- 第二阶段不把 `/proc` 写接口作为默认控制面；除非计划中明确列入，否则 `/proc` 仍保持只读证据面。

## 约束与边界

- `kernel_alpha` 仍是课程 OS 主线入口，`interactive_os` monitor 不替代课程 shell。
- Stage 2 应复用现有 guest runtime：`trap`、`vm_process`、`vm_fault`、`user_task`、`user_program`、`course_fs`、`procfs` 和 `kernel_alpha` demo 编排。
- syscall ABI 必须由内核和用户程序共享同一组编号和错误码，避免 demo 私有协议漂移。
- 用户态错误必须被隔离为进程级退出或 zombie，不允许把普通用户程序崩溃升级为 kernel panic。
- 文件描述符是 shell、用户程序和 `/proc` 的共同 I/O 接口，不再让每个命令绕过 syscall 直接读写内核对象。
- COW Fork 优先覆盖匿名用户页和课程用户程序页；不在第二阶段扩展到文件系统快照。
- Stage 2 正向输出需要稳定 marker，负向 demo 需要覆盖 syscall、FD、COW、crash 和 `/proc` guardrail。

## 方案

### 结构设计

第二阶段采用“五层闭环 + 一条创新线”：

| 层 | 职责 | 主要证据 |
|---|---|---|
| syscall ABI | `ecall` 分发、参数取回、错误码、用户指针校验 | syscall 单测、bad syscall 负向 demo |
| 进程与用户程序 | PCB、状态机、`fork` / `exec` / `waitpid`、用户程序装载 | `/proc/ps`、fork / wait 单测、5 个用户程序 |
| FD / FS / `/proc` | 进程 FD 表、文件和管道端点、只读 proc 节点 | 128 文件、64KB 文件、3 层目录、FD guardrail |
| shell | 命令解析、内置命令、外部程序、管道、重定向 | shell transcript、scriptable demo |
| demo / guardrail | 稳定 marker、正向课程场景、负向隔离场景 | `course-os-stage2` 正向和负向 guest tests |
| 创新线 | COW Fork、crash isolation、crash report、可观测统计 | `/proc/cow`、`/proc/crashlog`、COW fault 统计 |

### Syscall ABI

第二阶段 syscall 表至少包含：

- `read`
- `write`
- `open`
- `close`
- `seek`
- `exit`
- `fork`
- `exec`
- `wait`
- `waitpid`
- `getpid`
- `ps`
- `kill`

课程范围内的错误码至少覆盖：

- invalid syscall
- bad fd
- bad user pointer
- no such file
- no child
- no memory
- permission denied

用户指针校验属于 syscall 层的硬边界。任何来自用户态的 buffer、path、argv 指针都必须先经过用户地址空间范围检查，再进入 FS、procfs 或进程管理逻辑。

### 进程与 COW

PCB 至少记录：

- `pid`
- `ppid`
- `state`
- `exit_code`
- `crash_reason`
- `address_space`
- `open_files`
- `name`

进程状态至少包含：

- `ready`
- `running`
- `blocked`
- `zombie`
- `dead`

`fork` 默认采用 COW。父子进程共享用户页并清除写权限；首次写入触发 store page fault 后复制页面，再恢复写权限。COW 统计进入 `/proc/meminfo` 或独立 `/proc/cow`，至少暴露共享页数、COW fault 次数、实际复制页数。

`exec` 负责替换用户地址空间并初始化用户栈参数。第二阶段优先支持仓库内课程用户程序格式；如果现有 ELF loader 能低成本复用，可用 ELF 作为用户程序载体，但不为了“完整 ELF 动态链接”扩大范围。

### FD、FS 和 `/proc`

每个进程维护独立 FD 表。`0/1/2` 分别保留为 stdin、stdout、stderr，普通文件、管道端点和 `/proc` 节点都通过 FD 读写。

文件系统扩展保持教学级，不追求完整磁盘一致性。A 方案指标作为第二阶段验收边界：

- 至少 128 个文件。
- 单文件最大 64KB。
- 目录层级至少 3 层。
- 支持绝对路径和相对路径。
- 支持 `seek`。
- 支持 `mkfs` 或等价的可重复初始化入口。

`/proc` 仍是只读证据面，第二阶段扩展为：

- `/proc/ps`
- `/proc/meminfo`
- `/proc/schedstat`
- `/proc/fsstat`
- `/proc/syscalls`
- `/proc/cow`
- `/proc/crashlog`

### Shell 与用户程序

shell 是第二阶段主展示入口，至少支持：

- 内置命令：`help`、`ls`、`cat`、`echo`、`ps`、`kill`、`cd`、`pwd`、`exit`。
- 外部程序：通过命令名或 `exec` 启动。
- 参数传递：空格分隔的基础 argv。
- 单级管道：`cmd1 | cmd2`。
- 基础重定向：`>` 和 `<`。

至少提供 5 个用户程序：

- `hello`
- `echo`
- `cat`
- `forktest`
- `crash`

这些程序既用于答辩演示，也作为回归测试输入。`crash` 必须证明用户态崩溃不会影响 shell 和内核继续运行。

### Crash Isolation

用户态致命异常包括非法访存、非法指令和非法 syscall 策略命中。处理策略：

1. 捕获 `sepc`、`scause`、`stval` 和最小寄存器摘要。
2. 写入进程 crash report。
3. 将进程置为 `zombie`，等待父进程 `waitpid` 回收。
4. 保持 shell、内核和其他进程继续运行。

`/proc/crashlog` 或 `/proc/<pid>/crashlog` 需要提供最近 crash 证据。第二阶段优先做全局 `/proc/crashlog`，如果 per-pid proc 目录成本可控再扩展。

## 验证思路

第二阶段新增验证应分成窄门禁和全量门禁：

- syscall ABI：合法 syscall、非法编号、错误码、坏用户指针。
- 进程生命周期：`fork`、`exec`、`exit`、`wait`、`waitpid`、zombie 回收。
- COW：fork 后共享页、写后复制、父子数据隔离、统计可读。
- FD / FS：`open` / `read` / `write` / `close` / `seek`、关闭后访问、128 文件、64KB 文件、3 层目录。
- shell：内置命令、外部程序、参数、单级管道、输入输出重定向。
- `/proc`：所有节点只读，内容随 syscall、进程、COW、FS 状态变化。
- crash isolation：用户程序崩溃后 shell 继续运行，父进程能回收退出状态。

实现完成前，每个里程碑至少跑对应窄门禁；阶段完成前至少跑：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-unit-course_os_stage1`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `git diff --check`

如果新增 Stage 2 targets，应将它们加入阶段完成门禁，并保持 functional / pipeline guest demo 都能稳定验证 Stage 2 marker。

## 风险与取舍

- Stage 2 覆盖面大，不能一次把所有功能直接塞进 `kernel_alpha/main.c`。课程 OS 逻辑应继续下沉到 `guest/kernel/` 的专用模块，入口只做编排。
- COW 依赖 page fault 和页引用关系，若现有 `vm_object` 语义不足，应先做匿名页 COW 的最小闭环，不扩大到文件页和 FS snapshot。
- shell 管道会把 FD、pipe、fork、exec 串在一起。第二阶段只承诺单级管道，先保证可测闭环。
- `kill` 不做完整 signal 语义，避免把进程控制范围扩大到 POSIX 兼容。
- `mkfs` 可先是 host-side 工具或 guest 初始化函数，重点是文件系统初始状态可重复，不要求真实磁盘工具链完整。
- `/proc` 容易膨胀成控制面。第二阶段仍以只读证据为主，写接口留到后续独立设计。

## 当前有效性说明

- 当前有效：本文档作为课程 OS `kernel_alpha` 第二阶段的设计口径。
- 第一阶段完成态仍以 [../design/course_os_kernel_alpha_stage1_design.md](course_os_kernel_alpha_stage1_design.md) 和 [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan) 为准。
- 第二阶段实时执行状态以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。
