# my_visual_CPU 项目模块导读

## 1. 这份文档解决什么问题

这份文档不是逐文件罗列，而是帮助你在第一次阅读仓库时快速建立 3 个认知：

1. 这个项目现在已经做到什么程度。
2. 代码按什么层次组织。
3. 如果你想继续开发，应该先从哪里读起。

## 2. 项目一句话概括

`myCPU` 是一个已经可运行的 RISC-V 功能级模拟器原型。它保留了一条简单、正确、可调试的参考执行路径，同时已经具备最小平台设备、Sv39、特权基础、guest supervisor runtime，以及一条可切换的五级流水线 backend。它的主目标不是“做快”，而是逐步成为一台能够支撑自制 OS bring-up 的模拟平台。

## 3. 当前工程状态

从工程形态看，这个仓库已经不再是单文件 C 实验，而是一个“参考语义仍然清晰简单，但外围结构已经模块化”的项目：

- 平台装配已经独立成 `Machine + Bus + Device + Loader`。
- CPU 状态已经拆出 `CoreState + CsrFile + TrapController + AddressSpace`。
- ISA 语义已经分拆为整数、控制流、访存、系统/CSR 四类模块。
- `InstructionSemantics + InsnEffects + ExecutionContext` 已经成为后端共享语义源。
- 执行方式已经拆分为 `FunctionalBackend` 和 `PipelineBackend`。
- guest 侧已经有 supervisor runtime、trap、VM、U-mode bring-up，而不只是汇编测试。

这意味着：当前仓库应该被理解为“一个正在稳定工程边界的功能模拟器”，而不是“还没落地的设计草图”。

## 4. 顶层结构

| 路径 | 作用 |
|---|---|
| `README.md` | 仓库入口，说明项目定位、构建、运行、测试和阅读顺序。 |
| `AGENTS.md` | 项目阶段、设计优先级、测试要求和协作约束。 |
| `CLAUDE.md` | 与 `AGENTS.md` 对齐的协作文档。 |
| `docs/` | 设计计划、平台合同、模块导读等说明文档。 |
| `myCPU/` | 模拟器主体代码、guest 运行时、测试和构建入口。 |

## 5. 先建立整条执行链路

从 host 启动到 guest 代码执行，主路径可以先记成这 10 步：

1. `main.cpp` 解析命令行参数。
2. 构造 `Machine`。
3. `Machine` 组装 `Ram + Bus + Uart16550 + Clint + Plic + SimpleStorage`。
4. `ElfLoader` 或 `BinaryLoader` 把镜像写入 RAM。
5. `cpu_init()` 初始化架构状态。
6. `Machine::run()` 反复调用当前 `ExecutionBackend::step()`。
7. backend 在每步开始时统一处理 `Bus::tick()` 产生的平台事件。
8. backend 通过 `InstructionSemantics` 得到架构效果。
9. `AddressSpace` 负责 bare-mode 或 Sv39 地址转换，再经 `Bus` 访问 RAM 或 MMIO。
10. trap、interrupt、`mret/sret` 由 `TrapController` 统一处理。

这个链路是整个项目最重要的骨架。后面无论你读设备、Sv39、guest runtime 还是流水线，都只是这条骨架上的局部深化。

## 6. Host 侧代码如何分层

### 6.1 入口与平台装配层

| 文件 | 作用 |
|---|---|
| `myCPU/src/main.cpp` | CLI 入口，解析 `--backend`、`--disk`、`-b` 等参数。 |
| `myCPU/src/platform/machine.h` | `Machine` 声明，持有 CPU、RAM、设备、loader、backend。 |
| `myCPU/src/platform/machine.cpp` | 平台装配、镜像装载、backend 构造和主执行循环。 |
| `myCPU/src/platform/platform_events.h` | 设备 `tick()` 汇总事件。 |
| `myCPU/include/platform_mmio.h` | host/guest 共用的平台 MMIO 常量。 |

这层回答的是：“一台机器由什么组成，如何启动。”

### 6.2 CPU 外观层

| 文件 | 作用 |
|---|---|
| `myCPU/src/cpu.h` | `CPU` 外观接口，聚合架构状态、trap、地址空间。 |
| `myCPU/src/cpu.cpp` | 参考执行路径 `cpu_step()`；也是 `FunctionalBackend` 的语义基线。 |

这层的意义是把“CPU 这台东西有哪些核心部件”先收口，再让不同 backend 去驱动它。

### 6.3 架构状态层

| 文件 | 作用 |
|---|---|
| `myCPU/src/arch/core_state.*` | GPR、`pc`、`cycle`、`instret`、halt、当前特权级。 |
| `myCPU/src/arch/csr_file.*` | CSR 存储和特殊语义规则。 |

这层只负责“状态是什么”，不负责“状态怎么被推进”。

### 6.4 Trap 与特权控制层

| 文件 | 作用 |
|---|---|
| `myCPU/src/trap.h` | `TrapController` 声明。 |
| `myCPU/src/trap.cpp` | trap 进入、interrupt 进入、`mret/sret`、delegation、平台事件同步。 |

如果你要读 CSR、特权级和异常/中断行为，这层是最关键的入口。

### 6.5 语义层与执行后端层

| 文件 | 作用 |
|---|---|
| `myCPU/src/exec/backend.h` | 后端统一接口。 |
| `myCPU/src/exec/functional_backend.*` | 参考执行路径的 backend 封装。 |
| `myCPU/src/exec/pipeline_backend.*` | 五级流水线 backend。 |
| `myCPU/src/exec/pipeline_types.h` | 流水线阶段寄存器。 |
| `myCPU/src/isa/effects.h` | `InsnEffects`、`MemoryRequest`、`ControlEffect` 等共享架构效果对象。 |
| `myCPU/src/isa/execution_context.*` | 语义层读状态的受控上下文。 |
| `myCPU/src/isa/instruction_semantics.*` | 后端共享的统一 ISA 语义入口。 |

这是当前项目最重要的工程边界之一。它表达的是：

- 参考执行路径继续保留。
- 流水线不是复制一套 ISA 语义，而是共享语义、改变推进方式。

### 6.6 指令语义分块

| 文件 | 作用 |
|---|---|
| `myCPU/src/exec/integer_ops.*` | 整数、`W` 类、RV64M、当前 `FENCE` no-op。 |
| `myCPU/src/exec/control_flow_ops.*` | `jal`、`jalr`、branch。 |
| `myCPU/src/exec/memory_ops.*` | `load/store`、扩展加载值、访存效果。 |
| `myCPU/src/exec/system_ops.*` | `ecall`、`ebreak`、`mret/sret`、`sfence.vma`、CSR 指令。 |
| `myCPU/src/decode.c` | 32 位指令译码。 |

读这层时要始终记住：这里描述的是“指令的架构效果”，不是“它花几个周期”。

### 6.7 内存、总线与地址转换层

| 文件 | 作用 |
|---|---|
| `myCPU/src/mem/ram.*` | RAM 设备封装。 |
| `myCPU/src/mem/bus.*` | 物理地址分发与设备 `tick()` 聚合。 |
| `myCPU/src/mem/address_space.*` | bare-mode / Sv39、页表遍历、TLB、fault-result API。 |
| `myCPU/src/memory.c` | legacy 底层内存 backing。 |

这层里最重要的区分是：

- `Bus` 处理“物理地址访问哪个设备”。
- `AddressSpace` 处理“CPU 语义上的访存如何翻译与报错”。

### 6.8 设备层

| 文件 | 作用 |
|---|---|
| `myCPU/src/devices/uart16550.*` | UART 输出与 THRE 中断源。 |
| `myCPU/src/devices/clint.*` | `mtime/mtimecmp` 与 timer interrupt。 |
| `myCPU/src/devices/plic.*` | 最小 PLIC。 |
| `myCPU/src/devices/simple_storage.*` | host-backed 块设备。 |

这层回答的是：“guest OS 实际上能碰到哪些平台外设。”

### 6.9 镜像加载层

| 文件 | 作用 |
|---|---|
| `myCPU/src/loader/elf_loader.*` | ELF64 加载。 |
| `myCPU/src/loader/binary_loader.*` | flat binary 加载。 |

这层的存在让镜像装载不再混进 CPU 或 RAM 内部逻辑。

## 7. `PipelineBackend` 现在到底做到哪一步

当前 `PipelineBackend` 已经不是空骨架。它已经具备：

- 经典五级 `IF/ID/EX/MEM/WB` 阶段寄存器。
- 整数、控制流、访存和当前仓库已实现的 `system/CSR` 路径。
- `EX/MEM`、`MEM/WB -> EX` forwarding。
- 单气泡 `load-use` interlock。
- EX 级 redirect/flush。
- MEM 级访存。
- WB 级 GPR/CSR/trap-return 提交。
- fetch/load/store fault 与 interrupt 的 commit-boundary 处理。
- CSR 可见值投影，保证 WARL、alias、counter 读值与当前语义一致。
- 对当前 asm 回归集的 `pipeline` 后端运行覆盖。
- `functional` / `pipeline` 的 host 差分 smoke。

当前它仍然是“增量中的 in-order backend”，不是更高阶微架构模型。也就是说，它的重点仍然是：

- 和参考路径共享语义。
- 对当前已实现 ISA/平台子集保持可比较。
- 用回归和差分测试稳定行为。

## 8. Guest 侧代码如何理解

guest 侧不是“为了测试凑几段汇编”，而是已经形成了最小 OS bring-up 路径。

### 8.1 guest 平台驱动层

| 路径 | 作用 |
|---|---|
| `myCPU/guest/include/platform.h` | guest 看到的平台 API。 |
| `myCPU/guest/lib/platform.S` | 平台驱动汇编实现。 |
| `myCPU/include/platform_mmio.h` | host/guest 共用的 MMIO 常量。 |

### 8.2 guest 基础服务层

| 路径 | 作用 |
|---|---|
| `guest/kernel/console.c` | UART console。 |
| `guest/kernel/storage.c` | storage 读块封装。 |
| `guest/kernel/timer.c` | CLINT timer 辅助。 |
| `guest/kernel/panic.c` | panic/shutdown。 |
| `guest/include/riscv.h` | guest 侧 CSR/汇编辅助。 |

### 8.3 guest 内存与 VM 层

| 路径 | 作用 |
|---|---|
| `guest/kernel/memory.c` | early allocator 与内存布局。 |
| `guest/kernel/pmm.c` | 物理页管理。 |
| `guest/kernel/vm.c` | Sv39 页表、地址空间、对象、fault policy。 |
| `guest/kernel/runtime_context.c` | 当前活动 process/address-space/trap-context。 |

### 8.4 guest trap 与 user runtime 层

| 路径 | 作用 |
|---|---|
| `guest/kernel/trap.c` | supervisor trap 分发、handler 注册、user runtime 管理。 |
| `guest/lib/trap_runtime.S` | U-mode enter/resume 汇编桥。 |
| `guest/kernel/user_task*.c` | 单用户任务生命周期封装。 |
| `guest/kernel/user_program*.c` | 更高层的用户程序装配、smoke 和标准路径。 |

### 8.5 guest demo 启动路径

| 路径 | 作用 |
|---|---|
| `guest/supervisor_demo/start.S` | M-mode 初始入口、delegation、切入 S-mode。 |
| `guest/supervisor_demo/main.c` | `kernel_main()`，串起内存、VM、trap、user program、platform smoke。 |

如果你关心“这台模拟器能不能支撑自制 OS”，guest 目录就是最直接的答案。

## 9. 测试体系怎么读

### 9.1 `tests/asm/`

这里是最核心的架构行为回归。它覆盖：

- UART 输出。
- 控制流。
- CSR、trap、`mret/sret`。
- timer / external interrupt。
- delegation 边界。
- Sv39、TLB、`SUM/MXR`。
- PLIC、CLINT、storage。

这些测试的判断标准基本都是“guest 可观察到的输出”，因此它们构成了当前仓库最真实的架构合同。

### 9.2 `tests/host/`

这里不是替代 asm 测试，而是补结构级和 backend 级验证：

- `instruction_semantics_smoke.cpp`：共享语义层 smoke。
- `address_space_faults_smoke.cpp`：fault-result API smoke。
- `pipeline_backend_smoke.cpp`：流水线专用路径 smoke。
- `backend_differential_smoke.cpp`：`functional` / `pipeline` 提交级差分 smoke。
- `backend_cli.sh`：CLI backend 选择回归。

### 9.3 测试目标

| 命令 | 作用 |
|---|---|
| `make test` | 参考后端完整基线。 |
| `make test-pipeline` | asm 回归在 `pipeline` 后端下的覆盖，加 host smoke。 |
| `make test-host-pipeline_backend` | 流水线专用 host smoke。 |
| `make test-host-backend_differential` | `functional` / `pipeline` 差分 smoke。 |

## 10. 推荐阅读顺序

### 10.1 想先读 host 主路径

1. `README.md`
2. `AGENTS.md`
3. `myCPU/src/main.cpp`
4. `myCPU/src/platform/machine.cpp`
5. `myCPU/src/cpu.cpp`
6. `myCPU/src/exec/backend.h`
7. `myCPU/src/exec/functional_backend.cpp`
8. `myCPU/src/exec/pipeline_backend.cpp`
9. `myCPU/src/trap.cpp`
10. `myCPU/src/mem/address_space.cpp`

### 10.2 想先读流水线

1. `docs/five_stage_pipeline_plan.md`
2. `myCPU/src/exec/pipeline_types.h`
3. `myCPU/src/exec/pipeline_backend.h`
4. `myCPU/src/exec/pipeline_backend.cpp`
5. `myCPU/tests/host/pipeline_backend_smoke.cpp`
6. `myCPU/tests/host/backend_differential_smoke.cpp`

### 10.3 想先读 guest bring-up

1. `docs/platform_mmio_contract.md`
2. `guest/supervisor_demo/start.S`
3. `guest/supervisor_demo/main.c`
4. `guest/kernel/trap.c`
5. `guest/kernel/vm.c`
6. `guest/kernel/user_program.c`

## 11. 当前最重要的开发判断标准

当前这个项目最应该优先考虑的，不是“还能不能再多塞一点微架构”，而是：

- 参考路径是否仍然简单、透明、可调试。
- 语义和时序是否继续解耦，而不是重新缠回一起。
- 平台合同是否清晰，guest 侧是否可继续长大。
- 新行为是否能被 asm 回归和 host smoke 明确验证。

## 12. 当前缺口与下一步

当前仍然没有的东西包括：

- 完整 privileged spec 覆盖。
- 更真实的设备平台。
- variable-latency memory、cache、多核、OoO。

因此，近期最合理的方向依然是：

1. 保持 `functional` 路径为黄金参考。
2. 继续围绕 OS bring-up 方向完善平台与特权功能。
3. 让 `pipeline` 后端在当前已实现子集上保持可比较、可回归、可维护。

## 13. 总结

如果只记住一句话，可以记这句：

这个仓库当前最重要的价值，不是“已经做了一个五级流水线”，而是“已经把参考模拟器、平台设备、guest runtime 和 alternate backend 组织成了一条可持续扩展的工程路线”。
