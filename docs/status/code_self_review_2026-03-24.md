# 代码自检报告（2026-03-24）

## 文档定位

本文档保留 `2026-03-24` 那次系统性自检的核心结论，以及后续修复进展。

它现在不再追求保留当时的完整逐项审查过程，而是作为：

- 一份关键历史问题摘要
- 一份当前仍有效的风险跟踪文档

## 当时审查的总体结论

当次审查覆盖 simulator 参考执行路径、CSR / 特权 / MMU、平台设备、加载路径、guest runtime / VM / trap 以及文档状态。

核心结论是：

- 仓库已经具备比较明确的 Phase 1 基础骨架。
- 但当时仍有若干问题已经触及 reference model 的 correctness 底线。
- 此外还存在一批会在 OS / kernel bring-up 继续推进时放大的结构性风险。

## 当时识别出的关键问题

### 已被确认的高优先级问题

- 非法整数保留编码被误执行。
- `DIV/REM/DIVW/REMW` 的 `INT_MIN / -1` 边界会触发宿主未定义行为。
- ELF loader 对纯 BSS `PT_LOAD` 段处理不完整。
- bus / device 接口对非法访问宽度和区间冲突缺少足够防御。

### 当时已识别出的结构性风险

- guest 侧 `vm.c`、`trap.c`、smoke 文件体量过大。
- 一批固定上限常量在早期 bring-up 尚可接受，但会成为后续 kernel 扩展的限制。
- `SimpleStorage` 仍然只是最小同步块设备。
- reference robustness 回归还不够系统。

## 后续修复进展（更新到 2026-03-25）

以下问题已经完成修复或第一轮收口：

- 非法整数保留编码现在会稳定进入 `illegal instruction`，并已补回归：
  - `tests/asm/illegal_integer_encodings.S`
- `DIV/REM/DIVW/REMW` 的 `INT_MIN / -1` 边界不再依赖宿主未定义行为，并已补回归：
  - `tests/asm/muldiv_edge_cases.S`
- ELF loader 现在支持 pure-BSS `PT_LOAD` 的 `zero-fill`，并已补单元回归：
  - `tests/unit/elf_loader_bss.cpp`
- `Bus::attach()` 会拒绝设备区间重叠，UART / PLIC / CLINT / `SimpleStorage` 已收紧第一轮访问宽度约束，并已补单元回归：
  - `tests/unit/bus_device_guards.cpp`

## 与本次自检直接相关的后续状态

第 `9` 节之后提到的“第一次真正的小 kernel bring-up”已经开始落地，并形成了当前可回归的 `kernel_alpha` 基线：

- `guest_kernel_alpha_demo`
  - 当前输出 `KMVPETDS`
  - 覆盖 boot、PMM、自建 Sv39、内核显式映射、UART / CLINT / PLIC / storage lazy map、一次 supervisor external interrupt、一次 timer interrupt、一次 storage readiness probe 和一次 block read
- `guest_kernel_alpha_fault_demo`
  - 当前输出 `KMVX`
  - 覆盖“VM 已开启但 CLINT 未映射”时的 fault / panic 路径
- `guest_kernel_alpha_storage_no_media_demo`
  - 当前输出 `KMVNX`
  - 覆盖“VM 已开启且 storage MMIO 可达、但未附加镜像”时的 metadata / `NO_MEDIA` error 合同
- `guest_kernel_alpha_storage_bad_block_count_demo`
  - 当前输出 `KMVBX`
  - 覆盖“VM 已开启且 storage 已附加”时的 `BAD_BLOCK_COUNT` / clear-error 合同
- `guest_kernel_alpha_storage_lba_range_demo`
  - 当前输出 `KMVLX`
  - 覆盖“VM 已开启且 storage 已附加”时的 `LBA_RANGE` / clear-error 合同
- `guest_kernel_alpha_storage_bad_command_demo`
  - 当前输出 `KMVCX`
  - 覆盖“VM 已开启且 storage 已附加”时的 `BAD_COMMAND` / clear-error 合同
- `guest_kernel_alpha_plic_not_ready_demo`
  - 当前输出 `KMVPX`
  - 覆盖“VM 已开启且 UART / CLINT / PLIC MMIO 可达，但 PLIC 未初始化”时的 device readiness timeout / panic 合同
- `guest_kernel_alpha_timer_not_ready_demo`
  - 当前输出 `KMVPETX`
  - 覆盖“VM 已开启且 UART / CLINT / PLIC MMIO 可达、第一次 external interrupt 已成功到达，但未安排第一次 timer delivery”时的 device readiness timeout / panic 合同

截至本次状态更新：

- `guest_supervisor_demo` 输出 `KRN`
- `make test` 保持通过

## 当前仍然有效的风险点

虽然当次自检里的 correctness 底线问题已经基本处理，但以下风险仍然有效：

- [myCPU/guest/kernel/vm.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm.c)
  仍承担过多职责，后续继续扩 process / address-space 管理时会先承压。
- [myCPU/guest/kernel/trap.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap.c)
  仍同时承担 dispatch、policy 与 runtime lifecycle。
- `SimpleStorage`
  仍是最小同步块设备：无 completion interrupt、`BLOCK_COUNT` 仅支持 `1`、写入不回写宿主文件。
- reference robustness 回归仍可继续扩展：
  - 非法编码矩阵
  - MMIO 非法偏移 / 宽度
  - 更真实的 ELF 段布局
- 一批固定上限常量在第一次真正的小 OS / kernel bring-up 前仍应优先关注。

## 当前使用方式

阅读本文件时，请把它当作：

- 关键历史问题摘要
- 当前剩余风险提示

而不是一份“所有条目都仍未修复”的实时问题清单。
