# Undefined-OS 学习笔记

## 背景

本文是对比赛作品 `/home/liangjiaqi/projects/T202510003995291-2331-master` 的第一轮主线阅读笔记，用于给当前
`kernel_alpha` 分线提供参考。它不是完整代码审查，也不声明该作品所有能力已经逐项验证。

当前阅读依据：

- 作品入口文档：`/home/liangjiaqi/projects/T202510003995291-2331-master/README.md`
- 构建与比赛入口：`Cargo.toml`、`Makefile`、`scripts/oscomp_test.sh`、`scripts/make/oscomp.mk`
- 主线源码入口：`src/main.rs`、`src/entry.rs`、`src/syscall.rs`、`src/mm.rs`
- 核心模块：`core/src/mm.rs`、`process/src/process.rs`、`process/src/thread.rs`
- 文件系统与 FD：`src/fs/mount.rs`、`src/fs/imp/tmp.rs`、`api/src/core/file/fd.rs`、`api/src/imp/fs/fd_ops.rs`
- PDF 文本曾在 `/tmp/决赛文档.txt`、`/tmp/阶段性提交文档.txt` 抽取成功；本文以源码和脚本主线为主。

## 它的实现方法

Undefined-OS 不是从零写一个最小教学内核，而是在 ArceOS 生态上做单体内核扩展。顶层 README 明确写为
`A monolithic kernel based on ArceOS`，工程上把 ArceOS 作为底座，把比赛需要的 Linux 用户态兼容能力放在
自己的 workspace crate 和 app 层里。

顶层 Rust workspace 分成 `api`、`core`、`process`、`syscall_trace`、`modules/vfs` 等 crate，并排除
`arceos`、`apps`、`vendor`。这个结构的核心价值是把 syscall/API、进程模型、内存加载、VFS、trace 分成
独立边界，而不是把所有比赛补洞塞进一个入口文件。

启动路径很直接：`src/main.rs` 初始化 root、FD 表和挂载点，然后从 `AX_TESTCASES_FILE` 读取测试列表，
构造 `/musl/busybox sh -c <testcase_list>` 作为首个用户态命令。也就是说，它把比赛测试统一包装成一个
busybox shell 脚本入口，而不是为每个用例在内核里写特殊分支。

用户程序执行由 `src/entry.rs` 串起来：创建用户地址空间，复制必要内核映射，映射 signal trampoline，
解析目标路径，加载 ELF，构造 `UspaceContext`，创建 ArceOS task，再绑定自定义 process/thread 数据。
这条路径把 ArceOS 的调度、地址空间和自己的进程对象连接在一起。

ELF / 内存加载在 `core/src/mm.rs` 里实现。它负责新建用户地址空间、映射 ELF `PT_LOAD` 段、处理脚本
shebang、处理 `PT_INTERP`，构造 argv/envp/auxv 用户栈，并映射固定 user heap。这里的一个关键做法是：
动态链接器不是特殊 fake 输出，而是通过 interpreter path 递归进入同一套 loader 路径。

syscall 分发集中在 `src/syscall.rs`。它用 `Sysno` 解析 syscall number，按 match arm 显式分发到
`undefined_os_api::{imp, interface}` 下的实现。第一轮脚本统计显示该文件大约有 214 个 syscall match arms，
其中一部分仍是 stub 或固定错误返回。这里的工程取舍很清楚：广覆盖 syscall 面，但允许部分 syscall 先以
可诊断的固定错误或 stub 形式存在。

进程模型在 `process` crate 中独立维护。`Process` 包含 pid、threads、process group、children、parent、
zombie 状态和 exit code；`Thread` 单独维护 tid 与所属 process。`clone` 路径进一步结合 trap frame、
地址空间共享或复制、FD/FS namespace 复制、TLS 和子线程返回值。这比我们当前 Stage 11 的 helper child
语义更接近真实 Linux 进程/线程模型。

VFS 采用挂载式结构。`mount_all()` 把 `/dev`、`/tmp`、`/proc` 挂到全局 FS context；`tmpfs` 自己维护
inode、metadata、目录项、read/write/append/resize/link/unlink 等节点操作。这个方向对比赛很实用：真实
用户态程序会频繁依赖 `/tmp`、`/proc`、`/dev/null`、目录枚举和 metadata，不能只靠只读 rootfs catalog。

比赛测试入口做得比较系统。`scripts/oscomp_test.sh` 会下载对应架构的 sdcard 镜像，设置 `BLK=y NET=y`
和 `FEATURES=fp_simd,lwext4_rs`，按 basic、busybox、lua、libctest 等测试组改写 `apps/oscomp/testcase_list`，
运行 QEMU 后用 judge 脚本判定输出。`scripts/make/oscomp.mk` 还定义了构建比赛 kernel-rv / kernel-la 和
`oscomp_run` 的入口。

多架构是它的一个重要基础设计。README 和 Makefile 都把 `riscv64`、`x86_64`、`aarch64`、`loongarch64`
作为一等配置；Makefile 针对不同架构选择不同 target，并要求 QEMU、musl cross toolchain、ArceOS config
共同配合。这一点和我们当前自研模拟器 + RV64 主线不同：它优先扩展运行环境覆盖面，我们优先守住自研模拟器
上的可观察性和 reference path。

## 子系统的更深细节（Codex 第一轮未覆盖）

以下是对 Codex 第一轮阅读的补充，聚焦于对 kernel_alpha Stage 11 有直接参考价值的子系统。

### Session → ProcessGroup → Process → Thread 四层 POSIX 进程模型

`process/src/` 四个文件实现了完整的 POSIX 进程层级：

```
Session (sid) ──1:N──> ProcessGroup (pgid) ──1:N──> Process (pid) ──1:N──> Thread (tid)
```

| 层 | 源码 | 关键行为 |
|------|------|------|
| Session | `process/src/session.rs` | `create_session()` 创建新会话，进程成为 session leader + group leader。session leader 不能移动到其他进程组 |
| ProcessGroup | `process/src/process_group.rs` | 前台/后台作业控制基础设施。`send_signal_process_group()` 按 pgid 广播信号（Ctrl-C 等） |
| Process | `process/src/process.rs` | 完整生命周期：`fork()`/`exit()`/`release()`。**孤儿回收**：父进程 exit 时子进程自动移交给 `init`(pid=1)。`is_zombie` 原子标志 |
| Thread | `process/src/thread.rs` | `is_main_thread()`（tid==pid）。`execve` 时只保留主线程，杀死其他线程 |

PID/TID 共享同一个命名空间（`AtomicU32` 递增），Linux 风格。`ResourceLimits`（`core/src/resource.rs`）
实现了 15 种 `rlimit`：CPU/FSIZE/DATA/STACK/CORE/RSS/NPROC/NOFILE/MEMLOCK/AS 等，默认 `NOFILE=1024`。

对 kernel_alpha 的参考：Stage 11 当前的 helper child 没有 parent/child/zombie/reaper 这些状态。
`git` 和 `gcc` 子进程的 wait/zombie 回收、Ctrl-C 信号转发都需要至少 Process + ProcessGroup 两层。

### futex 完整实现

`api/src/imp/task/futex.rs`（~100 行）实现了全部主要 futex op：

| op | 程度 |
|------|:--:|
| `FUTEX_WAIT` | 完整：原子校验 `*uaddr==value`（不满足→`EAGAIN`），支持超时 |
| `FUTEX_WAKE` | 完整：唤醒最多 `value` 个等待者，返回实际唤醒数 |
| `FUTEX_REQUEUE` | 完整：唤醒 `value` 个 + 将剩余 `value2` 个迁移到另一 futex |
| `FUTEX_CMP_REQUEUE` | 完整：额外校验 `*uaddr==value3` |
| `FUTEX_WAIT_BITSET` / `FUTEX_WAKE_BITSET` | 部分：bitset 必须为 `FUTEX_BITSET_MATCH_ANY` |

数据结构：per-process `BTreeMap<usize, Arc<WaitQueue>>`，futex key = 用户态地址。`FUTEX_CMP_REQUEUE` 是
glibc `pthread_cond_broadcast` 的关键依赖（"唤醒+转移"语义）。

对 kernel_alpha 的参考：这是 Stage 11 任务 4 步骤 3b 最需要的子系统之一。代码量小、结构清晰，可直
接作为 C 实现的参照。`gcc` 的多线程子进程（`cc1`/`as`/`ld`）很可能在 futex 上卡住。

### signal 完整实现

`api/src/imp/task/signal.rs` 实现了完整的 Linux signal 子系统。

信号分发匹配四层进程模型：

```
kill(pid>0)  → send_signal_process(pid)
kill(pid=0)  → send_signal_process_group(当前 pgid)
kill(pid=-1) → send_signal_process(所有进程)
kill(pid<-1) → send_signal_process_group(-pid)
tkill(tid)   → send_signal_thread(tid)
tgkill(tgid,tid) → check_thread + send_signal_thread
```

已实现的 signal syscall：`rt_sigaction`（SIGKILL/SIGSTOP 拒绝修改）、`rt_sigprocmask`
（BLOCK/UNBLOCK/SETMASK）、`rt_sigpending`、`rt_sigreturn`（从用户栈恢复 trap frame）、
`rt_sigsuspend`、`rt_sigtimedwait`、`rt_sigqueueinfo`/`rt_tgsigqueueinfo`（带 siginfo）、
`sigaltstack`（size ≥ `MINSIGSTKSZ` 校验）。

信号处理 5 种 action：Terminate / CoreDump / Stop / Continue / Handler（用户态 handler）。
`POST_TRAP` hook：每次从用户态回到内核时检查 pending 信号并处理。

对 kernel_alpha 的参考：`gcc` 的子进程管理（`as`/`ld`/`cc1`）很可能需要信号来协调退出。`POST_TRAP`
hook 的设计可以作为你在 `trap_dispatch` 中增加信号检查的模板。

### mmap 完整语义矩阵

`api/src/imp/mm/mmap.rs` 的 mmap 实现远超教学级。相比 kernel_alpha 当前只支持
`MAP_ANONYMOUS|MAP_PRIVATE`，Undefined-OS 额外支持：

| 能力 | kernel_alpha | Undefined-OS |
|------|:--:|:--:|
| `MAP_FIXED`（重叠区域 unmap） | ❌ | ✅ |
| `MAP_FIXED_NOREPLACE`（重叠则失败） | ❌ | ✅ |
| `MAP_SHARED`（共享物理帧） | ❌ | ✅（独立 Backend::Shared） |
| `MAP_STACK` | ❌ | ✅ |
| 大页 2M/1G | ❌ | ✅（`PageSize::Size2M/Size1G`） |
| 文件映射（read file → mapped pages） | ❌ | ✅ |
| 设备内存映射（DeviceMem，物理地址直映射） | ❌ | ✅ |
| `mprotect`（READ/WRITE/EXEC） | ❌ | ✅ |

对 kernel_alpha 的参考：`git` 和 `gcc` 大量使用 `mmap(MAP_PRIVATE)` 做文件读取，不是全部走
`read`/`write`。文件映射可以让 overlay 读写路径更高效。

### ext4 真实磁盘文件系统

`modules/lwext4_rust/` 通过 Rust FFI 绑定 C 库 lwext4，提供了完整 ext4 读写能力：

`Ext4Filesystem::new(dev)` → 初始化 superblock / block cache / block device 绑定。
支持：`create`（分配 inode + 目录项 + `.`/`..` 自引用）、`read_at`/`write_at`（块缓存读写）、
`set_len`（truncate）、`set_symlink`、`lookup`、`read_dir`、`link`（硬链接，`EISDIR` 拒绝）、
`unlink`（`ENOTEMPTY` 检查 + nlink 递减）、`rename`（跨目录，含 `..` 更新和 nlink 调整）、
`stat`（superblock 统计）。`WritebackGuard` 用 RAII 控制块缓存写回。

对 kernel_alpha 的参考：如果 Stage 12/13 需要持久化文件系统，lwext4 FFI 绑定是比从零实现 ext4
实际得多的路径。

### ArceOS AddrSpace 三种映射 Backend

`arceos/modules/axmm/src/backend/mod.rs` 定义了三种映射后端：

```rust
pub enum Backend {
    Shared { shared_frame: Arc<SharedFrame>, align }  // COW 共享内存
    Linear { pa_va_offset, align }                      // 线性映射（设备 MMIO）
    Alloc  { populate: bool, align }                     // 惰性/预分配
}
```

- **Linear**：vaddr - paddr = 常数，用于内核直接映射和设备 MMIO
- **Alloc**：`populate=true` 时预先分配所有物理帧，`populate=false` 时 page fault 惰性分配
- **Shared**：多个地址空间共享同一物理帧，`Arc<SharedFrame>` 管理生命周期

对 kernel_alpha 的参考：当前 Demand Paging 直接基于 `vm_fault` 实现，没有 Backend 抽象层。如果后
续要支持 `MAP_SHARED`，这个三层 Backend 设计值得参考。

### syscall stub 三层治理策略

Undefined-OS 把未完整实现的 syscall 分为三个层级，不是简单返回 `ENOSYS`：

| 策略 | 行为 | 举例 |
|------|------|------|
| **stub_bypass** | 返回 0，无副作用 | `sync`、`fsync`、`setgid`、`flock`、`prctl`、`msync`、`setsid`、`fallocate` |
| **stub_unimplemented** | 返回 `ENOSYS` | 任何未识别的 syscall number |
| **显式 errno** | 返回特定 Linux errno | `getsockopt→EFAULT`、`msgrcv→ENOMSG`、`setpriority→ESRCH`、`adjtimex→EPERM` |

对 kernel_alpha 的参考：Stage 11 已对 `fsync`/`sync` 做了 no-op。`stub_bypass` 模式可扩展到更多
"被 libc 调用但无害"的 syscall（`flock`、`prctl`、`msync` 等），减少不必要的 fail-closed 噪声。

## 对我们项目的学习清单

### 立刻值得学

1. 把 Stage 11 的真实工作流继续做成“测试列表驱动”的入口。Undefined-OS 用 busybox shell 包装测试列表；
   我们可以保留 `course-os> ` 交互入口，同时给 external workflow smoke 增加更清晰的 command list /
   per-command result / stop reason 记录。

2. syscall 面要分层治理。Undefined-OS 把 syscall number 到实现函数的映射集中管理；我们现在的
   `linux_compat_*` 也应该继续保持集中 request/dispatch/trace 合同，新增 syscall 前先补最窄红灯，
   不把 Linux ABI 混进课程级 `course_*` 模块。

3. 进程模型要从 helper child 走向可观察的最小真实模型。Stage 11 当前已经有 `clone -> wait4`、
   `execve`、pipe/dup 最小合同，但后续需要逐步补 pid、parent/child、zombie、exit status、fd/fs
   继承这些状态对象，而不是继续靠单次 helper 返回值硬撑。

4. VFS / overlay 需要 inode-like metadata。Undefined-OS 的 tmpfs 明确维护 inode、nlink、metadata、
   directory entry 和 file content。我们的 writable rootfs overlay v0 已经有 session-local 节点，
   后续应优先补齐对 Git/GCC 有影响的 metadata 稳定性：mtime、mode、link count、目录 offset、rename /
   unlink 后 opened fd 行为。

5. `/proc`、`/dev`、`/tmp` 是比赛用户态兼容的基础设施，不是展示加分项。我们当前已在 Stage 11 推过
   `/dev/null` 等 blocker，后续遇到 Git/GCC/Vim 卡住时，应优先把这些伪文件系统当成 contracts 补洞，
   并固定到 unit + host smoke。

6. OSComp rootfs 工作流要保留 opt-in。Undefined-OS 默认就面向 QEMU + sdcard；我们不同，我们的默认
   `make test` 不能依赖本机外部镜像。值得学的是外部镜像 preflight、manifest 和分组验证，不是把外部
   rootfs 变成默认门禁。

7. 动态链接器路径不能长期停在“能识别 PT_INTERP”。Undefined-OS 把 interpreter 递归纳入 loader path。
   我们 Stage 8-11 已经能记录 dynamic loader 元数据并推进真实 `/usr/bin/git`，下一步要继续以真实 stop
   point 为准推进 auxv、TLS、mmap/mprotect、file-backed mapping，而不是回到 help-run fake 路线。

8. 判题脚本化值得学。它的 `judge_basic.py`、`judge_busybox.py` 等把输出判定从人工观察里拿出来。我们可
   在 host smoke 里继续强化“命令增量 UART 输出 + trace + stop reason”的结构化断言，减少看日志猜进度。

### 需要谨慎学

1. 不照搬 ArceOS 依赖。Undefined-OS 的优势来自成熟底座：调度、地址空间、driver、QEMU、多架构、Cargo
   feature 管理。我们项目是已可运行的模拟器原型，价值在自研模拟器、reference-first 和可观察性；不能把
   “换底座”当成学习结论。

2. 不追求多架构优先。比赛作品支持四架构很强，但我们当前 `kernel_alpha` 主线还在 RV64 Linux compat
   本地工作流收口。当前阶段不应把 x86/aarch64/loongarch64 作为目标，否则会稀释 Stage 11。

3. 不盲目扩大 syscall 数量，但要学它的 stub 分层策略。Undefined-OS 有大量 syscall match arms，其中
   一部分是 stub 或固定错误。它把 stub 分成三层：**stub_bypass**（无害→返回 0，如 `sync`/`fsync`/
   `flock`/`prctl`）、**stub_unimplemented**（返回 `ENOSYS`）、**显式 errno**（返回特定错误码）。
   我们更适合 trace-driven：只为真实 blocker 增加语义；但对那些"libc 必调但无关 workflow 正确性"
   的 syscall（如 `flock`、`prctl`、`set_robust_list`），可以直接借鉴 stub_bypass 返回 0，减少
   fail-closed 噪声。

4. 不把 shell 语法做成大工程。Undefined-OS 用 busybox shell 承接脚本；我们目前只需要 Stage 11 的
   最小 `&&`、cwd 传递和 PATH fallback。完整 POSIX shell、job control、多级管道仍应保持非目标。

5. 不把 external workflow 成功误写成 Stage 11 已完成。我们当前状态仍是 `/usr/bin/git` 真实执行进入
   更深用户态后在 step budget 处停止；这更像性能/热点/用户态长循环诊断点，不是 Git/Vim/GCC 工作流已
   闭环。

## 建议映射到 kernel_alpha 后续工作

近期最有价值的三个动作：

1. 给 `test-host-course_os_linux_compat_external_workflow_smoke` 增加更结构化的 per-command summary：
   command、resolved path、loader kind、interp、exit/stop reason、last pc、unsupported syscall、errno、
   trace count。

2. 把 Stage 11 process state 从 unit-proven helper child 继续拆成小对象。借鉴 Undefined-OS 的
   Session→ProcessGroup→Process→Thread 四层模型，优先实现 Process（parent/child/zombie/exit code/
   reaper 回收）和 ProcessGroup（信号按 pgid 转发），先服务 `git` 和 `gcc` 子进程，不做完整调度
   承诺。

3. 围绕当前 `/usr/bin/git` `pc=0x400cc6d2` 长跑点补读写路径可观察性：read 返回值、EOF、errno、短读、
   directory iteration、stat metadata、mmap/mremap 行为。同时关注两个未实现的子系统：**futex**（
   `gcc` 多线程子进程极可能在此卡住，参考 Undefined-OS `futex.rs` ~100 行）和 **signal**（`gcc`
   子进程协调退出可能需要 `SIGCHLD`/`SIGPIPE`）。只有拿到新的明确 blocker 后再补 ABI。

中期可以规划但不应抢 Stage 11 的工作：

1. 借鉴 `apps/oscomp/testcase_list` 的组织方式，给我们自己的 Course OS / Linux compat smoke 建一个
   command-list manifest，方便前端展示和 CI 过滤。

2. 把 `/proc`、`/dev`、`/tmp` 的 Linux compat 语义整理成独立设计文档，明确哪些是课程证据面，哪些是
   Linux 用户态兼容所需 pseudo filesystem。

3. 如果后续进入 Stage 12/13，再学习 Undefined-OS 的 sdcard image + test group + judge script 模式，
   用于网络、Rust toolchain 或更大 OSComp 子集验证。
