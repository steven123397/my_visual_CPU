# 当前项目优先级路线图

## 文档定位

本文档只保留当前仍然开放的优先级判断，不重复记录已经完成的整段执行过程。

它不替代 [mainline_status.md](mainline_status.md)。`mainline_status` 负责回答“现在主线是什么状态”，本文档负责回答“下一轮最值得做什么，以及什么不该抢跑”。

## 关联文档

- 相关设计：
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [../design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [xv6_linux_jit_status.md](xv6_linux_jit_status.md)
- 当前计划：
  - [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)
- 已完成计划归档：
  - [../plan/history_plan.md#phase4-prep1-bus-memory-region-plan](../plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)
  - [../plan/history_plan.md#vector-frontend-visualization-plan](../plan/history_plan.md#vector-frontend-visualization-plan)
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)

## 当前判断

- `P0` correctness 修补、`P1` 结构收口和 `P2` 首轮验证补洞都已经完成；自 `2026-04-21` 起，当前已经不再停留在“继续评估要不要切主线”，而是正式激活了标准 OS bring-up 切换线。
- 当前主线的优先级判断已经改为：先落 `RV64A + virtio + CSR / privilege 补全 + xv6-riscv workload harness / bring-up`，并让这轮结构决策直接服务后续 `Linux` 与 `JIT / DBT`。
- 默认延续线没有被丢弃：`V4`、`P4-prep-1`、`kernel_alpha`、`debug/frontend` 与既有回归矩阵继续作为当前主线的 guardrail、观测基础与回归支架。
- `future_expansion_roadmap_design.md` 仍然是路线菜单；当前真正已经激活的执行方案，以 [xv6_linux_jit_status.md](xv6_linux_jit_status.md)、[mainline_status.md](mainline_status.md) 和 [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md) 为准。
- `2026-04-23` 也已把独立 `MMIO NPU / TPU-like` tensor accelerator 方向收口成正式 design / status / wave 1 plan：它采用静态子图、`scratchpad + DMA`、host / guest 共用设备 ABI 的路线，但当前仍只作为未来候选方向保留，不覆盖 `xv6 / Linux` 这条已激活主线。
- `2026-04-22` 第一轮 A / B / C / D foundation 与首轮 B / C post-integration follow-up 都已进入主工作树；同日进一步的 A / B bug-driven follow-up 先把 `xv6` 推过旧的 early-boot trap，随后又在真实 `virtio-blk` board path 上稳定到 shell，并落下了 Linux-facing `flat/payload/set_gpr + linux_proto profile` foundation；后续同日的 Linux bring-up follow-up 又补上最小 `linux_sbi_shim`、PLIC contiguous source window contract，以及 repo-generated `mycpu_virt.dtb` `chosen/cmdline/timebase` 合同，把真实 Linux 推到 `Unpacking initramfs...`、`devtmpfs: initialized` 与 `xor: measuring software checksum speed` checkpoint。因此“下一轮最值得做什么”的答案已进一步收敛到：继续用 `xv6` shell 当 guardrail，沿真实 Linux boot path 做 bug-driven hardening。

## 当前优先级

### 1. 常态维护仍是默认前提

- 继续维护 reference correctness 矩阵，不让 illegal / MMIO / ELF / CSR / Sv39 合同回退。
- 继续守住 `kernel_alpha` 十条 guest 基线、`guest_supervisor_demo` 与当前 debug / frontend 链路。
- 继续把新增 bug 的最小持久回归补到已有门禁中，而不是重新打开低收益的大规模回归扩面。

### 2. 用 `xv6` shell 守住真实 `virtio` board path，同时把近端重点转向 Linux-facing foundation

- PLIC source wiring、`Machine` block transport 选择与 `mycpu_virt` board profile 切换都已完成，`xv6` 已开始真实消费 `virtio-mmio + virtio-blk` contract。
- 当前最窄、最值钱的下一步不再是继续证明 `xv6` 能不能到 shell，而是把已经稳定的 shell/userland/filesystem 路径作为 guardrail，用它约束后续 Linux-facing 变更。
- 这条线仍然应该优先保持“可复用的 external workload 入口 + 稳定 smoke”，而不是为了快跑演示去写一次性特判。
- `Linux` 后续会直接复用这层 harness / profile 结构；当前已经落下 generic `flat/payload/set_gpr`、`linux_proto`、最小 `linux_sbi_shim` 与 repo-generated `dtb/chosen/cmdline/timebase` contract，并且真实 `Image + initrd` 已能推进到 `Unpacking initramfs...`、`devtmpfs: initialized`、`workingset`、`jitterentropy` 与 `xor` checkpoint；同日 probe harness 侧的长 UART wait 也已收口成增量搜索，下一步更该沿这条真实 boot path 继续补后续 blocker，而不是继续在 `xv6` 自身 shell 用例上横向扩面。

### 3. A / B 两条 contract 线都进入 bug-driven 支撑位

- `virtio` 这条线当前不再缺 transport / queue / board wiring；后续只随着 `xv6` 暴露的新平台缺口补最小 contract，不主动扩大更多 `virtio` 设备或额外 platform 特性。
- A 线同样不再是“先要不要合”的问题，而是随着真实 workload 暴露新的缺口，最小化补上 `pmp*`、`menvcfg`、`stimecmp` 等后续 contract；当前这类新缺口更可能来自后续 Linux bring-up，而不是 `xv6` 到 shell 之前的路径。
- 这两条线都要继续保证单一事实来源：ISA 语义仍归 `InstructionSemantics`，platform contract 仍归统一 `Machine / device` 边界。
- 一旦新缺口能被更窄的 asm / host smoke 固化，就继续沿当前 hardening 路线补最小回归。

### 4. observation / profile foundation + 默认延续线 guardrail 仍是并行必须项

- 当前主线虽然切到标准 OS bring-up，但 `V4`、`P4-prep-1`、`kernel_alpha`、`interactive_os` 和 debug / pipeline workload 仍然要继续守住。
- 当前更值得并行保留的，不只是单纯的 `V4` hardening，还包括面向 `Linux / JIT / DBT` 的 execution profile / observation foundation；这条线现在也已经开始给 `run_debug_cli_probe`、payload/gpr seed summary 和 Linux boot profile dry-run 提供读侧支架。
- 这条线的目标是为后续 hot-path 定位、memory behavior 观察、cache 评估和 JIT 候选路径选择提供证据，而不是现在就抢跑真实 JIT。
- 当前 D 线已经整合进主线，并把 `execution_profile_smoke` 接进默认 `make test` / `make test-pipeline`；下一步更应该用它锁住新出现的 `xv6 / virtio` 路径，而不是反向抢占 A/B/C 的 contract ownership。

### 5. Spike 外部差分与更激进 `Phase 3` 继续维持条件触发

- 当前 Spike 线最有价值的角色，仍然是在 reference correctness 出现疑点时提供外部 oracle。
- 当前没有证据支持优先重开更激进的 `issue / replay / speculation`；即使以后重开，也应先有真实 workload hotspot，再看是否值得先做 issue decoupling。

## 当前明确不优先做的事

1. 不直接抢跑 `Linux` 完整 bring-up。
2. 不在当前 foundation 还没站稳前，直接实现 `JIT / 动态二进制翻译`。
3. 不为 `xv6` 先造一批短寿命、只服务单一 demo 的 special case 或 Makefile 特判。
4. 不在当前单发射 + coarse replay 基线上，继续主动扩大更激进的 `Phase 3` issue / replay / memory disambiguation。
5. 不在 `xv6` foundation 与 workload 观测都还不稳定之前，直接抢跑更重的 `Phase 4` cache / DMA / multicore / coherence。
6. 不把 `debug/frontend` 顺势扩成更大的产品功能面或通用调试器。
7. 不把“真实 `virtio` board path 已到 shell”或“Linux 已推进到 `devtmpfs: initialized` / initramfs unpack / xor checkpoint”误判成“Linux bring-up 已接近完成”；当前离 `devtmpfs: mounted`、rootfs 和 `init` 仍有明显距离，而不是 `xv6` shell 自身继续扩面就能自然解决。
8. 不把独立 `NPU / TPU-like` AI accelerator 模块误读成当前近端主线；当前它只是已经完成设计收口的未来候选方向，真正启动时仍应单开对应 `status / plan`，而不是并入当前 `xv6 / Linux` 主线执行。

## 如需新开计划

1. 当前主线的总计划以 [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md) 为准。
2. 如果 Workstream A / B / C / D 任一条线后续需要再细分成第二层专项，新增计划也应继续挂到 [xv6_linux_jit_status.md](xv6_linux_jit_status.md)，不要重新分裂事实来源。
3. 当前已经验证真实 `Image + initrd + repo-generated dtb/chosen/cmdline + linux_sbi_shim` 路径能稳定进入 Linux `devtmpfs: initialized` 与 initramfs unpack 之后的更后 boot 阶段；但只有在进一步稳定到 rootfs / init 前后，才考虑新开更完整的 `Linux` bring-up 专项计划。
4. 只有在 profile / hot-path / workload 证据足够明确之后，才考虑新开 `JIT / DBT` 专项计划。
5. 如果未来出现真实 `Phase 3` stall hotspot、`debug/frontend` bug 或 Spike correctness 缺口，再围绕具体问题单开最小专项计划。
