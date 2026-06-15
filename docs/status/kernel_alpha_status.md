# kernel_alpha 状态

## 文档定位

本文档只记录 `kernel_alpha` 的当前定位、课程 OS 当前完成态、当前仍有效的限制和下一步。

它不再维护逐条执行流水账；更细的实现过程统一回写到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [../design/course_os_kernel_alpha_linux_compat_plus_design.md](../design/course_os_kernel_alpha_linux_compat_plus_design.md)
  - [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
  - [../design/course_os_gap_closure_boundary_design.md](../design/course_os_gap_closure_boundary_design.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
- 当前计划：
  - [../plan/course_os_arch_followup_plan.md](../plan/course_os_arch_followup_plan.md)
  - [../plan/course_os_plus_external_validation_plan.md](../plan/course_os_plus_external_validation_plan.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md#course-os-display-gap-closure-plan](../plan/history_plan.md#course-os-display-gap-closure-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage11-post-v0-convergence-plan](../plan/history_plan.md#course-os-kernel-alpha-stage11-post-v0-convergence-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-review-remediation-and-linux-compat-convergence-plan](../plan/history_plan.md#course-os-kernel-alpha-review-remediation-and-linux-compat-convergence-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-quality-review-plan](../plan/history_plan.md#course-os-kernel-alpha-quality-review-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage11-writable-rootfs-process-file-plan](../plan/history_plan.md#course-os-kernel-alpha-stage11-writable-rootfs-process-file-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan](../plan/history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan](../plan/history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan](../plan/history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan](../plan/history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan](../plan/history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan](../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan](../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage3-plan](../plan/history_plan.md#course-os-kernel-alpha-stage3-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage2-plan](../plan/history_plan.md#course-os-kernel-alpha-stage2-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
  - [../plan/history_plan.md#kernel-alpha-storage-error-contract-plan](../plan/history_plan.md#kernel-alpha-storage-error-contract-plan)

## 当前状态

`kernel_alpha` 当前已经从 Phase 1 bring-up demo 切换为《操作系统课程设计》A 方案主线入口；旧
`KMVPETDS` 只保留为历史 guardrail，不再描述当前正向能力。
第一阶段按 [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
落地进程、内存、文件系统 3 个模块的 9 个功能点，并提供只读 `/proc` 指标证据面。

第二阶段按 [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
完成 A 方案核心闭环和创新线：syscall ABI、真实进程生命周期、FD / FS 统一 I/O、5 个课程用户程序、
shell、单级管道、重定向、COW Fork、用户态崩溃隔离，以及 `/proc/syscalls`、`/proc/cow`、
`/proc/crashlog` 可观测证据。

Stage 2 新增 `course_os_stage2` 总编排层，不把 demo 流程堆在 `kernel_alpha/main.c`；Stage 2 完成时
`kernel_alpha_demo` 会在 `K/M/V/P/E/T` 基础 bring-up 后依次输出 Stage 1 与 Stage 2 summary。

Stage 2 基线输出曾固定为：

- `KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog`

其中 `K/M/V/P/E/T` 继续证明基础 bring-up、PLIC、external interrupt 和 timer interrupt 仍可用；
`course-os-stage1 ...` 固定第一阶段调度、Demand Paging / Clock、文件系统索引和 `/proc` 证据摘要；
`course-os-stage2 ...` 固定第二阶段 syscall、进程、FD / FS、shell、管道、COW、crash isolation
和扩展 `/proc` 证据摘要。

第三阶段按 [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
完成“课程满分基线真实化”：教学级 ELF / libc、真实用户程序、FCFS / RR 指标化、
semaphore / mutex、Sv39 fault-driven COW 证据链、`mkfs` / `seek` / `unlink` / `rmdir`、
shell 脚本模式，以及 `/proc/cpuinfo`、`/proc/uptime`、`/proc/<pid>/status`、`/proc/<pid>/fd`、
`/proc/<pid>/maps`。

当前 `guest_kernel_alpha_demo` 正向输出为 Stage 1 + Stage 2 + Stage 3 串联：

- `KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog|course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid`

Stage 3 稳定 marker 为：

- `course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid`

Stage 4 已完成前端交互 shell 接入。该阶段没有修改 `kernel_alpha_demo` 的一次性
Stage 1 / Stage 2 / Stage 3 summary，而是新增独立 `guest_course_os_shell_demo`，
启动后进入 `course-os> ` prompt，并通过 `/console` 的 manifest / terminal / Lab workbench
展示 Stage 3 已完成的课程 shell、FD / FS、procfs、ELF / libc、COW 和 crash isolation 能力。
前端继续复用现有 debug session 和 UART terminal 合同，没有新增并行执行协议。

Stage 4 之后的 plus 方向已补充为独立设计边界：在本 myCPU 模拟器上继续扩展自写
`kernel_alpha` 内核，尝试运行 testsuits-for-oskernel README 中涉及的 Linux 用户态程序。
这条 plus 线必须旁路新增 Linux ABI / ELF / FS / syscall 模块，不把课程级 `course_*`
实现直接膨胀成通用 Linux 兼容层，也不改变 Stage 1 / Stage 2 / Stage 3 marker 和
Stage 4 shell prompt。

Stage 5 v0 已完成 Linux compat Plus 第一刀。`course-os> ` 现在新增显式
`linux <path-or-command> [args...]` launcher；`linux /bin/busybox --help` 和
`linux /usr/bin/git -h` 会进入旁路 `linux_compat_*` 后端，经过最小 rootfs catalog、
RV64 little-endian ELF header / program-header inspection、进程 ABI 标记和
fail-closed syscall 诊断后回到同一个 `course-os> ` prompt。直接 `git -h` 自动 fallback
在 Stage 5 当时尚未启用，课程命令和 Stage 3 catalog 仍优先保持原语义。

Stage 6 已完成 Linux compat Plus 的 rootfs / syscall 第一层语义。`linux_compat_*`
现在提供最小 rootfs metadata、`stat`、FD 表、`openat/read/lseek/close`、`getdents64`、
`brk`、`mmap`、`write`、`clock_gettime`、`exit_group` 和 unsupported syscall fail-closed
合同；`linux /bin/busybox --help` 与 `linux /usr/bin/git -h` 不再停在 unsupported syscall，
而是通过这组最小 syscall 路径输出稳定 help 文本后回到同一个 `course-os> ` prompt。
该阶段仍使用内置最小 rootfs catalog 和模拟 help 路径，不声明外部真实 rootfs、动态链接器
或通用 Linux 用户态兼容已经完成。

Stage 7 已完成 Linux compat 外部 rootfs 资产链路。显式 opt-in target 现在可以从外部目录
或 ext4 rootfs 提取 `/bin/busybox`、`/usr/bin/git`，生成可编译 C provider，并让
`course-os> linux ...` 使用真实 RV64 ELF bytes 和 metadata 进入现有 Linux compat lookup /
stat / read / ELF inspect / help 路径。默认 `make test` 仍使用 builtin provider，不依赖外部
rootfs、`debugfs` 或本机特定镜像。

Stage 8 已完成 Linux compat loader / trace 收口。`linux_compat_loader` v0 现在能为 RV64
`ET_EXEC` / `ET_DYN`、`PT_LOAD` 和 `PT_INTERP` 生成只读 load plan，并把显式
`course-os> linux ...` run path 的输出扩展为 `loader=static|dynamic`、`interp=<path|none>`、
`segments=<n>`、`stack=argv/envp/auxv` 和 `trace=...` 诊断。外部 rootfs generator 新增
optional interpreter asset 记录，manifest 能区分 required / optional path 的 present / missing；
`linux_compat_runtime` 也新增固定上限 syscall trace record buffer，为后续真实 trace-driven
syscall 扩展提供稳定观察面。该阶段仍不声明真实动态链接器运行、真实 ELF 执行、完整 Linux
syscall 面、rootfs 写语义或自动 `git -h` fallback 已完成。

Stage 9 已完成 Linux compat 真实 ELF 执行第一刀。显式
`course-os> linux ...` launcher 现在结束 Stage 5-8 的硬编码 help 字符串和模拟 syscall
序列；静态 RV64 ELF 会经过真实 PT_LOAD 段映射、argv / envp / auxv 用户栈、U-mode
入口、真实 ecall dispatch、UART `write` 和 `exit_group` 闭环后回到同一个
`course-os> ` prompt。端到端验收覆盖 hand-crafted 最小 ELF、`linux /bin/busybox --help`、
`linux /bin/busybox echo hello` 和 `linux /usr/bin/git -h`，输出中包含 `exec=real` 与真实
syscall trace。Stage 9 当时仍不启用直接命令 fallback，不声明动态链接器运行、完整 Linux
syscall 面、完整 signal / futex、rootfs 写语义或通用 Linux 用户态兼容。

Stage 10 已完成 OSComp Linux 用户态 help-run 基线。`course-os> git -h` 和
`course-os> git help` 现在会在课程内置命令与 Stage 3 课程用户程序均未命中后，通过受限
Linux PATH fallback 解析到 `/usr/bin/git`，进入真实 Linux compat exec 路径，并在输出
`usage: git`、`exec=real` 和真实 syscall trace 后回到同一个 `course-os> ` prompt。
`course-os> vim -h`、`course-os> gcc --h`、`course-os> rustc -h` 在 builtin provider
缺少对应资产时不再落回课程 shell 裸 `error`，而是进入 Linux compat fail-closed 诊断，
输出 resolved path、`errno=2`、`path: no such file` 和 prompt 回归。Stage 10 同时补齐
OSComp help-run 所需的 provider manifest、direct fallback、dynamic-loader v0 元数据和
最小只读 / 低副作用 syscall 面；默认回归仍不依赖外部 rootfs。

Stage 11 当前已打通 writable rootfs / process-file workflow v0。external rootfs opt-in
target 能完成 `git init stage11repo`、`vim stage11repo/hello.c` 保存源码、
`git add/commit/log`，并执行 `cd stage11repo && gcc hello.c && ./a.out`；最终 `./a.out`
从 session-local overlay 解析为 `/stage11repo/a.out`，经 Linux compat loader real-exec
输出 `stage11 hello` 后回到同一个 `course-os> ` prompt。该闭环仍是 Stage 11 v0：
`gcc` driver 的 `clone/execve/wait4` 仍采用最小 helper-child 语义，`a.out` 由 Linux
compat 在 `gcc` 成功退出后生成教学级 RV64 ELF artifact，不声明真实 `cc1/as/ld` 子进程链、
完整 toolchain 执行、完整 signal / futex、完整 TTY、网络 git 或 `rustc` 大工具链已完成。

2026-06-10 已完成一次 `review-only` 质量审查，范围覆盖课程 OS 主体、Stage 1-4
编排、`course_os_shell` / `kernel_alpha` 入口，以及课程 OS 与 Linux compat 的边界。
本轮未修改生产代码，结论是边界总体仍符合课程 OS 主体、Linux compat 旁路和共享 guest
runtime 的分层；没有建议推进 Stage 12 / Stage 13、`rustc` 或更宽 Linux syscall breadth。
审查当时记录了 1 个后续必须修复项和 3 个建议收敛项：

- `[必须修复]` `course_fd_read()` 当前在读取 `size` 字节后额外写 `out[size] = '\0'`，
  但 API 形态是 read-style `buffer + size`，调用方如果只提供精确 `size` 字节会有越界写风险。
  后续应先补 canary 红灯，再明确 raw read / NUL-terminated read 的合同。
- `[建议修改]` `course_shell_t` 现在同时承载课程 FS / scheduler / process / FD / procfs、
  Linux compat runtime、VM process、trap runtime 和大块 scratch buffer；`course_shell.c`
  也同时实现 parser、builtin、课程用户程序、Linux launcher、cwd 同步和 `&&` 链控制。
  后续收敛已把 Linux launcher 物理拆到 `course_shell_linux.c` / `course_shell_linux.h`；
  `&&` 的 structured command status 也已落地，不再用输出字符串判断 Linux 命令是否允许继续。
- `[建议修改]` `course_fs_t` 是可实例化对象，但文件内容 backing 仍在 `course_fs.c`
  的全局数组中。当前 smoke 顺序下可接受，后续若复用多个 FS 实例或做更真实 VFS /
  metadata，应把 storage backend 显式化或收口为清晰 singleton contract。
- `[建议修改]` `/proc/<pid>/fd` 目前只挂一个 fd table 与 owner pid，能证明 shell
  当前 FD 表，但不能完整表达每个进程自己的 FD 表。后续若继续强化 `/proc/<pid>`
  证据面，应让 procfs 通过 process / fd resolver 查询目标 pid 的 FD 状态。

Undefined-OS 参考的采用边界也已明确：可参考 process lifecycle 对象、VFS inode /
metadata、地址空间 backend 和 syscall stub 分层治理；谨慎参考完整 futex / signal /
mmap / ext4；不采用换底座、多架构优先或继续追完整 Linux userland 的方向。

2026-06-11 已完成 Stage 11 v0 后续收敛。external workflow smoke 现在使用显式
command list 和 per-command summary，并把 Linux compat run summary 接入 host 断言面；
Linux compat process state 已抽成 `linux_compat_process_table`；overlay / VFS metadata
补齐 `nlink`、`mtime`、opened-fd rename / unlink 生命周期和 close-on-exec 释放；
pseudo filesystem 合同固定 `/dev/null`、`/dev/random`、`/proc/self/exe` 和 overlay-created
`/tmp` 的支持 / fail-closed 边界；`mprotect(PROT_NONE)` 已落到 VM region 权限元数据，
file-backed `MAP_PRIVATE` 读路径和 `MAP_SHARED` fail-closed 边界也已有回归；syscall trace
新增 `policy=bypass|errno|unsupported` 分类合同。

2026-06-12 已用本机 Alpine riscv64 ext4 rootfs 重新复验 Stage 11 external workflow opt-in。
首轮真实复验在 `git init stage11repo` 暴露动态加载器 RELRO `mprotect(PROT_READ)` 子范围
请求返回 `EINVAL`，导致 `/usr/bin/git` 以 127 退出；本轮把 Linux compat VM 的
page-aligned read-only subrange `mprotect` 固定为 low-effect 成功合同，并用回归锁住。
随后 `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=/home/liangjiaqi/local/oscomp-rootfs/alpine-linux-riscv64-ext4fs.img`
的 `test-host-course_os_linux_compat_external_workflow_smoke` 已重新跑通 `git init`、
`vim hello.c`、`git add/commit/log` 和 `gcc hello.c && ./a.out`。

同日已完成 Stage 11 frontend host-only manifest 小切片：`/api/tests` 中的
`guest_course_os_shell_demo` 现在携带只读 `hostOnlyWorkflow`，列出 external workflow
的 `git init`、`vim`、`git add/commit/log` 和 `gcc hello.c && ./a.out` 命令及关键
markers；Scenario inspector 会展示这组 host-only 命令清单，但不新增浏览器 external rootfs
运行入口，也不改变现有 session load / terminal API。

2026-06-14 已完成 shell / terminal 统一收敛小切片。`guest_interactive_os_demo`、
`guest_course_os_shell_demo` 和配置后出现的 `linux_proto_console` 现在通过统一 manifest
metadata 表达 prompt、boot marker、command budget、terminal title / target 和外部资产信息；
前端 terminal 标题与 pending-input hint 优先读取 manifest presentation metadata，旧 entry
继续保守 fallback。三条 host terminal smoke 复用 `tests/host/terminal_smoke_harness.h`
的 load / prompt wait / UART input / command-output failure helper；默认 builtin
Course OS shell 的 `git init` 继续是 help-run / usage 行为，不会初始化 writable repo。
Stage 11 external workflow 仍只通过 `hostOnlyWorkflow` 暴露给 Scenario inspector 和 host
smoke，未新增浏览器 loadable external-rootfs route。guest 侧 `monitor_commands.c` 与
`course_shell.c` 的 parser / dispatch 边界已审查：两者只有 token equality 级别表面相似，
Course OS shell 还承载 argv、pipe、redirect、`&&`、课程程序和 Linux fallback，因此本轮明确
`no guest extraction`，不新增 `guest_command_dispatch`。

该轮调研结论是：futex 继续保持当前 low-effect `FUTEX_WAIT` / `FUTEX_WAKE`，不补 wait
queue / requeue / bitset；signal 继续保持 `rt_sigaction` / `rt_sigprocmask` bypass，
不先引入 ProcessGroup；Stage 11 command list 继续 host-only，不接入前端 external opt-in
route。下一步若继续推进，应以新的真实 trace blocker 为入口，优先在完整 Linux 子进程链、
signal / futex 或前端 host-only manifest 中另建窄计划，而不是直接扩大 Stage 12 / Stage 13。

2026-06-15 已将课程 OS 对照 A 方案的缺口收口拆成三条计划：展示前课程证据补洞、架构后续增强、
Plus / 外部验证，并新增上游边界设计。展示前计划当前只把目录枚举 `course_fs_listdir` 作为已完成
小切片；在线抢占调度、真实课程 ELF、UART 中断驱动、context switch cost 和 OSComp 验证不再作为
展示前 P0 混入同一计划。

2026-06-15 已把展示前计划收口完成并归档：`course_fs_listdir` 变成完整输出合同，`ls` 不再是
stub，`kill` 现在能区分缺失 pid、权限拒绝和真实进程终止；`course-os> ` shell 新增
`sem`、`mutex`、`concurrency_demo` 和 `mkfs` 展示命令，`/proc/cpuinfo` 也固定输出
`timer_hz=100` 作为课程时钟频率证据。后续在线抢占调度、真实 trap / timer 证据、
UART 中断驱动、OSComp / 外部资产验证继续留在架构后续计划和 Plus / 外部验证计划中，不混入
展示前 P0。

2026-06-10 已完成质量审查后的 `fix-and-validate` 小步收敛。`course_fd_read()` 现在明确为
raw read 合同，只写实际返回的字节数，不再隐式追加 `NUL`；`test-unit-course_os_stage2_fd_fs`
新增 exact-size read canary 回归覆盖该边界。`linux_compat.c` 也完成第一刀行为保持拆分：
UART debug / syscall diagnostic helper 已迁移到独立 `linux_compat_debug.c` /
`linux_compat_debug.h`，主文件继续保留 syscall trace record、dispatcher 和 run facade。
`course_shell_run_line()` 也已把 `&&` 链控制改为内部 structured command status：
普通命令输出中包含 `linux-compat:` 不再误阻断右侧命令，Linux compat run 仍按
`linux_compat_run()` 结果和 runtime exit code 决定是否允许继续。

2026-06-11 已继续完成 shell / Linux launcher 物理拆分小切片：显式 `linux ...` launcher
和 Linux PATH fallback 现在迁移到 `course_shell_linux.c` / `course_shell_linux.h`，
`course_shell.c` 保留 parser、builtin、课程用户程序、cwd 同步和链式分发。Makefile 新增
`COURSE_SHELL_UNIT_OBJS`，统一连接 `course_shell_guest.o` 与 `course_shell_linux_guest.o`，
避免各 Stage 2 / Stage 3 / Stage 11 单测重复维护同一 shell object 列表。

2026-06-11 质量审查剩余两个安全收敛项也已完成。`course_fs_t` 现在有显式
`course_fs_storage_t` backing contract：默认仍使用单例 backing 维持既有调用行为，需要
多实例隔离时可用 `course_fs_mkfs_with_storage()` 传入独立 storage，新增单测覆盖稀疏写不会
串读其他 FS 实例留下的字节。`/proc/<pid>/fd` 新增 fd table resolver 合同，旧的
`procfs_attach_fd_table()` 继续作为单表 fallback；Stage 3 procfs 单测现在固定 pid 1 和 pid 2
分别解析到不同 fd table，防止 per-pid FD 证据面串表。至此 2026-06-10 质量审查记录的
1 个必须修复项和 3 个建议收敛项均已收口。

旧 Phase 1 `KMVPETDS` 正向输出不再作为课程 OS 当前行为承诺；它降级为历史 bring-up 基线：

- `K`：进入独立 kernel 入口
- `M`：memory / PMM 初始化完成
- `V`：自建 Sv39 内核页表启用并稳定工作
- `P`：PLIC 最小 supervisor 初始化完成
- `E`：第一次 supervisor external interrupt 到达
- `T`：第一次 timer interrupt 到达
- `D`：storage readiness / metadata probe 完成
- `S`：`LBA 0` 读取和签名校验完成

与旧正向基线一起形成的 9 条负向 demo 继续保留为基础设施 guardrail 和历史回归：

- `guest_kernel_alpha_fault_demo = KMVX`
- `guest_kernel_alpha_plic_not_ready_demo = KMVPX`
- `guest_kernel_alpha_timer_not_ready_demo = KMVPETX`
- `guest_kernel_alpha_storage_no_media_demo = KMVNX`
- `guest_kernel_alpha_storage_not_ready_demo = KMVRX`
- `guest_kernel_alpha_storage_bad_magic_demo = KMVGX`
- `guest_kernel_alpha_storage_bad_block_count_demo = KMVBX`
- `guest_kernel_alpha_storage_lba_range_demo = KMVLX`
- `guest_kernel_alpha_storage_bad_command_demo = KMVCX`

旧 `D/S` storage readiness / signature 正向 marker 不再绑在当前 `kernel_alpha_demo` 正向 smoke 上；storage / interrupt / fault 合同由 9 条负向 demo 和 `kernel_alpha_*` 单元门禁继续守住。

## 关键历史节点

- `2026-06-12`
  - 完成 external rootfs opt-in 复验：真实 rootfs 首轮暴露 `/usr/bin/git` 动态加载器
    RELRO `mprotect(PROT_READ)` 子范围失败，本轮补 Linux compat VM low-effect 子范围
    mprotect 回归后，external workflow smoke 重新跑通 `git init`、`vim`、
    `git add/commit/log` 和 `gcc hello.c && ./a.out`。
  - 完成 frontend host-only manifest：`guest_course_os_shell_demo` 的前端 manifest
    增加只读 Stage 11 workflow 命令清单和 markers，Scenario inspector 展示该 host-only
    合同，但仍不暴露 external rootfs 浏览器运行 route。
- `2026-06-11`
  - 完成 Stage 11 post-v0 收敛：external workflow command-list / per-command summary、
    Linux compat process table、overlay metadata / opened-fd 生命周期、pseudo path 合同、
    VM `mprotect` / file-backed mmap 边界，以及 syscall stub policy trace 分类均已落地；
    external rootfs opt-in 未设置时仍不声明 external workflow 重新通过。
  - `PROJECT_EVOLUTION` P0 维护门禁重跑 `kernel_alpha` Stage 2 正向证据面、课程 OS 单元门禁、
    functional / pipeline `kernel_alpha_demo` 和旧 Phase 1 负向 demo；当前结论是 Stage 2 marker、
    Stage 3 串联 marker、storage / PLIC / timer / fault 历史 guardrail 继续稳定，不把新能力混入旧
    `KMVPETDS` 语义。本轮同时把 `kernel_alpha` guest / pipeline guest smoke 的 timeout
    预算从普通快速 smoke 中拆出，避免慢速主机把稳定 marker 误报成超时。
  - 完成质量审查剩余安全收敛：`course_fs_t` 新增显式 `course_fs_storage_t` backing
    contract 与独立 storage 验证；`/proc/<pid>/fd` 新增 fd table resolver，并用 pid 1 /
    pid 2 不同 fd 表回归固定 per-pid 证据面。
  - 完成 `course_shell` Linux launcher 物理拆分：新增 `course_shell_linux.c` /
    `course_shell_linux.h` 承载显式 launcher、Linux PATH fallback、VM / trap setup 和
    Linux compat command status；`course_shell.c` 不再直接承载这组 Linux launcher 细节。
- `2026-06-10`
  - 完成 `course_shell` structured command status 小切片：`&&` 链不再扫描输出字符串判断
    Linux 命令成败，新增回归覆盖普通输出文本包含 `linux-compat:` 时仍继续右侧命令；既有
    `linux /nope && ...` 失败短路合同继续由 Stage 11 Linux compat 单测守住。
  - 完成 `kernel_alpha` quality review remediation 第一轮修复：`course_fd_read()` exact-size
    buffer 越界风险已由 canary 单测覆盖并修复；`linux_compat.c` 已拆出 debug /
    diagnostic helper 第一刀。详细归档见
    [../plan/history_plan.md#course-os-kernel-alpha-review-remediation-and-linux-compat-convergence-plan](../plan/history_plan.md#course-os-kernel-alpha-review-remediation-and-linux-compat-convergence-plan)。
  - 完成 `kernel_alpha` 课程 OS 层只读质量审查。审查未改生产代码；结论保留 1 个
    `course_fd_read()` exact-size buffer 安全修复项，以及 shell / Linux launcher
    边界、course FS backing、procfs per-pid FD 证据面的 3 个后续收敛建议。详细归档见
    [../plan/history_plan.md#course-os-kernel-alpha-quality-review-plan](../plan/history_plan.md#course-os-kernel-alpha-quality-review-plan)。
- `2026-06-09`
  - Stage 11 external workflow smoke 首次完整通过本地有状态链路：
    `git init`、`vim hello.c`、`git add`、`git commit`、`git log`、
    `gcc hello.c && ./a.out` 均回到 `course-os> ` prompt；最终输出包含
    `stage11 hello`、`linux-compat: path=/usr/bin/gcc`、`exec=real`。
  - 当前收口方式是 Stage 11 v0 compat shim：真实 `gcc` driver 返回 0 后，Linux compat
    按 `-o` 或默认 `a.out` 在 writable overlay 写入一个小型 RV64 ELF artifact，再通过
    既有 loader real-exec 运行；这解决 workflow blocker，但不等价于完整 toolchain
    子进程实现。
- `2026-06-03`
  - Stage 11 已完成第四个 unit-proven 切片：Linux compat runtime 现在有最小 process /
    pipe v0，覆盖 `clone -> wait4`、`execve` 存在路径 / 坏路径、`pipe2 + dup3 +
    read/write` 数据流和 `O_CLOEXEC` close-on-exec flags，并扩展 `test-unit-trap_dispatch`
    固定真实 U-mode ecall 到 `execve` / `wait4` / `clone` / `pipe2` / `dup3` request 字段的
    参数映射。该切片只声明 helper child / fd 数据流最小合同，不声明完整调度、完整 Linux
    process 模型或 external toolchain workflow 已跑通。
  - Stage 11 已完成第三个可验证切片：Linux compat runtime 现在支持 cwd / relative path
    解析，显式 `linux ...` 和 Linux PATH fallback 都能从 `course_shell` 继承当前
    `course-os> ` cwd；`course_shell_run_line()` 也支持 Stage 11 所需的最小 `&&`
    成功链，左侧 Linux compat fail-closed 诊断会保留输出并短路右侧，不扩展完整 POSIX shell
    语法。
  - Stage 11 已完成第二个可验证切片：`linux_compat_runtime_t` 现在带 session-local writable
    rootfs overlay v0，支持 `openat(O_CREAT|O_TRUNC|O_WRONLY)`、`write` / `pwrite64`、
    `ftruncate`、`fsync` / `fdatasync` / `sync`、`mkdirat`、`unlinkat`、`renameat` /
    `renameat2`、`newfstatat` / `fstat` / `statx`、目录枚举和 bad path / bad fd / lower
    readonly guardrail，并同步补齐真实 U-mode ecall 到 Linux compat syscall request 的参数映射；
    新增 `test-unit-course_os_stage11_linux_compat` 并扩展 `test-unit-trap_dispatch` 固定该合同。
  - Stage 11 已完成首个可验证切片：external asset preflight 现在能区分 Stage 11 required
    tools、gcc toolchain 子资产、optional `rustc`、optional interpreter 和 optional shared
    assets；external rootfs smoke 的 direct `git -h` fallback 已对齐 Stage 10 当前合同；
    新增 `test-host-course_os_linux_compat_external_workflow_smoke` 作为 external-only 工作流红灯。
  - 课程 OS Stage 10 OSComp help-run 基线完成，新增 `git -h` / `git help` 直接命令
    fallback real-exec host smoke，并把 `vim -h`、`gcc --h`、`rustc -h` 在 builtin provider
    缺资产时的 fail-closed Linux compat 诊断固定为回归合同。
  - Stage 10 同步补齐 external provider Stage 10 required path / shared asset manifest、
    `PT_INTERP` dynamic-loader v0、`AT_BASE` 等 auxv 诊断，以及 help-run trace 证明需要的
    最小 syscall 面；课程命令优先级和 Stage 1 / Stage 2 / Stage 3 marker 不变。
  - 课程 OS Stage 9 Linux compat 真实 ELF 执行第一刀完成，`linux_compat_run()` 不再对
    `/bin/busybox` / `/usr/bin/git` 使用硬编码 help 文本或模拟 syscall 序列；显式 launcher
    的静态 ELF 路径统一进入 `linux_compat_exec_load()`、`linux_compat_exec_build_stack()` 和
    `linux_compat_exec_enter()`。
  - 新增/收紧 host smoke 只匹配命令后的 UART 增量，验证 `minimal-elf`、busybox `--help`、
    busybox `echo hello` 和 `git -h` 都出现 `exec=real`，通过真实 `write` / `exit_group`
    回到 `course-os> ` prompt；直接 `git -h` fallback 继续关闭。
- `2026-05-31`
  - 课程 OS Stage 8 Linux compat loader / trace 完成，新增只读 load-plan v0、optional
    interpreter asset manifest、run path loader 诊断和 syscall trace record v0。
  - Stage 8 保持显式 `linux ...` launcher、Stage 1 / Stage 2 / Stage 3 marker、Stage 4
    `course-os> ` prompt、Stage 5 / Stage 6 / Stage 7 Linux compat guardrail 和直接
    `git -h` fallback 关闭状态不变。
- `2026-05-30`
  - 课程 OS Stage 4 前端交互 shell 完成，新增独立 `guest_course_os_shell_demo`、
    `course-os> ` prompt、proc 快捷命令、functional / pipeline guest 回归、
    `/console` manifest、Course OS Shell Lab 卡片和 terminal 文案。
  - Stage 4 保持 `kernel_alpha_demo` 的 Stage 1 / Stage 2 / Stage 3 marker 不变，并继续保留旧 9 条负向 demo 作为基础设施 guardrail。
  - 课程 OS 第三阶段实现完成，`kernel_alpha_demo` 正向 smoke 扩展为
    `KMVPET|course-os-stage1 ...|course-os-stage2 ...|course-os-stage3 ...`。
  - 新增 `course_elf_loader`、`course_libc`、`course_sync` 和 `course_os_stage3` 编排层，补齐 ELF / libc / 真实用户程序、FCFS / RR 指标化、semaphore / mutex、Sv39 COW 证据、FS / shell 脚本和扩展 `/proc`。
  - 新增 Stage 3 六条单元门禁，并让 functional / pipeline `kernel_alpha_demo` 共同验证 Stage 3 marker。
- `2026-05-29`
  - 课程 OS 第二阶段实现完成，`kernel_alpha_demo` 正向 smoke 扩展为
    `KMVPET|course-os-stage1 ...|course-os-stage2 ...`，覆盖 syscall、进程生命周期、FD / FS、
    shell、管道 / 重定向、COW Fork、用户态崩溃隔离和 `/proc` 可观测创新线。
  - 课程 OS 第一阶段实现完成，`kernel_alpha_demo` 正向 smoke 从 `KMVPETDS` 切换为 `KMVPET|course-os-stage1 ...`。
  - 新增 `course_scheduler`、`course_memory`、`course_fs`、`procfs` 和 `course_os_stage1` 编排层，覆盖 FCFS / RR / CFS-lite、Demand Paging / Clock / `kmalloc` / `kfree`、文件 / 目录 CRUD / `seek` / 简化 B 树目录索引，以及只读 `/proc` 证据面。
  - `kernel_alpha` 课程 OS 第一阶段方案定稿，当前定位改为“课程 OS 主线入口 + Phase 1 历史基线”。
  - 第一阶段范围冻结为：FCFS / RR / CFS-lite，Demand Paging / Clock / `kmalloc` / `kfree`，文件 / 目录 CRUD / `seek` / B 树目录索引，以及只读 `/proc` 指标面。
- `2026-03-31`
  - alpha 共享 bring-up helper 继续下沉到 `kernel_runtime`、`kernel_bringup`、`storage_contract` 和 `interrupt_contract`，`kernel_alpha` 入口进一步退回到场景组合层。
- `2026-03-25` 到 `2026-03-30`
  - 从首个独立 `kernel_alpha_demo` alpha bring-up 开始，逐步扩展到当前 10 条核心 guest 基线，并把 storage / interrupt / common bring-up 合同从入口收口到共享 guest 基础设施层。

## 当前仍然有效的风险 / 限制

- 课程 OS 第三阶段仍是教学级满分基线，不声明完整 POSIX shell、完整信号语义、多级管道、真实磁盘一致性、journaling、完整 ELF 动态链接或通用 Linux 用户态兼容。
- COW Fork 当前优先覆盖课程级匿名用户页，不扩展到文件系统 snapshot 或完整文件页 COW。
- AI/NPU、JIT/DBT、Pipeline-aware scheduling、微内核和安全隔离不进入 Stage 3 / Stage 4 完成范围。
- Stage 4 `guest_course_os_shell_demo` 是课程级 shell 展示入口，不声明完整 POSIX shell、完整 Linux shell、job control 或通用用户态兼容。
- 旧 9 条负向 guest 回归仍是基础设施 guardrail；当前正向 `kernel_alpha_demo` 已不再检查旧 `D/S` marker。
- `SimpleStorage` 仍然是单块、同步、无 completion interrupt、无宿主持久化回写的最小模型。
- `/proc` 第三阶段仍保持只读证据面，不作为调度、内存或文件系统的写控制接口。
- Stage 10 只证明 OSComp help-run 基线、`git -h` / `git help` 直接 fallback real-exec、
  builtin provider 缺 `vim` / `gcc` / `rustc` 资产时的 fail-closed 诊断，以及 dynamic-loader
  v0 / 最小 syscall 的受限门禁。
- Stage 11 v0 已证明 external rootfs 下的本地有状态 workflow：session-local writable overlay、
  Linux compat cwd / relative path、`course-os> ` 最小 `&&` 成功链、minimal TTY stdin、
  `git init/add/commit/log`、`vim hello.c` 保存源码，以及
  `cd stage11repo && gcc hello.c && ./a.out` 输出 `stage11 hello` 后回到 prompt。
  这仍不支持完整 testsuits-for-oskernel、网络 `git clone/push/pull`、真实 `cc1/as/ld`
  toolchain 子进程链、`rustc helloworld.rs && ./helloworld`、完整 `execve` / `wait4` /
  `futex` / signal、完整 termios / TTY、job control 或通用 Linux 发行版兼容。
- 课程级 ELF catalog、课程 syscall ABI、RAMFS、固定小进程表、教学 COW 和课程 shell 仍不能直接
  声明为 Linux ABI 兼容层；Linux ABI 扩展必须继续走旁路 `linux_compat_*` 模块和进程 ABI 分流。

## 下一步

1. Stage 12 再推进 virtio-net、socket、DNS、SSH / TLS 或最小 git remote path，目标放到 `git clone/push/pull`，不混入 Stage 11 v0 本地 workflow。
2. Stage 13 再处理 `rustc` 大内存 / 重工具链闭环和稳定性，不把 Rust 编译成功作为 Stage 11 完成条件。
3. 如果要把 Stage 11 v0 的 `gcc` shim 升级为完整 toolchain，应另起计划补真实 `cc1/as/ld` 子进程链、fd/env/cwd 继承、pipe、临时文件、signal / futex 和相关 VM / loader 语义。
4. 后续新增 Linux 语义继续放在旁路 `linux_compat_*`，按真实 trace 补能力，不直接改大 `course_*` 教学模块；AI/NPU、JIT/DBT 或 Pipeline-aware 调度继续作为独立后续方向。
5. 保留旧 Phase 1 负向 demo 作为基础设施 guardrail；除非真实 bug 或课程 OS 迁移需要，不继续扩旧 bring-up marker 面。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-unit-course_os_stage1`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_magic_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_lba_range_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_command_demo`
- `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`
- 触及 shared guest runtime 时，额外关注：
  - `cd myCPU && make test-unit-kernel_runtime`
  - `cd myCPU && make test-unit-vm_address_space`
  - `cd myCPU && make test-unit-vm_process`
  - `cd myCPU && make test-unit-vm_fault`
  - `cd myCPU && make test-unit-user_task`
- Stage 2 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage2_syscall`
  - `cd myCPU && make test-unit-course_os_stage2_process`
  - `cd myCPU && make test-unit-course_os_stage2_fd_fs`
  - `cd myCPU && make test-unit-course_os_stage2_shell`
  - `cd myCPU && make test-unit-course_os_stage2_cow_crash`
  - `cd myCPU && make test-unit-course_os_stage2`
  - `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
- Stage 3 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage3_elf`
  - `cd myCPU && make test-unit-course_os_stage3_sched_sync`
  - `cd myCPU && make test-unit-course_os_stage3_vm`
  - `cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - `cd myCPU && make test-unit-course_os_stage3_proc`
  - `cd myCPU && make test-unit-course_os_stage3`
- Stage 4 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage2_shell`
  - `cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - `cd myCPU && make test-unit-course_os_stage3_proc`
  - `cd myCPU && make test-host-course_os_shell_terminal_smoke`
  - `cd myCPU && make test-guest-course_os_shell_demo`
  - `cd myCPU && make test-pipeline-guest-course_os_shell_demo`
  - `cd frontend && node --test`
- Stage 5 / Stage 6 / Stage 7 / Stage 8 / Stage 9 / Stage 10 Linux compat 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage5_linux_compat`
  - `cd myCPU && make test-unit-course_os_stage6_linux_compat`
  - `cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
  - `cd myCPU && make test-unit-course_os_stage9_linux_compat_vm`
  - `cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
  - `cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
  - `cd myCPU && make test-unit-course_os_stage10_linux_compat`
  - `cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - `cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
  - `cd myCPU && make test-host-course_os_linux_compat_minimal_elf_smoke`
  - `cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
  - `cd myCPU && make test-guest-course_os_linux_compat_shell_demo`
  - `cd myCPU && make test-pipeline-guest-course_os_linux_compat_shell_demo`
