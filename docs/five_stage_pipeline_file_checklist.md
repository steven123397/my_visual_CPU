# myCPU 五级流水线落地清单

## 1. 这份文档的用途

这份文档是 [five_stage_pipeline_plan.md](/home/lenovo/Projects/my_visual_CPU/docs/five_stage_pipeline_plan.md) 的下一层细化版本。

前一份文档回答的是：

- 为什么当前仓库不能直接改成五级流水
- 需要哪些核心抽象
- 应该按什么阶段推进

这一份文档回答的是：

- 具体应该新增哪些文件
- 现有哪些文件需要改
- 每个文件应承担什么职责
- 推荐按什么顺序落小 patch

目标是让后续实现能够按“很小的可验证提交”推进，而不是一次性重写。

## 2. 推荐的总体补丁顺序

建议按下面顺序推进：

1. 新增 backend 抽象，不改现有行为
2. 把当前参考路径迁成 `FunctionalBackend`
3. 让 `Machine` 支持 backend 选择
4. 抽离统一语义层和效果对象
5. 改 `AddressSpace` 的 fault 返回模型
6. 新建 `PipelineBackend` 骨架和阶段寄存器
7. 先接整数、控制流、访存
8. 再接 CSR、trap、interrupt
9. 最后补 pipeline 专用测试和 trace

## 3. 第一阶段：backend 骨架

### 3.1 新增 `myCPU/src/exec/backend.h`

职责：

- 定义所有执行后端的统一接口

建议内容：

```cpp
#pragma once

class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;
    virtual void step() = 0;
};
```

这一步的意义：

- 不碰 ISA 语义
- 只先把“执行方式”抽象出来

### 3.2 新增 `myCPU/src/exec/functional_backend.h`

职责：

- 定义当前参考执行器的 backend 包装

建议内容：

```cpp
#pragma once

#include "backend.h"

class CPU;
class Bus;

class FunctionalBackend : public ExecutionBackend {
public:
    FunctionalBackend(CPU& cpu, Bus& bus);
    void step() override;

private:
    CPU& cpu_;
    Bus& bus_;
};
```

### 3.3 新增 `myCPU/src/exec/functional_backend.cpp`

职责：

- 先复制当前 `cpu_step()` 的行为

第一版可以非常直接：

```cpp
void FunctionalBackend::step() {
    cpu_step(cpu_, bus_);
}
```

后续再逐步把 `cpu_step()` 内容内联迁过来。

### 3.4 修改 `myCPU/src/platform/machine.h`

当前 `Machine` 直接持有 CPU、设备和 Bus，但还没有 backend 成员。  
这一文件建议增加：

- backend 选择枚举
- `std::unique_ptr<ExecutionBackend>` 成员

建议新增内容：

```cpp
enum class BackendKind : uint8_t {
    Functional,
    Pipeline,
};
```

以及：

```cpp
void set_backend_kind(BackendKind kind);
```

新增成员：

```cpp
BackendKind backend_kind_{BackendKind::Functional};
std::unique_ptr<ExecutionBackend> backend_;
```

### 3.5 修改 `myCPU/src/platform/machine.cpp`

职责变化：

- 构造平台对象后创建对应 backend
- `run()` 不再直接调 `cpu_step()`

建议新增一个私有 helper：

```cpp
void Machine::rebuild_backend();
```

第一版实现：

- `Functional` -> `FunctionalBackend`
- `Pipeline` -> 先占位，后续切换为 `PipelineBackend`

然后把：

```cpp
cpu_step(cpu_, bus_);
```

改为：

```cpp
backend_->step();
```

### 3.6 修改 `myCPU/src/main.cpp`

职责变化：

- 增加命令行 backend 选择

建议新增参数：

- `--backend functional`
- `--backend pipeline`

第一版先让默认值保持 `functional`。

### 3.7 修改 `myCPU/Makefile`

需要：

- 把新增的 `functional_backend.cpp`
- 以及后续的 `pipeline_backend.cpp`

加入 `CPP_SRCS`

第一阶段先只加入：

- `src/exec/functional_backend.cpp`

## 4. 第二阶段：抽离统一语义层

这一阶段是五级流水能否正确落地的关键。

### 4.1 新增 `myCPU/src/isa/effects.h`

职责：

- 定义所有后端共享的架构效果对象

建议放入：

- `TrapRequest`
- `RegWrite`
- `CsrWrite`
- `MemoryRequest`
- `ControlEffect`
- `InsnEffects`

这一文件应只包含值对象，不包含执行逻辑。

### 4.2 新增 `myCPU/src/isa/execution_context.h`

职责：

- 给语义层提供统一的只读/受控上下文

第一版建议内容：

```cpp
class ExecutionContext {
public:
    ExecutionContext(CPU& cpu, Bus& bus);

    CPU& cpu();
    CoreState& core();
    CsrFile& csr();
    AddressSpace& address_space();
    Bus& bus();

private:
    CPU& cpu_;
    Bus& bus_;
};
```

### 4.3 新增 `myCPU/src/isa/instruction_semantics.h`

职责：

- 统一暴露语义执行入口

建议接口：

```cpp
#pragma once

#include "../decode.h"
#include "effects.h"

class ExecutionContext;

class InstructionSemantics {
public:
    static InsnEffects execute(const Insn& insn, ExecutionContext& ctx);
};
```

### 4.4 新增 `myCPU/src/isa/instruction_semantics.cpp`

职责：

- 按 opcode 把语义派发到各个族

它的职责会接近当前 `cpu.cpp` 里的 `execute()`，但区别是：

- 不直接写 `CoreState`
- 不直接改 `pc`
- 不直接 `enter_trap`
- 只返回 `InsnEffects`

### 4.5 修改 `myCPU/src/exec/integer_ops.h/.cpp`

当前职责：

- 直接写 GPR

目标职责：

- 只计算 ALU 结果
- 产出 `InsnEffects.rd_write`
- 必要时产出 `TrapRequest`

建议方式：

- 先保留旧函数
- 新增一个新的 `build_integer_effects(...)`
- 等功能后端和流水后端都切到新接口后，再删除旧接口

### 4.6 修改 `myCPU/src/exec/control_flow_ops.h/.cpp`

当前职责：

- 直接写返回地址寄存器
- 直接给 `next_pc`

目标职责：

- 产出 `rd_write`
- 产出 `control.redirect_pc`
- 对 branch 给出 taken/not-taken 结果

### 4.7 修改 `myCPU/src/exec/memory_ops.h/.cpp`

当前职责：

- 直接调用 `AddressSpace.load/store()`
- 直接写回寄存器

目标职责：

- EX 只生成有效地址
- 语义层只产出 `MemoryRequest`
- load 的真正取值放到 MEM 阶段
- store 的真正写入放到 MEM/commit 边界

### 4.8 修改 `myCPU/src/exec/system_ops.h/.cpp`

当前职责：

- 直接 `enter_exception`
- 直接 `return_from_mret/sret`
- 直接写 CSR

目标职责：

- `ecall/ebreak/illegal` 产出 `TrapRequest`
- `mret/sret` 产出控制类提交请求
- CSR 指令产出 `CsrWrite + rd_write`
- `halt` 变成 `ControlEffect.halt`

这一块要特别谨慎，因为它最容易把“架构语义”和“backend 调度”重新耦回去。

### 4.9 修改 `myCPU/src/cpu.cpp`

这一文件最终不应再承担核心执行逻辑。

推荐演进方式：

- 第一阶段：保留 `cpu_step()` 给 `FunctionalBackend` 过渡使用
- 第二阶段：把 `execute()` 逻辑迁到 `InstructionSemantics`
- 第三阶段：`cpu.cpp` 只保留轻量 facade / 初始化接口

## 5. 第三阶段：改 AddressSpace fault 模型

### 5.1 修改 `myCPU/src/mem/address_space.h`

建议新增：

```cpp
struct MemoryAccessResult {
    bool ok{false};
    bool fault{false};
    uint64_t value{0};
    uint64_t cause{0};
    uint64_t tval{0};
};
```

并把接口改成类似：

```cpp
MemoryAccessResult fetch32(Bus& bus, uint64_t pc);
MemoryAccessResult load(Bus& bus, uint64_t addr, int size);
MemoryAccessResult store(Bus& bus, uint64_t addr, uint64_t value, int size);
```

### 5.2 修改 `myCPU/src/mem/address_space.cpp`

当前问题：

- `raise_access_fault()`
- `raise_page_fault()`

都会直接调用 `TrapController`

目标：

- 不直接触发 trap
- 而是把 `cause/tval` 封装进返回值

建议实现顺序：

1. 先新增平行接口，不删旧接口
2. backend 切过去后，再逐步移除旧接口

### 5.3 对 `TrapController` 的影响

`TrapController` 本身不需要在这一阶段改行为。  
它仍然保留为：

- trap 进入唯一实现
- trap 返回唯一实现

改变的是调用时机：

- 由 backend 在提交点调用它

## 6. 第四阶段：落 `PipelineBackend` 骨架

### 6.1 新增 `myCPU/src/exec/pipeline_types.h`

建议把流水寄存器定义单独放在这里：

```cpp
struct IfIdReg;
struct IdExReg;
struct ExMemReg;
struct MemWbReg;
```

以及需要的辅助结构：

- forwarding 选择
- stage 状态
- flush/stall 控制信号

### 6.2 新增 `myCPU/src/exec/pipeline_backend.h`

职责：

- 声明五级流水 backend

建议接口：

```cpp
class PipelineBackend : public ExecutionBackend {
public:
    PipelineBackend(CPU& cpu, Bus& bus);
    void step() override;

private:
    void step_wb();
    void step_mem();
    void step_ex();
    void step_id();
    void step_if();

    void commit_next_state();
    void flush_younger_than_ex();
    void inject_bubble_into_idex();

    CPU& cpu_;
    Bus& bus_;
    ...
};
```

### 6.3 新增 `myCPU/src/exec/pipeline_backend.cpp`

第一版职责只做骨架：

- 每个 cycle 调 `bus.tick()`
- 依次推进 `WB -> MEM -> EX -> ID -> IF`
- 暂时可先不完整实现 forwarding
- 先把空 pipeline 推进和 stage register 生命周期写对

推荐最小起步：

- 先支持“直通执行 + 无 hazard 程序”
- 再逐步补 forwarding/stall/flush

### 6.4 修改 `myCPU/src/platform/machine.cpp`

这时把 `PipelineBackend` 真正接上 `backend_kind == Pipeline` 分支。

## 7. 第五阶段：hazard 与控制逻辑

### 7.1 `PipelineBackend` 内的 forwarding helper

建议在 `pipeline_backend.cpp` 内增加：

- `resolve_forwarded_rs1()`
- `resolve_forwarded_rs2()`
- `detect_load_use_hazard()`

### 7.2 第一版 forwarding 范围

建议覆盖：

- `EX/MEM -> EX`
- `MEM/WB -> EX`

先不在 ID 级做复杂旁路。

### 7.3 load-use stall

建议行为：

- 冻结 `IF/ID`
- PC 不推进
- 向 `ID/EX` 注入 bubble

### 7.4 branch / jump flush

建议行为：

- 在 EX 级判断 redirect
- flush `IF/ID` 与 `ID/EX`
- 更新 fetch PC

## 8. 第六阶段：系统指令、CSR 与 trap

### 8.1 修改 `PipelineBackend` 提交逻辑

在 `WB` 或 commit 边界，需要集中处理：

- GPR 写回
- CSR 写回
- halt
- redirect 提交
- `mret/sret`
- trap 请求

### 8.2 trap 提交顺序建议

对于一条即将提交的 faulting 指令：

1. younger 阶段全部作废
2. 不提交该 faulting 指令之后的任何结果
3. 调 `TrapController::enter_exception(...)`
4. pipeline 清空
5. fetch 从 trap vector 重新开始

### 8.3 interrupt 建议

建议在“每个 cycle 的提交边界”判断：

- 当前是否有可递送 interrupt
- 当前是否允许进入 interrupt

若进入：

- 先清空 pipeline
- 再调 `TrapController::enter_interrupt(...)`

## 9. 第七阶段：CLI 与调试支持

### 9.1 修改 `myCPU/src/main.cpp`

建议最终支持：

```bash
./mycpu --backend functional image.elf
./mycpu --backend pipeline image.elf
```

### 9.2 新增提交 trace

建议新增一个轻量 trace 选项，例如：

```bash
./mycpu --backend pipeline --trace-commit image.elf
```

提交 trace 至少记录：

- cycle
- commit pc
- insn raw
- rd / rd value
- trap cause
- privilege mode

### 9.3 推荐新增文件

如果不想污染 `main.cpp`，可以考虑新增：

- `myCPU/src/debug/commit_trace.h`
- `myCPU/src/debug/commit_trace.cpp`

这不是五级流水必需，但会极大降低调试难度。

## 10. 第八阶段：测试落地

### 10.1 修改 `myCPU/Makefile`

建议后续新增专门的 pipeline 测试目标，例如：

- `make test-functional`
- `make test-pipeline`
- `make test-diff`

### 10.2 第一批双后端共用测试

建议先跑：

- `hello`
- `sum`
- `control_flow`
- `loads_signed_unsigned`
- `alu_word`
- `branches_signed_unsigned`
- `csr_trap`
- `exception_traps`

### 10.3 新增 pipeline 专用汇编测试

建议新加：

- `pipeline_forward_ex.S`
- `pipeline_forward_mem.S`
- `pipeline_load_use_stall.S`
- `pipeline_branch_flush.S`
- `pipeline_jump_flush.S`
- `pipeline_precise_exception.S`

第一批不用多，但必须覆盖流水线专有风险点。

## 11. 每个阶段的完成标准

### 阶段 A 完成标准

- backend 抽象存在
- `FunctionalBackend` 可运行
- 默认行为与当前仓库一致

### 阶段 B 完成标准

- 语义层不再直接提交架构状态
- `InsnEffects` 可表达基础整数 / branch / memory / CSR 语义

### 阶段 C 完成标准

- `AddressSpace` fault 不再直接 trap
- backend 可以决定何时提交异常

### 阶段 D 完成标准

- `PipelineBackend` 能推进 5-stage 寄存器
- 可跑无 hazard 的简单程序

### 阶段 E 完成标准

- forwarding / stall / flush 可用
- 基础整数、branch、访存程序能正确运行

### 阶段 F 完成标准

- trap / interrupt / CSR 在 pipeline 下可顺序提交
- 与功能后端能做差分验证

## 12. 当前最适合作为第一批 patch 的文件

如果马上开始编码，我建议第一轮只碰这些文件：

- 新增 [backend.h](/home/lenovo/Projects/my_visual_CPU/myCPU/src/exec/backend.h)
- 新增 [functional_backend.h](/home/lenovo/Projects/my_visual_CPU/myCPU/src/exec/functional_backend.h)
- 新增 [functional_backend.cpp](/home/lenovo/Projects/my_visual_CPU/myCPU/src/exec/functional_backend.cpp)
- 修改 [machine.h](/home/lenovo/Projects/my_visual_CPU/myCPU/src/platform/machine.h)
- 修改 [machine.cpp](/home/lenovo/Projects/my_visual_CPU/myCPU/src/platform/machine.cpp)
- 修改 [main.cpp](/home/lenovo/Projects/my_visual_CPU/myCPU/src/main.cpp)
- 修改 [Makefile](/home/lenovo/Projects/my_visual_CPU/myCPU/Makefile)

这轮 patch 的目标只有一个：

**让当前功能参考核先成为一个显式 backend。**

做到这一步后，后续五级流水改造就不会直接压在现有参考核主路径上。

## 13. 结论

在当前仓库结构下，最稳妥的实现方式不是“直接做五级流水”，而是先做一层“执行后端化”。

具体说：

- 先把当前功能核包装成 `FunctionalBackend`
- 再抽语义
- 再改 fault 返回模型
- 最后落 `PipelineBackend`

只要按这个顺序推进，每一步都可以保持仓库可编译、可运行、可验证，也更符合当前项目强调的“保留简单参考模型、逐步引入新执行后端”的路线。
