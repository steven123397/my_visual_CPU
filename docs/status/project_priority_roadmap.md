# 当前项目优先级路线图

## 文档定位

本文档只保留当前仍然开放的优先级判断，不再重复记录已经完成的整段执行过程。

它不替代 [mainline_status.md](mainline_status.md)。`mainline_status` 负责回答“现在主线是什么状态”，本文档负责回答“下一轮最值得做什么，以及什么不该抢跑”。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [design/debug_frontend_ui_refresh_design.md](../design/debug_frontend_ui_refresh_design.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/blocked_by_unresolved_store_boundary.md](../design/blocked_by_unresolved_store_boundary.md)
  - [design/phase3_issue_replay_speculation_assessment.md](../design/phase3_issue_replay_speculation_assessment.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
  - [design/vector_vlite_v0_v1_design.md](../design/vector_vlite_v0_v1_design.md)
  - [design/vector_v2_operator_guest_design.md](../design/vector_v2_operator_guest_design.md)
  - [design/vector_v3_minimal_cnn_guest_design.md](../design/vector_v3_minimal_cnn_guest_design.md)
  - [design/vector_v4_minimal_vector_pipeline_design.md](../design/vector_v4_minimal_vector_pipeline_design.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
- 当前计划：
  - 当前无活跃计划。
- 已完成计划归档：
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)
  - [../plan/history_plan.md#vector-v3-hardening-v4-design-plan](../plan/history_plan.md#vector-v3-hardening-v4-design-plan)
  - [../plan/history_plan.md#vector-v3-plan](../plan/history_plan.md#vector-v3-plan)
  - [../plan/history_plan.md#vector-v0-v1-plan](../plan/history_plan.md#vector-v0-v1-plan)
  - [../plan/history_plan.md#vector-v2-plan](../plan/history_plan.md#vector-v2-plan)
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)
  - [../plan/history_plan.md#p2-validation-gap-backfill-round-1](../plan/history_plan.md#p2-validation-gap-backfill-round-1)
  - [../plan/history_plan.md#p2-validation-gap-backfill-round-2](../plan/history_plan.md#p2-validation-gap-backfill-round-2)
  - [../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
  - [../plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan)
  - [../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)

## 当前判断

- `P0` correctness 修补已经完成，`P1` 结构收口已经全部关闭，`P2` 首轮验证补洞也已经完成两轮收口。
- Spike 外部差分验证 V1 也已完成第一轮落地；当前已经有一条独立离线的 `myCPU vs Spike` final-state oracle，并已补上 returning trap handler 的 first-trap checkpoint，但它还不构成新的主线扩展任务。
- 因此，当前路线图不再需要继续维护一长串“已完成问题记录”；现在真正开放的事项已经缩小到少数几个具体边界。
- 当前如果新开计划，优先级应围绕 reference / guest 的 bug-driven hardening 展开；`Phase 3` 的 decode 边界后续判断已经完成，`debug/frontend` 也不再需要主动新开更重的浏览器压力验证计划。
- 如果后续准备重新打开一条有明确结构收益的新长期主线，优先级应先看“向量扩展 + ML workload”，而不是直接切入更重的 `Phase 4` cache / DMA / multicore / coherence；前者更符合当前 reference-first、workload-driven 的仓库方法论。
- 当前这条线的 `V0 / V1` 与 `V2` 首刀都已经完成：`V-lite` 设计冻结、shared semantics / `functional` reference path、host 回归、`pipeline` 的正确 serializing fallback、`dot / GEMM / Conv / ReLU` workload smoke，以及独立最小 guest 向量 demo 都已接通。
- 当前也已经完成 `V3` 与一轮更窄的 `V3 hardening`：固定 `conv -> relu` guest demo、functional / pipeline 两侧 guest 门禁、`vector_vlite_smoke` / `vector_backend_smoke` 的显式 `make` alias，以及新的 `vector_cnn_smoke` host regression 都已落地；这一轮继续保持现有 `V-lite` 语义面和 `pipeline` serializing fallback 不变。
- 当前也已经完成 `V4` 首刀落地，并补上一轮很窄的 `V4 hardening`：non-memory vector ALU 已脱离统一 serializing fallback，改为“execute 先 materialize、commit 再落地”的最小 vector-aware pipeline；`vector_state_busy` 也已从粗粒度“任何 older vector pending 都阻塞”收窄到“pending serializing vector 或 direct older source dependency 未 materialize 才阻塞”，同时继续不扩到向量 load/store path、lane 模型或更重 memory speculation。

## 当前优先级

### 1. `debug/frontend` 维持当前够用门禁，并只做不扩功能面的 UI 收口

- 真实 `debug server + mycpu --debug-cli` 端到端 smoke 已经落地，但它仍主要覆盖最小交互和短会话。
- 当前已经补上一组更窄的 Node/runtime 回归：持续 `run/pause`、运行中 session replacement，以及更高吞吐 terminal 输入聚合。
- 当前也已经进一步补到 repeated `run/pause` 长会话恢复、`reset` 后 terminal reset / offset 重启语义，以及真实 `guest_interactive_os_demo` 的 `run/pause + terminal-input` e2e。
- 对当前单用户、本地教学/调试使用，这组门禁已经足够；后续按真实 bug 或明确新需求补最小回归即可，不再主动扩大到更长时间 soak 或更重浏览器压力。
- 当前工作区也在推进一轮 `debug/frontend` UI refresh，但它的边界仍应收窄在浏览器壳层的布局、视觉层级和 `terminal collapsed` 交互语义，不得顺手扩大 `debug_session/protocol`、guest 合同或新的浏览器压力验证面。
- 这条线的目标仍然只是“教学演示可用”，不是通用调试器，也不需要为当前使用方式预先建设更重的浏览器端压测体系。

### 2. `Phase 3` 后续取舍已收口：当前不主动扩大更激进的 `issue / replay / speculation`

- 当前 `Phase 3-B/C` 已经完成最小真实 `OoO execute`，而 decode 级 `BlockedByUnresolvedStore` 的第一轮边界收窄也已经落地。
- 现在已确认：`BlockedByUnresolvedStore` 只表示“older store 地址未知才阻塞”；地址已知但 data 未 ready 的 older store 不再全局阻塞非重叠年轻 load，而重叠场景继续暴露 `BlockedByOverlappingStore`。
- 进一步评估后，当前主线结论已经明确：在 decode 级 load 前置分类、单 memory execute 通道和 coarse replay flush 仍然成立的前提下，主动继续扩更激进的 `issue / replay / memory disambiguation` 收益不足。
- 最近又用真实 `debug server + pipeline` 对 `hello`、`guest_interactive_os_demo` 和 `guest_kernel_alpha_demo` 做了一轮短 smoke，观察到的主导 stall 仍主要是 `memory_path_busy` 和 `source_operands_not_ready`；没有形成稳定的 decode 级 load/store 串行化或 replay hotspot，因此当前也没有新的证据支持优先重开 issue decoupling。
- 因此，这条线当前不再作为主动推进事项；只有在出现真实 workload stall 证据或明确研究目标时，才值得重开。
- 如果未来重开，第一刀也应优先评估 issue decoupling，而不是直接放宽 unknown-address speculation 或进一步扩大 replay 触发面。

### 3. Spike 外部差分进入维护态，不主动扩成更大框架

- `make test-host-spike_differential` 当前已经可用，能为一批 host 微场景提供真实 `myCPU vs Spike` final-state oracle；当前也已经接上第一批 device-free `Sv39/page fault` final-state subset，以及 returning trap handler 的 first-trap checkpoint。
- 这条线当前最有价值的角色，是在 reference correctness 出现疑点时提供外部真值，而不是立刻扩成默认主门禁或统一多后端框架。
- 因此，后续只按真实 bug 或明确收益继续补最小场景面，例如更广 `Sv39 / page fault`、不依赖设备 side effect 的 privilege / CSR 合同，或确有必要时的更复杂多 checkpoint 变体；当前不主动扩到设备场景、guest workload 或逐提交 trace framework。

### 4. `向量扩展 + ML workload` 当前优先稳住 `V4` 首刀，而不是更重的 `Phase 4`

- 当前更健康的后续大方向，不是直接去做更重的 `Phase 4`，而是沿已落地的“向量扩展 + ML workload”这条 workload-guided、ISA-first 主线继续前推。
- 当前已完成的部分，不再只有最小整数 `V-lite` 的 `V0 / V1`、`V2` 与 `V3`；`V3 hardening` 也已落地，具体包括新的 `vector_cnn_smoke` host regression，用于守住 mixed `SEW/VL` 的 `conv -> relu` 数据链和全负卷积输出的 `relu` 零钳位。更完整的最小 CNN inference demo 与 vector-aware `pipeline` 仍是后续阶段。
- 当前更健康的下一步，不是顺着 `V4` 立刻继续扩到向量 load/store path、`Pool / FC`、模型文件加载或更重 guest runtime，而是继续围绕已落地的 `V4` 首刀做 bug-driven hardening 和 workload 观察；当前第一轮 hardening 已经补上更像真实依赖链的 host smoke，并把 direct dependency 的执行边界收窄了一步，这也比直接跳到更重 memory path 扩面更符合当前 reference-first 方法论。
- 与 `Phase 4` 的主要冲突在于：在尚无稳定向量 workload 之前，cache / DMA / scratchpad 的收益判断缺少足够信号，而 multicore / coherence 则会过早放大平台状态空间与验证成本。
- 因此，如果未来只能新开一条重主线，默认应先开“向量扩展 + ML workload”，再由真实 workload 决定哪些 `Phase 4` 工作值得推进。
- 唯一适合提前做的 `Phase 4` 相关事项，只应是那些有独立结构收益、且不会提前扩大外部语义面的准备性收口，例如更清晰的 bus / memory contract 或更好的 memory 观测面。

### 5. 常态维护项

- 继续维护 reference correctness 矩阵，不让 illegal / MMIO / ELF / CSR / Sv39 合同回退。
- 继续守住 `kernel_alpha` 十条 guest 基线和 `guest_supervisor_demo` 的稳定输出。
- 继续维护 guest runtime 已经形成的 `vm*`、`trap*`、`kernel_bringup`、`kernel_runtime`、`user_program*` 边界，不让新增 bug 修复重新把职责揉回去。

## 当前明确不优先做的事

1. 不继续把 `debug/frontend` 往断点、条件暂停、任意文件加载或更大 UI 功能面扩张。
2. 不为了当前单用户本地使用场景，继续主动补更重的浏览器端压力验证或多客户端门禁。
3. 不把 `interactive_os` 当作新的产品主线；它当前仍应服务于 monitor / terminal / 调试链路验证。
4. 不把 `SimpleStorage` 更完整的设备模型抢在当前 correctness / structure hardening 前面推进。
5. 不在当前单发射 + coarse replay 基线上，继续主动扩大更激进的 `Phase 3` issue / replay / memory disambiguation。
6. 不在 `V4` 首刀落地后先经过一轮 bug-driven hardening、并形成更稳定的向量 workload 证据之前，主动抢跑更重的 `Phase 4` cache / DMA / multicore / coherence。
7. 不把 Spike 外部差分在当前阶段直接扩大成默认主门禁、统一多 oracle framework，或设备 / guest 系统级大场景验证。

## 如需新开计划

1. 如果后续出现真实 `Phase 3` stall hotspot，再围绕“issue decoupling 是否值得单开最小计划”建专项，而不是直接为 unknown-address speculation 或更宽 replay 开计划。
2. 如果后续出现真实 `debug/frontend` bug，再围绕具体故障单开最小修复 / 回归计划，而不是泛化成新的浏览器压力专项。
3. 如果后续 Spike 外部差分暴露出 reference correctness 缺口，再围绕具体语义面单开最小计划，例如更广 `Sv39 final-state subset`、`device-free privilege contract` 或必要时的多 checkpoint / nested trap 变体，而不是一开始就做大一统 trace framework。
4. 当前如果要新开向量子线计划，应继续围绕已落地的 [../design/vector_v4_minimal_vector_pipeline_design.md](../design/vector_v4_minimal_vector_pipeline_design.md) 做更窄的 `V4` hardening / observation：优先继续守住 direct dependency、serializing guard 与 workload 观察，而不是直接跳到向量 load/store path、`Pool / FC` 或更远的 cache / DMA / multicore 大专项。
5. 如果下一轮还要并行推进，建议围绕“guest runtime bug-driven hardening / reference correctness 补洞 / 条件触发后的 `Phase 3` 专项”拆 ownership，而不是继续机械沿用旧的 `P2-1..P2-7` 编号分线。
