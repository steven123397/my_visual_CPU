---
title: "xv6 云端回归的工具链布局与墙钟预算差异"
date: 2026-10-01
problem_type: test_failure
component: validation
module: xv6
severity: medium
symptoms:
  - "Ubuntu 24.04 构建 CSR 指令时报 extension zicsr required"
  - "xv6 启动的固定 PC 和 profile 基线在新编译器下失败"
  - "xv6 shell 被 300 秒墙钟上限终止"
root_cause: "测试依赖旧工具链的隐式 ISA 扩展、编译布局及未测量的总执行预算"
resolution_type: code_fix
tags:
  - xv6
  - riscv-toolchain
  - github-actions
  - exact-baseline
  - timeout
---

# xv6 云端回归的工具链布局与墙钟预算差异

## Context

相同 vendored xv6 源码从本地 RISC-V GCC 10.2 转到 Ubuntu 24.04 / GCC 13.2 后，连续出现汇编、启动 checkpoint 和完整 shell 超时失败。模拟器语义未变，不能通过跳过 xv6 或放宽断言来消除这些信号。

## Root Cause

GCC 13 工具链要求显式声明 CSR / fence 扩展名称；本次旧 GCC 10 实际拒绝这些名称，也拒绝所尝试的 GCC `-misa-spec=2.2` 参数。只硬编码一组新参数会破坏旧环境。

两代编译的 memset 指令字节相同，但函数地址及启动路径计数不同。5000 步 checkpoint 均落在 `memset+0x24`；单独把 PC 或计数放宽会丢失原有门禁信息。

shell 启动需要约 4.3 亿 guest 步，主要在内存填充循环。完整序列在两代内核的本地诊断中分别耗时 508.384 / 470.412 秒；云端启动本身即 304.651 秒，超过原测试总上限。

## Guidance / Solution

- 按编译器能力探测扩展名称，并让默认 guest / xv6 参数共用结果。当前定义见 [Makefile](../../../myCPU/Makefile) 的 `myCPU/Makefile:298` 与 [board profile](../../../myCPU/workloads/boards/mycpu_virt.mk) 的 `myCPU/workloads/boards/mycpu_virt.mk:19`。
- 先比较同源码的符号和反汇编，再保留两套完整精确基线。[boot smoke](../../../myCPU/tests/host/xv6_boot_smoke.cpp) 的 `myCPU/tests/host/xv6_boot_smoke.cpp:54` 从生成的符号表读地址；缺失或未知布局失败。profile 测试要求整组 checkpoint 匹配，不混用新旧字段。
- 超时先观测阶段、PC 和内存填充地址是否推进，测完完整命令后调整有限墙钟上限。`myCPU/Makefile:275` 为 1200 秒；所有 guest 步数预算与断言保留，`myCPU/Makefile:827` 的 timeout 仍使 gate 失败，并保留已捕获日志。

## Why This Matters

编译器升级可以同时改变合法汇编参数、代码布局和运行耗时；这些失败需要分别解释。证明其中一项属于环境差异，不代表可以忽略其他项，也不能证明新编译器下所有语义正确。

## Prevention & Detection

[CI 元测试](../../../myCPU/tests/host/ci_targets_test.py) 实际编译 guest 与 xv6 参数下的 CSR / fence 指令。新增编译布局须重新审计精确 checkpoint，并实际执行 boot 与完整 shell；不得从被测输出自动生成期望。

技术版本 `f69ea75` 的 [完整 run 36797868411 / attempt 1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797868411/attempts/1) 成功，shell 全部 16 条命令含 forktest / stressfs 通过，阶段总耗时 535.740 秒。实际 1 秒覆盖实验仍使 Make 失败且保留启动日志。

本经验仅验证 GCC 10.2 和 Ubuntu GCC 13.2 的默认工具链组合；未知布局保持拒绝，自定义 guest 与 xv6 工具链跨代组合未验证。真实 Linux / OSComp / Spike 外部资产不由这些证据覆盖。现行验证契约见 [Wiki Verification](https://github.com/steven123397/my_visual_CPU/wiki/Verification)。
