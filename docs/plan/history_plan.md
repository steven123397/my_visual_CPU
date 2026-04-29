# 已完成计划归档

## 文档定位

本文档用于归档 `docs/plan/` 下已经完成的计划文档。

`docs/plan/` 当前只保留：

- 仍在执行的计划文档
- [template.md](template.md)
- 本归档文件

计划完成后，不再长期保留原始 checklist 文档；只在这里保留“什么时候做了什么”的摘要记录，必要时补一两句实现过程说明。

## 当前归档规则

- 完成态计划先在对应 `status` 文档回写结果，再归档到本文档。
- 归档条目至少保留：完成时间、完成内容。
- 如有必要，可额外补一段很短的实现过程摘要，但不回灌完整 checklist 或逐步执行细节。
- 原计划文档在归档完成后删除，不再保留在 `docs/plan/`。
- `design`、`status` 与后续活跃计划引用历史计划时，统一链接到本文档对应条目。
- 当前如果没有活跃计划，`docs/plan/` 只保留 [template.md](template.md) 和本文档。

### 2026-04-29

#### mainline-wave4-ai-accelerator-slices-plan

- 原文件：
  - `mainline_wave4_ai_accelerator_slice_a_dynamic_shape_workload_plan.md`
  - `mainline_wave4_ai_accelerator_slice_b_profile_frontend_plan.md`
  - `mainline_wave4_ai_accelerator_slice_c_softmax_attention_stretch_plan.md`
- 完成内容：完成主线 `Wave 4` 中的 AI accelerator A/B/C 三段切片。切片 A 把 bounded dynamic shape 从 `dynamic GEMM / FC-like` 扩到现有 op family 的正向或 fail-closed 合同，并新增 `dynamic_tiny_model`；切片 B 把 `timed-simple` profile 的 tile setup 归到 `stall_cycles`，并把 `guest_ai_accel_demo` workload presentation 与 AI accelerator aggregate counters 接入 frontend；切片 C 作为 stretch 完成最小静态 `Softmax` 与 `tiny_attention_static`，固定为 `gemm -> softmax -> gemm` 小闭环。
- 实现过程摘要：这一轮继续采用“先切片 A/B 核心目标，再按绿灯条件启动 stretch”的路径；debug snapshot 仍保持 aggregate-only schema，没有把 host-side itemized op summary 扩成 MMIO / debug ABI。`Softmax + tiny static attention` 只证明当前 graph package / scheduler / profile path 能表达最小 attention-like block，不代表完整 Transformer runtime、动态 sequence length、KV-cache、多 head attention、训练、`INT4`、MobileNet 或 Linux-facing NPU driver。
- 结果参考：[future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)、[npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)、[mainline_status.md](../status/mainline_status.md)、[npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)

### 2026-04-27

#### mainline-roadmap-rewrite-and-linux-checkpoint-closure-plan

- 原文件：`mainline_roadmap_rewrite_and_linux_checkpoint_closure_plan.md`
- 完成内容：把 [future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md) 从“未来候选路线菜单”重写成主线长期排期设计，并把当前 active wave 明确切到 Wave 3；同时把 Linux block-rootfs fourth-stage runtime baseline 再往前冻结一刀，新增 `stage=unlinkat-open-fd-survives`，把最小 open-fd lifetime 合同并入既有 `symlink/readlink/hardlink/link metadata` checkpoint 线。
- 实现过程摘要：这一轮继续沿“先补 strings/runtime 断言、再补最小实现、最后统一回写 status / AGENTS / index / design 并删除活跃 plan”的路径推进；repo-generated `rootfs` staging 也改成 `install -m`，避免 runtime guardrail 因 staging 目录已有目标文件而假失败。验证覆盖 `python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`、`MYCPU_RUN_LINUX_PROTO_RUNTIME=1 MYCPU_LINUX_PROTO_RUNTIME_IMAGE=/tmp/mycpu-linux-build-riscv64-linux-gnu/arch/riscv/boot/Image python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested`、`make test-host-run_debug_cli_probe`、`make test`、`make test-pipeline` 与 `git diff --check`。
- 结果参考：[future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)、[mainline_status.md](../status/mainline_status.md)

#### phase4-prep2-memory-observation-shadow-cache-plan

- 原文件：`phase4_prep2_memory_observation_shadow_cache_plan.md`
- 完成内容：完成 `C1 / P4-prep-2 memory observation / shadow cache` 第一刀，把 `ExecutionMemoryObservation` 扩展为可携带物理地址的 memory observation，并在 `ExecutionProfile` 内新增只读 shadow-cache 统计；当前 debug JSON、`debug_cli_smoke` 与 `run_debug_cli_probe` 文本摘要都已能暴露 `profile.shadow_cache`，cacheable RAM 的重复 line access 会统计 `hits / misses / evictions / bypasses`，MMIO / fault / non-cacheable 场景继续作为 bypass 观测。
- 实现过程摘要：这一轮继续沿“先补窄 host/probe 红灯，再接最小观测模型，最后统一回写 status / design / AGENTS 并删除活跃 plan”的路径推进；`shadow_cache` 只做 workload 证据收集，不参与执行提交语义，也不改变 guest 可见行为。验证覆盖 `make test-host-execution_profile_smoke`、`make test-host-debug_cli_smoke`、`make test-host-run_debug_cli_probe`、`make test`、`make test-pipeline` 与 `git diff --check`。
- 结果参考：[phase4_preparation_design.md](../design/phase4_preparation_design.md)、[future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)、[mainline_status.md](../status/mainline_status.md)

#### xv6-linux-jit-wave1-plan

- 原文件：`xv6_linux_jit_wave1_plan.md`
- 完成内容：完成 `xv6 / Linux / JIT` Wave 1 收口，把 `RV64A + virtio + CSR / privilege + xv6-riscv` 主线基础、真实 `virtio-blk` board guardrail，以及 Linux block-rootfs 的 `console-opened -> rootfs-rw-ok -> proc-readable -> sys-readable -> /init reached -> file-readable -> rootfs-rw-roundtrip-ok -> fork-child-wrote -> parent-wait4-ok -> execve-third-stage -> mkdir-chdir-ok -> nested-file-roundtrip-ok -> getdents64-nested-visible -> fstatat-nested-stat-ok -> renameat2-syscall-ok -> renameat2-nested-ok -> renameat2-dirent-updated -> renameat2-cleanup-ok -> unlinkat-parent-dirent-gone -> mkdirat-dir-name-reusable -> mkdirat-reused-dir-empty -> mkdirat-reused-dir-dot-only -> mkdirat-reused-dir-parent-stat-ok -> third-stage-reached -> post-init reached` baseline 一起收口到主工作树；同时把当时并行维护的主线状态口径统一收口回 `mainline_status` 与 `docs/index.md`。
- 实现过程摘要：这一轮继续沿“先补最窄 checkpoint、再回写状态与优先级、最后删除活跃计划文件”的收口路径；Wave 1 不再保留活跃 `plan`，后续 Linux 继续沿已冻结的 post-init userland baseline 推进更后的 checkpoint。
- 结果参考：[xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-24

#### npu-tpu-accelerator-wave3-plan

- 原文件：`npu_tpu_accelerator_wave3_plan.md`
- 完成内容：完成独立 `MMIO NPU / TPU-like` AI accelerator Wave 3，把 runtime-shape fail-closed matrix、host profile manifest 负向矩阵、`--ai-profile-manifest` 的 itemized profile 文本出口，以及 profile summary 的 success / fault / reset lifecycle 一起收口到主工作树；同时继续保持 AI accelerator 为独立 `MMIO` 设备路线，不把语义混入 CPU `InstructionSemantics` reference path。
- 实现过程摘要：整体继续采用“先补红灯 smoke / malformed-input regression、再补最小 host-side 文本出口与设备侧显式 reject、最后统一回写 status 与归档”的克制路径；这一轮把 `runtime_shape_table_offset` 的 overlap / out-of-window 从“碰巧解析失败”收窄为显式 fail-closed 合同，同时只把 itemized profile 外推到 host text manifest，不顺势扩大 MMIO 或 debug snapshot ABI。
- 结果参考：[npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)、[npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)、[future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)

#### npu-tpu-accelerator-wave2-plan

- 原文件：`npu_tpu_accelerator_wave2_plan.md`
- 完成内容：完成独立 `MMIO NPU / TPU-like` AI accelerator Wave 2，把 profile attribution、host-side per-op / per-tile summary、固定 `tiny_model` workload、bounded dynamic shape contract / reject matrix，以及 dynamic `GEMM / FC-like` 第一刀一起收口到主工作树；同时保持 completion entry ABI 不变，不把动态语义混进 CPU `InstructionSemantics` reference path。
- 实现过程摘要：整体继续采用“先补红灯、再做最小 contract / workload / execute 落地、最后统一回写 status 与归档”的克制路径；其中 dynamic shape 先用 `runtime_shape_table_offset` + `dynamic_bounded` package 把 runtime dims resolve 成 concrete static package，只接到 matmul-family 第一刀，不顺势扩大到完整动态图、训练栈、`Softmax / attention`、`INT4` 或 frontend 可视化。
- 结果参考：[npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)、[npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)、[future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)

### 2026-04-23

#### npu-tpu-accelerator-wave1-plan

- 原文件：`npu_tpu_accelerator_wave1_plan.md`
- 完成内容：完成独立 `MMIO NPU / TPU-like` AI accelerator Wave 1 foundation，把 `DMA-ready` memory contract、静态 graph package / tensor golden model、独立 AI accelerator 控制面、`scratchpad + DMA/load-store engine`、静态调度器与代表性 compute path、host `ai_proto` packaging/profile 入口，以及 guest driver / demo / debug profile 闭环一起落到主工作树。
- 实现过程摘要：整体按 `DMA-ready -> graph/golden -> control plane -> data plane -> compute path -> host profile -> guest/debug` 的顺序小步推进，并把性能口径收口为 `timed-simple simulated cycles`，不使用宿主机 wall-clock 表述“加速”。同日后续 hardening 又补上 manifest `format=ai_proto_manifest_v1` / 重复单值 key reject，以及 guest `ai_accel` queue helper，避免 host profile 与 guest completion ring 继续依赖 fail-open 假设。
- 结果参考：[npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)、[npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)、[phase4_preparation_design.md](../design/phase4_preparation_design.md)

### 2026-04-12

#### phase4-prep1-bus-memory-region-plan

- 原文件：`phase4_prep1_bus_memory_region_plan.md`
- 完成内容：完成 `P4-prep-1`，新增统一 `memory_region` 类型与 `Bus::describe_region()/describe_span()`，把 `RAM / MMIO / unmapped` 与 `cacheable / dma_visible / has_side_effect / supports_burst / label` 收口成单一事实来源；同轮也把 `vector` span 预校验、`pipeline` RAM/MMIO 判断与 `LSQ` RAM-only forwarding 迁到这一路径，并新增 `bus_region_contract` unit test 守住 region / span 合同。
- 实现过程摘要：整体继续采用“先补红灯 unit test、再做最小 contract 落地、最后迁移现有调用点并守总门禁”的克制路径；这一轮明确不把问题顺势放大到 `cache / DMA / multicore / coherence`，也不改变现有 guest fault / MMIO side effect 口径，并最终守住 `cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。
- 结果参考：[phase4_preparation_design.md](../design/phase4_preparation_design.md)、[mainline_status.md](../status/mainline_status.md)

#### vector-frontend-visualization-plan

- 原文件：`vector_frontend_visualization_plan.md`
- 完成内容：完成 `vector / NN frontend visualization`，当前浏览器端已经能直接选择 `guest_vector_demo` 与 `guest_vector_cnn_demo`，并展示 workload 说明卡、向量指令 `config / memory / ALU` 高亮、`SEW / VL + v0..v31` 最小寄存器快照、固定 `conv -> relu` 专题卡，以及当前 `vector_state_busy` / serializing guard 的最小执行边界提示。
- 实现过程摘要：整体继续采用“先设计冻结、再按 `P0 -> P3` 顺序小步落地、最后统一回写状态与归档”的克制路径；这一轮只新增最小 `DebugSnapshot` 向量状态与前端只读展示，不扩成通用调试器，也不把问题顺势放大到通用模型可视化、lane 级性能图或更大协议面，并最终守住 `cd frontend && node --test`、`cd myCPU && make test`、`cd myCPU && make test-pipeline` 与 `cd myCPU && make test-host-debug_cli_smoke`。
- 结果参考：[debug_frontend_integration.md](../design/debug_frontend_integration.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-10

#### vector-v0-v1-plan

- 原文件：`vector_v0_v1_plan.md`
- 完成内容：完成 `V-lite` `V0 / V1` 首轮落地，新增 `VectorState`、共享 `VectorRequest`、`vsetcfg / vle.v / vse.v / vadd.vv / vmul.vv / vmax.vv / vdot.vv`、最小 host smoke，以及 `pipeline` 的正确 serializing fallback。
- 实现过程摘要：整体采用“先设计冻结、再补窄 smoke、再把状态修改统一收口到 commit boundary”的克制路径；`pipeline` 当前只保证正确的串行化 fallback，不提前扩成 vector-aware 执行模型，并最终守住 `cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。
- 结果参考：[vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)、[mainline_status.md](../status/mainline_status.md)

#### vector-v2-plan

- 原文件：`vector_v2_plan.md`
- 完成内容：完成 `V-lite` `V2` 首刀落地，新增 `vector_operator_smoke` 的 `dot / GEMM / Conv / ReLU` workload 回归、独立 `guest/vector_demo`，以及 functional / pipeline 两侧 guest 门禁。
- 实现过程摘要：整体采用“先补 host 算子 smoke、再补独立最小 guest demo、最后统一回写状态与归档”的克制路径；这一轮明确不改现有 `V-lite` ISA 面、不接 `guest/kernel/*` 主线，也不把 `pipeline` 扩成 vector-aware 执行模型，并最终守住 `cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。
- 结果参考：[vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)、[mainline_status.md](../status/mainline_status.md)

#### vector-v3-plan

- 原文件：`vector_v3_plan.md`
- 完成内容：完成 `V-lite` `V3` 首刀落地，新增独立 `guest/vector_cnn_demo`，以固定 `conv -> relu` 链路形成最小 CNN-style guest 闭环，并接通 functional / pipeline 两侧 guest 门禁；同轮也补上 `test-host-vector_vlite_smoke` 与 `test-host-vector_backend_smoke` 的显式 `make` alias。
- 实现过程摘要：整体继续采用“先补最小工程卫生改动、再补独立固定 guest workload、最后统一回写状态与归档”的克制路径；这一轮保持现有 `V-lite` ISA 面与 `pipeline` serializing fallback 不变，不把问题顺势放大到 `Pool / FC`、模型加载、`guest/kernel/*` 主线或 vector-aware `pipeline`，并最终守住 `cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。
- 结果参考：[vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)、[mainline_status.md](../status/mainline_status.md)

#### vector-v3-hardening-v4-design-plan

- 原文件：`vector_v3_hardening_v4_plan.md`
- 完成内容：完成一轮很窄的 `V3 hardening`，新增 `vector_cnn_smoke` host 回归，直接守住 mixed `SEW/VL` 的 `conv -> relu` 链路与全负卷积输出的 `relu` 零钳位；同轮也完成 `V4` 首刀设计收口，明确下一步只收窄 non-memory vector ALU 的最小 vector-aware pipeline 边界。
- 实现过程摘要：整体继续采用“先补最窄 host regression、再顺手把下一刀设计冻结”的克制路径；这一轮不扩 guest 新 demo、不改现有 `V-lite` ISA 面，也不把问题顺势放大到向量 load/store path、lane 模型或更重 `Phase 4`，并最终守住 `cd myCPU && make test-host-vector_cnn_smoke`、`cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。
- 结果参考：[vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)、[mainline_status.md](../status/mainline_status.md)

#### vector-v4-plan

- 原文件：`vector_v4_minimal_vector_pipeline_plan.md`
- 完成内容：完成 `V4` 首刀落地，让 non-memory vector ALU 脱离统一 serializing fallback，形成“execute 先 materialize、commit 再落地 architected vector state”的最小 vector-aware pipeline；同轮也新增 `vector_pipeline_smoke`，直接守住“older scalar ROB head 未退休时 vector ALU 仍可先执行”与“older vector state 未提交时 younger vector ALU 必须保守等待”的边界。
- 实现过程摘要：整体继续采用“先补最窄红灯 smoke、再以最小 payload / ROB / commit 接线收窄当前边界”的克制路径；这一轮明确不扩到向量 load/store path、lane 模型、vector rename、vector phys file 或更重 memory speculation，并最终守住 `cd myCPU && make test-host-vector_pipeline_smoke`、`cd myCPU && make test` 与 `cd myCPU && make test-pipeline`。
- 结果参考：[vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-06

#### spike-external-differential-validation-plan

- 原文件：`spike_differential_validation_plan.md`
- 完成内容：完成 Spike 外部差分验证 V1 第一轮落地，新增显式入口 `cd myCPU && make test-host-spike_differential`，并把 `alu_mem_csr`、`control_flow`、`predictable_branch_loop`、`trap_return`、`illegal_trap`、`delegated_user_ecall_to_supervisor` 这 6 条 host 微场景接成真实的 `myCPU vs Spike` 正向 final-state differential。
- 实现过程摘要：本轮把共享 `Scenario / FinalState / DiffReport` 规格抽出到 `tests/host/spike_differential/`，为 Spike 侧补齐 bootstrap 初始化、合法 ELF 封装、严格 debug CLI 解析、final privilege 读取，以及 trap-program controlled-exit / 非 M-mode 初始态支持；同时保持 `make test` 与 `make test-pipeline` 不依赖 Spike。
- 结果参考：[spike_differential_validation_design.md](../design/spike_differential_validation_design.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-05

#### phase3-blocked-by-unresolved-store-boundary-plan

- 原文件：`phase3_blocked_by_unresolved_store_boundary_plan.md`
- 完成内容：把 decode 级 `BlockedByUnresolvedStore` 收窄到“仅 older store 地址未知才阻塞”，并补齐 `LSQ` / `pipeline` host smoke，守住 overlap block、automatic replay、forwarding 和 commit-boundary 既有合同。
- 实现过程摘要：本轮只调整 `classify_load()` 判定顺序，不扩状态枚举，也不把问题直接放大为更激进的 memory speculation。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[mainline_status.md](../status/mainline_status.md)


### 2026-03-25

#### pipeline-core-integration-plan

- 原文件：`pipeline_core_integration_plan.md`
- 完成内容：完成 `pipeline core` 第一轮主线接入，正式引入 `ExecutionBackend`、`FunctionalBackend`、`PipelineBackend`、共享 `InstructionSemantics`、CLI `--backend pipeline` 与 `make test-pipeline` 主入口。
- 实现过程摘要：先把 backend 抽象和 fault-result 访存接口接回主线，再补 host-side smoke / differential；`debug/frontend` 留到后续第二轮单独接入。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[mainline_status.md](../status/mainline_status.md)

#### kernel-alpha-storage-error-contract-plan

- 原文件：`kernel_alpha_storage_error_contract_plan.md`
- 完成内容：补齐独立 `kernel_alpha` 对 storage 错误合同的最小消费能力，新增 `BAD_BLOCK_COUNT` 负向路径，并把后续 storage 错误扩展纳入稳定 bring-up 基线。
- 实现过程摘要：保持 `SimpleStorage` 设备语义不变，主要在 guest platform / storage helper 和独立 demo 入口上扩最小合同。
- 结果参考：[platform_mmio_contract.md](../design/platform_mmio_contract.md)、[kernel_alpha_status.md](../status/kernel_alpha_status.md)

### 2026-03-26

#### phase1-hardening-regressions-plan

- 原文件：`phase1-hardening-regressions_plan.md`
- 完成内容：完成第一轮更系统的 Phase 1 hardening 回归扩充，把 illegal 编码、CPU 侧 MMIO access fault、ELF segment / reject / header、bus / device guard、MMIO contract matrix 与 CSR illegal matrix 接入现有门禁。
- 实现过程摘要：整体策略是优先补回归、只做最小修复，把 reference path 的高风险 correctness 边界压成持续门禁。
- 结果参考：[regression_completion_criteria.md](../design/regression_completion_criteria.md)、[mainline_status.md](../status/mainline_status.md)

#### sv39-mprv-semantics-plan

- 原文件：`sv39_mprv_semantics_plan.md`
- 完成内容：补齐 `mstatus.MPRV` 驱动的 Sv39 数据访存语义，使 `M-mode` 下的 `load/store` 在 `MPRV=1` 时按 `MPP` 指定的有效特权级执行地址翻译与权限检查。
- 实现过程摘要：保持现有 `AddressSpace` 结构不变，只在有效特权级判定和权限检查路径上补最小修复，并接入 asm 门禁。
- 结果参考：[regression_completion_criteria.md](../design/regression_completion_criteria.md)、[mainline_status.md](../status/mainline_status.md)

#### sv39-pagewalk-contracts-plan

- 原文件：`sv39_pagewalk_contracts_plan.md`
- 完成内容：补齐 Sv39 page-walk 对 misaligned superpage 和 non-leaf PTE 保留位的合同校验，并把对应 asm 回归纳入 `make test` 与 `make test-pipeline`。
- 实现过程摘要：先用最小 asm 样本稳定复现缺口，再只修 page-walk 校验逻辑，不扩大功能面。
- 结果参考：[regression_completion_criteria.md](../design/regression_completion_criteria.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-03-27

#### phase3-branch-prediction-plan

- 原文件：`phase3_branch_prediction_plan.md`
- 完成内容：完成 `Phase 3-A` 第一轮分支预测增强，给 `pipeline` 接上最小 predictor 子模块、预测相关快照字段以及对应的 host-side / frontend 验证闭环。
- 实现过程摘要：保持 `pipeline` 仍为 in-order 后端，只让预测影响取指方向和 mispredict 恢复，不改变 architected 语义来源与提交模型。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-02

#### phase3-ooo-readiness-plan

- 原文件：`phase3_ooo_readiness_plan.md`
- 完成内容：完成 `Phase 3-B/C` OoO 接线前置准备，补齐执行模型设计、投机执行契约、sequence / retire trace、commit boundary helper，以及未接线但可单测的 `rename_map / reorder_buffer / load_store_queue` 基础模块。
- 实现过程摘要：这一步不直接做 OoO 接线，而是先把结构边界、观测面和验证基建补齐，让后续大块接线不再混着改模型和改实现。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-03

#### phase3-phys-free-list-plan

- 原文件：`phase3_phys_free_list_plan.md`
- 完成内容：为当前 `rename + ROB` 主路径补齐最小 `phys free-list / recycle`，让 phys tag 不再只依赖线性增长，并把 commit 回收与 rollback 恢复纳入稳定合同。
- 实现过程摘要：free-list 直接收口在 `RenameMap` 内，由 ROB head commit 回收 old committed phys，flush / rollback 恢复 checkpoint 快照。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-lsq-replay-contract-plan

- 原文件：`phase3_lsq_replay_contract_plan.md`
- 完成内容：为 `LSQ` 建立最小 `replay-needed` 合同与观测面，能显式区分 unresolved store 阻塞、overlap 阻塞和 `replay_required` 中间态。
- 实现过程摘要：这一轮只把 memory-order 风险变成稳定接口和 smoke，不直接引入自动 replay machinery。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-lsq-automatic-replay-plan

- 原文件：`phase3_lsq_automatic_replay_plan.md`
- 完成内容：把 `replay_required` 从观测状态推进成最小可执行的 automatic replay recovery，让 backend 在安全边界执行 coarse、`RAM-only` 的 replay flush。
- 实现过程摘要：直接复用现有 committed rollback + flush 主路径，不另起新的局部 recovery 机制。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-lsq-store-to-load-forwarding-plan

- 原文件：`phase3_lsq_store_to_load_forwarding_plan.md`
- 完成内容：补齐最小 `RAM-only store-to-load forwarding`，让 younger RAM load 可从 older ready store 前递结果，而不是只能回 RAM 读旧值。
- 实现过程摘要：forwarding 只支持 full-cover RAM 场景，不扩到 MMIO、partial merge 或更激进的 memory speculation。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### phase3-minimal-ooo-execute-plan

- 原文件：`phase3_minimal_ooo_execute_plan.md`
- 完成内容：把当前 `pipeline` 从“近似顺序 execute”推进到“最小真实 OoO execute”，让 younger ALU 可在 older memory op 未完成时先完成，但 architected side effect 仍只在顺序 commit 时生效。
- 实现过程摘要：核心收口是让 `ROB head` 成为真实退休源头，并把 memory 路径整理成最小独立执行单元，同时继续守住 MMIO non-speculative 与 precise 边界。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[mainline_status.md](../status/mainline_status.md)

#### phase3-ooo-execution-plan

- 原文件：`phase3_ooo_execution_plan.md`
- 完成内容：完成 `Phase 3-B/C` 首轮总收口，把 `rename + ROB + LSQ`、最小 phys 生命周期、`LSQ replay-needed` 合同、coarse automatic replay、`RAM-only` store-to-load forwarding、最小真实 `OoO execute` 和最小 `ROB / LSQ` 观测面一起收成当前主线路径。
- 实现过程摘要：整体采用“先 readiness、再分块子合同、最后回到总计划收口”的推进节奏；当前 `pipeline` 已稳定在“单发射、顺序退休、最小 OoO 完成窗口”的克制形态，后续重点转向 bug-driven hardening 与下一轮微架构取舍。
- 结果参考：[phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)、[pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)、[mainline_status.md](../status/mainline_status.md)

### 2026-04-04

#### p1-debug-frontend-boundary-refinement-plan

- 原文件：未单独保留活跃 plan；本轮直接按路线图 `P1-6` 收口。
- 完成内容：完成 `debug_protocol` / `debug_server` 的协议与运行时边界收口；当前 `debug_protocol` 已拆成命令解码、响应序列化与 `CLI loop` 三块，`debug_server` 已拆成 `DebugCliSession`、server runtime 与 HTTP / WebSocket 入口，terminal 跟踪不再和子进程管理、路由分发揉在同一文件里。
- 实现过程摘要：整体采用“先补新边界测试，再做文件切分”的克制路径；C++ 侧新增 `debug_protocol_command_smoke` 守住协议解码合同，Node 侧新增 `debug_server_runtime` 直测，并继续复用既有 `debug_server` / `interactive_terminal` smoke 守住外部行为不漂移。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### p1-guest-smoke-orchestration-refinement-plan

- 原文件：`p1_guest_smoke_orchestration_refinement.md`
- 完成内容：完成 `P1-2` guest smoke orchestration 收口，把 `user_program_smoke` 的 `prepare / enter round / active memory` 改成更窄的内部阶段 helper，并让 `supervisor_demo_smoke` 退回 bootstrap / user / session 组合层。
- 实现过程摘要：先补 `prepare_standard()` runtime-stage rollback characterization，再按 freestanding 约束整理 guest smoke 内部编排；本轮刻意不扩到 `P1-5` 的 guest public header API 收口。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### p1-guest-public-header-boundary-refinement-plan

- 原文件：`p1_guest_public_header_boundary_refinement.md`
- 完成内容：完成 `P1-5` guest 公共头文件边界收口，把 `kernel_runtime`、`supervisor_runtime` 与 `user_program_smoke` 的跨模块使用面改成以窄 helper / accessor 为主，减少 direct `struct` layout 依赖。
- 实现过程摘要：先补 interrupt state 的 configure / counter / delivered / wait helper，再把 `monitor_commands`、`kernel_alpha`、`supervisor_demo_smoke` 和相关单测迁到新访问面；本轮刻意不把这些 public struct 整体改成 opaque handle，也不顺手扩成更大范围的 guest 生命周期重构。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### p1-pipeline-backend-boundary-refinement-plan

- 原文件：`p1_pipeline_backend_boundary_refinement.md`
- 完成内容：完成 `P1-1` `pipeline_backend` 边界收口，把原本单文件承载的主调度、commit-replay、execute 和 frontend 职责拆成独立编译单元，并保持 `PipelineBackend` 外部接口与现有 `pipeline` 合同不变。
- 实现过程摘要：整体采取“只拆文件边界、不改行为合同”的克制路径；`pipeline_backend.cpp` 退回构造与 debug snapshot 侧职责，其余逻辑分别下沉到 `pipeline_backend_cycle.cpp`、`pipeline_backend_execute.cpp` 与 `pipeline_backend_frontend.cpp`。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### p1-reference-platform-contract-refinement-plan

- 原文件：未单独保留活跃 plan；本轮直接按路线图 `P1-12 / P1-13 / P1-14` 并行收口。
- 完成内容：完成 `page walk` 总线失败 fault 分类、`PLIC` claim / complete 按 context 记账，以及 ELF header reject 合同三项 reference / platform 边界收口；当前页表物理访问失败与 A/D 位回写失败会稳定回到 access fault，错误 context 的 `PLIC complete` 不再释放 claim，ELF loader 也已明确 reject endianness / ident version / ELF version / type / machine / entry-range 非法输入。
- 实现过程摘要：代码改动分别在独立 worktree 上并行落地，子分支只改生产代码和测试；最终统一回到 `main` 合并，并在主线文档中一次性回写路线图、状态和归档。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### p2-validation-gap-backfill-round-1

- 原文件：未单独保留活跃 plan；本轮直接按路线图 `P2-1 / P2-2 / P2-3 / P2-4 / P2-5 / P2-7` 的建议拆分推进，并由主集成线统一回写状态。
- 完成内容：完成 `BinaryLoader` 直接单测、`Machine::load_elf()/load_binary()` 最小 reload/reset 回归、`supervisor_demo_smoke` 直接单测、真实 `debug server + mycpu --debug-cli` 端到端 smoke、Node 侧调试预算常量收口，以及 `pipeline` mega-smoke 首轮拆分。
- 实现过程摘要：代码线按 `A/B/C/D` worktree 并行落地，子线只改代码和测试；主线最后统一集成并重新跑 `cd myCPU && make test`、`cd myCPU && make test-pipeline` 与 `cd frontend && node --test`。本轮刻意没有把 `user_program_smoke` 更窄 helper 直测和全部跨语言预算常量一次性收完，后续仍按剩余 gap 继续推进。
- 结果参考：[mainline_status.md](../status/mainline_status.md)

#### p2-validation-gap-backfill-round-2

- 原文件：未单独保留活跃 plan；本轮直接沿 `p2-validation-gap-backfill-round-1` 剩余 gap 继续补洞。
- 完成内容：补齐 `user_program_smoke` 的 `active-memory / interrupt round` 更窄直测，并把 C++ `debug_session.cpp`、`interactive_terminal_smoke.cpp` 的预算常量收口到共享命名入口。
- 实现过程摘要：继续在主集成线以“先窄门禁、后总验证”的方式推进；guest 侧只扩 host stub 与直测，不改 `user_program_smoke` public surface，C++ 侧新增 `debug_budget.h` 统一 `step_commit` 与 interactive boot/command budget，然后重新跑 `cd myCPU && make test`、`cd myCPU && make test-pipeline` 与 `cd frontend && node --test`。
- 结果参考：[mainline_status.md](../status/mainline_status.md)
