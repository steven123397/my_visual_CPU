# myCPU — RISC-V 模拟器

当前仓库已经是一个已可运行的模拟器原型，而不是纯设计稿。当前主线已经达成 `phase1-stable`（`283aee6`）这一 Phase 1 冻结基线：具备 RV64I / RV64M 参考执行路径、UART / CLINT / PLIC / MMIO storage 平台、基础 `M/S/U` 特权路径、Sv39，以及一套最小 guest supervisor runtime。此后主线又继续接入了 `pipeline` 执行后端、本地 `debug_session/protocol` 和浏览器前端教学演示链路。到 `2026-03-26` 为止，reference path 又完成了一轮 Phase 1 hardening regression 扩充，补上了更系统的非法编码、CPU 侧 MMIO access-fault、ELF 段布局 / reject 和 CSR 非法访问矩阵回归。

## 目录结构

```text
my_visual_CPU/
├── frontend/           # 本地前端调试器与 Node 调试服务
├── myCPU/
│   ├── guest/          # 最小 guest supervisor runtime / 平台层 / demo
│   ├── src/
│   │   ├── main.cpp    # C++ 入口，CLI 参数、backend 选择与 Machine 启动
│   │   ├── cpu.cpp/h   # CPU 外观接口 + functional 参考执行路径
│   │   ├── decode.c/h  # 指令解码
│   │   ├── debug/      # DebugSnapshot / DebugSession / --debug-cli
│   │   ├── exec/       # backend 骨架 + pipeline core + 指令语义分块
│   │   ├── mem/        # C++ Ram/Bus/AddressSpace 边界
│   │   ├── devices/    # UART / CLINT / PLIC / SimpleStorage
│   │   └── ...
│   ├── tests/asm/      # 汇编测试程序与平台 smoke coverage
│   ├── tests/unit/     # host-side 单元回归（loader segment/reject、bus-device/MMIO contract、guest runtime helper 合同）
│   └── Makefile
└── docs/               # 规划与平台契约文档
```

## 模块关系与运行流程

模拟器从命令行读取镜像路径和 backend 选择，通过 `Machine` 组装平台对象，然后把控制权交给当前 `ExecutionBackend`。整体调用关系如下：

```text
main.cpp
  └── Machine
        ├── Ram                      初始化 128MB 主内存
        ├── Uart16550 / Clint / Plic / SimpleStorage
        ├── Bus                      分发 RAM 与设备访问
        ├── ElfLoader / BinaryLoader     加载 ELF 或平坦二进制
        ├── cpu_init()               初始化 CoreState、CsrFile
        ├── ExecutionBackend         默认 functional，可选 pipeline
        └── while (!halted)
              └── backend->step()
                    ├── functional -> cpu_step(cpu, bus)
                    └── pipeline -> IF/ID/EX/MEM/WB + commit-boundary trap/interrupt
```

从模块职责看：

- `main.cpp` 负责 CLI 参数解析、`--backend functional|pipeline`、`--debug-cli` 选择和启动 `Machine` 或 debug protocol。
- `platform/machine.*` 负责组装 CPU、Ram、Bus、PLIC、storage 等平台对象，管理 backend 生命周期，并驱动主执行循环，同时向 debug session 暴露只读平台状态入口。
- `platform/address_map.h` 负责平台地址映射常量，避免设备层依赖 legacy `Memory` 头。
- `arch/core_state.*` 负责通用寄存器、`pc`、周期计数和停机状态。
- `arch/csr_file.*` 负责已实现 CSR 集合、`misa/satp/time` 这类带架构约束的特殊语义，以及 `sstatus/sie/sip` 对 `mstatus/mie/mip` 的别名视图。
- `mem/ram.*` 和 `mem/bus.*` 提供平台总线与 RAM 边界。
- `mem/address_space.*` 负责 CPU 侧地址访问边界，当前提供 bare-mode 直通、Sv39 三级页表遍历、最小 TLB、受限 `satp` 模式切换、`sfence.vma` 刷新，以及 instruction/load/store 的 fault 路由。
- `devices/uart16550.*`、`devices/clint.*`、`devices/plic.*` 和 `devices/simple_storage.*` 提供独立 MMIO 设备对象。
- `devices/device.h` 提供统一设备接口，供 `Bus` 附加和分发。
- `loader/elf_loader.*` 和 `loader/binary_loader.*` 提供镜像装载边界，直接通过 `Ram` 接口写入镜像内容。
- `cpu.cpp/h` 负责 functional reference path，并把 `CoreState + CsrFile + TrapController` 接到共享 ISA 语义层。
- `isa/*` 负责共享 `InstructionSemantics`、`ExecutionContext` 和 `InsnEffects`，作为 `functional` 与 `pipeline` 的统一 ISA 语义来源。
- `exec/backend.h`、`exec/functional_backend.*` 和 `exec/pipeline_backend.*` 负责执行后端抽象、默认 functional backend，以及当前五级 pipeline core。
- `debug/*` 负责 `DebugSnapshot`、`DebugSession` 与 `--debug-cli` JSON line protocol，本地前端调试器通过这层消费 simulator 快照。
- `decode.c` 负责把 32 位机器码拆成执行阶段可用的字段。
- `memory.c` 负责主内存访问，MMIO 分发由 `Bus` 与设备对象处理。
- `trap.cpp/h` 负责 `TrapController`，集中处理异常/中断入口、`mret/sret` 返回、timer/external interrupt 路由，以及当前最小 `medeleg/mideleg` supervisor trap 委托路径。
- `exec/integer_ops.*`、`exec/control_flow_ops.*`、`exec/memory_ops.*`、`exec/system_ops.*` 负责按指令族拆分参考语义，避免 `cpu.cpp` 持续膨胀。

一次指令执行的数据流可以概括为：

```text
PC
  -> mem_read(取指)
  -> decode(解析 opcode/寄存器/立即数)
  -> execute(算术/跳转/访存/CSR/系统指令)
  -> 可能访问 memory / UART / CLINT / PLIC / SimpleStorage
  -> 可能进入 trap
  -> 更新 PC
```

## 编译

```bash
cd myCPU
make
```

## 运行

```bash
# 运行 ELF 程序
./mycpu <program.elf>

# 显式选择 pipeline backend 运行 ELF
./mycpu --backend pipeline <program.elf>

# 作为本地调试协议进程运行
./mycpu --debug-cli

# 运行平坦二进制（指定加载地址，十六进制）
./mycpu -b 80000000 <program.bin>
```

## 本地前端调试演示

先构建模拟器：

```bash
cd myCPU
make
```

再从仓库根目录启动本地服务：

```bash
node frontend/server/debug_server.mjs
```

默认地址：

```text
http://127.0.0.1:4173
```

当前前端支持：

- 选择仓库内现有 asm / guest / `kernel_alpha` demo
- 切换 `functional` / `pipeline`
- `Load / Run / Pause / Step Cycle / Step Commit / Reset`
- 查看五级流水线、最近周期时间线、寄存器变化、CSR / Trap、最近一次总线访问，以及 UART / CLINT / PLIC / Storage 状态

## 测试

需要 RISC-V 交叉编译工具链：

```bash
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf

cd myCPU
make test
make test-pipeline
make test-guest-kernel_alpha_demo
make test-guest-kernel_alpha_fault_demo

cd ../frontend
node --test
```

`make test` 是默认 `functional` reference path 的主回归，覆盖 asm、host-side unit、`guest_supervisor_demo`，以及 `kernel_alpha` 正向与全部负向 demo。

`make test-pipeline` 是 `pipeline` backend 的完整门禁，覆盖同一批 asm 输出、host-side smoke / differential / debug CLI，以及 `guest_supervisor_demo` 和全部 `kernel_alpha` demo 在 `pipeline` 下的一致性。

`cd frontend && node --test` 负责守住本地调试服务、测试清单和前端纯状态逻辑。

其余 `kernel_alpha` storage / PLIC / timer 负向 demo 也都有独立测试目标；README 只保留常用入口，具体名称以 [myCPU/Makefile](myCPU/Makefile) 为准。

当前 Phase 1 / Phase 2 回归做到什么程度可认为阶段性收口，见 [docs/status/regression_completion_criteria_2026-03-26.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/regression_completion_criteria_2026-03-26.md)。

## 内存映射

| 地址范围 | 设备 |
|---|---|
| `0x80000000` + 128MB | 主内存 |
| `0x10000000` | UART (16550) |
| `0x10001000` | SimpleStorage (MMIO block device) |
| `0x02000000` | CLINT (定时器) |
| `0x0c000000` | PLIC |

## 已实现特性

- RV64I 基础整数指令集
- RV64M 乘除法扩展
- 非法整数保留编码稳定触发 `illegal instruction`
- `DIV/REM/DIVW/REMW` 的 `INT_MIN / -1` 边界按 RISC-V 规范返回
- ELF64 程序加载
- 纯 BSS `PT_LOAD` 段装载与 zero-fill
- 更真实的 ELF 多 `PT_LOAD` / mixed data+BSS 段布局，以及 malformed header / program-header reject 回归
- CSR 指令（CSRRW/CSRRS/CSRRC 及立即数变体）
- M-mode 异常与中断（ECALL、EBREAK、MRET）
- 第一批 M/S/U 特权语义：`MPP` 跟踪、`ecall` cause 区分、`sret` 返回、CSR 访问约束
- CSR 非法访问矩阵，包括跨特权级访问、只读 counter CSR 写保护和 `misa` 只读写保护
- 基于 `medeleg` 的最小 supervisor 异常委托
- 基于 `mideleg` 的最小 supervisor 定时器/外部中断递送
- **Sv39 虚拟内存**：3 级页表遍历、页错误、权限检查、大页支持、最小 TLB、A/D bit 维护
- `satp` CSR 支持（MODE 字段按 WARL 约束到 bare/Sv39；不支持值不会以“读得出分页模式、实际却 bare” 的形式泄漏）
- `sfence.vma` 指令（当前执行本地 TLB 全量失效）
- `AddressSpace` 访问边界，以及 unmapped fetch/load/store 的 access-fault trap
- CSR 特权级/只读属性检查，非法访问触发 illegal-instruction trap
- `misa` 作为固定实现能力视图对 guest 只读暴露，不允许软件改写 ISA 声明
- UART MMIO（写入直接输出到 stdout）
- CLINT 定时器中断，以及 `mtime/mtimecmp` 的 1/2/4/8 字节 MMIO 访问
- `time` CSR 与 CLINT `mtime` 保持一致，guest 侧 CSR/MMIO 看到同一平台时间源
- PLIC machine/supervisor external interrupt 最小路径
- host-backed block-oriented MMIO storage device，支持 attached-but-not-ready 状态、bad-magic probe 注入与 `STORAGE_ERR_NOT_READY`
- 设备区间重叠防御，以及第一轮 MMIO 非法访问宽度白名单
- CPU 侧 MMIO 非法 offset / width 稳定触发 access-fault trap，host-side 也已有 MMIO contract matrix
- 最小 guest supervisor runtime：包含统一 trap dispatch、基础平台库、early allocator / PMM / guest-side Sv39 VM、显式 trap/runtime/task/program helper，以及由 `user_program_smoke` 提供的阶段化 lifecycle / prepare / enter helper 与 `supervisor_demo_smoke` 提供的单入口 demo runner，覆盖 `guest_supervisor_demo` 的 bootstrap、U-mode 进入/返回、page fault 恢复、timer/external interrupt 与生命周期清理 smoke
- 独立 `kernel_alpha_demo`：覆盖 boot marker、PMM 初始化、自建 Sv39 内核页表、内核镜像/early heap/managed RAM 显式映射、UART / CLINT / PLIC / storage 的 MMIO lazy map、一次 supervisor external interrupt、第一次 supervisor timer interrupt、一次 storage readiness probe，以及一次最小块设备读取
- 独立 `kernel_alpha_fault_demo`：覆盖独立 kernel alpha 在 VM 已开启但 CLINT 未映射时的 fault / panic 负向路径
- 独立 `kernel_alpha_storage_no_media_demo`：覆盖独立 kernel alpha 在 VM 已开启且 storage MMIO 可达、但未附加镜像时的 metadata / `NO_MEDIA` error 负向路径
- 独立 `kernel_alpha_storage_not_ready_demo`：覆盖独立 kernel alpha 在 VM 已开启且 storage 已附加但 `READY` 缺失时的 readiness / `NOT_READY` / clear-error 负向合同
- 独立 `kernel_alpha_storage_bad_magic_demo`：覆盖独立 kernel alpha 在 VM 已开启且 storage 已附加、但 `MAGIC` 元数据损坏时的 probe-fail / data-path-still-live 负向合同
- 独立 `kernel_alpha_storage_bad_block_count_demo`：覆盖独立 kernel alpha 在 VM 已开启且 storage 已附加时，`BLOCK_COUNT != 1` 的 `BAD_BLOCK_COUNT` 与 `COMMAND = NONE` clear-error 负向合同
- 独立 `kernel_alpha_storage_lba_range_demo`：覆盖独立 kernel alpha 在 VM 已开启且 storage 已附加时，`LBA == capacity_blocks` 的 `LBA_RANGE` 与 `COMMAND = NONE` clear-error 负向合同
- 独立 `kernel_alpha_storage_bad_command_demo`：覆盖独立 kernel alpha 在 VM 已开启且 storage 已附加时，非法 `COMMAND` 值的 `BAD_COMMAND` 与 `COMMAND = NONE` clear-error 负向合同
- 独立 `kernel_alpha_plic_not_ready_demo`：覆盖独立 kernel alpha 在 VM 已开启且 UART / CLINT / PLIC MMIO 可达、但 PLIC 未初始化时的 readiness timeout / panic 负向合同
- 独立 `kernel_alpha_timer_not_ready_demo`：覆盖独立 kernel alpha 在 VM 已开启且 UART / CLINT / PLIC MMIO 可达、第一次 external interrupt 已成功到达、但未安排第一次 timer delivery 时的 readiness timeout / panic 负向合同
- `ecall` a7=93 退出约定

## 源码文件说明

### 根目录

- `readme.md`：项目总说明，包含功能、编译、运行和测试方式。
- `docs/background/request.md`：课程项目背景与目标说明，描述了“从 0 实现一个可运行程序的指令集模拟器”的教学目标。

### `myCPU/`

- `Makefile`：本地编译规则、RISC-V 汇编样例构建规则和 `make test` 测试入口。
- `tests/unit/`：host-side 单元测试，当前覆盖 ELF pure-BSS 装载、bus/device 守边界、storage readiness，以及 `supervisor_runtime`、`kernel_runtime`、`kernel_alpha/common`、`kernel_alpha/interrupt_contract` 和 `kernel_alpha/storage_contract` 这些 guest runtime helper 合同。
- `mycpu`：编译产物，运行后加载并执行 RISC-V 程序镜像。

### `myCPU/guest/`

- `include/`：guest 平台层、trap、timer、memory、pmm、vm、`user_task`、`user_task_bootstrap`、`user_program`、`user_program_smoke`、`supervisor_demo_smoke` 等最小内核接口。
- `kernel/`：guest 最小内核实现，包含 `console` / `storage` / `timer` / `trap` / `memory` / `pmm` / `vm` / `runtime_context` / `kernel_bringup` / `kernel_runtime`，以及 `user_task` / `user_task_bootstrap` / `user_program` / smoke helpers。当前重点已覆盖页表与 VM 管理、trap/runtime 生命周期、单用户任务 bring-up，以及由阶段化 `user_program_smoke` helper 与单入口 `supervisor_demo_smoke` runner 封装的 `guest_supervisor_demo` bootstrap、fault/interrupt/lifecycle 与 platform-tail 流程。
- `kernel_alpha/`：独立 kernel alpha bring-up 入口，当前用于验证第一次真正的小 kernel alpha 的 boot / PMM / Sv39 / PLIC/UART external interrupt / timer interrupt / storage readiness probe / storage read 最小基线；其中 `common.c` 已收口 alpha 专属的 phase helper，`interrupt_contract.c` 已承接 non-storage readiness / panic 合同，`storage_contract.c` 已承接 storage 负向合同检查。
- `kernel_alpha/fault_main.c`：独立 kernel alpha 的 fault / panic 负向回归入口，当前故意构造未映射 CLINT MMIO 访问。
- `kernel_alpha/storage_no_media_main.c`：独立 kernel alpha 的 storage no-media 负向回归入口，当前故意在未挂盘条件下验证 metadata / `NO_MEDIA` error 路径。
- `kernel_alpha/storage_not_ready_main.c`：独立 kernel alpha 的 storage not-ready 负向回归入口，当前故意在 attached-but-not-ready 条件下验证 readiness / `NOT_READY` / clear-error 合同。
- `kernel_alpha/storage_bad_magic_main.c`：独立 kernel alpha 的 storage bad-magic 负向回归入口，当前故意在 bad-magic 条件下验证 probe-fail / data-path-still-live 合同。
- `kernel_alpha/storage_bad_block_count_main.c`：独立 kernel alpha 的 storage bad-block-count 负向回归入口，当前故意提交 `BLOCK_COUNT != 1` 的 read 命令并验证 clear-error 合同。
- `kernel_alpha/storage_lba_range_main.c`：独立 kernel alpha 的 storage LBA-range 负向回归入口，当前故意提交 `LBA == capacity_blocks` 的 read 命令并验证 clear-error 合同。
- `kernel_alpha/storage_bad_command_main.c`：独立 kernel alpha 的 storage bad-command 负向回归入口，当前故意提交非法 `COMMAND` 值并验证 clear-error 合同。
- `kernel_alpha/plic_not_ready_main.c`：独立 kernel alpha 的 PLIC not-ready 负向回归入口，当前故意不初始化 PLIC 并验证 supervisor external interrupt readiness timeout 合同。
- `kernel_alpha/timer_not_ready_main.c`：独立 kernel alpha 的 timer not-ready 负向回归入口，当前故意不安排第一次 timer delivery 并验证 deadline timeout 合同。
- `lib/platform.S` / `lib/trap_runtime.S`：共享 guest MMIO 平台库入口，以及 U-mode enter/resume 的共享汇编桥。
- `supervisor_demo/`：最小 supervisor runtime、linker script 和 bring-up demo。

### `myCPU/src/`

- `main.cpp`
  程序入口。负责解析 `--backend`、`-b`、`--disk`、`--disk-not-ready` 和 `--disk-bad-magic` 参数、创建 `Machine`，并根据镜像类型调用 `load_elf()` 或 `load_binary()` 后启动执行。

- `platform/machine.h`
  `Machine` 类声明。聚合 `CPU`、`Ram`、`Bus` 和 `ExecutionBackend`，为平台装配与 backend 选择提供统一入口。

- `platform/machine.cpp`
  `Machine` 实现。当前负责镜像加载、`cpu_init()` 调用、backend 重建和执行循环；默认 backend 为 `functional`，`pipeline` 作为可选执行模型接入。

- `platform/address_map.h`
  平台地址映射常量定义。集中声明 RAM、UART、CLINT、PLIC、storage 的基地址与大小，供入口、设备和 legacy 内存 backing 共享。

- `mem/ram.h`
  `Ram` 类声明。负责主内存生命周期、RAM 范围内的 load/store，以及供 loader 使用的 bulk write/fill 接口。

- `mem/ram.cpp`
  `Ram` 实现。内部调用现有 `mem_init()/mem_free()` 管理 RAM backing，并通过显式 RAM 接口暴露单点和批量写入能力。

- `mem/bus.h`
  `Bus` 类声明。维护统一设备映射表，并向上暴露统一的 load/store/tick 接口；`tick()` 返回平台事件而不是暴露具体设备状态。

- `mem/bus.cpp`
  `Bus` 实现。负责设备附加、地址分发以及平台 tick 结果汇总；RAM 也作为总线设备接入，不再保留专门的 RAM 分支。当前还负责第一轮设备区间重叠防御和非法设备访问收口。

- `mem/address_space.h`
  `AddressSpace` 类声明。定义 CPU 侧 fetch/load/store 访问入口，提供虚拟地址到物理地址的转换边界。

- `mem/address_space.cpp`
  `AddressSpace` 实现。支持 bare-mode 直通和 Sv39 三级页表遍历。M-mode 始终使用物理地址；S/U-mode 根据受限 `satp.MODE` 决定是否启用分页。页表遍历包含权限检查（R/W/X/U 位）、大页对齐检查，以及 instruction/load/store page fault 触发。

- `devices/device.h`
  设备基类声明。定义统一的 `contains/load/store` 接口，供平台总线附加和寻址，并为非法 MMIO 访问提供统一报错入口。

- `devices/uart16550.h`
  `Uart16550` 类声明。封装最小 16550 串口寄存器访问与 stdout 输出行为。

- `devices/uart16550.cpp`
  `Uart16550` 实现。当前支持最小发送路径和 `LSR` 就绪读取。

- `devices/clint.h`
  `Clint` 类声明。封装 `mtime/mtimecmp`、定时器 tick 和 MMIO 访问接口。

- `devices/clint.cpp`
  `Clint` 实现。负责定时器状态推进、`mtime/mtimecmp` 读写、分宽度 MMIO 访问，并在 tick 时返回是否产生待处理中断。

- `devices/plic.h` / `devices/plic.cpp`
  最小 PLIC 设备实现。当前支持 machine/supervisor context、UART THRE source、claim/complete 与 pending/enable/threshold 路径。

- `devices/simple_storage.h` / `devices/simple_storage.cpp`
  最小块化 MMIO storage 设备实现。通过 `LBA/BLOCK_COUNT/COMMAND/DATA_WINDOW` 暴露同步 block read/write 接口，并支持宿主 `--disk`、`--disk-not-ready` 与 `--disk-bad-magic` 镜像附加、attached-but-not-ready 状态、bad-magic probe 注入和 `STORAGE_ERR_NOT_READY` 合同。

- `loader/elf_loader.h`
  `ElfLoader` 类声明。定义 ELF 镜像装载接口。

- `loader/elf_loader.cpp`
  `ElfLoader` 实现。解析最小 ELF64 头和程序头，把可加载段通过 `Ram` 接口写入内存，并返回入口地址；当前已支持纯 BSS `PT_LOAD` 段的 `zero-fill`。

- `loader/binary_loader.h`
  `BinaryLoader` 类声明。定义平坦二进制装载接口。

- `loader/binary_loader.cpp`
  `BinaryLoader` 实现。校验镜像大小后，把平坦二进制通过 `Ram` 接口装入指定地址。

- `cpu.h`
  CPU 外观接口定义。当前聚合 `CoreState`、`CsrFile` 和 `TrapController`，并通过 `Bus` 访问平台内存与设备。

- `cpu.cpp`
  CPU 调度入口。实现 `cpu_init()`、`csr_read()/csr_write()`、`execute()` 和 `cpu_step()`；当前 functional 路径已经通过共享 `InstructionSemantics` + `InsnEffects` 工作，并通过 `CoreState + CsrFile + TrapController` 管理 CPU 状态与 trap 路由。

- `arch/core_state.h`
  `CoreState` 声明。封装 32 个通用寄存器、`pc`、周期计数和停机状态，为后续继续拆语义和执行后端提供稳定状态边界。

- `arch/core_state.cpp`
  `CoreState` 实现。负责状态复位、寄存器读写、PC 更新、周期推进和停机标志维护。

- `arch/csr_file.h`
  `CsrFile` 声明。封装 CSR 地址常量、`mstatus/mie/mip` 位定义以及 CSR 存储接口。

- `arch/csr_file.cpp`
  `CsrFile` 实现。负责 CSR 状态复位、普通 CSR 读写，以及固定 `misa` 视图、受限 `satp` WARL 语义和 `time -> CLINT mtime` 这类特殊规则。

- `exec/backend.h`
  执行后端抽象声明。定义 `ExecutionBackend` 最小接口，供 `Machine` 统一驱动不同执行模型。

- `exec/functional_backend.h` / `exec/functional_backend.cpp`
  默认 functional backend 实现。直接包装当前 reference `cpu_step()` 路径。

- `exec/pipeline_backend.h` / `exec/pipeline_backend.cpp`
  五级 pipeline backend 核心实现。当前包含 IF/ID/EX/MEM/WB、forwarding、load-use interlock、redirect/flush，以及 commit-boundary trap / interrupt 处理。

- `exec/integer_ops.h`
  整数指令族执行接口声明。承接 `LUI/AUIPC`、整数立即数、整数寄存器、`W` 变体和当前 `FENCE` no-op 路径。

- `exec/integer_ops.cpp`
  整数指令族执行实现。负责 RV64I/RV64M 中的整数算术、比较、移位和 `W` 类语义；当前已收紧非法整数保留编码判定，并显式处理 `DIV/REM/DIVW/REMW` 的宿主 UB 边界。

- `exec/control_flow_ops.h`
  控制流指令执行接口声明。承接跳转与条件分支语义。

- `exec/control_flow_ops.cpp`
  控制流指令执行实现。负责 `JAL`、`JALR` 以及各类条件分支的目标地址与 next-pc 更新。

- `exec/memory_ops.h`
  访存指令执行接口声明。承接 load/store 语义。

- `exec/memory_ops.cpp`
  访存指令执行实现。负责带符号/无符号 load 和各宽度 store，并通过 `Bus` 访问 RAM 或设备。

- `exec/system_ops.h`
  系统/CSR 指令执行接口声明。当前承接 `opcode 0x73` 的语义执行入口。

- `exec/system_ops.cpp`
  系统/CSR 指令执行实现。负责 `ecall`、`ebreak`、`mret`、`sret`、`sfence.vma` 以及 CSR 读改写语义，并通过 `TrapController` 进入 trap 路径。

- `isa/effects.h` / `isa/execution_context.*` / `isa/instruction_semantics.*`
  共享 ISA 语义层。把寄存器写回、CSR 写回、访存请求、trap / redirect / trap-return 等效果统一成值对象，并作为 `functional` 与 `pipeline` 共用的语义来源。

- `decode.h`
  `Insn` 指令结构定义，供译码阶段和执行阶段共享。

- `decode.c`
  译码器实现。根据 opcode 判断 I/S/B/U/J 等格式，提取 `rd/rs1/rs2`、`funct3/funct7` 和符号扩展后的立即数。

- `memory.h`
  `Memory` 结构和 legacy RAM backing 接口定义，包括底层单点/批量读写辅助；平台地址常量已拆到 `platform/address_map.h`。

- `memory.c`
  RAM 访问实现。提供底层单点读写与批量写入/填零辅助，供 `Ram` 封装后复用。

- `trap.h`
  `TrapController` 声明。封装异常进入、中断进入、`mret` 返回以及待处理中断检查接口。

- `trap.cpp`
  `TrapController` 实现。负责保存现场、写入 `mepc/mcause/mtval`、根据 `mtvec/stvec` 进入 trap、处理 timer/external interrupt 挂起位，以及在 `mret/sret` 时恢复执行。


### `myCPU/tests/asm/`

- `hello.S`：通过 UART MMIO 输出 `Hello, RISC-V!\n` 的最小样例。
- `sum.S`：计算 `1+2+...+10` 并输出 `55` 的整数运算与分支样例。
- `control_flow.S`：验证 `beq`、`jal`、`jalr` 和反向分支回跳的控制流回归样例。
- `csr_trap.S`：验证 CSR 读写、立即数 CSR 指令以及 `ecall`/`mret` 的基础陷入返回路径。
- `timer_interrupt.S`：验证 CLINT 定时器中断、`mtvec` 向量入口以及 `mret` 返回后的继续执行。
- `mtvec_modes.S`：验证 `mtvec` direct/vectored 两种模式下，异常和定时器中断命中正确的 trap 入口。
- `supervisor_timer_interrupt.S`：验证 `mideleg` 驱动的 supervisor timer interrupt 递送、`stvec` 向量入口以及 `sret` 返回。
- `plic_supervisor_external_interrupt.S`：验证 PLIC 驱动的 supervisor external interrupt 递送与 claim/complete。
- `storage_device_basic.S`：验证块化 MMIO storage 的 read/write/error 路径。
- `clint_split_access.S`：验证 CLINT `mtime/mtimecmp` 的 8/4/2/1 字节拆分访问与定时器触发一致性。
- `supervisor_platform_smoke.S`：验证 guest 平台层对 UART/PLIC/storage 契约的最小消费。
- `trap_state.S`：验证 trap 进入/返回时 `mstatus` 的 `MIE/MPIE` 状态变化，以及 `mepc` 的保存与恢复。
- `exception_traps.S`：验证 `ebreak` 与非法指令 trap 的 `mcause`、`mepc`、`mtval` 行为。
- `loads_signed_unsigned.S`：验证 `LB/LBU`、`LH/LHU`、`LW/LWU`、`LD` 的符号扩展与零扩展语义。
- `alu_word.S`：验证 `ADDIW/SLLIW/SRLIW/SRAIW` 与 `ADDW/SUBW/SLLW/SRLW/SRAW` 的 32 位结果截断和符号扩展。
- `branches_signed_unsigned.S`：验证 `BLT/BGE` 与 `BLTU/BGEU` 在相同输入下的 signed/unsigned 语义差异。
- `muldiv.S`：验证 `RV64M` 乘除、取模以及除零边界行为。
- `muldiv_edge_cases.S`：验证 `DIV/REM/DIVW/REMW` 的 `INT_MIN / -1` 规范边界。
- `illegal_integer_encodings.S`：验证非法整数保留编码稳定进入 `illegal instruction` trap。
- `fence_noop.S`：固定当前 `FENCE/FENCE.I` 在参考模型中的 no-op 行为。
- `csr_semantic_consistency.S`：验证 `misa` 只读语义、`satp.MODE` 受限 WARL 语义，以及 `time` CSR 与 CLINT `mtime` 的一致性。

## 当前项目定位

这个项目当前已经是一个已可运行的模拟器原型，不只是代码框架。它适合用于理解 ISA、寄存器、取指译码执行流程、异常中断和 MMIO 的基本机制，也能支撑小型 OS / kernel bring-up 的前期工作。

不过它还不是完整系统平台。当前实现仍是单核、以 reference model 为中心，尚未覆盖压缩指令 `C` 扩展、完整 privileged CSR 集、真实磁盘协议或更复杂微架构模型。
