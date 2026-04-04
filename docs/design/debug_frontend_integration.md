# Debug / Frontend 集成设计

## 文档定位

本文档用于说明如何在当前主线上，把 `my-project-3-24` 分支中的 `debug_session/protocol` 与 `frontend` 可视化调试器，以最小兼容方式重新接入仓库。

它承接 [pipeline_core_integration.md](pipeline_core_integration.md) 已完成的 pipeline core 集成，聚焦第二轮“教学演示可用”目标：

- 仓库使用者可以本地启动一个调试服务和前端页面
- 可以对现有 asm、guest、`kernel_alpha` demo 做可视化单步演示
- 不扩大为一个新的通用调试器项目

当前状态补充：

- 本文档对应的第二轮 `debug/frontend` 接入已经完成。
- 当前主线已经具备 `--debug-cli`、本地 Node 调试服务、浏览器前端演示，以及对应的 Node 测试门禁。
- 当前运行与验证方式以 [readme.md](../../readme.md) 和状态文档为准。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/code_self_review_status.md](../status/code_self_review_status.md)
- 相关计划：
  - 无。该轮执行细节已回写到相关 `status` 文档。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，记录 `debug/frontend` 已落地的设计边界与非目标。
- 当前运行方式、验证入口和近期稳定化重点以 [status/mainline_status.md](../status/mainline_status.md)、[status/code_self_review_status.md](../status/code_self_review_status.md) 与 [readme.md](../../readme.md) 为准。

## 背景

截至 `2026-03-25`，当前主线已经具备：

- `ExecutionBackend + FunctionalBackend + PipelineBackend`
- 共享 `InstructionSemantics` 语义层
- `make test` 守住 Phase 1 / guest / `kernel_alpha`
- `make test-pipeline` 守住 asm、host、guest 与 `kernel_alpha` 的 `pipeline` 验证

此前 pipeline core 集成明确排除了：

- `myCPU/src/debug/*`
- `frontend/*`

而远端 `my-project-3-24` 中的调试前端依赖一套比当前主线更大的 debug surface。第二轮不应把那套接口整体搬回主线，而应基于当前主线边界，只补前端演示真正需要的最小只读调试面。

## 本轮目标

本轮目标如下：

1. 为 `functional` 和 `pipeline` 提供统一的只读调试快照。
2. 正式接入 `DebugSession` 与 `--debug-cli` JSON line protocol。
3. 接入本地 Node 调试服务和浏览器端前端页面。
4. 让仓库内现有 asm、`guest_supervisor_demo` 与 `kernel_alpha` demo 可以直接作为演示数据源。
5. 增加最小可维护的 host / frontend 验证门禁。

## 本轮非目标

本轮明确不做以下事情：

1. 不引入断点、条件暂停、差分视图或任意文件上传。
2. 不改变 `functional` / `pipeline` 的执行语义。
3. 不为了调试 UI 重新设计 `Machine` 或 backend 的主执行边界。
4. 不让 debug 路径成为新的 ISA 语义来源。
5. 不引入浏览器框架迁移；前端继续使用原生 HTML / CSS / ESM。

## 集成原则

1. `functional` 继续是默认 reference path，debug 只观察，不干预执行。
2. `pipeline` 的快照来自当前 backend 内部真实状态，不复制一套流水线模拟逻辑。
3. `Machine`、`Bus`、各设备只暴露前端真正需要的最小只读 helper。
4. `step_commit` 继续基于 `instret` 前后变化寻找提交边界，不新增第二套提交语义。
5. CLI、Node server、frontend 都围绕“仓库内现成 demo 可演示”这一目标收口。

## 目标架构

接入后的调试链路如下：

```text
browser
  -> frontend/app/*
  -> frontend/server/debug_server.mjs
      -> debug_server_runtime.mjs
           -> debug_cli_session.mjs
                -> mycpu --debug-cli
       -> DebugSession
       -> Machine
            -> ExecutionBackend::debug_snapshot()
            -> CPU / CSR / Bus / UART / CLINT / PLIC / Storage
```

其中：

- `DebugSession` 负责加载镜像、重置、按 cycle / commit 单步以及汇总快照。
- `debug_protocol` 当前已拆成命令解码、响应序列化与 `CLI loop` 三个内部边界，但对外仍保持统一的 `--debug-cli` JSON line 协议。
- `frontend/server/debug_server.mjs` 负责静态文件服务、HTTP API 入口与 WebSocket 接线。
- `frontend/server/debug_server_runtime.mjs` 负责 session queue、generation guard、run loop 与 terminal 跟踪。
- `frontend/server/debug_cli_session.mjs` 负责 `mycpu --debug-cli` 子进程生命周期、请求队列和 teardown。
- `frontend/app` 只负责状态管理和视图呈现。

## 最小调试数据面

### 1. backend 调试快照

`ExecutionBackend` 补最小只读接口：

- `name()`
- `debug_snapshot()`

其中：

- `FunctionalBackend` 返回空流水线快照，但保留统一字段结构。
- `PipelineBackend` 返回真实的 IF / ID / EX / MEM / WB 五级状态，以及：
  - `stalled`
  - `redirected`
  - `redirect_target`
  - `pending_fetch_fault`
  - `trap_flush`
  - `committed`
  - `empty`

### 2. Machine 调试入口

`Machine` 补最小只读 / 控制接口：

- `cpu()`
- `bus()`
- `uart()`
- `clint()`
- `plic()`
- `storage()`
- `backend()`
- `loaded()`
- `clear_storage_image()`
- `reset_loaded_image()`

这组接口仅供 debug session 使用，不改现有 run path。

### 3. Bus 与设备 introspection

前端需要展示最近一次总线访问与关键 MMIO 设备状态，因此补以下最小 helper：

- `Bus::last_access()`
- `Uart16550`
  - `ier()`
  - `thre_interrupt_asserted()`
  - `output()`
  - `output_size()`
  - `set_mirror_stdout(bool)`
- `Clint`
  - `mtimecmp()`
  - `timer_interrupt_pending()`
- `Plic`
  - `priority()`
  - `source_level()`
  - `source_pending()`
  - `source_claimed()`
  - `machine_enables()`
  - `supervisor_enables()`
  - `machine_threshold()`
  - `supervisor_threshold()`
  - `machine_has_pending()`
  - `supervisor_has_pending()`
- `SimpleStorage`
  - `attached()`
  - `status()`
  - `capacity_blocks()`
  - `lba()`
  - `block_count()`
  - `error_code()`
  - `clear_image()`

这些接口都只读或局部 reset，不改变设备合同。

## Debug CLI 设计

CLI 入口为：

```text
./mycpu --debug-cli
```

首版命令集维持最小集合：

- `load`
- `snapshot`
- `step_cycle`
- `step_commit`
- `reset`
- `quit`

协议格式继续使用单行 JSON 输入 / 输出，便于 Node 子进程直接驱动。

## Frontend 设计

前端保持远端分支的轻量结构：

- `frontend/app`
  - 静态页面
  - 浏览器状态管理
  - 渲染函数
  - pipeline / 面板组件
- `frontend/server`
  - 测试清单
  - 本地 HTTP 服务
  - WebSocket 广播
- `frontend/tests`
  - Node 内置测试

默认运行地址保持：

```text
http://127.0.0.1:4173
```

演示能力保持在以下范围：

- 选择仓库内现有测试
- 切换 `functional` / `pipeline`
- `Load / Run / Pause / Step Cycle / Step Commit / Reset`
- 查看：
  - 五级流水线
  - 最近周期时间线
  - 寄存器变化
  - 关键 CSR / Trap
  - 最近一次总线访问
  - UART / CLINT / PLIC / Storage 状态

## 测试清单策略

旧分支前端测试清单只覆盖 asm 和 `guest_supervisor_demo`。本轮应扩到当前主线已有演示入口：

- 全部 `tests/asm/*.elf`
- `guest_supervisor_demo`
- `kernel_alpha_demo`
- `kernel_alpha_fault_demo`
- 六条 storage 负向 demo
- `kernel_alpha_plic_not_ready_demo`
- `kernel_alpha_timer_not_ready_demo`

这样前端才能直接展示当前主线真正维护的 Phase 1 / post-Phase1 基线。

## 验证门禁

本轮至少守住：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd frontend && node --test`

必要时，把前端测试聚合进根 README 的演示说明，但不把 Node 前端测试并入 `myCPU/Makefile`。

## 风险与控制

### 风险 1：debug surface 反向污染执行路径

控制方式：

- 所有新增接口都保持只读或局部 reset
- 不在 backend 中加入影响语义的“调试专用执行分支”

### 风险 2：前端依赖旧分支字段格式，与主线现状不匹配

控制方式：

- 保持 `DebugSnapshot` 结构稳定
- 由 `debug_protocol` 做统一 JSON 输出，不让前端直接推导 backend 内部结构

### 风险 3：前端可运行，但没有真实门禁

控制方式：

- 加 `debug_cli_smoke`
- 加 Node server / state tests
- 保持 `make test` 与 `make test-pipeline` 为主线 correctness 门禁

## 实施顺序

1. 先补调试快照值对象、backend / machine / bus / device 最小 introspection。
2. 接入 `DebugSession`、`debug_protocol` 和 `--debug-cli`。
3. 用 host smoke 守住 CLI 基本路径。
4. 接入 `frontend/` 目录与 Node tests。
5. 更新 README、`AGENTS.md` 与状态文档。
