# Course OS OSComp External Validation Design

## 文档定位

本文档定义 `kernel_alpha` 在 Stage 4 课程 OS shell 基线之后，如何把
OSComp / `testsuits-for-oskernel` 作为 Linux compat Plus 的 opt-in 外部验证证据接入。

它不改变 Stage 1-4 课程 OS 基线完成定义，不替代
[course_os_kernel_alpha_linux_compat_plus_design.md](course_os_kernel_alpha_linux_compat_plus_design.md)
的长期 plus 边界，也不承担实时进度记录。当前进展以
[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 相关设计：
  - [course_os_gap_closure_boundary_design.md](course_os_gap_closure_boundary_design.md)
  - [course_os_kernel_alpha_linux_compat_plus_design.md](course_os_kernel_alpha_linux_compat_plus_design.md)
- 已完成计划：
  - [../plan/history_plan.md#course-os-plus-external-validation-plan](../plan/history_plan.md#course-os-plus-external-validation-plan)
- 外部参考：
  - [testsuits-for-oskernel](https://github.com/oscomp/testsuits-for-oskernel)

## 背景与问题

Stage 10/11 已经证明 Linux compat Plus 可以从 builtin 或 external rootfs 运行一组低风险
Linux 用户态路径，并在失败时输出 rootfs、path、loader、syscall trace 和 errno 诊断。
OSComp / `testsuits-for-oskernel` 适合作为课程展示后的扩展证据，但它覆盖的工具链、网络、
signal、futex、pthread 和大内存场景远大于 Stage 1-4 课程 OS 基线。

因此本轮只定义一个保守基础子集：用 host-only opt-in target 证明外部 rootfs 资产可解析、
基础 Linux 用户态程序能真实执行或 fail-closed 诊断清晰。缺外部资产时默认回归必须不受影响。

## 目标

- 提供一个不依赖网络、不依赖真实包管理器、不要求完整 signal / futex / pthread 的 OSComp
  基础验证入口。
- 把 rootfs、测试二进制、dynamic loader、shared library 和 testsuits checkout 的资产合同写清楚。
- 在缺资产、坏路径、缺 loader / shared object、unsupported syscall 或非零 exit 时输出可复查诊断。
- 保持 Linux compat Plus 与课程 Stage 1-4 基线分离。

## 非目标

- 不把 OSComp 通过情况写成 Stage 1 / Stage 2 / Stage 3 / Stage 4 完成条件。
- 不新增浏览器 external rootfs 运行入口。
- 不推进 Stage 12 / Stage 13，不处理网络 `git clone/push/pull`、完整 toolchain、完整
  signal / futex / pthread、完整 TTY/job control 或 `rustc`。
- 不把 `testsuits-for-oskernel` 全量测试矩阵接进默认 `make test`。
- 不在本轮要求真实包管理器、发行版 init 系统或完整 Linux 发行版兼容。

## 基础子集范围

基础子集只覆盖 low-risk userland smoke：

1. `busybox help/echo`
   - 候选命令：`linux /bin/busybox --help`、`linux /bin/busybox echo oscomp-basic`。
   - 验证目的：外部 rootfs provider、ELF loader、argv/envp/auxv、`write`、`exit_group` 和
     prompt 回归。
   - 不要求：真实 shell 脚本、复杂管道、job control、完整 termios。

2. `git help-run`
   - 候选命令：`git -h` 或 `git help`。
   - 验证目的：PATH fallback、`/usr/bin/git` resolved path、loader 诊断、help 输出或
     fail-closed errno / loader / syscall trace。
   - 不要求：`git init/add/commit/log`、网络 remote、SSH/TLS、credential、packfile 全流程。

3. 缺失文件和 unsupported path 诊断
   - 候选命令：`linux /oscomp-basic-missing`。
   - 验证目的：稳定输出 `path`、`errno=2`、`message=path: no such file` 并回到
     `course-os> ` prompt。

4. 只读文件 / 目录元数据
   - 仅作为后续可选扩展；本轮不强制接入。
   - 候选能力：`stat`、`getdents64`、`read/lseek/close`。

明确排除：

- `pthread` / `libpthread`、futex wait queue、signal delivery、process group。
- 网络、socket、DNS、SSH/TLS。
- 真实包管理器、完整 `gcc` toolchain 子进程链、`rustc`。
- 大规模 libc-test / lmbench / unixbench 之类需要宽 syscall 面或长耗时的矩阵。

## 资产合同

### 必需环境变量

- `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS`
  - 指向外部 rootfs 目录或 ext4 镜像。
  - 只能在显式 opt-in target 中使用。
  - 未设置时 host target 必须清晰 `SKIP`，并退出 0，证明默认回归不依赖外部资产。
  - 设置但路径无效、无法读取、缺 required asset 或 ext4 读取工具缺失时，asset 生成必须
    fail-closed 并返回非 0。

### 可选环境变量

- `MYCPU_OSCOMP_TESTSUITS`
  - 指向本机 `testsuits-for-oskernel` checkout。
  - 本轮 basic smoke 不要求该 checkout；未设置时只打印 `optional not set`。
  - 如果设置但路径不存在，host smoke 应 fail-closed，避免把拼错的路径误当成已验证资产。

- `MYCPU_OSCOMP_BASIC_COMMAND_MAX_STEPS`
  - 覆盖单条 shell 命令的 guest step budget。
  - 无效值必须 fail-closed。

### Required guest assets

- `/bin/busybox`
- `/usr/bin/git`

这两个文件由 host-only OSComp basic asset provider 作为 required path 提取。缺失时不进入
guest 执行，直接在生成阶段 fail-closed。

### Optional / derived assets

- dynamic loader：
  - `/lib/ld-musl-riscv64.so.1`
  - `/lib64/ld-linux-riscv64-lp64d.so.1`
- shared libraries：
  - `/lib/libc.so.6`
  - `/lib/libgcc_s.so.1`
  - `/usr/lib/libstdc++.so.6`
  - required ELF `DT_NEEDED` 指向的 shared object 由 asset 工具按 rootfs library path
    自动收集；找不到时 fail-closed。

optional loader 缺失不阻止 asset manifest 生成，但对应程序真实运行时必须输出 loader /
errno 诊断，不得伪造成功。

## Host-Only Smoke Contract

显式 target：

```sh
cd myCPU && MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=/path/to/rootfs \
  make test-host-course_os_oscomp_basic_smoke
```

可选：

```sh
MYCPU_OSCOMP_TESTSUITS=/path/to/testsuits-for-oskernel
MYCPU_OSCOMP_BASIC_COMMAND_MAX_STEPS=1200000000
```

行为：

- 未设置 `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS`：打印 `SKIP`、所需变量和可选变量，不生成
  external rootfs provider，不构建 guest generated ELF，退出 0。
- 设置 rootfs：生成 host-only OSComp basic provider 和 `guest/generated/course_os_oscomp_basic_shell.elf`。
- 运行时输出：
  - resolved host rootfs path。
  - optional testsuits path 或 `not set`。
  - 每条 guest command。
  - `linux-compat: rootfs=external`。
  - resolved guest path，例如 `/bin/busybox`、`/usr/bin/git`。
  - `loader=`、`interp=`、`exec=real`、`trace_count=`、`last=`、`/errno=`、`exit=`。
  - 缺文件时的 `errno=2` 和 `message=path: no such file`。
  - unsupported syscall 或 loader 缺口时的 `errno`、`last_error` / trace 诊断。

## 验收口径

### 程序真实执行通过

满足全部条件才记为真实执行通过：

- 目标命令经 Linux compat Plus 路径解析到 expected guest path。
- 输出包含 `rootfs=external`、`exec=real`、`trace_count=` 和 `exit=0`。
- 对应 user-visible 输出出现，例如 BusyBox usage、`oscomp-basic` 或 `usage: git`。
- 回到同一个 `course-os> ` prompt。

### Fail-Closed 输出可诊断原因

以下情况不记为程序通过，但可作为本轮有效诊断证据：

- 缺 rootfs env：target skip。
- required asset 缺失：asset 生成 fail-closed。
- dynamic loader / shared object 缺失：输出 loader / errno 诊断并回到 prompt。
- unsupported syscall：输出 syscall、errno、PC / trace 或 `last_error` 诊断并回到 prompt。
- 缺失 guest path：输出 resolved path、`errno=2` 和 `message=path: no such file`。

### 不在本轮支持范围

遇到以下需求时不得扩展本轮目标，应另起计划：

- 全量 `testsuits-for-oskernel`。
- 网络 git、真实包管理器、完整 toolchain、`rustc`。
- 完整 signal / futex / pthread。
- 浏览器 external rootfs 运行入口。

## 风险与取舍

- 外部 rootfs 的具体动态链接器和 shared library layout 会漂移；本轮用 manifest 和
  fail-closed 诊断吸收差异，不把某个发行版布局写成默认门禁。
- `git -h` 在不同 rootfs 上可能走成功输出，也可能被 loader / syscall 缺口挡住；basic smoke
  接受清晰 fail-closed 诊断，但不会把它描述成 OSComp 通过。
- 新 target 不进入默认 `make test`，避免本机资产、`debugfs`、rootfs 大小或外部 checkout
  污染默认回归。

## 当前有效性说明

- 当前有效：本文档是 Course OS / Linux compat Plus 对接 OSComp 基础外部验证的长期边界。
- 当前结果以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 和
  [../plan/history_plan.md#course-os-plus-external-validation-plan](../plan/history_plan.md#course-os-plus-external-validation-plan)
  为准。
