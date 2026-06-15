# 课程 OS Plus / 外部验证计划

> **文档状态：** 候选后续计划，默认回归不执行

## 文档定位

本文档承接原单体缺口收口计划中的 OSComp / testsuits-for-oskernel 验证项。该项属于 Linux compat Plus 的外部验证，不属于 Stage 1-4 课程 OS 基线补洞。

## 关联文档

- 边界设计：[../design/course_os_gap_closure_boundary_design.md](../design/course_os_gap_closure_boundary_design.md)
- Linux compat Plus 设计：[../design/course_os_kernel_alpha_linux_compat_plus_design.md](../design/course_os_kernel_alpha_linux_compat_plus_design.md)
- 课程要求：[../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)
- 当前状态：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)

## 目标

把 testsuits-for-oskernel / OSComp 基础用例整理成 opt-in、可诊断、可跳过的 Linux compat Plus 验证入口，用于课程展示后的扩展证据。

## 验证层级

- 默认门禁：
  - `git diff --check`
  - 仅验证 opt-in target 在缺资产时给出清晰 skip / fail-closed 诊断。
- Slow guest 门禁：
  - 无默认 slow guest。
- Opt-in external 门禁：
  - `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=/path/to/rootfs cd myCPU && make test-host-course_os_oscomp_basic_smoke`
  - 如需 testsuits-for-oskernel checkout，必须显式声明路径变量，例如 `MYCPU_OSCOMP_TESTSUITS=/path/to/testsuits-for-oskernel`。

## 完成定义

- 缺外部 rootfs 或 testsuits checkout 时，默认回归不失败。
- 有外部资产时，host smoke 能运行选定的 OSComp 基础子集，并输出失败原因、缺失 syscall 或缺失文件。
- 不把 OSComp 通过情况写成 Stage 1-4 基线已完成条件。
- 不新增浏览器 external rootfs route；前端最多展示 host-only manifest 或执行说明。

## 任务

### 任务 1：OSComp 基础子集调研（G12）

**文件：**
- 新增或修改：`docs/design/course_os_oscomp_external_validation_design.md`

- [ ] **步骤 1：选定范围。** 从 testsuits-for-oskernel 中筛选不需要网络、不需要完整 signal / futex / pthread、不需要真实包管理器的基础用例。
- [ ] **步骤 2：记录资产合同。** 写明 rootfs、测试二进制、动态 loader、环境变量和缺资产行为。
- [ ] **步骤 3：定义验收口径。** 区分“程序真实执行通过”“fail-closed 输出可诊断原因”和“不在本轮支持范围”。

### 任务 2：host-only opt-in smoke

**文件：**
- 新增：`myCPU/tests/host/course_os_oscomp_basic_smoke.cpp`
- 修改：`myCPU/Makefile`
- 可选修改：`frontend` manifest 暴露 host-only 说明，不新增浏览器运行入口

- [ ] **步骤 1：补缺资产路径测试。** 未设置 rootfs / testsuits 路径时，target 应清晰跳过或 fail-closed，不污染默认 `make test`。
- [ ] **步骤 2：实现 host smoke。** 复用现有 `course-os> linux ...` / Linux compat helper，运行选定基础用例。
- [ ] **步骤 3：输出诊断。** 失败时报告 resolved path、exit code、unsupported syscall 或缺失文件。
- [ ] **步骤 4：验证。** 缺资产运行一次默认跳过；有资产时运行 `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=/path/to/rootfs cd myCPU && make test-host-course_os_oscomp_basic_smoke`。

### 任务 3：展示材料归档

**文件：**
- 可选修改：`docs/showcase/*`
- 修改：`docs/status/kernel_alpha_status.md`

- [ ] **步骤 1：记录 Plus 证据。** 如果 OSComp 基础子集跑通，只在 Plus 小节记录为外部验证证据。
- [ ] **步骤 2：保留边界。** 展示材料中明确 Stage 1-4 不依赖 OSComp；OSComp 是 plus line。
- [ ] **步骤 3：回写状态。** 更新 status 的当前 Plus 能力与剩余风险。

## 完成态回写要求

- 完成后追加 [history_plan.md](history_plan.md)，删除本计划。
- [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 只记录当前可复验证据、资产依赖和剩余风险。
