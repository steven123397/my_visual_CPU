# course_os_kernel_alpha Stage 11 writable rootfs / process-file workflow 计划

> **文档状态：** 执行中

## 文档定位

本文档记录 `kernel_alpha` Linux compat Stage 11 的活跃执行计划。

Stage 11 的主线目标是可写 rootfs + Linux process / file 语义第一轮闭环，验收目标从
Stage 10 的 help-run 提升到本地有状态工作流：

- `git init/add/commit/log`
- `vim hello.c`
- `gcc hello.c && ./a.out`

外部 OSComp rootfs 下的真实 `git` / `vim` / `gcc` 资产 probe 仍是 Stage 11 的前置红灯和
诊断入口，但不再是 Stage 11 的完成定义。`git clone/push/pull` 网络路径留给 Stage 12；
`rustc` 大内存 / 重工具链闭环留给 Stage 13。

实时状态仍以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准；
本文档只维护执行 checklist 和验收口径。完成后应归档到
[history_plan.md](history_plan.md)，并删除本文档。

## 关联文档

- 来源设计：
  - [../design/course_os_kernel_alpha_linux_compat_plus_design.md](../design/course_os_kernel_alpha_linux_compat_plus_design.md)
  - [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
- 目标状态：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 已完成前置：
  - [history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan](history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan)
  - [history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan](history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan)
  - [history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan](history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan)
  - [history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan](history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan)

## 目标

- 让 external / OSComp rootfs provider 能为 Stage 11 工作流提供真实 `/usr/bin/git`、
  `/usr/bin/vim`、`/usr/bin/gcc`、必要 interpreter、shared assets 和 toolchain 子程序资产。
- 新增 Linux compat writable rootfs overlay v0，支持文件 / 目录创建、truncate、write /
  pwrite、readback、metadata 更新、rename / unlink、fsync / sync no-op 成功语义和失败诊断。
- 把 Linux compat runtime 的 cwd、relative path、FD、process ABI、exec context 和退出状态串起来，
  支撑 `git -C <repo>`、`vim hello.c`、`gcc hello.c` 与 `./a.out` 这类本地工作流。
- 按真实 trace 最小补齐 `execve` / `wait4` / `clone` 或 `vfork`、pipe / dup、`fcntl`、
  `getcwd` / `chdir`、`statx` / `newfstatat`、`readlinkat`、`mprotect`、`futex` / signal
  fail-closed 或低副作用语义。
- 给 `course-os> ` shell 增加 Stage 11 所需的 Linux cwd 传递和最小 `&&` 成功链执行，
  但保持课程命令、课程用户程序和显式 `linux ...` 路由优先级不变。
- 固定 Stage 11 opt-in host smoke：缺外部 rootfs 时 fail-closed；有外部 rootfs 时验证
  `git init/add/commit/log`、`vim hello.c`、`gcc hello.c && ./a.out` 真实执行、trace
  和 prompt 回归。

## 非目标

- 不承诺跑完 `testsuits-for-oskernel` 全部用例。
- 不在 Stage 11 完成 `git clone/push/pull`、virtio-net、socket、DNS、SSH、TLS 或远端认证。
- 不在 Stage 11 完成 `rustc helloworld.rs && ./helloworld`、大内存 toolchain 或 Rust crate 构建。
- 不声明完整动态链接器、完整 relocation、完整 TLS、完整 signal / futex、完整 TTY / termios、
  完整 shell 语法、job control 或通用 Linux 发行版兼容。
- 不把 Linux syscall 直接塞进课程 `course_syscall`，也不改变课程 shell / 课程用户程序的 ABI。
- 不让 external rootfs opt-in target 进入默认 `make test`；默认回归继续不依赖宿主机 rootfs。

## 完成定义

- 缺少 `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS` 时，Stage 11 external opt-in targets fail-closed，
  并输出明确环境变量要求；默认 `make test` 不依赖外部 rootfs。
- external provider 的 manifest 明确记录 Stage 11 资产：
  - required：`/usr/bin/git`、`/usr/bin/vim`、`/usr/bin/gcc`
  - required 或 trace-proven：`sh` / `as` / `ld` / `cc1` / libc startup objects 等 gcc
    工作流必需资产
  - optional：`/usr/bin/rustc`，只作为 Stage 13 预检记录，不作为 Stage 11 pass 条件
  - optional shared assets：interpreter、libc、libgcc、libstdc++ 等
- 新增 `test-unit-course_os_stage11_linux_compat` 固定 writable rootfs、cwd / relative path、
  write syscall、process wait / exec 和 shell `&&` 合同。
- 新增 `test-host-course_os_linux_compat_external_workflow_smoke`，在 external rootfs 下固定：
  - `git init stage11repo`
  - 写入或编辑 `stage11repo/hello.c`
  - `git -C stage11repo add hello.c`
  - `git -C stage11repo commit -m init`
  - `git -C stage11repo log --oneline`
  - `vim hello.c` 能通过最小 terminal 输入保存文件
  - `gcc hello.c && ./a.out` 输出稳定 hello 文本
- 每个新增 syscall / loader / process / file 语义都必须由真实 external rootfs trace 证明需要；
  未知 syscall、缺资产、坏路径、坏用户指针和用户态崩溃继续 fail-closed，不能伪成功。
- Stage 1 / Stage 2 / Stage 3 marker、Stage 4 `course-os> ` prompt、Stage 5-10 Linux compat
  guardrail、旧 9 条负向 demo、`make test` 和 `make test-pipeline` 保持稳定。

## 任务

### 任务 1：Stage 11 scope guard、asset preflight 与 workflow 红灯

**文件：**
- 修改：`myCPU/tools/linux_compat_rootfs_asset.py`
- 修改：`myCPU/tests/host/linux_compat_rootfs_asset_test.py`
- 修改：`myCPU/tests/host/course_os_linux_compat_external_rootfs_smoke.cpp`
- 创建：`myCPU/tests/host/course_os_linux_compat_external_workflow_smoke.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：补 external asset preflight 红灯**
  - 在 `linux_compat_rootfs_asset_test.py` 增加 synthetic external source 覆盖 Stage 11 required
    tools、gcc toolchain 子资产、optional `rustc` 和 optional shared assets。
  - 运行：`cd myCPU && python3 -m unittest tests.host.linux_compat_rootfs_asset_test`
  - 预期：manifest 未能区分 required / optional / shared asset 时 FAIL。
- [x] **步骤 2：对齐 Stage 10 direct fallback 历史 smoke**
  - 将 `course_os_linux_compat_external_rootfs_smoke.cpp` 中“direct git fallback should remain
    disabled”的旧断言改为 Stage 10 当前合同：external provider 下直接 `git -h` 应进入
    `/usr/bin/git` Linux compat real-exec 或明确 Linux compat fail-closed 诊断。
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_external_rootfs_smoke`
  - 预期：无 `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS` 时 target fail-closed，并提示环境变量要求。
- [x] **步骤 3：新增 Stage 11 workflow host smoke 红灯**
  - target 名称固定为 `test-host-course_os_linux_compat_external_workflow_smoke`。
  - host smoke 使用 `guest/generated/course_os_linux_compat_external_shell.elf`，只匹配当前命令后的
    UART 增量。
  - 缺外部 rootfs 时预期 fail-closed；有外部 rootfs 时预期暴露第一个 writable rootfs、
    process、TTY 或 toolchain blocker。
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_external_workflow_smoke`

### 任务 2：writable rootfs overlay v0

**文件：**
- 修改：`myCPU/guest/include/linux_compat_rootfs.h`
- 修改：`myCPU/guest/kernel/linux_compat_rootfs_builtin.c`
- 修改：`myCPU/guest/generated/linux_compat_rootfs_asset.c`
- 修改：`myCPU/guest/include/linux_compat.h`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 创建或修改：`myCPU/tests/unit/course_os_stage11_linux_compat.c`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：补 writable rootfs unit 红灯**
  - 覆盖 `openat(O_CREAT|O_TRUNC|O_WRONLY)`、`write`、`lseek`、`readback`、`fstat`、`unlink`、
    `rename`、目录创建和 bad path / bad fd / read-only provider guardrail。
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`
  - 预期：当前只读 rootfs 实现 FAIL。
- [ ] **步骤 2：实现内存 overlay**
  - overlay 叠在 builtin / external provider 之上；原 provider 继续提供只读 lower layer。
  - overlay 节点维护 inode-like id、mode、size、mtime、directory / regular 标记和 dirty 标记。
  - 写入只影响当前 guest session，不声明宿主持久化回写。
- [ ] **步骤 3：接入 Linux syscall 路径**
  - `openat` flags、`write` / `pwrite64`、`ftruncate`、`fsync` / `fdatasync` / `sync`、
    `mkdirat`、`unlinkat`、`renameat` / `renameat2`、`newfstatat` / `statx` 返回稳定 Linux errno
    和 trace record。
  - 用户指针读写继续经过现有 VM 校验；坏指针 fail-closed。
- [ ] **步骤 4：验证 writable rootfs 层**
  - 运行：`cd myCPU && make test-unit-course_os_stage6_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage10_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`

### 任务 3：cwd、relative path 与 `course-os> ` workflow shell

**文件：**
- 修改：`myCPU/guest/include/linux_compat.h`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/guest/include/course_shell.h`
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/tests/unit/course_os_stage10_linux_compat.c`
- 修改：`myCPU/tests/unit/course_os_stage11_linux_compat.c`
- 修改：`myCPU/tests/host/course_os_linux_compat_terminal_smoke.cpp`

- [ ] **步骤 1：补 cwd / relative path 红灯**
  - Stage 11 unit 覆盖 Linux compat request 从 `course_shell` 继承 cwd，`git -C repo ...`、
    `./a.out` 和 `hello.c` relative path 都解析到同一 overlay 工作目录。
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`
  - 预期：当前 Linux fallback 未携带 Linux cwd 时 FAIL。
- [ ] **步骤 2：补最小 `&&` 成功链红灯**
  - `gcc hello.c && ./a.out` 只有左侧命令退出码为 0 时才执行右侧命令。
  - 左侧 fail-closed 时输出诊断并保留 prompt，不执行右侧。
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`
- [ ] **步骤 3：实现 shell 合同**
  - 课程命令、Stage 3 catalog、显式 `linux ...`、Linux PATH fallback 的既有顺序不变。
  - `course-os> cd` / `pwd` 的 cwd 可传给 Linux compat 进程，但不把课程 FD / FS ABI 改成
    Linux ABI。
  - `&&` 只支持 Stage 11 需要的简单命令链，不扩展完整 POSIX shell 语法。
- [ ] **步骤 4：验证 shell guardrail**
  - 运行：`cd myCPU && make test-unit-course_os_stage10_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`

### 任务 4：process / exec / wait 与 toolchain 子进程

**文件：**
- 修改：`myCPU/guest/include/linux_compat.h`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/guest/kernel/linux_compat_exec.c`
- 修改：`myCPU/guest/kernel/linux_compat_loader.c`
- 修改：`myCPU/guest/kernel/linux_compat_vm.c`
- 修改：`myCPU/guest/kernel/trap_dispatch.c`
- 修改：`myCPU/tests/unit/course_os_stage9_linux_compat_exec.c`
- 修改：`myCPU/tests/unit/course_os_stage9_linux_compat_syscall.c`
- 修改：`myCPU/tests/unit/course_os_stage11_linux_compat.c`

- [ ] **步骤 1：记录真实 blocker trace**
  - 运行：`MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=<external-rootfs> make test-host-course_os_linux_compat_external_workflow_smoke`
  - 对 `git commit`、`vim`、`gcc` 的第一个 blocker 记录 path、argv、loader kind、interpreter、
    first unsupported syscall、PC、errno 和已执行 trace records。
- [ ] **步骤 2：补 process unit 红灯**
  - 覆盖 `execve` 继承 cwd / envp / fd、`wait4` 读取退出码、`clone` 或 `vfork` 的最小子进程
    生命周期、`pipe2` / `dup3` / `close` 的工具链子进程数据流。
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`
- [ ] **步骤 3：实现 trace-driven 最小语义**
  - 只为 `git init/add/commit/log`、`vim hello.c`、`gcc hello.c && ./a.out` 真实 trace 需要的
    process / syscall 补洞。
  - `futex`、`rt_sigaction`、`rt_sigprocmask`、`set_tid_address`、`set_robust_list` 等优先实现
    可诊断最小语义；不能静默伪成功。
  - 每个 syscall 固定 Linux errno、用户指针校验、trace record 和 unsupported fallback。
- [ ] **步骤 4：验证 process / exec 层**
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`

### 任务 5：minimal TTY / terminal input for `vim hello.c`

**文件：**
- 修改：`myCPU/guest/include/linux_compat.h`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/guest/kernel/console_input.c`
- 修改：`myCPU/guest/course_os_shell/main.c`
- 修改：`myCPU/tests/unit/course_os_stage11_linux_compat.c`
- 修改：`myCPU/tests/host/course_os_linux_compat_external_workflow_smoke.cpp`

- [ ] **步骤 1：补 terminal 红灯**
  - host smoke 启动 `vim hello.c` 后发送最小编辑序列：进入插入模式、写入 `hello.c` 内容、
    退出插入模式、`:wq` 保存退出。
  - 预期：当前缺少 stdin read / termios / TTY 时暴露明确 blocker。
- [ ] **步骤 2：实现最小 TTY 合同**
  - 支持 `read` from fd 0 消费 UART input queue。
  - 支持 `ioctl(TCGETS/TCSETS/TIOCGWINSZ)`、`isatty` 相关查询和 terminal size 稳定返回。
  - 保持 prompt 回归；用户态崩溃或 unsupported TTY 能力必须 fail-closed。
- [ ] **步骤 3：验证 `vim hello.c` 文件结果**
  - `vim` 退出后通过 `cat hello.c` 或 Linux compat `open/read` 读取 overlay 内容。
  - 内容必须包含后续 `gcc` 使用的 hello 程序源码。
  - 运行：`MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=<external-rootfs> make test-host-course_os_linux_compat_external_workflow_smoke`

### 任务 6：`git init/add/commit/log` 与 `gcc hello.c && ./a.out` 端到端收口

**文件：**
- 修改：`myCPU/tests/host/course_os_linux_compat_external_workflow_smoke.cpp`
- 修改：`myCPU/tests/unit/course_os_stage11_linux_compat.c`
- 修改：`myCPU/Makefile`
- 按真实 trace 修改：`myCPU/guest/kernel/linux_compat.c`
- 按真实 trace 修改：`myCPU/guest/kernel/linux_compat_exec.c`
- 按真实 trace 修改：`myCPU/guest/kernel/linux_compat_loader.c`
- 按真实 trace 修改：`myCPU/guest/kernel/linux_compat_vm.c`

- [ ] **步骤 1：固定 git 本地工作流**
  - host smoke 验证 `git init` 创建 `.git`，`git add hello.c` 写 index，`git commit -m init`
    写对象 / refs，`git log --oneline` 输出提交摘要。
  - Git author / committer 环境变量在 test request 中固定，避免宿主环境漂移。
- [ ] **步骤 2：固定 gcc 工作流**
  - host smoke 验证 `gcc hello.c && ./a.out`。
  - `gcc` 产生的 `a.out` 必须来自 overlay 写入，并通过 Linux compat loader real-exec。
  - 输出固定为 hello 文本，且 trace 包含 `exec=real`。
- [ ] **步骤 3：保持 external-only 门禁边界**
  - Stage 11 workflow smoke 不进入默认 `make test`。
  - 缺 `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS` 时目标 fail-closed，提示如何设置 env。
  - builtin provider 下 `git -h` / `git help`、缺 `vim/gcc/rustc` 的 Stage 10 fail-closed 合同不变。
- [ ] **步骤 4：运行 Stage 11 workflow**
  - 运行：`MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=<external-rootfs> make test-host-course_os_linux_compat_external_workflow_smoke`
  - 预期：`git`、`vim`、`gcc`、`./a.out` 均回到同一个 `course-os> ` prompt，且输出不来自固定伪造文本。

### 任务 7：Stage 11 回归、状态回写与归档

**文件：**
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`docs/design/course_os_kernel_alpha_linux_compat_plus_design.md`
- 修改：`docs/index.md`
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md`

- [ ] **步骤 1：跑 Stage 11 固定门禁**
  - 运行：`cd myCPU && python3 -m unittest tests.host.linux_compat_rootfs_asset_test`
  - 运行：`cd myCPU && make test-unit-course_os_stage5_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage6_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_vm`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
  - 运行：`cd myCPU && make test-unit-course_os_stage10_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage11_linux_compat`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_minimal_elf_smoke`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_external_rootfs_smoke`
  - 运行：`MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=<external-rootfs> make test-host-course_os_linux_compat_external_workflow_smoke`
- [ ] **步骤 2：跑全量回归**
  - 运行：`cd myCPU && make test`
  - 运行：`cd myCPU && make test-pipeline`
  - 运行：`git diff --check`
- [ ] **步骤 3：回写完成态**
  - 在 `kernel_alpha_status.md` 增加 Stage 11 完成摘要、关键历史节点、剩余限制和新的下一步。
  - 在 `course_os_kernel_alpha_linux_compat_plus_design.md` 更新当前完成态 / 当前计划链接。
  - 在 `docs/index.md` 把活跃计划链接改为 `history_plan.md` Stage 11 锚点。
  - 在 `history_plan.md` 追加 `course-os-kernel-alpha-stage11-writable-rootfs-process-file-plan`
    归档条目。
  - 删除本文档，确保 `docs/plan/` 不长期保留完成态 checklist。

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 必须更新：
  - Stage 11 完成结果摘要。
  - writable rootfs / process-file workflow 的实际结果。
  - `git init/add/commit/log`、`vim hello.c`、`gcc hello.c && ./a.out` 的验收证据。
  - Stage 11 仍不支持的 testsuits 子项。
  - Stage 12 网络 git 和 Stage 13 `rustc` 的下一步。
- [history_plan.md](history_plan.md) 必须追加：
  - 完成时间。
  - 完成内容。
  - 简短实现过程摘要。
  - 验证摘要。
- 本计划归档完成后删除，不再作为并行事实来源保留。
