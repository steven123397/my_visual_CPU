# 课程 OS 真实用户 ELF 来源设计

## 文档定位

本文档定义课程 OS Stage 3 用户程序和 `course_os_shell` 外部 ELF 加载的统一来源合同，对应
[课程 OS 架构后续增强计划](../plan/history_plan.md#course-os-arch-followup-plan)
中的真实课程 ELF 用户程序任务，以及
[../plan/project_evolution_priority_p1_plan.md](../plan/project_evolution_priority_p1_plan.md) 的
`course_os_shell` 外部 ELF 加载任务。

本文档不承担实时进度更新。当前完成范围以
[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。

## 关联文档

- 状态文档：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 相关计划：
  - [../plan/history_plan.md#course-os-arch-followup-plan](../plan/history_plan.md#course-os-arch-followup-plan)
  - [../plan/project_evolution_priority_p1_plan.md](../plan/project_evolution_priority_p1_plan.md)
- 边界设计：[course_os_gap_closure_boundary_design.md](course_os_gap_closure_boundary_design.md)
- 课程 OS 基线设计：[course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)
- Linux compat Plus 设计：[course_os_kernel_alpha_linux_compat_plus_design.md](course_os_kernel_alpha_linux_compat_plus_design.md)

## 背景与问题

课程 OS Stage 3 已经有教学级 `course_elf_loader`、`course_process_exec()` 和 5 个课程用户程序，
但早期 catalog 中多个程序共享同一个最小占位 ELF，loader 只能证明 ELF 框架存在，不能证明每个
程序都有独立 entry / `PT_LOAD` 视图。

同时，P1 计划要求 `course_os_shell` 能从受控文件源加载外部 RV64 ELF。这个能力应当复用课程
ELF / process 路径，而不是倒灌到 `linux_compat_*`，也不应把 host 任意路径、外部 rootfs 或
交叉编译器资产变成默认回归依赖。

## 目标

- 让 Stage 3 的 5 个课程用户程序使用不同的最小 RV64 `ET_EXEC` ELF bytes，loader 能观察到不同
  entry、code segment 和 data segment。
- 为内置 catalog ELF 和课程 FS 文件 ELF 提供同一条 `course_process_exec_image()` 装载路径。
- 支持 `course-os> exec /path/to/prog [arg]` 从课程 FS 读取受控 ELF 文件并执行。
- 保持 `exec hello`、直接 `hello`、课程 syscall ABI、Stage marker 和 Linux compat Plus 旁路边界不变。
- 默认验证不依赖本机交叉编译器、外部 rootfs、OSComp 资产或网络。

## 非目标

- 不执行 host 任意路径上的 ELF。
- 不把外部 Linux rootfs、OSComp 资产或真实发行版 rootfs 接入课程 ELF 默认路径。
- 不支持动态链接、`PT_INTERP`、Linux auxv / envp 真实 ABI 或通用 Linux 用户态兼容。
- 不把课程 ELF catalog 的语义迁移到 `linux_compat_*`，也不把 Linux compat syscall 面倒灌回
  `course_*` 教学模块。
- 不声明用户程序 bytes 已经由真实交叉编译器构建，默认资产仍是仓库内手写最小 ELF。
- 不声明 ELF 中的指令已经被真实 U-mode 执行；当前课程程序的可见输出仍由教学级 libc / syscall
  harness 模拟。

## 约束与边界

- 默认 ELF 资产采用仓库内手写最小 RV64 little-endian `ET_EXEC`，不依赖外部工具链。
- 外部 ELF 来源只接受课程 FS 中的绝对路径文件，例如 `/demo/ext.elf`。
- 第一刀只读取小型静态 ELF；当前 `course_os_shell` 对外部 ELF 文件设置 4096 字节上限。
- 缺文件、目录、空文件、超限文件、非 ELF、无有效 `PT_LOAD`、entry 不在 executable segment 内等情况
  都必须 fail-closed。
- 外部 ELF 装载失败不得修改目标进程当前 image；shell fork 出来的临时子进程也应被回收。

## 方案

### 结构设计

课程 ELF 来源分成两类 provider：

- 内置 catalog provider：`course_user_program_lookup()` 返回内置课程程序的 ELF bytes、entry 和教学级
  program kind。
- 课程 FS provider：`exec /path` 从 `course_fs` / `course_fd` 读取文件 bytes，再交给同一装载入口。

两类来源都汇入 `course_process_exec_image()`。`course_process_exec()` 继续保留为 catalog wrapper，
用于保持 `exec hello` 和直接 `hello` 的旧语义。

### 接口 / 数据 / 契约

`course_process_exec_image(table, pid, image_name, elf_image, elf_size, argv)` 的合同是：

- 先完整调用 `course_elf_loader_load()` 验证 ELF。
- 验证通过后才释放旧用户页、更新 process name / argv / entry / stack / maps / ABI / state。
- 验证失败返回 `COURSE_PROCESS_ERR_BAD_ELF`，不改写已有 process image。

`course_os_shell` 的外部 ELF 合同是：

- `exec /path/to/prog [arg]` 只在课程 FS 中解析 `/path/to/prog`。
- 成功后输出 `program=<path> entry=<hex> exit=<status>`，用于 host/unit 和 guest transcript 断言。
- 缺文件或目录输出 `exec: no such file`。
- 文件存在但 ELF 格式不合格输出 `exec: bad elf`。
- 失败命令会设置 structured command status 为 false，因此 `exec /bad && echo after` 不执行右侧命令。

### 验证思路

- `test-unit-course_os_stage3_elf` 断言 5 个 Stage 3 程序不共享同一段占位 ELF，且 loader 能看到不同
  entry / code segment。
- `test-unit-course_os_stage3_elf` 断言 `course_process_exec_image()` 能装载外部 bytes，坏 ELF 不修改
  当前 process image。
- `test-unit-course_os_stage2_shell` 断言 `exec /path` 能从课程 FS 加载合法 ELF，并覆盖缺文件、目录、
  非 ELF 和 `&&` 短路。
- guest smoke 继续验证 Stage 1 / Stage 2 / Stage 3 marker 和 `course-os> ` prompt 不漂移。

## 风险与取舍

- 手写最小 ELF 可让默认回归完全自包含，但它不是 toolchain 产物。后续若要展示真实编译产物，应另开
  opt-in 或显式工具链计划，并声明工具链变量和缺资产行为。
- 当前 shell 外部 ELF 仅证明课程 FS -> ELF loader -> process image 的合同，不证明完整用户态机器码
  执行。若未来要把课程程序改成真实 U-mode 指令执行，应另行设计 syscall / trap / libc 的执行边界。
- 外部 ELF 第一刀设置 4096 字节上限，优先降低 shell scratch buffer 和课程 FS 失败面的复杂度；后续可在
  引入分段读取和 page-backed image 后扩大。

## 当前有效性说明

- 当前有效。
- 本文档对应的当前结果以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。
