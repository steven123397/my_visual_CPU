# AGENTS.md

## 适用范围

本文件适用于 [myCPU](.) 子树下的 simulator 主体代码、平台设备、加载路径、测试与构建逻辑。

如果工作落在 guest runtime 子树，请继续阅读：

- [guest/AGENTS.md](guest/AGENTS.md)

需要看当前主线状态、active wave、近端 blocker 或当前优先级时，只看：

- [../docs/status/mainline_status.md](../docs/status/mainline_status.md)

## 稳定边界

- 共享 `InstructionSemantics + functional backend` 仍是 ISA 真值来源。
- `pipeline`、未来 `JIT` 和其他执行形态只能消费共享语义，不得复制 ISA 解释。
- CPU 访存路径保持：
  - `CPU -> AddressSpace -> Bus -> Ram/Device`
- 平台事件路径保持：
  - `Device::tick() -> Bus::tick() -> TrapController`
- `debug/frontend` 只消费 machine/backend/device 的只读快照，不反向成为执行语义来源。
- `simple_storage` 与 `virtio-blk` 当前并存；显式选择真实 transport 时，guest 可见语义仍应通过统一平台合同进入。
- `ExecutionProfile`、`memory observation` 和 `shadow_cache` 当前只做观测，不改变 guest 可见行为。

## 模块地图

- [src/main.cpp](src/main.cpp)
  CLI 参数、镜像选择和启动入口。
- [src/platform](src/platform)
  `Machine`、平台组装、镜像加载和执行循环。
- [src/arch](src/arch)
  `CoreState`、`CsrFile` 和架构状态。
- [src/mem](src/mem)
  `Ram`、`Bus`、`memory_region`、`AddressSpace`。
- [src/devices](src/devices)
  UART、CLINT、PLIC、storage、virtio、AI accelerator 等设备对象。
- [src/trap.cpp](src/trap.cpp)
  trap / interrupt 路由与返回。
- [src/loader](src/loader)
  ELF / binary 装载边界。
- [src/debug](src/debug)
  debug snapshot、debug session 与 `--debug-cli` 协议。
- [src/exec](src/exec)
  `pipeline`、predictor、`rename + ROB + LSQ` 与相关 backend 路径。
- [tests/asm](tests/asm)
  ISA / privilege / MMU / trap 合同的汇编回归。
- [tests/unit](tests/unit)
  host-side 单元回归。
- [tests/host](tests/host)
  host smoke、debug CLI、workload guardrail 和 `pipeline` 回归。
- [workloads](workloads)
  `xv6`、`linux_proto` 与其他外部 workload 接线。

## 局部规则

- 保留一个简单、正确、可调试的 reference core。
- 语义修复优先落在共享语义层与公共 simulator 边界，不在 backend 私有路径偷修。
- 不要把一次性 smoke 需求固化成只服务单一 workload 的临时特判。
- 任何支持声明都必须以真实实现和回归验证为准。
- 触及 workload harness、probe、build glue 或 runtime guardrail 时，先确认当前 contract 是不是已经写进 `mainline_status`；不要再并行维护新的状态口径。
- 修改文档时，`myCPU/AGENTS.md` 只补规则、方法、验证要求和稳定边界，不回写实时 checkpoint。

## 改动时的默认检查点

- ISA / CSR / privilege 修复：
  优先补 `tests/asm/*` 或 `tests/unit/*` 的最窄红灯，再改共享语义层。
- `pipeline` 修复：
  优先补 `tests/host/*pipeline*`、`debug_cli_smoke` 或相关 differential smoke。
- workload / debug CLI / probe 改动：
  优先补 host 单测或最窄 probe guardrail，再扩到完整 `make test` / `make test-pipeline`。
- 文档治理改动：
  先更新 `docs/status/mainline_status.md`、相关 `AGENTS.md` 和 [../docs/index.md](../docs/index.md)，再考虑其他引用。

## 验证要求

只要触及以下路径之一：

- `src/cpu.cpp`
- `src/trap.cpp`
- `src/arch/*`
- `src/mem/*`
- `src/devices/*`
- `src/loader/*`
- `tests/asm/*`
- `tests/unit/*`

默认都应守住：

- `cd myCPU && make test`

如果触及以下任一路径：

- `src/main.cpp`
- `src/platform/machine.cpp`
- `src/debug/*`
- `src/exec/*`
- `guest/*`
- `tests/host/*`
- `workloads/*`

还应额外守住：

- `cd myCPU && make test-pipeline`

如果改动主要集中在 loader、debug CLI、workload harness 或 Linux / `xv6` guardrail，还应至少关注：

- `cd myCPU && make test-unit-binary_loader`
- `cd myCPU && make test-unit-machine_loader_reset`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-virtio_blk_smoke`
- `cd myCPU && make test-host-xv6_boot_smoke`
- `cd myCPU && make test-host-xv6_shell_smoke`
- `cd myCPU && make run-workload-xv6`

如果改动主要集中在 `rename / ROB / LSQ / speculation / vector pipeline`，还应至少关注：

- `cd myCPU && make test-host-physical_register_file_smoke`
- `cd myCPU && make test-host-rename_map_smoke`
- `cd myCPU && make test-host-reorder_buffer_smoke`
- `cd myCPU && make test-host-load_store_queue_smoke`
- `cd myCPU && make test-host-pipeline_rename_commit_smoke`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-vector_pipeline_smoke`
- `cd myCPU && make test-host-vector_cnn_smoke`
