# kernel_alpha Linux 用户态兼容 Plus 设计

## 文档定位

本文档记录《操作系统课程设计》`kernel_alpha` 在 Stage 4 课程 OS shell 基线之后的 Linux
用户态兼容 plus 长期设计边界。

这条线的目标是在本项目 myCPU 模拟器上继续扩展自写 `kernel_alpha` 内核，让已经接入
浏览器 `/console` 的 `course-os> ` 交互 shell 能作为统一入口，逐步加载运行
`testsuits-for-oskernel` README 中涉及的 RISC-V64 Linux 用户态程序。

本文档不记录实时进度 checklist。当前状态以
[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准；具体执行步骤写入
`docs/plan/`，完成后归档到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 当前计划：
  - [../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md](../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md)
- 已完成计划：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan](../plan/history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan](../plan/history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan](../plan/history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan](../plan/history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan](../plan/history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan](../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan)
- 课程 OS 基线设计：
  - [course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)
- 背景文档：
  - [../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)

## 设计边界

Linux compat plus 不是 QEMU、LoongArch64 或真实开发板适配线，也不是把课程级 `course_*`
模块直接声明成 Linux ABI。它是在保留课程 OS 既有 marker、`course-os> ` 体验和回归门禁的
前提下，新增旁路 Linux 用户态兼容后端。

核心边界固定为：

- `course-os> ` 是统一用户入口；课程命令和 Linux 兼容程序共享前端体验，不共享 ABI。
- 课程程序继续使用课程 syscall、课程 FD / FS、课程 ELF/libc 和 Stage 1-4 合同。
- Linux 兼容程序使用独立进程 ABI 标记、Linux syscall number、Linux errno、Linux rootfs、
  Linux ELF loader 和 `linux_compat_*` 后端。
- 所有未支持能力必须 fail-closed，输出 path、ELF 类型、PC、syscall number、errno、trace
  或 rootfs source 等可诊断字段，不能用固定 help 文本伪造成功。
- Plus 不能修改 Stage 1 / Stage 2 / Stage 3 marker、Stage 4 `course-os> ` prompt、
  functional / pipeline guest 回归或旧 9 条负向 demo。

## 已完成基线

### 课程 OS Stage 1-4

Stage 1-4 已经形成课程 OS 基线：

- Stage 1：进程、内存、文件系统 3 模块 9 功能点和只读 `/proc` 指标面。
- Stage 2：syscall ABI、进程生命周期、FD / FS、shell、管道 / 重定向、COW Fork、
  用户态崩溃隔离和扩展 `/proc`。
- Stage 3：教学级 ELF / libc、5 个课程用户程序、FCFS / RR / CFS-lite、semaphore /
  mutex、Sv39 COW 证据、`mkfs` / `seek` / `unlink` / `rmdir`、shell 脚本和
  `/proc/<pid>` 证据面。
- Stage 4：独立 `guest_course_os_shell_demo`、`course-os> ` prompt、浏览器 `/console`
  manifest / terminal / Lab workbench 接入。

这些内容的稳定设计口径见
[course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)。

### Linux compat Stage 5-10

Stage 5-10 已完成 plus 的第一轮基础设施与 OSComp help-run 基线：

- Stage 5：显式 `course-os> linux <path-or-command> [args...]` launcher、旁路
  `linux_compat_*` 模块、进程 ABI 标记、最小 rootfs catalog、ELF inspection 和 fail-closed
  诊断。
- Stage 6：最小 rootfs metadata、FD 表、`openat/read/lseek/close`、`getdents64`、`brk`、
  `mmap`、`write`、`clock_gettime`、`exit_group` 和 unsupported syscall fail-closed 合同。
- Stage 7：外部 rootfs 资产链路，能从目录或 ext4 rootfs 提取 `/bin/busybox`、
  `/usr/bin/git` 并生成 C provider；默认回归仍使用 builtin provider。
- Stage 8：Linux compat loader / trace 收口，支持 RV64 `ET_EXEC` / `ET_DYN`、
  `PT_LOAD`、`PT_INTERP` 的只读 load-plan 诊断、optional interpreter asset manifest、
  stack / auxv 摘要和固定上限 syscall trace record。
- Stage 9：静态 RV64 ELF real-exec 第一刀，显式 launcher 对 `/bin/minimal-elf`、
  `/bin/busybox --help`、`/bin/busybox echo hello` 和 `/usr/bin/git -h` 走真实 PT_LOAD
  映射、argv / envp / auxv 用户栈、U-mode 入口、真实 ecall dispatch、UART `write`
  和 `exit_group` 闭环。
- Stage 10：OSComp help-run 基线，`git -h` / `git help` 直接 fallback 进入 `/usr/bin/git`
  real-exec；`vim -h`、`gcc --h`、`rustc -h` 在 builtin provider 缺资产时进入 Linux compat
  fail-closed 诊断并回到 prompt；dynamic-loader v0 和最小 syscall 面按 help-run trace 收口。

Stage 9 之后，硬编码 help 字符串和模拟 syscall 序列不再是正向路径。缺少 real-exec context
的 host-only 调用必须 fail-closed。

## 目标

- 保留 `course-os> ` 作为主要人机入口，而不是新增最终用户可见的 `linux-compat> ` 主入口。
- 支持显式 `linux /path [args...]` launcher，并在稳定后支持直接命令 fallback，例如
  `git -h`、`vim -h`、`gcc --h`、`rustc -h`。
- 通过 PATH fallback 只在课程内置命令和课程用户程序未命中后查询 Linux rootfs，避免课程命令被
  Linux rootfs 中同名程序覆盖。
- 从 builtin 或 external / OSComp rootfs provider 加载真实 RISC-V64 Linux ELF 和必要 metadata。
- 逐步支持静态 ELF、`PT_INTERP` dynamic-loader v0、argv/envp/auxv、最小 TLS / random /
  brk / mmap 策略和真实 syscall trace。
- Stage 10 已以 `testsuits-for-oskernel` 的低风险帮助输出完成 help-run 基线；Stage 11
  转入 writable rootfs、本地工具链和交互终端，Stage 12 / Stage 13 再分别处理网络 git
  和 `rustc` 重工具链。
- 对未支持 syscall、非法 ELF、坏路径、缺失 interpreter、坏用户指针和用户态崩溃保持
  fail-closed。

## 非目标

- 不声明完整 Linux 发行版兼容。
- 不把 `course_syscall`、`course_shell`、`course_user_programs` 直接改造成 Linux ABI 实现。
- 不把固定文本输出当成 Linux 工具运行证据。
- 不在 help-run 阶段承诺完整动态链接器、完整 relocation、完整 TLS、完整 signal / futex、
  完整 termios / TTY 或完整 rootfs 写语义。
- 不在 Stage 10 承诺 `git init/add/commit/log`、`git clone/push/pull`、`vim hello.c`、
  `gcc hello.c && ./a.out` 或 `rustc helloworld.rs && ./helloworld`。
- 不把网络、认证、完整工具链矩阵或多进程构建全集绑定到 help-run 完成定义。
- 不在 Stage 11 承诺网络 `git clone/push/pull` 或 `rustc` 编译闭环。

## 架构合同

### Shell 路由顺序

`course-os> ` 的命令解析顺序固定为：

1. 课程 shell 内置命令和 proc 快捷命令。
2. Stage 3 课程用户程序 catalog。
3. 显式 `linux <path-or-command> [args...]` launcher。
4. Linux rootfs PATH fallback。

该顺序是长期合同。若发生歧义，用户可用 `linux /path ...` 显式指定 Linux 兼容路径；实现不能让
Linux rootfs 覆盖课程 OS 既有展示命令。

### 进程 ABI 分流

进程级 ABI 至少区分：

```text
COURSE_ABI
LINUX_COMPAT_ABI
```

课程 shell、课程内置程序和 Stage 3 catalog 程序继续使用 `COURSE_ABI`。通过显式 launcher 或
Linux rootfs fallback 启动的进程使用 `LINUX_COMPAT_ABI`。用户态 `ecall` 回到 trap 层后按
当前进程 ABI 分发：

```text
COURSE_ABI        -> course_syscall_dispatch()
LINUX_COMPAT_ABI -> linux_compat_syscall_dispatch()
```

ABI 标记必须跟随进程生命周期、FD 表、地址空间、trap context、crash 诊断和退出路径流转。
Linux 兼容进程退出后控制权回到同一个 `course-os> ` prompt。

### Rootfs provider

Linux compat rootfs 分为两类 provider：

- builtin provider：默认回归使用，内置最小静态资产，不能依赖宿主机镜像。
- external / OSComp provider：显式 opt-in，从目录或 ext4 rootfs 提取真实工具、interpreter
  和必要 shared assets，生成 C provider 和 JSON manifest。

provider 必须能回答：

- path lookup。
- 普通文件 / 目录 metadata。
- 文件大小、权限、时间戳、mode、inode-like id。
- `stat` family、`getdents64` 和 seek 所需的只读语义。
- required asset 缺失和 optional asset 缺失的差异化诊断。

Stage 11 开始把可写 rootfs 作为独立 overlay 设计：lower layer 继续来自 builtin 或 external
provider，upper layer 只在当前 guest session 内维护文件 / 目录创建、truncate、write /
pwrite、readback、metadata 更新、rename / unlink、fsync / sync no-op 成功语义和失败诊断。
这一层用于支撑 `git init/add/commit/log`、`vim hello.c`、`gcc hello.c && ./a.out`，不声明
宿主持久化回写或完整发行版磁盘一致性。

### ELF / dynamic-loader v0

Stage 9 已证明静态 RV64 ELF 的 real-exec 路径。后续 dynamic-loader v0 的合同是：

- 读取 main ELF 的 `PT_INTERP`，从 rootfs provider 查找 interpreter。
- 映射 main ELF 和 interpreter 的 `PT_LOAD` 段，记录 load bias、entry、phdr、segment 区间。
- 构建 Linux 用户栈：argc / argv / envp / auxv。
- auxv 至少对 help-run 所需字段给出真实或可诊断的最小策略，例如 `AT_PHDR`、`AT_PHENT`、
  `AT_PHNUM`、`AT_ENTRY`、`AT_BASE`、`AT_PAGESZ`、`AT_RANDOM`。
- relocation、TLS、unsupported ABI、缺失 interpreter 或权限不满足时 fail-closed，不伪造执行成功。

dynamic-loader v0 的目标是支撑 help-run 的加载运行，不是完整动态链接器实现声明。

### Syscall 扩展方法

Linux syscall 扩展必须 trace-driven：

1. 先运行目标命令，记录第一个阻塞目标 workflow 的 syscall number、PC、参数、path / fd、errno
   和已执行 trace。
2. 只实现该 trace 证明需要的最小语义。
3. 每个 syscall 都必须固定 Linux errno、用户指针校验、trace record 和 unsupported fallback。
4. 不允许为了“看起来能跑”吞掉未知 syscall 或返回伪成功。

Stage 10 help-run 阶段优先补只读、低副作用或可明确 fail-closed 的 syscall。Stage 11
workflow 阶段新增的 `execve` / `wait4` / `clone` 或 `vfork`、pipe / dup、writable file
syscall、TTY / termios、`mprotect`、`readlinkat`、`faccessat` / `access`、`uname`、
`prlimit64`、`set_tid_address`、`set_robust_list`、`rt_sigaction`、`rt_sigprocmask`、
`writev`、`pread64`、`statx` 等接口，也必须由真实工具 trace 决定。

### 失败诊断

所有失败路径都应尽量输出稳定字段：

- command / argv。
- rootfs source。
- path / resolved path。
- ELF class / machine / type / loader kind / interpreter path。
- PC / entry / segment count。
- syscall number / name / errno / trace。
- fail reason。

这些字段用于 host smoke、文档报告和后续 trace-driven 补洞，不作为用户体验装饰。

## Stage 11-13：OSComp workflow 路线

Stage 10 已完成 `OSComp Linux 用户态 help-run 基线`。Stage 11 之后的路线按能力面拆分，
避免把可写文件系统、本地工具链、网络 git 和重型 Rust 工具链揉成一个不可验证阶段。

Stage 11 的定位是 `writable rootfs + process / file workflow`，目标是本地有状态工作流：

- `git init/add/commit/log`
- `vim hello.c`
- `gcc hello.c && ./a.out`

Stage 11 的验收重点：

- external / OSComp rootfs 下的真实 `git`、`vim`、`gcc` 资产和必要 shared assets 能被 provider
  诊断到；这只是前置条件，不是完成定义。
- writable rootfs overlay 支持文件 / 目录创建、写入、rename、unlink、metadata 和 readback。
- Linux compat runtime 能支撑 cwd、relative path、FD、`execve` / `wait4` / 子进程、pipe / dup
  和 trace-driven syscall 补洞。
- `course-os> ` shell 保持课程命令优先，同时为 Linux fallback 传递 cwd，并提供最小 `&&`
  成功链执行。
- 工具输出来自真实执行和真实 rootfs，不使用固定文本伪造。
- Stage 5-9 显式 launcher、bad path、bad ELF、unsupported syscall 和 fallback 关闭历史 guardrail
  不被破坏。

Stage 12 的定位是 `network / git remote path`，再推进 virtio-net、socket、DNS、SSH / TLS
或最小 git remote 能力，目标放到 `git clone/push/pull`。

Stage 13 的定位是 `rustc large-memory / toolchain closure`，再处理 `rustc helloworld.rs &&
./helloworld`、大内存占用、重型动态链接和稳定性问题。Stage 11 可以把 `/usr/bin/rustc`
作为 optional provider 预检资产记录，但不把 Rust 编译成功作为 Stage 11 完成条件。

## 验证要求

Plus 每一刀都必须同时守住课程 OS 基线：

- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-course_os_shell_demo`
- `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
- `cd myCPU && make test-pipeline-guest-course_os_shell_demo`
- `git diff --check`

Stage 5-10 现有 Linux compat 门禁继续有效：

- `cd myCPU && make test-unit-course_os_stage5_linux_compat`
- `cd myCPU && make test-unit-course_os_stage6_linux_compat`
- `cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
- `cd myCPU && make test-unit-course_os_stage9_linux_compat_vm`
- `cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
- `cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
- `cd myCPU && make test-unit-course_os_stage10_linux_compat`
- `cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
- `cd myCPU && make test-host-course_os_linux_compat_minimal_elf_smoke`
- `cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
- `cd myCPU && make test-guest-course_os_linux_compat_shell_demo`
- `cd myCPU && make test-pipeline-guest-course_os_linux_compat_shell_demo`

Stage 11 workflow 门禁必须保持 external-only：

- `cd myCPU && make test-unit-course_os_stage11_linux_compat`
- `cd myCPU && make test-host-course_os_linux_compat_external_rootfs_smoke`
- `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=<external-rootfs> make test-host-course_os_linux_compat_external_workflow_smoke`

阶段完成前默认还应运行：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `git diff --check`

## 风险与取舍

- 直接命令 fallback 会增加 shell 歧义；因此解析顺序必须固定，课程命令优先。
- dynamic-loader v0 很容易滑向完整动态链接器；Stage 10 只绑定 help-run 需要的最小执行证据。
- trace-driven syscall 容易被“伪成功返回”污染；未知 syscall 必须 fail-closed。
- OSComp rootfs provider 如果依赖宿主机资产，默认回归会变脆；默认 `make test` 仍必须走 builtin 或显式跳过外部资产。
- `vim`、`gcc`、`git commit` 会快速引入 TTY、signal、futex、execve、wait4、临时文件、动态链接
  和可写 rootfs 细节；Stage 11 必须按真实 trace 小步补洞，不能用伪成功绕过。
- `rustc` 会引入更重的内存与 toolchain 压力；默认留给 Stage 13，不作为 Stage 11 pass 条件。
- Plus 修改课程 OS marker 会破坏课程线证据链；已有 Stage 1-4 demo 和旧 9 条负向 demo 必须继续作为 guardrail。

## 当前有效性说明

- 当前有效：本文档是 Stage 4 后 Linux 用户态兼容 plus 的长期设计边界。
- 当前完成态：Stage 5 / Stage 6 / Stage 7 / Stage 8 / Stage 9 / Stage 10 已完成显式 launcher、
  最小 rootfs / syscall、外部 rootfs asset provider、loader / trace 诊断，以及静态 RV64
  ELF real-exec 第一刀和 OSComp help-run 基线。
- 当前活跃计划：Stage 11 writable rootfs / process-file workflow，见
  [../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md](../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md)。
- 当前不是完整兼容声明：`kernel_alpha` 仍不声明完整 Linux syscall 面、完整动态链接器、
  完整 signal / futex、rootfs 写语义、完整 TTY、网络 git 或自动跑完 OSComp testsuits。
