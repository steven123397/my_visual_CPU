# my_visual_CPU

## 项目定位

`my_visual_CPU` 当前的主体是 `myCPU`。它不是一个停留在课堂作业阶段的单文件解释器，而是一个已经可以运行、并且正在持续工程化的 RISC-V 功能级模拟器原型。

这个项目当前的主目标有两个：

1. 保留一条简单、正确、可调试的 ISA 级参考执行路径。
2. 把这台模拟器逐步扩展成能够支撑自制 OS bring-up 的运行平台。

在这个目标之上，仓库已经开始做第三件事：

3. 在不复制 ISA 语义的前提下，引入可切换的多执行后端，当前已经有 `functional` 和增量中的 `pipeline` 两条路径。

## 当前状态

当前仓库已经具备：

- ELF64 和 flat binary 装载。
- RV64 整数、控制流、访存、CSR 与 trap 基础路径。
- `M/S/U` 特权基础、最小 supervisor delegation、`mret/sret`。
- Sv39 地址翻译、页错误、最小 TLB、`SUM/MXR` 语义。
- UART、CLINT、PLIC、最小 MMIO block storage 平台。
- guest 侧最小 supervisor runtime、VM、trap 和 U-mode demo bring-up。
- 共享语义层 `InstructionSemantics + InsnEffects + ExecutionContext`。
- 可切换的 `ExecutionBackend`，当前支持 `functional` 和 `pipeline`。

当前 `PipelineBackend` 已经实现并通过现有回归验证的能力包括：

- 经典 `IF/ID/EX/MEM/WB` 五级阶段寄存器推进。
- 整数、控制流、`load/store`、当前仓库已实现的 `system/CSR` 路径。
- `EX/MEM`、`MEM/WB -> EX` 的操作数 forwarding。
- CSR 可见值投影，包含 WARL/alias/counter 视图的一致性处理。
- 单气泡 `load-use` interlock。
- `jal/jalr/branch` 的 EX 级 redirect 与 flush。
- MEM 级访存、WB 级 GPR/CSR/trap-return 提交。
- commit-boundary interrupt、fetch/load/store fault、illegal instruction、`ecall`、`mret/sret` 的精确提交路径。
- 与 `functional` 后端的 host 差分 smoke，以及当前 asm 回归集的 `pipeline` 运行覆盖。

这意味着：当前流水线不是“还没成型的空骨架”，而是一条已经能运行当前回归集的增量 in-order backend。

## 快速开始

### 构建

```bash
cd myCPU
make
```

### 运行

```bash
./mycpu tests/asm/hello.elf
./mycpu --backend functional tests/asm/hello.elf
./mycpu --backend pipeline tests/asm/hello.elf
./mycpu --disk tests/data/storage_basic.txt tests/asm/storage_device_basic.elf
```

说明：

- 默认后端是 `functional`。
- `functional` 是项目的黄金参考路径。
- `pipeline` 是当前已经可用的五级流水线 backend，但它的定位仍然是“可验证、可比较的 alternate backend”，不是取代参考核。

### 测试

```bash
cd myCPU
make test
make test-pipeline
make test-host-pipeline_backend
make test-host-backend_differential
```

各目标的含义：

- `make test`：参考后端基线回归。
- `make test-pipeline`：现有 asm 回归在 `--backend pipeline` 下的覆盖，加上流水线 host smoke。
- `make test-host-pipeline_backend`：流水线专用 host smoke。
- `make test-host-backend_differential`：`functional` / `pipeline` 的提交级差分 smoke。

## 仓库入口

如果你第一次读这个项目，先看这几个文件：

1. `README.md`：知道项目是什么、怎么跑。
2. `AGENTS.md`：知道项目为什么这么演进、当前优先级是什么。
3. `docs/project_module_guide.md`：知道代码按什么层次组织。
4. `docs/platform_mmio_contract.md`：知道 guest 看到的平台寄存器合同。
5. `docs/five_stage_pipeline_plan.md`：知道流水线方案、落地状态和剩余边界。

## 代码结构速览

| 路径 | 作用 |
|---|---|
| `myCPU/src/main.cpp` | CLI 入口，解析镜像、磁盘和 backend 选择。 |
| `myCPU/src/platform/` | `Machine`、平台地址图、事件汇总。 |
| `myCPU/src/arch/` | `CoreState`、`CsrFile` 等架构状态。 |
| `myCPU/src/trap.*` | trap、interrupt、`mret/sret`、delegation 路由。 |
| `myCPU/src/exec/` | 功能后端、流水线后端、整数/控制流/访存/系统语义。 |
| `myCPU/src/isa/` | 后端共享的 `InsnEffects`、执行上下文和统一语义入口。 |
| `myCPU/src/mem/` | `Ram`、`Bus`、`AddressSpace`、Sv39/TLB。 |
| `myCPU/src/devices/` | UART、CLINT、PLIC、SimpleStorage。 |
| `myCPU/src/loader/` | ELF 和 flat binary loader。 |
| `myCPU/guest/` | guest supervisor runtime、VM、trap、U-mode demo。 |
| `myCPU/tests/asm/` | 汇编级架构行为回归。 |
| `myCPU/tests/host/` | host 侧 smoke、backend 差分和结构回归。 |

## 推荐阅读顺序

如果你主要关心“这台机器怎么跑起来”，建议按这个顺序读：

1. `myCPU/src/main.cpp`
2. `myCPU/src/platform/machine.cpp`
3. `myCPU/src/cpu.cpp`
4. `myCPU/src/exec/backend.h`
5. `myCPU/src/exec/functional_backend.cpp`
6. `myCPU/src/exec/pipeline_backend.cpp`
7. `myCPU/src/trap.cpp`
8. `myCPU/src/arch/csr_file.cpp`
9. `myCPU/src/mem/address_space.cpp`
10. `myCPU/src/devices/*.cpp`

如果你主要关心“guest OS 怎么被带起来”，建议从这里开始：

1. `docs/platform_mmio_contract.md`
2. `myCPU/guest/supervisor_demo/start.S`
3. `myCPU/guest/supervisor_demo/main.c`
4. `myCPU/guest/kernel/trap.c`
5. `myCPU/guest/kernel/vm.c`
6. `myCPU/guest/kernel/user_program.c`

## 当前开发重点

当前主线仍然不是“继续堆更复杂的微架构”，而是：

1. 保持参考执行路径正确、可调试、可验证。
2. 继续完善支撑 OS bring-up 的功能模拟器地基。
3. 让 `pipeline` 后端在当前 ISA/平台子集上保持可比较、可验证、可回归。

换句话说，当前最重要的工程判断标准仍然是：

- 正确性优先于性能。
- 清晰边界优先于炫技抽象。
- 可验证性优先于过早复杂化。

## 相关文档

- `AGENTS.md`：项目定位、阶段目标、设计原则、测试要求。
- `CLAUDE.md`：与 `AGENTS.md` 对齐的协作文档。
- `docs/project_module_guide.md`：源码分层和目录职责导读。
- `docs/platform_mmio_contract.md`：平台 MMIO 合同。
- `docs/five_stage_pipeline_plan.md`：五级流水线方案与当前落地状态。
- `docs/five_stage_pipeline_file_checklist.md`：流水线涉及文件和推荐补丁顺序。
