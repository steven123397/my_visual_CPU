# 代码自检报告（2026-03-24）

## 1. 文档目的

本文档用于整理 2026-03-24 对 `myCPU` 仓库进行的一次全面代码自检结果，覆盖以下 4 个方向：

1. 理论层面是否存在错误，尤其是 CPU / Trap / Privileged / MMU 语义问题。
2. 项目文件架构和代码实现逻辑是否存在明显缺陷。
3. 是否存在潜在漏洞、疑点，或者会在后续开发计划中暴露的问题。
4. 是否存在过于臃肿的实现，或者已经废弃、被替代但未清理的代码和文件。

本次审查基线对应仓库最新提交：

- `c98bdf2` `refactor(guest runtime): 收口 supervisor demo smoke 编排边界`

## 2. 审查范围与验证方式

本次审查覆盖以下模块：

- simulator 参考执行路径：`src/cpu.cpp`、`src/exec/*`、`src/trap.cpp`
- CSR / 特权 / MMU：`src/arch/csr_file.cpp`、`src/mem/address_space.cpp`
- 平台设备与加载路径：`src/mem/bus.cpp`、`src/devices/*`、`src/loader/*`
- guest runtime / VM / trap：`guest/kernel/vm.c`、`guest/kernel/trap.c`、`guest/kernel/user_program_smoke.c`、`guest/kernel/supervisor_demo_smoke.c`
- 文档与仓库状态：`AGENTS.md`、`CLAUDE.md`、`readme.md`

验证方式包括：

- 静态代码审查
- 现有回归运行：`make test`
- 最小化临时复现实验，用于确认“看起来可疑”的点是否真会表现为错误

## 3. 总体结论

整体上，仓库已经形成了比较清晰的 Phase 1 基础骨架：

- `Machine / Bus / Ram / Device / Loader / TrapController / AddressSpace` 的主边界是成立的
- 当前回归集可以稳定覆盖一批 ISA、特权、设备和 guest bring-up 路径
- 最近一轮 `supervisor_demo_smoke` / `user_program_smoke` 的对外 orchestration 收口方向是正确的

但是，本次审查发现有 2 个问题已经触及“ISA 参考模型正确性”底线，属于必须优先修复的问题：

- 非法整数编码被误当作合法指令执行
- 有符号除法溢出边界会触发宿主未定义行为，甚至直接崩掉模拟器

除此之外，还存在若干结构性风险：

- ELF 装载路径对纯 BSS `PT_LOAD` 段处理不完整
- 总线 / 设备接口对非法访问宽度和地址冲突缺少防御
- guest 侧 `vm.c`、`trap.c`、smoke 实现文件已经明显偏大
- 当前有不少固定上限常量，短期可用，但在第一次真正的小 OS / kernel bring-up 时容易先成为限制条件

## 4. 必须修复的问题

### 4.1 非法整数编码未被稳定判为 illegal instruction

#### 位置

- `myCPU/src/exec/integer_ops.cpp`

重点包括但不限于：

- `0x13` 路径对 shift-immediate 编码的合法性检查不完整
- `0x33` 路径对 `funct7` 的合法性检查不完整
- `0x3B` 路径对 `funct7` 的合法性检查不完整

#### 问题描述

当前整数执行器对部分指令只检查了“会影响具体行为的几个位”，但没有把保留编码整体收紧为“非法指令”。结果是：

- 某些本应触发 `illegal instruction` 的编码
- 会落入 `ADD/SUB/SRL/SRA` 或相关整数路径
- 从 guest 视角看，CPU 语义不再是严格的 RISC-V ISA 参考模型

这类问题比“缺功能”更严重，因为它会制造“静默错误”：

- 程序不会 trap
- 测试也可能不容易第一时间发现
- 后续做 differential testing 时会出现难定位的不一致

#### 已确认的复现现象

通过临时构造一个带非法 `funct7` 的 R-type ELF 进行验证，结果没有进入 `illegal instruction trap`，程序直接走到了失败路径，输出 `X`。

这说明问题不是理论猜测，而是当前实现中的真实行为缺口。

#### 影响

- 破坏 ISA 级 reference model 的可信度
- 会污染后续特权、MMU、OS bring-up 调试
- 会在以后引入差分测试时放大定位成本

#### 修复建议

- 对 `0x13 / 0x33 / 0x3B` 路径按 `funct3 + funct7 + shamt 高位约束` 建立严格合法表
- 所有未落入合法表的编码一律进入 `illegal instruction`
- 为非法 `funct7`、非法 W-shift immediate、非法保留 system/integer 编码补专门回归

### 4.2 有符号除法溢出边界会触发宿主未定义行为

#### 位置

- `myCPU/src/exec/integer_ops.cpp`

重点包括：

- `DIV`
- `REM`
- `DIVW`
- `REMW`

#### 问题描述

当前实现直接使用宿主 C++ 的有符号除法和取模。例如：

- `INT64_MIN / -1`
- `INT32_MIN / -1`

在 RISC-V 规范里，这类边界结果是有定义的：

- `DIV` / `DIVW` 应返回被除数本身
- `REM` / `REMW` 应返回 `0`

但在 C / C++ 宿主语义里，这类运算会触发未定义行为。

#### 已确认的复现现象

通过临时构造包含 `INT64_MIN / -1` 的最小 ELF 进行验证，模拟器直接异常退出，退出码为 `136`。

这说明 guest 指令当前可以直接把宿主模拟器打崩。

#### 影响

- 直接破坏“正确且可调试”的参考执行器定位
- 使某些正常的 guest 程序可触发 host 崩溃
- 会给后续 OS bring-up 带来非常高的调试不确定性

#### 修复建议

- 在 `DIV / REM / DIVW / REMW` 进入宿主算术前，显式处理：
  - 除零
  - `INT_MIN / -1`
- 不依赖宿主 UB
- 为 `RV64` 与 `RV64W` 两套路径分别补充溢出边界测试

## 5. 建议修改的问题

### 5.1 ELF loader 对纯 BSS `PT_LOAD` 段处理不完整

#### 位置

- `myCPU/src/loader/elf_loader.cpp`

#### 问题描述

当前 loader 在 `p_type == PT_LOAD && p_filesz == 0` 时会直接跳过整个段。

这意味着如果以后内核或用户程序出现以下布局：

- 某个 `PT_LOAD` 段完全是 `NOBITS`
- `p_memsz > 0`
- `p_filesz == 0`

则该段不会被显式清零，也不会被正确建段。

#### 风险

- 当前简化样例可能暂时没踩到
- 一旦链接脚本更贴近真实内核布局，这会变成实际 bring-up 问题

#### 修复建议

- `PT_LOAD` 判断应以 `p_memsz` 为主，而不是以 `p_filesz != 0` 为主
- 当 `p_filesz == 0 && p_memsz > 0` 时，仍应对目标内存做 `zero-fill`

### 5.2 总线 / 设备边界缺少非法访问防御

#### 位置

- `myCPU/src/mem/bus.cpp`
- `myCPU/src/devices/uart16550.cpp`
- `myCPU/src/devices/plic.cpp`
- `myCPU/src/devices/simple_storage.cpp`

#### 问题描述

当前实现更像“默认 guest 会按规矩访问”，而不是“平台自身把非法访问拦住”。例如：

- `Bus::attach()` 不检查设备地址区间重叠
- `find_device()` 采用简单顺序匹配
- 多数设备对 `size` 参数基本不做严格约束
- 非法宽度访问往往只是返回 `0` 或静默忽略

#### 风险

当前测试能过，主要是因为：

- 访问模式受控
- 平台设备数量少
- 驱动都按当前约定工作

但后续一旦：

- 增加新设备
- 扩展存储协议
- 引入更复杂 guest 驱动

这类“接口没守住边界”的实现会更容易引入顺序依赖 bug 和静默错误。

#### 修复建议

- `Bus::attach()` 时检测地址区间重叠
- 设备侧对合法访问宽度做白名单校验
- 非法 MMIO 访问尽量形成一致的可观察行为，而不是静默吞掉

### 5.3 guest 侧核心文件已经出现职责过载

#### 位置

- `myCPU/guest/kernel/vm.c`
- `myCPU/guest/kernel/trap.c`
- `myCPU/guest/kernel/user_program_smoke.c`
- `myCPU/guest/kernel/supervisor_demo_smoke.c`

#### 现状

文件体量已经很高：

- `vm.c` 约 1900+ 行
- `trap.c` 约 700+ 行
- `user_program_smoke.c` 约 1100+ 行
- `supervisor_demo_smoke.c` 约 600+ 行

虽然最近 public API 已经明显收口，但内部实现仍然堆在同一个文件中。

#### 风险

这类文件在当前阶段还可以维护，但已经接近“继续加功能就会明显恶化”的临界点。后续会优先暴露在以下场景：

- process / runtime refinement
- 更完整的 fault policy
- 新的 user interrupt 类别
- 真正的小内核 bring-up

#### 修复建议

- `vm.c` 优先拆分为：
  - address_space
  - process
  - object / region
  - page_fault / fault_policy
- `trap.c` 拆分为：
  - trap dispatch
  - trap context / policy
  - trap user runtime lifecycle
- smoke 文件继续按 lifecycle / active / fault / interrupt / platform-tail 细化

## 6. 潜在风险与后续容易暴露的问题

### 6.1 资源与对象上限是硬编码的

当前 guest 侧存在多处固定上限，例如：

- `VM_MAX_ADDRESS_SPACES = 2`
- `VM_MAX_USER_REGIONS = 16`
- `VM_PROCESS_MAX_USER_REGIONS = 8`
- `VM_MAX_FAULT_ACTIONS = 16`
- `TRAP_MAX_INTERRUPT_CAUSE = 16`
- `TRAP_MAX_EXCEPTION_CAUSE = 16`

这些上限在当前 smoke/demo 阶段是可接受的，但第一次真正做小 OS / kernel bring-up 时，很可能先暴露为结构限制，而不是实现 bug。

建议后续至少做到：

- 在文档中明确哪些是“当前 bring-up 上限”
- 在失败路径上尽量可观察
- 为以后从固定池演进到更通用容器留边界

### 6.2 `SimpleStorage` 仍是最小同步块设备

当前 `SimpleStorage` 仍有明显阶段性限制：

- `BLOCK_COUNT` 只能是 `1`
- 没有 completion interrupt
- 写入不回写宿主文件

这不算当前 bug，但它会影响后续文件系统、块缓存和更真实驱动模型的设计空间。

### 6.3 现有测试更多是在验证“已接通路径”，不是在做鲁棒性防御验证

当前回归做得已经不错，但覆盖重点仍然是：

- 正向功能路径
- 一部分边界 trap / page fault / delegation 语义

还缺少几类更容易暴露 reference model 弱点的测试：

- 非法编码矩阵
- 更系统的 MMIO 非法宽度 / 非法偏移访问
- M 扩展算术溢出边界
- 更真实 ELF 段布局

## 7. 臃肿、废弃代码与文件清理判断

### 7.1 暂未发现明确“已被替代但仍保留”的 tracked 源文件

本轮没有发现明显属于以下类型的 tracked 源码：

- 功能已经完全迁移，但老版本文件仍保留且还在编译
- public API 已替换，但旧实现完全无人使用
- 完整重复实现未清理

当前更大的问题不是“废弃文件未删”，而是：

- 核心实现仍偏集中
- 需要继续按职责拆小，而不是继续堆功能

### 7.2 当前工作区存在未跟踪构建产物

当前唯一明确需要注意的文件是：

- `myCPU/guest/supervisor_demo.elf`

它是构建产物，不应作为源码长期留在工作区或进入版本库。

## 8. 已完成的核验结论

### 8.1 现有回归状态

本次审查时，以下验证已通过：

- `make test`

结果包括：

- 全部汇编回归通过
- `guest_supervisor_demo` 输出 `KRN`

这说明当前问题主要属于：

- 现有测试尚未覆盖的 correctness 缺口
- 或者结构性风险

而不是“现有回归已经红灯但尚未处理”的状态。

### 8.2 文档一致性

本次审查时，以下核验通过：

- `cmp -s AGENTS.md CLAUDE.md`

结果为：

- `SAME`

## 9. 建议的处理优先级

建议后续按以下顺序修复：

1. 修 `integer_ops.cpp` 中的 illegal-encoding 判定与 `DIV/REM` 溢出边界。
2. 为上述两类问题补最小回归，确保 reference model 不再静默错误或被 guest 打崩。
3. 修 ELF loader 对纯 BSS `PT_LOAD` 的处理。
4. 加强 `Bus` / `Device` 边界防御，避免平台扩展后出现静默错误。
5. 继续拆分 guest 侧 `vm.c` 和 `trap.c`，把当前收口工作真正落到“实现层也更清晰”。

## 10. 简要结语

这次自检的结论不是“项目整体设计有根本性错误”，而是：

- 主方向是对的
- 当前结构重构也在往正确方向走
- 但 reference model 仍有两处必须尽快修的 correctness 问题
- 同时 guest runtime 已经到了需要继续做实现层拆分的阶段

如果后续要继续沿着 “Phase 1 小 OS / kernel bring-up” 推进，那么上述问题中最优先的仍然是：

- 保证 CPU 语义正确
- 保证模拟器不会被正常 guest 指令打崩
- 然后再继续扩功能
