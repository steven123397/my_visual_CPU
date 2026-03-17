# myCPU C++ 重构方案

## 1. 目的与约束

当前 `myCPU` 已经是一个可运行的 RISC-V 功能模拟器原型，具备：

- ELF64 / flat binary 加载
- RV64I 基础整数执行与 RV64M 乘除扩展
- CSR 访问
- M-mode trap / interrupt 基础处理
- UART MMIO 输出
- CLINT 定时器基础支持

下一阶段不应直接在当前小型 C 原型上继续把特权、MMU、设备平台、后续执行后端全部叠加到 `cpu.c` 中。更合理的做法是先完成一次有结构收益的 C++ 重构，建立清晰的模块边界和状态归属，为后续 Phase 1 的 OS bring-up 功能打地基。

这次重构必须满足以下约束：

- 保留一个简单、正确、可调试的 ISA 级参考执行路径
- 不做“只改语言、不改结构”的表面迁移
- 在迁移过程中保持仓库始终可构建、可运行
- 后续新增特权、MMU、设备和执行后端时，不重复拷贝指令语义

## 2. 当前 C 原型的结构问题

当前实现虽然已经能工作，但结构扩展性有限，主要问题如下：

### 2.1 `cpu.c` 职责过重

当前 [cpu.c](../myCPU/src/cpu.c) 同时承担了：

- 架构状态读写
- CSR 访问
- 指令执行语义
- trap / interrupt 检查
- 时钟推进

这在原型阶段可接受，但继续往里加入：

- `M/S/U` 特权级
- trap delegation
- `satp` / Sv39 / page fault
- PLIC / storage / bus
- 多种执行后端

会迅速把 `cpu.c` 变成难以维护的中心文件。

### 2.2 外围入口已完成第一步解耦，但状态边界仍需继续收紧

当前 [main.cpp](../myCPU/src/main.cpp) 已经通过 `Machine` 组织加载和运行流程，`g_mem` 这类全局内存入口也已经从执行路径中移除。这一步已经带来了明确的结构收益，但下一阶段仍然要继续把边界收紧，避免新的隐式耦合重新出现：

- `Machine` 仍然直接驱动 `cpu_step()` 和现有 C 核心语义
- `Bus` 目前还是对 `memory.c` 的薄适配层
- CPU、CSR、trap 状态还没有拆成更明确的对象
- 后续多核、MMU、设备扩展仍然需要更清晰的依赖注入

### 2.3 指令语义与执行控制混在一起

当前 `cpu_step()` 直接完成：

- 更新时间
- 检查中断
- 取指
- 译码
- 执行

当未来引入 single-cycle / multi-cycle / pipeline backend 时，如果不提前拆开，几乎必然会把同一条指令的语义复制到多个执行路径里。

### 2.4 设备平台缺少统一抽象

当前 [memory.c](../myCPU/src/memory.c) 用地址判断直接分发 RAM / UART / CLINT。这对两个简单设备足够，但不适合继续扩展到：

- PLIC
- block device
- ROM / firmware region
- 更复杂的平台总线

### 2.5 Trap / CSR / Privilege 尚未形成独立边界

目前 trap 和 CSR 已经从文件层面独立出一部分，但状态仍然是“CPU 大数组 + 若干函数”的组织方式。随着 supervisor 模式和虚拟内存引入，最好把：

- 通用寄存器和 PC
- CSR 文件
- privilege 状态
- trap routing

区分成更明确的组件。

## 3. 重构的总体目标

重构后的 C++ 架构要服务于两个近中期目标：

1. 继续作为简单、正确、可调试的参考模拟器。
2. 为后续 OS bring-up 所需的特权、MMU、设备平台扩展预留结构。

具体目标如下：

- 用显式对象和依赖关系替代全局状态
- 把“架构语义”与“执行调度方式”分离
- 把 RAM / MMIO / 设备挂接统一到总线模型
- 把 CSR、trap、privilege、MMU 从“大一统 CPU 文件”中拆出
- 保证未来新增 backend 时仍能复用一套 instruction semantics

## 4. 建议的目标目录结构

建议将 `myCPU/src` 逐步演进为如下结构：

```text
myCPU/
├── include/mycpu/
│   ├── arch/
│   │   ├── core_state.h
│   │   ├── csr_file.h
│   │   ├── privilege.h
│   │   └── trap_types.h
│   ├── isa/
│   │   ├── decoded_insn.h
│   │   ├── decoder.h
│   │   └── semantics.h
│   ├── exec/
│   │   ├── backend.h
│   │   ├── functional_backend.h
│   │   └── step_result.h
│   ├── mem/
│   │   ├── bus.h
│   │   ├── memory_interface.h
│   │   ├── ram.h
│   │   └── mmu.h
│   ├── devices/
│   │   ├── device.h
│   │   ├── uart16550.h
│   │   ├── clint.h
│   │   └── plic.h
│   ├── loader/
│   │   ├── elf_loader.h
│   │   └── binary_loader.h
│   ├── platform/
│   │   ├── machine.h
│   │   └── memory_map.h
│   └── util/
│       ├── bit_ops.h
│       └── log.h
├── src/
│   ├── arch/
│   ├── isa/
│   ├── exec/
│   ├── mem/
│   ├── devices/
│   ├── loader/
│   ├── platform/
│   └── main.cpp
└── tests/
    ├── asm/
    ├── unit/
    └── integration/
```

这个结构的关键不是“目录多”，而是把后续复杂度增长点提前隔离开。

## 5. 核心模块设计

### 5.1 `CoreState`: 纯架构状态

职责：

- 32 个通用寄存器
- `pc`
- 当前 privilege level
- 周期计数、退休计数等基础统计

建议保持它尽量“哑数据化”，避免把设备、MMU、加载器等外部依赖塞进来。

示意：

```cpp
class CoreState {
public:
    uint64_t read_gpr(uint32_t idx) const;
    void write_gpr(uint32_t idx, uint64_t value);

    uint64_t pc() const;
    void set_pc(uint64_t value);

    PrivilegeMode privilege() const;
    void set_privilege(PrivilegeMode mode);

    uint64_t cycle() const;
    void advance_cycle(uint64_t n = 1);

private:
    std::array<uint64_t, 32> gpr_{};
    uint64_t pc_{0};
    PrivilegeMode priv_{PrivilegeMode::Machine};
    uint64_t cycle_{0};
};
```

意义：

- 把“核心架构状态”从执行逻辑里剥离出来
- 后续多 backend 共享同一份状态定义
- 更容易做快照、trace 和差分测试

### 5.2 `CsrFile`: CSR 存储与访问规则

职责：

- 保存 CSR 状态
- 处理只读 / 可写 / 屏蔽位规则
- 提供按特权级检查的读写接口
- 暴露 `mstatus/mie/mip/mtvec/mepc/mcause/mtval` 等语义化访问器

不建议继续只用一个裸 `uint64_t csr[4096]` 暴露给所有模块直接读写。可以内部仍然使用数组存储，但对外接口要更严格。

示意：

```cpp
class CsrFile {
public:
    uint64_t read(uint16_t addr) const;
    void write(uint16_t addr, uint64_t value);

    uint64_t mstatus() const;
    void set_mstatus(uint64_t value);

    bool interrupt_enabled_machine() const;
};
```

意义：

- 为 supervisor CSR 扩展预留位置
- 后续 trap delegation / interrupt pending 逻辑可逐步集中到这里

### 5.3 `TrapController`: trap / interrupt 路由

职责：

- 异常进入
- 中断进入
- `mret/sret`
- `medeleg/mideleg` 处理
- 设置 `epc/cause/tval/status` 等寄存器

现在的 [trap.c](../myCPU/src/trap.c) 逻辑很适合作为这个组件的起点，但要从“函数”升级为“依赖显式的对象”。

示意：

```cpp
class TrapController {
public:
    TrapController(CoreState& core, CsrFile& csr);

    void enter_exception(uint64_t cause, uint64_t tval);
    void enter_interrupt(uint64_t cause);
    void mret();
    void sret();
};
```

意义：

- 后续加入 supervisor mode 时，不会把 trap 逻辑重新撒回 `cpu.cpp`
- 便于独立写 trap 行为单测

### 5.4 `Decoder` 与 `DecodedInsn`

职责：

- 仅负责把原始指令转换成结构化结果
- 不负责执行副作用

现在 [decode.c](../myCPU/src/decode.c) 已经比较接近这个目标。C++ 重构中建议保持译码器纯函数化：

```cpp
DecodedInsn decode32(uint32_t raw);
```

未来如果支持 `C` 扩展，可扩展成：

```cpp
DecodedInsn decode(FetchPacket packet);
```

其中 `FetchPacket` 可携带 16-bit / 32-bit 信息。

### 5.5 `InstructionSemantics`: 指令语义唯一来源

这是整个重构里最关键的点。

职责：

- 根据 `DecodedInsn` 和执行上下文，描述一条指令的架构效果
- 不包含 pipeline stage、等待周期、旁路、乱序等时序细节

建议的做法是把当前 `execute()` 拆成：

- `InstructionSemantics`
- `ExecutionContext`

示意：

```cpp
class ExecutionContext {
public:
    CoreState& core();
    CsrFile& csr();
    Bus& bus();
    TrapController& trap();
};

class InstructionSemantics {
public:
    static StepResult execute(const DecodedInsn& insn, ExecutionContext& ctx);
};
```

`StepResult` 可以显式描述：

- 正常推进 PC
- 跳转到新 PC
- 进入 trap
- 请求停机

意义：

- 后续 functional backend、multi-cycle backend、pipeline backend 都调用同一套语义
- “指令做什么” 与 “指令如何被调度执行” 被拆开

### 5.6 `Bus` + `Device` + `Ram`

职责：

- 提供统一地址空间访问入口
- 把 RAM 和 MMIO 设备都挂在总线上

建议抽象：

```cpp
class MemoryInterface {
public:
    virtual ~MemoryInterface() = default;
    virtual uint64_t load(uint64_t addr, unsigned size) = 0;
    virtual void store(uint64_t addr, uint64_t value, unsigned size) = 0;
};

class Device {
public:
    virtual ~Device() = default;
    virtual bool contains(uint64_t addr) const = 0;
    virtual uint64_t load(uint64_t addr, unsigned size) = 0;
    virtual void store(uint64_t addr, uint64_t value, unsigned size) = 0;
};
```

`Bus` 负责地址分发，`Ram`、`Uart16550`、`Clint`、未来的 `Plic` / `BlockDevice` 都按统一接口接入。

意义：

- 去掉 `g_mem`
- 设备接入不再需要不断往 `memory.c` 里堆 `if (addr >= ...)`
- 更适合以后构建一个可描述的平台对象

### 5.7 `Mmu`

第一阶段可以先只实现 “bare mode passthrough”：

- 接收虚拟地址
- 当前直接返回物理地址

等到 Sv39 引入时，再扩展：

- 页表遍历
- 权限检查
- A/D 位
- page fault / access fault
- TLB

这样做的意义是提前把地址翻译放进访问路径，但不在第一步就实现完整分页。

### 5.8 `Machine`

`Machine` 表示一台完整模拟机器，而不是单个核心。

职责：

- 聚合 `CoreState`
- 聚合 `CsrFile`
- 聚合 `TrapController`
- 聚合 `Bus`
- 挂接 `Ram/UART/CLINT/...`
- 提供 `load_image()`、`step()`、`run()` 等统一入口

示意：

```cpp
class Machine {
public:
    Machine(MachineConfig config);

    void load_elf(const std::string& path);
    void load_binary(const std::string& path, uint64_t addr);

    void step();
    void run();

private:
    CoreState core_;
    CsrFile csr_;
    Bus bus_;
    Ram ram_;
    Uart16550 uart_;
    Clint clint_;
    TrapController trap_;
    std::unique_ptr<ExecutionBackend> backend_;
};
```

意义：

- `main.cpp` 只负责 CLI，不再直接拼接所有细节
- 后续多核时可以演进为 `Machine` 持有多个 `Hart`

### 5.9 `ExecutionBackend`

这是为 Phase 2 提前留好的接口。

```cpp
class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;
    virtual void step() = 0;
};
```

初期只有：

- `FunctionalBackend`

后续再加：

- `MultiCycleBackend`
- `PipelineBackend`

要求是所有 backend 都调用同一套 `InstructionSemantics`。

## 6. 建议的对象依赖关系

建议采用如下方向的依赖：

```text
main.cpp
  -> Machine
      -> FunctionalBackend
          -> Decoder
          -> InstructionSemantics
              -> ExecutionContext
                  -> CoreState
                  -> CsrFile
                  -> TrapController
                  -> Bus
                      -> Ram / Uart / Clint / ...
```

原则：

- 高层模块依赖低层抽象
- 设备层不反向依赖执行后端
- 指令语义不直接依赖具体 CLI / loader
- `CoreState` 不依赖 `Bus`

## 7. 迁移路线

为了避免“大爆炸式重写”，建议按以下阶段推进。

当前仓库已经完成了阶段 0 和阶段 1 的第一步落地：`main.cpp`、`Machine`、`Bus`、`Ram` 已经存在，运行路径也已经摆脱全局 `g_mem`。下面的路线按“已完成基础、继续深化边界”的视角更新。

### 阶段 0：先建立 C++ 构建骨架（已完成）

目标：

- 把构建系统改成可同时编译 `.c` / `.cpp`
- 新增 `include/` 和 `src/*` 目录骨架
- 先不改已有功能行为

建议动作：

- 将 `Makefile` 扩展为 `CXX = g++` 或直接切到 CMake
- 增加最小 C++ 编译单元，例如 `main.cpp` 或 `machine.cpp`
- 保留旧 C 代码可运行

完成标志：

- 仓库能同时编译旧 C 实现和新 C++ 骨架

### 阶段 1：先迁移外围，不碰核心语义（已部分完成）

当前状态：

- 已有 `main.cpp`
- 已有 `Machine`
- 已有 `Bus`
- 已有 `Ram`
- `cpu_step()` 已显式接收 `Memory*`
- 运行路径已经摆脱全局 `g_mem`

这一阶段剩余的主要工作：

- 继续把 `elf_loader.c` 收敛到更明确的 loader 边界
- 让 `Bus` 从 `memory.c` 适配层逐步演进为真正的平台总线
- 保持现有参考执行路径可构建、可运行

完成标志：

- 在不改变现有 ISA 语义的情况下，外围对象边界继续稳定收敛
- `Bus` 和 loader 的职责不再只是对旧 C 模块的命名包装

### 阶段 2：拆分架构状态与 trap / CSR

优先迁移：

- `CPU` 结构体
- CSR 读写
- trap 入口/返回

建议动作：

- `CPU` 拆为 `CoreState + CsrFile`
- `trap.c` 拆为 `TrapController`
- `cpu_step()` 中的中断检查逻辑迁到 backend 或 machine tick 中

完成标志：

- 不再有一个“大数组 + 大函数”的中心模型
- trap 与 CSR 具备单独测试入口

### 阶段 3：拆出统一指令语义层

这是最重要的一步。

建议动作：

- 保留现有 `decode` 思路
- 把 `execute()` 拆成按指令类别组织的 `InstructionSemantics`
- 让 `FunctionalBackend` 负责 step 流程控制

可以按文件拆分，例如：

- `isa/semantics_integer.cpp`
- `isa/semantics_branch.cpp`
- `isa/semantics_load_store.cpp`
- `isa/semantics_csr.cpp`
- `isa/semantics_system.cpp`

完成标志：

- 指令语义不再嵌在 backend 内
- `FunctionalBackend` 主要只做 fetch / decode / dispatch / cycle accounting

### 阶段 4：引入最小 MMU 抽象

建议动作：

- 所有 load/store/fetch 统一走地址翻译接口
- 第一版只实现 bare mode

完成标志：

- 后续 Sv39 只需扩展 MMU，而不是重写所有访存路径

### 阶段 5：补测试框架

建议新增：

- decoder 单元测试
- CSR 行为单元测试
- trap 行为单元测试
- RAM / UART / CLINT 设备测试
- 现有汇编冒烟测试保持不变

完成标志：

- 重构后的行为能和旧实现对比验证

### 阶段 6：删除旧 C 入口，保留参考行为

只有在新 C++ functional backend 通过全部现有测试后，才删除旧 C 入口。

注意：

- 删除的是“旧实现代码路径”
- 不是删除“参考功能模型”

## 8. 指令语义层的组织建议

为了避免未来再次集中到一个巨大的 `execute()`，建议从一开始就按功能分文件：

- `semantics_integer.cpp`
- `semantics_muldiv.cpp`
- `semantics_branch.cpp`
- `semantics_load_store.cpp`
- `semantics_csr.cpp`
- `semantics_system.cpp`

每个文件内部按 opcode / funct3 / funct7 分派，但最终都通过统一的 `InstructionSemantics::execute()` 暴露入口。

这样做的好处：

- 便于后续扩展 RV64A / supervisor / page fault
- 单元测试目标更清晰
- backend 不会重新实现这些语义

## 9. 状态所有权建议

建议使用以下所有权规则：

- `Machine` 拥有平台级对象的生命周期
- `FunctionalBackend` 通过引用访问 `CoreState/CsrFile/Bus/TrapController`
- 设备对象由 `Machine` 或 `Bus` 以 `std::unique_ptr` 管理
- `DecodedInsn`、`StepResult` 等值对象按值传递

不建议：

- 到处共享可写裸指针
- 使用单例或新的全局变量替代 `g_mem`
- 让设备直接修改核心状态，除非通过明确定义的中断线路或回调机制

## 10. 推荐的近期文件级落地顺序

如果要开始真正动手，建议按下面顺序提交小 patch：

1. 新建 `include/mycpu/` 和 `src/` 下的 C++ 骨架文件
2. 巩固 `Machine`、`Bus`、`Ram` 的边界，继续减少对旧 C 适配层的直接耦合
3. 保持 `main.cpp` 只承担 CLI 和启动职责
4. 把 `elf_loader.c` 迁到 `ElfLoader`
5. 把 `CPU` 拆成 `CoreState` 和 `CsrFile`
6. 把 `trap.c` 迁成 `TrapController`
7. 把 `execute()` 拆成 `InstructionSemantics`
8. 引入 `FunctionalBackend`
9. 加单元测试
10. 再开始 supervisor / MMU 扩展

这个顺序的核心是：

- 先拆依赖和状态
- 再拆语义
- 最后再扩功能

## 11. 风险与控制措施

### 风险 1：重构范围过大，长期无法回到可运行状态

控制措施：

- 每一步都保持 `make` 和基本测试可运行
- 每个阶段单独提交

### 风险 2：C++ 化但仍保留原有结构问题

控制措施：

- 每迁一个模块都问一个问题：边界是否更清晰，依赖是否更显式
- 如果只是把 `struct` 改成 `class`，但职责没变，就不算完成

### 风险 3：为未来 backend 过度设计

控制措施：

- 第一版只实现 `FunctionalBackend`
- 仅保留对 `ExecutionBackend` 的最小抽象

### 风险 4：语义层拆分不彻底

控制措施：

- backend 中不允许写具体指令效果
- backend 只负责调度，不负责定义 ISA 行为

## 12. Definition of Done

这次 C++ 重构完成的标志不应是“所有功能都已经升级”，而应是：

- 已有 ELF / binary 加载、RV64I/M、CSR、M-mode trap、UART、CLINT 功能都仍然可运行
- 没有全局 `g_mem` 之类的隐式共享状态
- 存在 `Machine`、`Bus`、`CoreState`、`CsrFile`、`TrapController`、`FunctionalBackend` 的清晰边界
- 指令语义已经从 backend 中拆出，形成唯一语义来源
- 后续加入 supervisor / MMU / 设备时不需要再次推翻整体结构

## 13. 最直接的下一步

如果按照“最小风险、最大结构收益”的原则，在当前仓库状态下最直接的下一步实现任务是：

**保持现有 `Machine + Bus + Ram` 参考路径不变，先把 `CPU` 状态拆成 `CoreState + CsrFile`，再为 `TrapController` 建立明确对象边界。**

原因：

- `Machine + Bus + Ram` 这一步已经完成，不需要再重复作为起点
- `CPU` 状态边界仍然是当前最集中的复杂度来源
- 这一步能直接为 supervisor、MMU 和更细的 trap/CSR 语义拆分铺路
- 做完后仓库仍然保持一个简单、可运行的参考执行器
