# course_os_kernel_alpha Stage 10 OSComp help-run 计划

> **文档状态：** 执行中

## 文档定位

本文档记录 `kernel_alpha` Linux compat Stage 10 的活跃执行计划。

Stage 10 的目标不是承诺跑完 `testsuits-for-oskernel` 全部用例，而是把 Stage 9 已完成的
显式 `course-os> linux ...` 静态 ELF real-exec 闭环推进到 OSComp Linux 用户态工具的
help-run 基线：能从 `course-os> ` 直接或显式加载目标工具，完成帮助输出或等价 usage
输出，并回到同一个 prompt。

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
  - [history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan](history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan)
  - [history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan](history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan)
  - [history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan](history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan)

## 目标

- 建立 OSComp Linux 用户态 help-run 基线，优先覆盖：
  - `git -h`
  - `git help`
  - `vim -h`
  - `gcc --h`
  - `rustc -h`
- 让直接命令 fallback 只在课程 shell 内置命令和 Stage 3 课程用户程序均未命中后触发。
- 从外部 rootfs/provider 读取真实 RV64 Linux ELF 和必要 interpreter / shared assets，不回退到固定 help 字符串。
- 支持 help-run 所需的 `PT_INTERP` / dynamic-loader v0 路径；只补真实 trace 证明需要的最小 syscall。
- 保持 Stage 1 / Stage 2 / Stage 3 marker、Stage 4 `course-os> ` prompt、Stage 5-9 Linux compat guardrail 和旧 9 条负向 demo 稳定。

## 非目标

- 不承诺跑完 `testsuits-for-oskernel` 全部用例。
- 不在 Stage 10 完成 `git init/add/commit/log`。
- 不在 Stage 10 完成 `git clone/push/pull`、网络栈或远端认证。
- 不在 Stage 10 完成 `vim hello.c` 的交互编辑保存。
- 不在 Stage 10 完成 `gcc hello.c && ./a.out` 或 `rustc helloworld.rs && ./helloworld`。
- 不声明完整动态链接器、完整 Linux syscall 面、完整 signal / futex、完整 TTY / termios 或可写 rootfs 语义。
- 不把 Linux syscall 直接塞进课程 `course_syscall`，也不改变课程 shell / 课程用户程序的 ABI。

## 完成定义

- `course-os> git -h` 和 `course-os> git help` 通过 Linux rootfs PATH fallback 启动真实 Linux compat 进程，输出 help / usage 证据并回到 `course-os> `。
- `course-os> vim -h`、`course-os> gcc --h`、`course-os> rustc -h` 至少完成真实 ELF 加载、必要 interpreter v0、真实 syscall trace 和 fail-closed 或正常退出闭环；如果工具自身对参数返回 usage/error，仍必须证明是工具真实执行后的输出，不是 host-side 固定文本。
- 显式 `linux /path ...` launcher 继续可用，坏路径、坏 ELF、未支持 syscall 和直接 fallback 歧义都输出可诊断原因。
- 新增 Stage 10 host smoke 固定上述命令矩阵，检查命令后的 UART 增量、`exec=real` 或等价 real-exec 诊断、syscall trace 和 prompt 回归。
- 默认回归仍通过 `make test`、`make test-pipeline` 和 `git diff --check`。

## 任务

### 任务 1：OSComp rootfs 资产面

**文件：**
- 修改：`myCPU/tools/linux_compat_rootfs_asset.py`
- 修改：`myCPU/guest/include/linux_compat_rootfs.h`
- 修改：`myCPU/guest/kernel/linux_compat_rootfs_builtin.c`
- 修改：`myCPU/tests/host/linux_compat_rootfs_asset_test.py`
- 可能生成：`myCPU/guest/generated/linux_compat_rootfs_asset.c`
- 可能生成：`myCPU/guest/generated/linux_compat_rootfs_asset.json`

- [ ] **步骤 1：补 host generator 红灯**
  - 在 `linux_compat_rootfs_asset_test.py` 增加覆盖 `/usr/bin/vim`、`/usr/bin/gcc`、`/usr/bin/rustc` 和 interpreter / shared asset manifest 的用例。
  - 运行：`cd myCPU && python3 -m unittest tests.host.linux_compat_rootfs_asset_test`
  - 预期：新增路径或 manifest 字段缺失导致 FAIL。
- [ ] **步骤 2：扩展 asset/provider 合同**
  - 让 generator 能把 Stage 10 required tools 与 optional interpreter/shared assets 写入 provider 与 JSON manifest。
  - 保持 builtin provider 不依赖外部 rootfs；没有外部资产时必须 fail-closed，而不是伪造工具存在。
- [ ] **步骤 3：验证资产面**
  - 运行：`cd myCPU && python3 -m unittest tests.host.linux_compat_rootfs_asset_test`
  - 运行：`cd myCPU && make test-unit-course_os_stage7_linux_compat`
  - 预期：generator/provider 合同 PASS，builtin / external source 名称和 missing optional 诊断稳定。

### 任务 2：直接命令 fallback 与 PATH 解析

**文件：**
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/guest/include/linux_compat.h`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/tests/unit/course_os_stage5_linux_compat.c`
- 修改：`myCPU/tests/host/course_os_linux_compat_terminal_smoke.cpp`

- [ ] **步骤 1：补 fallback 红灯**
  - 增加直接 `git -h` / `git help` fallback 测试，同时固定课程命令优先级。
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
  - 预期：当前直接 `git -h` 仍报错，新增测试 FAIL。
- [ ] **步骤 2：实现 PATH fallback v0**
  - 解析顺序固定为：课程内置命令 -> Stage 3 课程用户程序 -> 显式 `linux ...` -> Linux rootfs PATH fallback。
  - PATH v0 只查 `/bin`、`/usr/bin` 和 plan 中列出的工具，不做完整 shell expansion。
  - fallback 命中后复用 `linux_compat_run()`，并保留 `exec=real` / trace 诊断。
- [ ] **步骤 3：验证 fallback 不污染课程 shell**
  - 运行：`cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - 运行：`cd myCPU && make test-unit-course_os_stage5_linux_compat`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
  - 预期：课程命令语义不变，直接 `git -h` / `git help` 进入 Linux compat。

### 任务 3：动态链接器 v0 / `PT_INTERP` real-exec

**文件：**
- 修改：`myCPU/guest/include/linux_compat_loader.h`
- 修改：`myCPU/guest/kernel/linux_compat_loader.c`
- 修改：`myCPU/guest/include/linux_compat_exec.h`
- 修改：`myCPU/guest/kernel/linux_compat_exec.c`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/tests/unit/course_os_stage8_linux_compat_loader.c`
- 修改：`myCPU/tests/unit/course_os_stage9_linux_compat_exec.c`

- [ ] **步骤 1：补 interpreter execution 红灯**
  - 增加动态 ELF + `PT_INTERP` 的 load / stack / entry 测试，要求记录 main ELF、interpreter path、load bias、auxv 和拒绝原因。
  - 运行：`cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
  - 预期：当前只做 load-plan 诊断，真实 interpreter 入口闭环 FAIL。
- [ ] **步骤 2：实现 dynamic-loader v0**
  - 对 `PT_INTERP` 路径查 rootfs provider，映射 interpreter 和 main ELF 的 `PT_LOAD` 段。
  - 构建 help-run 所需 auxv：`AT_PHDR`、`AT_PHENT`、`AT_PHNUM`、`AT_ENTRY`、`AT_BASE`、`AT_PAGESZ`、`AT_UID/GID`、`AT_SECURE`、`AT_RANDOM` 的最小可诊断策略。
  - 对 TLS、relocation、unsupported ABI 或缺失 interpreter fail-closed，不伪造成功。
- [ ] **步骤 3：验证 loader / exec 层**
  - 运行：`cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
  - 预期：static ELF Stage 9 绿灯保持，dynamic-loader v0 诊断和入口闭环通过。

### 任务 4：真实 trace 驱动 syscall 补洞

**文件：**
- 修改：`myCPU/guest/include/linux_compat.h`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/tests/unit/course_os_stage9_linux_compat_syscall.c`
- 创建或修改：`myCPU/tests/unit/course_os_stage10_linux_compat.c`

- [ ] **步骤 1：记录 Stage 10 命令 trace**
  - 先跑目标命令，保留第一个 blocking syscall 的 number、PC、path / fd / errno 和已执行 trace。
  - 每次只把真实 trace 中阻塞 help-run 的 syscall 加入 Stage 10 backlog。
- [ ] **步骤 2：按最小语义补 syscall**
  - 优先补只读、无全局副作用或可 fail-closed 的 syscall。
  - `mprotect`、`readlinkat`、`faccessat` / `access`、`uname`、`prlimit64`、`set_tid_address`、`set_robust_list`、`rt_sigaction`、`rt_sigprocmask`、`writev`、`pread64`、`statx` 等只能在 trace 证明需要后进入实现。
  - 每个 syscall 必须写清 Linux errno、用户指针校验、trace record 和 unsupported fallback。
- [ ] **步骤 3：验证 syscall 层**
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
  - 运行：`cd myCPU && make test-unit-course_os_stage10_linux_compat`
  - 预期：新增 syscall 只满足 help-run trace，不扩大到完整 POSIX 语义声明。

### 任务 5：Stage 10 host smoke 门禁

**文件：**
- 创建：`myCPU/tests/host/course_os_linux_compat_oscomp_help_smoke.cpp`
- 修改：`myCPU/Makefile`
- 修改：`frontend/server/tests_manifest.mjs`（仅当 manifest 需要新增 Stage 10 opt-in 入口）

- [ ] **步骤 1：新增 host smoke 红灯**
  - 固定从 `course-os> ` 输入命令后的 UART 增量，不匹配旧 transcript。
  - 命令矩阵至少包含 `git -h`、`git help`、`vim -h`、`gcc --h`、`rustc -h`。
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
  - 预期：当前 Stage 9 能力不足，新增 smoke FAIL。
- [ ] **步骤 2：接入 Makefile target**
  - target 名称固定为 `test-host-course_os_linux_compat_oscomp_help_smoke`。
  - 如依赖外部 rootfs，target 必须显式要求环境变量或生成资产，缺失时 fail-closed 并提示原因。
- [ ] **步骤 3：验证 Stage 10 smoke**
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
  - 预期：五条 help-run 命令均完成真实加载 / trace / prompt 回归。

### 任务 6：回归、状态回写与归档

**文件：**
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/course_os_kernel_alpha_stage10_oscomp_help_run_plan.md`

- [ ] **步骤 1：跑 Stage 10 固定门禁**
  - 运行：`cd myCPU && make test-unit-course_os_stage5_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage6_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_vm`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
  - 运行：`cd myCPU && make test-unit-course_os_stage10_linux_compat`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_minimal_elf_smoke`
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
- [ ] **步骤 2：跑全量回归**
  - 运行：`cd myCPU && make test`
  - 运行：`cd myCPU && make test-pipeline`
  - 运行：`git diff --check`
- [ ] **步骤 3：回写完成态**
  - 在 `kernel_alpha_status.md` 增加 Stage 10 完成摘要、关键历史节点、剩余限制和新的下一步。
  - 在 `history_plan.md` 追加 `course-os-kernel-alpha-stage10-oscomp-help-run-plan` 归档条目。
  - 删除本文档，确保 `docs/plan/` 不长期保留完成态 checklist。

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 必须更新：
  - Stage 10 完成结果摘要。
  - Stage 10 仍不支持的 testsuits 子项。
  - 后续 Stage 11+ 的下一步。
- [history_plan.md](history_plan.md) 必须追加：
  - 完成时间。
  - 完成内容。
  - 简短实现过程摘要。
  - 验证摘要。
- 本计划归档完成后删除，不再作为并行事实来源保留。
