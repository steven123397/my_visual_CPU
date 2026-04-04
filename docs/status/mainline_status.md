# 主线状态

## 文档定位

本文档用于记录 `phase1-stable` 冻结后、`pipeline core` 与 `debug/frontend` 已完成正式接入之后，当前主线仍需继续推进的工程任务。

它面向下一轮实现工作，重点回答：

- Phase 1 近期主线还剩什么
- 已接入的 Phase 2 能力当前按什么方式继续推进
- 新对话继续工作时，应该优先看哪些入口文档和验证门禁

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/cpp_refactor_design.md](../design/cpp_refactor_design.md)
  - [design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
  - [design/phase3_branch_prediction_design.md](../design/phase3_branch_prediction_design.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [design/pipeline_core_integration.md](../design/pipeline_core_integration.md)
  - [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
- 相关状态：
  - [status/project_priority_roadmap.md](project_priority_roadmap.md)
- 当前计划：
  - 当前无活跃计划；最近完成项见 [plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan)
- 已完成计划归档：
  - [plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan)
  - [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan](../plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan)
  - [plan/history_plan.md](../plan/history_plan.md)

## 当前状态

当前主线已经稳定成立的事实如下：

- 仓库当前已经是一个已可运行的模拟器原型，而不是纯设计稿。
- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成。
- 默认 `functional` reference path、独立 `kernel_alpha` 正向与九条负向回归、`make test` 主门禁均已打通。
- `pipeline core`、`--backend pipeline`、`make test-pipeline`、`debug_session/protocol`、本地 Node 调试服务与浏览器前端教学演示链路都已经正式接入。
- `Phase 3-A` 第一轮分支预测增强已经落地：当前 `pipeline` 已具备最小 `branch_predictor`、`jal` static predict-taken、条件分支动态预测与继续复用现有 flush / redirect 的 mispredict 恢复路径。
- `Phase 3-B/C` 的基础收口已经继续推进到“最小真实 OoO execute”完成态：当前 `pipeline` 已具备 `rename + ROB` commit 主路径、最小 `LSQ` 接线、统一 speculative rollback、`RAM-only` forwarding、coarse automatic replay，以及 `ROB` 驱动退休 + 最小独立 memory execute；当前剩余工作已不再是“有没有真正进入 OoO execute”，而是更激进的 issue / memory speculation 与 bug-driven hardening。

这意味着当前主线不再把 `pipeline` 与 `debug/frontend` 视为“待合入功能”，而是把它们视为已经落地、需要继续稳定化的现有能力。
同时也意味着：当前 `Phase 3` 的主线不再是“准备好接线没有”，而是以已完成的 [plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan) 和 [plan/history_plan.md#phase3-minimal-ooo-execute-plan](../plan/history_plan.md#phase3-minimal-ooo-execute-plan) 作为当前基线，继续维持现有基础 OoO 执行模型、补新增 bug 的最小持久回归，并决定是否进入更激进的下一轮微架构扩展。

## 2026-04-04 P1-12 / P1-13 / P1-14 reference / platform 合同收口进展

本轮主线已完成 [plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan) 对应的三条 reference / platform 合同收口；目标是把路线图中剩余的 page-walk / PLIC / ELF 三处明确边界补成持久合同，而不是继续把这些问题留在“文档口径”或大类 smoke 里。

- `myCPU/src/mem/address_space.cpp` 已把 page walk 期间的页表项读取失败与 A/D 位回写失败从 page fault 收口为对应 access fault；`tests/host/address_space_faults_smoke.cpp` 已补齐 `fetch/load/store` 三类 cause 与 `tval=原始虚拟地址` 的 host-side 合同。
- `myCPU/src/devices/plic.cpp` 与 `myCPU/src/devices/plic.h` 已把 claim / complete 改为按 context 记账；错误 context 的 complete 不再释放 claim，`tests/unit/mmio_contract_matrix.cpp`、现有 machine/supervisor asm 以及 `pipeline_backend_smoke` 共同守住这条边界。
- `myCPU/src/loader/elf_loader.cpp` 已把 ELF header reject 扩到 endianness、ident version、ELF version、type、machine 与 entry-range；`elf_loader_header_rejects`、`elf_loader_rejects` 以及既有正向 ELF 单测 fixture 都已同步到同一口径。
- 这意味着路线图中的 `P1-12`、`P1-13`、`P1-14` 已全部关闭；当前仍残留的 `P1` 结构项主要是 `debug_protocol.cpp / debug_server.mjs` 的协议与运行时状态机边界。

本轮已新鲜验证通过：

- `cd myCPU && make test-host-address_space_faults_smoke`
- `cd myCPU && make test-sv39_pagewalk_contracts`
- `cd myCPU && make test-host-backend_differential_smoke`
- `cd myCPU && make test-unit-mmio_contract_matrix`
- `cd myCPU && make test-plic_machine_external_interrupt`
- `cd myCPU && make test-plic_supervisor_external_interrupt`
- `cd myCPU && make test-host-pipeline_backend_smoke`
- `cd myCPU && make test-unit-elf_loader_header_rejects`
- `cd myCPU && make test-unit-elf_loader_rejects`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

## 2026-04-04 P1-1 pipeline_backend 边界收口进展

本轮主线已完成 [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan) 对应的 `P1-1` 收口；目标是把 `pipeline_backend.cpp` 从“大一统单文件”退回到更窄的结构边界，而不是继续在这一轮顺手扩 `pipeline` 行为。

- `myCPU/src/exec/pipeline_backend.cpp` 现已退回构造、`debug_snapshot` 与 stage 文本格式化侧职责，不再继续同时承载周期主调度、commit/replay、memory execute 和 decode/fetch。
- `myCPU/src/exec/pipeline_backend_cycle.cpp`、`myCPU/src/exec/pipeline_backend_execute.cpp` 与 `myCPU/src/exec/pipeline_backend_frontend.cpp` 已分别承接 `step()/commit-replay`、`resolve_ex_*()/step_mem()/step_ex()` 以及 `sources_ready()/step_id()/step_if()` 这三组职责边界。
- `myCPU/Makefile` 已把新的 backend 编译单元接入现有 host / asm / guest 门禁；`PipelineBackend` 的类接口和外部调用方式保持不变。
- 本轮刻意没有扩 `debug snapshot` 字段、predictor 行为、LSQ 合同或 interrupt 语义；这次改动只处理文件职责与后续可维护性。

本轮已新鲜验证通过：

- `cd myCPU && make test-host-pipeline_backend_smoke`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-backend_differential_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-pipeline`

## 2026-04-04 P0 收口进展

本轮主线已按 [status/project_priority_roadmap.md](project_priority_roadmap.md) 完成剩余 P0 收口；当前 `main` 不再残留 P0 条目，后续优先级应从 P1 开始。

- `guest/kernel/kernel_bringup.c` 已把 VM 建立后的失败路径收口为对称回滚；当前无论是 `kernel_bringup_setup_vm()` 内部失败，还是后续 PMM probe 失败，只要 `vm_address_space_destroy()` 自己也失败，bring-up 都会保留 `out_space` 指针并直接传播失败，不再伪装成“已回滚完成”。
- `tests/unit/kernel_bringup.c` 已新增 setup/probe 两类 rollback-failure 回归，和既有 setup/probe 失败用例一起守住 `kernel_bringup` 的成功回滚与失败保真合同。
- `tests/asm/exception_traps.S` 已补齐 `instruction-address-misaligned` 专用回归；当前 `jal`、`jalr` 与 taken branch 跳到非对齐目标时，都会稳定守住 `mcause / mepc / mtval` 合同。

本轮已新鲜验证通过：

- `cd myCPU && make test-unit-kernel_bringup`
- `cd myCPU && make test-exception_traps`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`

## 2026-04-04 P1-5 guest 公共头文件边界收口进展

本轮主线已完成 [plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan](../plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan) 对应的 `P1-5` 收口；重点是把 guest public header 的跨模块使用面从直接 `struct` 字段访问收口成更窄的 helper / accessor，而不是把这些 runtime 对象一步推成 opaque handle。

- `guest/include/supervisor_runtime.h` 与 `guest/kernel/supervisor_runtime.c` 已补 interrupt state 的 configure / counter / delivered / wait helper；`kernel_runtime`、`monitor_commands`、`supervisor_demo_smoke` 和 `kernel_alpha` 不再直接碰 interrupt counter、expected source 或 post-handler 配置字段。
- `guest/include/kernel_runtime.h` 与 `guest/kernel/kernel_runtime.c` 已补 const interrupt-state 观察面与 `address_space` setter；`monitor_commands` 和相关单测改成通过 helper 观察 runtime，不再把 `kernel_runtime_t` 的 public layout 当成外部合同。
- `guest/include/user_program_smoke.h` 与 `guest/kernel/user_program_smoke.c` 已补 `program / runtime / reset-state` 观察 helper；`supervisor_demo_smoke` 与 `tests/unit/user_program_smoke.c` 不再直接依赖 `smoke.program / remap_region / invalid_region / remap_object` 的 scratch 布局。
- 相关 guest 输出保持不变：`guest_supervisor_demo = KRN`、`kernel_alpha_demo = KMVPETDS`、`kernel_alpha_fault_demo = KMVX`、`kernel_alpha_plic_not_ready_demo = KMVPX`、`kernel_alpha_timer_not_ready_demo = KMVPETX`。
- 当前仍保留的克制边界是：这些 public struct 还没有整体 opaque 化；同模块实现和少量测试桩仍可直接访问 layout，但跨模块生产调用面已经完成第一轮收口。

本轮已新鲜验证通过：

- `cd myCPU && make test-unit-supervisor_runtime`
- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-unit-kernel_alpha_common`
- `cd myCPU && make test-unit-kernel_alpha_interrupt`
- `cd myCPU && make test-unit-monitor_commands`
- `cd myCPU && make test-unit-user_program_smoke`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`

## 2026-04-04 P1-2 guest smoke orchestration 收口进展

本轮主线已完成 [plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan](../plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan) 对应的 `P1-2` 收口；重点仍是 guest 结构边界整理，而不是扩 public API 功能面。

- `guest/kernel/user_program_smoke.c` 已把 `prepare_standard()`、`enter_round()` 和 `exercise_active_memory()` 的内部编排改成更窄的阶段 helper；public surface 保持不变，`smoke->program` staged publish、round activation 和 active-memory 细节不再继续堆在单个长函数里。
- `guest/kernel/supervisor_demo_smoke.c` 已退回 bootstrap / user / session 组合层：标准 `prepare / round / active-memory` 继续通过 `user_program_smoke` helper 协作，文件本身主要保留 demo 特有参数组装、结果断言和 platform-tail 组合。
- `tests/unit/user_program_smoke.c` 已补 `prepare_standard()` runtime-stage rollback 回归；当前 address-space/runtime 失败都会守住 staged rollback 合同。
- 相关 guest smoke 输出保持不变：`guest_supervisor_demo = KRN`、`kernel_alpha_demo = KMVPETDS`、`kernel_alpha_fault_demo = KMVX`。
- 当时明确没有顺手扩到 `P1-5` 的 guest public header 收口；该项已在后续同日另一轮完成，见上节。

本轮已新鲜验证通过：

- `cd myCPU && make test-unit-user_program_smoke`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`

## 2026-04-03 P1 首批结构收口进展

本轮主线没有继续扩功能面，而是按 [status/project_priority_roadmap.md](project_priority_roadmap.md) 的 P1 首批问题做了三条结构收口：

- `myCPU/Makefile` 已把 asm functional / pipeline 测试和 guest functional / pipeline demo 测试收敛成共享 contract 宏；当前 `expected / timeout / extra args / backend` 不再分散成两套大段复制，后续新增或调整门禁时的漂移风险更低。
- `guest/kernel/kernel_runtime.c` 已继续下沉入口级 bring-up helper：`supervisor_demo` 入口改为复用统一的 trap bring-up，`interactive_os` 入口改为复用 `kernel_runtime_run_identity_superpage_bringup()`，`supervisor_demo_smoke` 的 storage signature + platform-tail 组合逻辑也已收敛到 `kernel_runtime`。
- `frontend/server/debug_server.mjs` 已统一 `debug_cli` 的 `{type:"error"}` 传播语义；`load / snapshot / step-cycle / step-commit / reset / terminal-input / terminal-output` 当前都不会再把 CLI 错误伪装成 `200` 假成功。

本轮已新鲜验证通过：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

## 2026-04-03 P1 第二批结构与门禁收口进展

本轮主线继续按 [status/project_priority_roadmap.md](project_priority_roadmap.md) 的 P1 已知问题做第二批收口，但重点仍然是合同一致性和现有门禁稳定性，而不是继续扩功能面：

- `myCPU/src/trap.cpp` 已把 `CLINT` timer pending 收口为平台电平语义：`TrapController` 当前会跟踪平台事件自己置过的 timer pending 位，并在 `mtime < mtimecmp` 或 handler 改大 `mtimecmp` 后稳定撤销对应 pending，不再把旧 timer pending 留成闩锁态。
- `myCPU/src/exec/branch_predictor.cpp` 与 `frontend/app/components/panels.js` 已把 predictor 对外统计口径统一成“已解析分支”合同；当前 `total_predictions / correct_predictions / mispredictions` 不再混入 wrong-path 上只被 fetch、从未真正解析的 branch，前端命中率展示也同步按这条口径收口。
- `frontend/server/debug_server.mjs` 已把 `DebugCliSession` 的 timeout / exit / close 语义收口到同一套 teardown 路径；当前 pending 请求会在超时或子进程退出时一致 reject，后续请求也不会再静默悬挂在失效 session 上。
- `myCPU/Makefile` 已继续放宽 `pipeline` guest demo 的门禁预算：当前 `PIPELINE_GUEST_TEST_TIMEOUT / PIPELINE_SUPERVISOR_GUEST_TEST_TIMEOUT` 分别为 `8s / 12s`。这次调整针对的是当前 host 上 `pipeline` guest bring-up 的真实运行时间，目的是避免 `guest_supervisor_demo`、`kernel_alpha_demo` 这类长路径在 debug 构建下被误报超时，而不是放松语义验证标准。

本轮已新鲜验证通过：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

## 2026-04-03 最小真实 OoO execute 补充进展

本轮主线已把 `Phase 3-C` 从“近似顺序 execute”继续推进到最小真实 `OoO execute`：

- [plan/history_plan.md#phase3-minimal-ooo-execute-plan](../plan/history_plan.md#phase3-minimal-ooo-execute-plan) 已完成；当前 `pipeline` 的退休逻辑已经从 `mem_wb` 单槽解绑，改由 `ROB head` 直接驱动 commit boundary。
- backend 当前已接上最小独立 memory execute：RAM / faulting access 会在 `ROB` 中形成可被 younger ALU 越过的最小 OoO 完成窗口；younger ALU 可以先把结果写入 `phys_regs + ROB ready`，但 architected GPR / CSR / RAM / MMIO 仍只在顺序 commit 时生效。
- 已知 MMIO load 当前继续维持 non-speculative 执行；这条限制是有意保留的，用来继续守住 `clint_split_access`、UART / CLINT / PLIC 和现有教学调试链路的设备可见性合同。
- `pipeline_rename_commit_smoke` 与 `pipeline_speculation_contracts_smoke` 现已直接覆盖“older memory 未完成时 younger ALU 先完成但不提前 commit”的新中间态；`pipeline_backend_smoke` 也同步守住了这次改动后的 interrupt commit-boundary 抢占边界。
- 本轮调试中还暴露并修正了一条真实设备时序回归：如果对已知 MMIO 访问也一律施加固定 memory issue delay，会破坏 `clint_split_access` 对 `mtime/time` 的可见 tick 合同。当前 backend 已改成只对 RAM / faulting access 保留最小 OoO 延迟窗口，已知 MMIO 维持原有非投机时序。
- 这意味着 `Phase 3` 设计文档要求的基础任务目前已经不再差“最小真实 OoO execute”这一块；当前剩余工作主要是 bug-driven hardening，以及是否继续扩 issue / replay / memory speculation。

关于当前主线中“回归相关工作做到什么程度可认为阶段性收口”的统一判断口径，见：

- [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
- 更清晰的当前优先级排序，见 [status/project_priority_roadmap.md](project_priority_roadmap.md)

## 2026-04-02 补充进展

本轮主线已把 `Phase 3-B/C` 从 readiness 推进到首轮最小接线状态：

- [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md) 与 [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md) 继续作为当前有效设计边界；当前实现没有偏离“单发射、统一 ISA 真值来源、in-order retire”的首轮约束。
- `pipeline` 已接上 `rename + ROB` 主路径：decode 侧会完成 `rename + ROB allocate`，younger 指令可通过 renamed source 读到 older 尚未 architecturally commit 的结果，而 architected GPR 只会在 `ROB head` commit 后真正切换。
- `pipeline` 已接上最小 `LSQ` 主路径：load / store 会进入 `LSQ` 管理，load 结果继续走 `ROB + phys-state`，RAM / MMIO store 只会在 commit boundary 真正落地；由于当前还没有 store-to-load forwarding / replay，younger load 会保守地等待 older store 离开 `ID/EX`，以守住 `clint_split_access` 这类顺序合同。
- 当前 mispredict、trap、trap-return flush，以及 commit-boundary interrupt service 都会统一回滚 speculative `rename / ROB / phys / LSQ` younger state；此前暴露过的 supervisor timer commit-boundary 卡死问题已通过这条 rollback 路径收口。
- 由于 `guest_supervisor_demo` 这类长 guest 路径会持续分配新 phys tag，phys register tag 已统一扩为 `uint32_t`，避免原先 `uint16_t` 在长时间运行下的回卷风险。
- 本轮继续把 `LSQ` 的 load-after-store 顺序合同收口成更细粒度的形态：store 会在 decode 侧先进入 `LSQ`，younger load 会按 `sequence_id + address/data-ready + address overlap` 判断是否需要等待；当前非重叠 load 不再被无谓拖慢，而 `clint_split_access` 这类重叠顺序合同仍保持稳定通过。
- 当时 `Makefile` 已把 `guest_supervisor_demo` 切到独立 timeout 预算；截至 `2026-04-03`，随着 `pipeline` guest bring-up 路径继续变长，当前 `PIPELINE_GUEST_TEST_TIMEOUT / PIPELINE_SUPERVISOR_GUEST_TEST_TIMEOUT` 已进一步调整为 `8s / 12s`，避免 host 上的长 guest 门禁误报超时。
- `debug_snapshot / debug_cli` 当前已补上最小 `ROB / LSQ` 观测面：能看到队列深度与 head sequence，继续与既有 stage / retire-trace / predictor 字段一起服务本地教学调试链路。

本轮已新鲜验证通过：

- `cd myCPU && make test-host-physical_register_file_smoke`
- `cd myCPU && make test-host-rename_map_smoke`
- `cd myCPU && make test-host-reorder_buffer_smoke`
- `cd myCPU && make test-host-load_store_queue_smoke`
- `cd myCPU && make test-host-pipeline_rename_commit_smoke`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-pipeline_backend_smoke`
- `cd myCPU && make test-host-backend_differential_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test`
- `cd frontend && node --test`

## 2026-04-03 补充进展

本轮主线继续把 `Phase 3-B/C` 的 phys 生命周期与 `LSQ` memory-order 合同收口到更稳的状态：

- [plan/history_plan.md#phase3-phys-free-list-plan](../plan/history_plan.md#phase3-phys-free-list-plan) 已完成；当前 `RenameMap` 不再只依赖 `next_phys_++`，而是同时维护 committed / speculative mapping 与 committed / speculative free-list。
- ROB head commit 现在会先把新 committed phys 从 free-list 移除，再回收 old committed phys；rollback / trap / interrupt flush 继续通过 committed checkpoint 恢复 speculative map 与 free-list 快照。
- 本轮新补的 `rename_map_smoke`、`pipeline_rename_commit_smoke` 与 `pipeline_speculation_contracts_smoke` 已分别守住 commit 后 stale phys 复用、rollback 后 free-list 恢复，以及 trap-return / flush 后 phys 不泄漏的合同。
- 本轮调试中还暴露并修正了一处由 free-list 引出的真实回归：如果一个 recycled phys 最终重新成为 committed live phys，而 committed free-list 没有同步移除它，后续 flush 后会把 live phys 再次发出。这个问题曾把 `test-pipeline-timer_interrupt` 打成 `X`，当前已通过 `RenameMap` 的 live-phys remove-on-commit 规则收口。
- [plan/history_plan.md#phase3-lsq-replay-contract-plan](../plan/history_plan.md#phase3-lsq-replay-contract-plan) 现已完成；当前 `LoadStoreQueue` 已提供最小 `LsqLoadState / LsqLoadStatus` 合同，能够显式区分 `none`、`blocked_by_unresolved_store`、`blocked_by_overlapping_store` 与 `replay_required`。
- 当前 `LSQ` 已能守住一条新的 late-overlap 合同：如果 younger load 已先通过，而 older store 地址稍后解析出来并确认 overlap，这条 younger load 会被稳定标记为 `replay_required`。
- `pipeline` 当前已把这条 `replay-needed` 中间态暴露到最小观测面：`DebugSnapshot` / debug JSON 现在会输出 `lsq_load_state`、`lsq_load_sequence_id` 与 `lsq_store_sequence_id`，用于说明当前是“需要 replay”而不是“已经 replay 完成”。
- [plan/history_plan.md#phase3-lsq-automatic-replay-plan](../plan/history_plan.md#phase3-lsq-automatic-replay-plan) 已完成；`pipeline` 已接上最小 automatic replay recovery：一旦 `LSQ` 中出现 `replay_required` load，backend 会在下一拍 cycle 入口沿现有 committed rollback + flush 主路径回到安全边界，并通过 `replay_flush` 观测位暴露这次恢复动作。
- 当前 automatic replay 仍然是 coarse、RAM-only recovery：它依赖 `LSQ` 已经给出 `replay_required`，恢复时直接回到当前 committed 边界重新取指；这条 replay 路径目前更像 recovery machinery，而不是高频自然触发的主执行策略。
- [plan/history_plan.md#phase3-lsq-store-to-load-forwarding-plan](../plan/history_plan.md#phase3-lsq-store-to-load-forwarding-plan) 已完成；`LoadStoreQueue` 已新增最小 forwarding helper，`pipeline` 在 `step_mem(load)` 时会先尝试对 older ready RAM store 做 full-cover forwarding，再决定是否回落到 `AddressSpace::load_result()`。
- 当前 forwarding 仍然严格停留在最小边界：只支持 `RAM-only`、只支持 full-cover forwarding、不做 MMIO forwarding，也不做复杂 partial merge / 更激进的 memory disambiguation；`backend_differential_smoke` 继续只守 architected 一致性，不让 pipeline 私有中间态泄漏到 `functional` 路径。

本轮已新鲜验证通过：

- `cd myCPU && make test-host-physical_register_file_smoke`
- `cd myCPU && make test-host-rename_map_smoke`
- `cd myCPU && make test-host-reorder_buffer_smoke`
- `cd myCPU && make test-host-load_store_queue_smoke`
- `cd myCPU && make test-host-pipeline_rename_commit_smoke`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-backend_differential_smoke`
- `cd myCPU && make test-pipeline-timer_interrupt`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test`

## 2026-03-26 补充进展

本轮主线已经完成一批新的 hardening 回归扩充：

- `tests/asm/illegal_integer_encodings.S` 已扩展更多非法整数编码样本。
- `tests/asm/mmio_access_faults.S` 已把 CPU 侧非法 MMIO 访问的 access-fault 合同接入 asm 门禁。
- `tests/unit/elf_loader_segments.cpp`、`tests/unit/elf_loader_rejects.cpp` 和 `tests/unit/elf_loader_header_rejects.cpp` 已补上更真实的 ELF segment/layout 与 malformed-input reject 回归。
- `tests/unit/bus_device_guards.cpp` 与 `tests/unit/mmio_contract_matrix.cpp` 已把 host-side MMIO guard / contract 做到第一轮矩阵化。
- `tests/asm/csr_illegal_matrix.S` 已把 CSR 非法访问、跨特权级访问和只读 CSR 写入的 trap 合同补成第一轮矩阵。
- `tests/asm/sv39_mprv.S` 已把 `mstatus.MPRV` 驱动的 Sv39 数据访存合同接入 asm 门禁；当前 `M-mode` 下 `load/store` 在 `MPRV=1` 时，会按 `MPP` 指定的有效特权级走地址翻译，并遵守 `SUM/MXR` 权限检查。
- `tests/asm/sv39_pagewalk_contracts.S` 已把 Sv39 page-walk 的 misaligned superpage 与 non-leaf PTE 保留位合同接入 asm 门禁；当前 non-leaf PTE 不再把带 `U/A/D` 保留位的条目误当成有效下级页表。
- `guest/kernel/kernel_runtime.c` 已新增 `kernel_runtime_run_bringup()`，把 common bring-up options 里的 runtime/self-context 装配下沉到 guest runtime 基础设施层；当前 `kernel_alpha` 入口与 interrupt/storage contract helper 不再手写 `pre_vm_context = &runtime`，相关边界已由 `tests/unit/kernel_runtime.c`、`tests/unit/kernel_alpha_interrupt.c` 与 `tests/unit/kernel_alpha_storage.c` 守住。
- `tests/asm/sv39_exec_privilege.S` 已把 Sv39 `U/S` 特权边界补成一条更完整的 asm 合同；当前 `S-mode` 对 `U=1` 可执行页的取指，以及 `U-mode` 对 supervisor-only 可执行页 / data page 的取指、load、store都会稳定触发 page fault，这条合同也已同时守住 `pipeline` asm 门禁。
- `tests/host/address_space_faults_smoke.cpp` 已补上 `AddressSpace::fetch32_result()` / `fetch32()` 在 Sv39 instruction page fault 下的接口合同；当前 result API 会返回 fault 而不落 trap CSR，legacy wrapper 仍会稳定进入 trap。
- `tests/host/backend_differential_smoke.cpp` 已新增 `delegated_user_ecall_to_supervisor`、`sret_to_user_halt`、`machine_timer_interrupt_at_cycle_start`、`sv39_mprv_fault`、`sv39_instruction_page_fault`、`sv39_load_page_fault`、`sv39_store_page_fault`、`sv39_reserved_non_leaf_fault`、`supervisor_mmio_instruction_access_fault`、`supervisor_mmio_load_access_fault`、`supervisor_mmio_store_access_fault`、`supervisor_timer_interrupt_after_mret`、`supervisor_timer_interrupt_after_sip_write`、`supervisor_timer_interrupt_after_sie_write`、`supervisor_timer_interrupt_after_sstatus_write`、`supervisor_timer_interrupt_at_cycle_start`、`user_mode_supervisor_timer_interrupt_at_cycle_start`、`user_mode_supervisor_external_interrupt_after_sret`、`supervisor_external_interrupt_after_sip_write` 与 `user_mode_supervisor_external_interrupt_at_cycle_start` 二十条 host-side 差分场景；当前已把 machine timer interrupt 基线、delegated user-ecall / `sret` privilege transition、delegated supervisor page-fault、delegated supervisor MMIO instruction/load/store access-fault，以及由 `sip/sie/sstatus/mret/sret` 驱动的 supervisor timer / external interrupt 在 S-mode / U-mode 下的 cycle-start / commit-boundary 稳定路径接入差分门禁。
- `tests/host/pipeline_backend_smoke.cpp` 已补上 `CLINT` 驱动的 supervisor timer interrupt 与 `PLIC+UART` 驱动的 supervisor external interrupt 两条 host-side smoke，用来守真实平台事件源在 `pipeline` 下的 flush / handler / return 基线；这两类路径按 cycle 前进，不再强行并入 functional-vs-pipeline 的逐事件差分。
- `tests/host/debug_cli_smoke.cpp` 已改成自包含 flat-binary smoke：当前会直接驱动最小 supervisor timer / external interrupt 场景，并检查 `DebugSnapshot` 中 `CLINT` / `PLIC` / `UART` 的关键字段，以及 `trap` / `flush` / `halt` 等 pipeline 可观察性事件。
- `frontend/tests/debug_server.test.mjs` 已把本地调试服务的 richer snapshot / event 透传和 WebSocket 广播接入 Node 门禁；当前会检查 `CLINT` / `PLIC` / `UART` 关键字段、pipeline `trap_flush / committed` 标志，以及 `run` 中重新 `load` 会先停掉旧会话定时器，避免教学 demo 在会话切换后被后台偷偷推进。
- `pipeline` 本轮又修正了一处 commit-boundary 时序：由刚退休 CSR 写入或 `mret/sret` trap-return 新触发可递送的 pending interrupt，不再让返回后的 younger 指令继续退休；同时保留“对 cycle 起点已可递送的 interrupt 仍可抢占”的路径，避免 tight loop 下的 timer interrupt 饥饿。
- `design/regression_completion_criteria.md` 已成为当前 Phase 1 / Phase 2 回归收口的正式判断口径。
- `docs/` 正式文档已经收口到 `background / design / plan / status + AGENTS.md + index.md` 结构，后续不再新增 `contracts / templates / archive / superpowers` 这类平行正式目录。

## 2026-03-27 补充进展

本轮主线已完成 `Phase 3-A` 第一轮分支预测增强：

- `src/exec/branch_predictor.*` 已引入最小 predictor 子模块；当前条件分支使用 `2-bit` bimodal counter + target 记忆，`jal` 使用 static predict-taken，`jalr` 仍维持不预测。
- `src/exec/pipeline_backend.*` 已把 predictor 接到 fetch / execute 主路径；当前分支预测只影响 `pipeline` 内部取指方向与 mispredict flush / redirect，不改变 `functional + shared InstructionSemantics` 的 ISA 真值来源，也不改变 in-order 提交模型。
- `tests/host/predictor_smoke.cpp` 已补上 predictor 独立 smoke，守住 `query / update / reset / stats` 最小合同。
- `tests/host/pipeline_backend_smoke.cpp` 已补上 `jal` predict-hit、predictable branch loop 与 backend rebuild 后 predictor cold-reset 三条 host-side smoke。
- `tests/host/backend_differential_smoke.cpp` 已新增 `predictable_branch_loop` 差分场景，继续守住 predictor 参与后 `functional vs pipeline` 的 architected 一致性。
- `DebugSnapshot` / `debug_cli_smoke` 已补 predictor mode / counters / 最近一次预测 / 最近一次 mispredict 字段；`frontend` 现有服务与纯状态测试继续兼容这些 richer snapshot。

## 2026-03-30 补充进展

本轮主线已把 guest runtime 的第一轮结构收口继续推进到 user lifecycle / smoke orchestration 层：

- `guest/kernel/kernel_bringup.c`、`guest/kernel/kernel_runtime.c`、`guest/kernel/vm_address_space.c`、`guest/kernel/vm_process.c`、`guest/kernel/vm_object.c`、`guest/kernel/vm_fault.c`、`guest/kernel/trap.c`、`guest/kernel/trap_dispatch.c`、`guest/kernel/supervisor_runtime.c`、`guest/kernel/user_task.c`、`guest/kernel/user_task_bootstrap.c`、`guest/kernel/user_program.c`、`guest/kernel/user_program_smoke.c` 与 `guest/kernel/supervisor_demo_smoke.c` 已完成一轮更系统的职责收口。
- 当前收口重点仍是行为不变的结构整理：
  - lifecycle / binding / rollback helper 下沉
  - trap / fault / wait / policy adapter 的重复分支收口
  - smoke prepare / round / platform-tail 的临时组装内聚
- `Makefile` 已新增并接入以下 host-side 单元门禁：
  - `test-unit-vm_process`
  - `test-unit-vm_object`
  - `test-unit-vm_fault`
  - `test-unit-trap_runtime`
  - `test-unit-trap_dispatch`
  - `test-unit-user_task`
  - `test-unit-user_task_bootstrap`
  - `test-unit-user_program`
  - `test-unit-user_program_smoke`
- `tests/unit/include/riscv.h` 也已同步补齐 guest/runtime 侧 host build 所需的最小 CSR / trap 常量与 stub 声明，避免为了单元编译去污染真实 guest 路径。
- 本轮收口后，`make test` 与 `make test-pipeline` 仍保持通过，`guest_supervisor_demo` 输出仍为 `KRN`，`kernel_alpha` 十条基线保持不变。

## Phase 1 近期主线

当前仍应优先推进的 Phase 1 / post-Phase1 主线工作如下：

1. 继续稳住 simulator reference path 的 correctness 与可观察性。
2. 在已落地第一轮 illegal / MMIO / ELF / CSR hardening 矩阵的基础上，继续按合同补洞，而不是重复堆叠同类回归。
3. 继续守住 `kernel_alpha` 十条回归基线：
   - `kernel_alpha_demo`
   - `kernel_alpha_fault_demo`
   - `kernel_alpha_storage_no_media_demo`
   - `kernel_alpha_storage_not_ready_demo`
   - `kernel_alpha_storage_bad_magic_demo`
   - `kernel_alpha_storage_bad_block_count_demo`
   - `kernel_alpha_storage_lba_range_demo`
   - `kernel_alpha_storage_bad_command_demo`
   - `kernel_alpha_plic_not_ready_demo`
   - `kernel_alpha_timer_not_ready_demo`
4. 继续推进 guest runtime 的 process / runtime refinement 与大文件拆分，尤其守住 `vm*`、`trap*`、`kernel_runtime`、`kernel_bringup`、`user_task*`、`user_program*` 与 `supervisor_demo_smoke` 当前已经形成的边界。

这些工作仍然是近期主线，不应因为 `pipeline` / `debug/frontend` 已接入而被搁置。

## 2026-03-27 补充进展

本轮主线已把最小可交互 monitor OS 路线推进到 guest / debug protocol / frontend / host smoke 四层闭环：

- `Uart16550` 已补最小 RX 合同，guest 侧轮询式 `platform_uart_rx_ready()` / `platform_uart_getc()` 已可用。
- `debug_session` / `debug_protocol` 已补 `uart_input(text)` 与 `uart_output(offset)`，并增加面向交互 smoke 的条件推进命令：
  - `run_until_uart_contains`
  - `run_until_halt`
- debug JSON string 解析已支持基础转义，`uart_input("help\\r")` 这类终端输入现已按真实回车语义进入 guest。
- 新的 `guest/interactive_os` 已独立落地，当前最小 monitor 闭环已成立：
  - banner / prompt
  - 可见 ASCII 回显
  - `Enter`
  - `Backspace`
  - 固定长度行缓冲
  - `help` / `echo` / `time` / `uptime` / `halt`
  - `disk info` / `disk read`
  - `regs` / `peek` / `pagewalk` / `pte dump`
- 前端已完成 Task 4 的终端桌面壳收口：
  - 主界面切为“终端主舞台 + 可折叠 Debug inspector”
  - terminal I/O 通过独立 API / WebSocket 增量同步，不并入 `DebugSnapshot`
  - 页面默认点击终端后才接管键盘
- `tests/unit/monitor_commands.c` 已接入，当前 monitor 正式命令集已有 host-side 单元门禁。
- `make test` / `make test-pipeline` 已纳入：
  - `test-unit-monitor_commands`
  - `test-host-interactive_terminal_smoke`
  - `test-guest-interactive_os_demo`
  - `test-pipeline-guest-interactive_os_demo`
- `tests/asm/mmio_access_faults.S` 已按 UART RX 新合同修正第二个 UART case：
  - 旧用例把 `UART_REG_THR` 读当作非法访问，但在引入 `UART_REG_RBR` 后，offset `0x0` 的字节读已成为合法 RX 路径；
  - 当前 asm 回归改为验证 `UART` 基址上的非法读宽度，继续守住 CPU 侧 `load access fault` 合同。
- `Makefile` 已把 `guest_supervisor_demo` 切到独立 timeout 预算：
  - functional 实测约 `2.33s`，pipeline 实测约 `4.51s`；
  - 当前不再复用 `2s / 4s` 的通用门限，避免既有高成本 smoke 误报失败。
- 为了把 monitor 启动成本控制在 host-driven smoke 可接受范围内，`interactive_os` 当前采用独立的最小 Sv39 bring-up 路径：
  - kernel 高地址 RAM 走 1G identity superpage
  - 低地址 MMIO 区走 1G identity superpage
  - 不改 `kernel_alpha` 默认 `kernel_runtime_run_bringup()` 路线

本轮已新鲜验证通过：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-unit-monitor_commands`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-pipeline-guest-supervisor_demo`
- `cd myCPU && make test-guest-interactive_os_demo`
- `cd myCPU && make test-pipeline-guest-interactive_os_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`

## 2026-03-30 补充进展

本轮继续把 `interactive_os / monitor / vm_debug` 往 post-Phase1 hardening 收口：

- `monitor_format.c` 现在承接 monitor 数字 / 十六进制 / ASCII preview 输出 helper，`monitor_commands.c` 不再继续持有这组格式化细节。
- `vm_debug.c` 已补 `vm_debug_read()` 只读调试 helper，`peek` 当前会先验证地址与宽度，再读取映射内容；未命中地址空间时返回 `peek miss ...`，不再让 monitor 自己制造 guest fault。
- `tests/unit/monitor_commands.c` 已扩 `disk read` 参数使用、`peek` unmapped/misuse 和 `pagewalk`/`pte` 错误路径回归。
- `tests/host/interactive_terminal_smoke.cpp` 已补 guest miss-path smoke，验证 `interactive_os` 在 functional / pipeline 下都能稳定暴露 `peek miss` 输出。

本轮已新鲜验证通过：

- `cd myCPU && make test-unit-monitor_commands`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd myCPU && make test-guest-interactive_os_demo`
- `cd myCPU && make test-pipeline-guest-interactive_os_demo`

同日，`interactive_os` 的浏览器终端输入链路也补了一轮最小 hardening：

- 前端 terminal 输入已改为批量排队发送，不再因为前一个字符尚未返回就直接丢掉后续按键。
- debug server 当前会区分“guest 可能回显的输入”和“guest 明确会忽略的输入”；后者现在直接返回当前 terminal delta，不再无意义推进整段 commit budget。

本轮已新鲜验证通过：

- `cd frontend && node --test`

## 2026-03-31 补充进展

本轮没有继续扩功能面，而是做了一轮文档与自检收口：

- [status/code_self_review_status.md](code_self_review_status.md) 已重写为“当前有效的自检结论 + 活跃风险 + 下一步顺序”，并吸收 `interactive_os terminal` 专项复检结果。
- `guest/kernel/kernel_runtime.c` 已继续收口 bring-up 基础设施：当前 `kernel_runtime_run_common_bringup()` 会为带 `pre_vm_setup` 且未显式给出 context 的调用自动回填 runtime self-context；`PLIC / first delivery / storage probe/signature` 这组 phase helper 也已下沉到 `kernel_runtime`。`guest/kernel_alpha/common.c` 当前只保留 alpha marker / 命名 wrapper，相关边界由 `tests/unit/kernel_runtime.c` 与 `tests/unit/kernel_alpha_common.c` 一起守住。
- `minimal_interactive_os` 的前端终端壳合同已经并回 [design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)，不再单独维护拆开的 shell 设计文档。
- `pipeline` 早期准备阶段结论已经并回 [design/pipeline_core_integration.md](../design/pipeline_core_integration.md)，不再单独维护独立的 prep 文档。
- 已完成且内容已被 `design/status` 吸收的 `docs` 治理、`debug/frontend` 接入和 `minimal_interactive_os` 执行计划，当前不再单独保留为 `plan` 文档。
- [readme.md](../../readme.md) 已收回入口文档定位，只保留项目定位、快速开始、`interactive_os` 演示、测试入口和文档导航。

## Phase 2 当前安排

当前对 Phase 2 的理解和安排如下：

- `pipeline core` 与 `debug/frontend` 的正式接入工作已经完成。
- 近期不再把 Phase 2 理解成“继续搬运更多旧分支代码”，而是进入稳定化和验证补强阶段。
- 当前最重要的 Phase 2 工程问题，不是继续扩 UI 或继续引入新模型，而是按已新增的回归收口标准把出门条件落实到差分和快照门禁。

在这个前提下，Phase 2 近期优先级如下：

1. 当前仓库对 Phase 2 的最小完成标准已经基本成立，后续优先维护既有门禁而不是继续扩功能面。
2. 继续维护 `pipeline` 的 correctness / differential / robustness 门禁；新增问题出现时补最小回归，而不是重复堆叠同类 case。
3. 继续把 `debug/frontend` 限定在“教学演示可用”的最小范围，重点守住快照结构、协议输出和本地测试门禁。
4. 在上述工作稳定之前，不急着把更多执行模型或更大的调试功能面并入当前主线。

## 当前仍然有效的风险 / 限制

- reference robustness 回归已经完成第一轮系统扩充；当前 `pipeline differential` 的高风险主干矩阵已基本形成闭环，后续剩余工作主要转为低收益变体控制和新增 bug 的定向回归。
- guest runtime 虽已完成第一轮拆分，`user_program_smoke / supervisor_demo_smoke` 的第一轮 orchestration 收口，以及 `kernel_runtime / supervisor_runtime / user_program_smoke` 的第一轮 public header 收口也已完成，但 `vm*`、`trap*`、`kernel_bringup` 与这些 helper 的后续边界仍需要继续守住，避免回退到单个大文件或重新暴露可变内部布局。
- guest smoke 当前已把编排边界收窄，但 `supervisor_demo_smoke` 仍缺直接单测，`user_program_smoke` 的 active-memory / interrupt round 失败路径也仍主要靠 guest demo 间接覆盖。
- `kernel_alpha` 已经达到 Phase 1 核心完成态，但更多 device readiness / fault / panic / runtime refinement 仍属于 post-Phase1 hardening。
- `pipeline` 已经正式可用，privileged / trap / interrupt / MMIO 行为的一致性验证主干也已基本成体系；后续以维护既有差分门禁和按新增问题补最小持久回归为主。
- 本轮收尾后，`interactive_os` 相关改动与总门禁已同步恢复到通过状态。
- `Phase 3-A` predictor 当前仍是首轮最小实现：条件分支 `2-bit` counter + target 记忆、`jal` static predict-taken、`jalr` 不预测；后续应先以 bug-driven hardening 与最小持久回归补洞为主，不急着扩成复杂 BTB / RAS / 多级 predictor。
- `debug/frontend` 已经正式接入，但仍应避免膨胀成断点 / 条件暂停 / 任意文件加载的通用调试器。
- `Phase 3-B/C` 虽已接上首轮 `rename + ROB + LSQ`、最小 phys free-list / recycle、coarse automatic replay recovery、`RAM-only` store-to-load forwarding与最小真实 `OoO execute`，但当前仍是单发射、in-order retire、最小 OoO 完成窗口的克制形态：还没有 MMIO forwarding、复杂 partial merge、显式 issue queue 或更激进的 memory speculation。

## 下一步

1. 先沿 reference path 继续维护已形成闭环的 `privilege / Sv39`、illegal / MMIO / ELF / CSR 合同矩阵，并在新增 bug 出现时补最小持久回归。
2. 继续把 `kernel_alpha` 十条回归和 `guest_supervisor_demo` 守在稳定输出上。
3. 继续推进 guest runtime 的 process / runtime refinement 与大文件拆分，但避免破坏现有层次边界；当前 `interactive_os / monitor / vm_debug`、`guest smoke orchestration` 和 guest public header API 的第一轮 hardening 都已完成，下一块更值得继续推进的是 `kernel_runtime / kernel_bringup` 的继续收口，以及 `supervisor_demo_smoke / user_program_smoke` 的更窄单测补洞。
4. 当前 Phase 2 的最小收口已经基本成立；后续按 [design/regression_completion_criteria.md](../design/regression_completion_criteria.md) 以维护既有 `pipeline` 差分 / 快照门禁和新增 bug 定向回归为主，而不是继续做低收益 case 堆叠。
5. `minimal_interactive_os` 计划当前也已完成；后续只在新增 bug 或设计边界变化时补最小持久回归，而不是继续把它扩成图形桌面项目。
6. 继续沿 [plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan) 维护 `Phase 3-B/C` 当前已落地的 `rename + ROB + LSQ + phys free-list / recycle + coarse automatic replay + RAM-only forwarding +` 最小真实 `OoO execute` 形态，优先守住现有 host / guest / debug 门禁，然后再考虑更激进的 issue / replay / memory speculation。
7. 在不扩功能面的前提下，继续维护 `debug/frontend` 教学演示链路的稳定测试；如果继续做 `P1`，当前最值得优先收口的就是 `debug_protocol.cpp / debug_server.mjs` 的协议层、会话层与 terminal projection 边界。

## 建议入口

新对话如果要继续推进当前主线工作，建议优先阅读：

- [AGENTS.md](../../AGENTS.md)
- [myCPU/AGENTS.md](../../myCPU/AGENTS.md)
- [myCPU/guest/AGENTS.md](../../myCPU/guest/AGENTS.md)
- [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
- [status/project_priority_roadmap.md](project_priority_roadmap.md)
- [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
- [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
- [plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan)
- [plan/history_plan.md#phase3-ooo-readiness-plan](../plan/history_plan.md#phase3-ooo-readiness-plan)
  当前作为已完成的 OoO readiness 收口记录保留。
- [status/code_self_review_status.md](code_self_review_status.md)
- [status/kernel_alpha_status.md](kernel_alpha_status.md)

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-unit-supervisor_runtime`
- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-unit-kernel_alpha_common`
- `cd myCPU && make test-unit-kernel_alpha_interrupt`
- `cd myCPU && make test-unit-kernel_alpha_storage`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_magic_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_lba_range_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_command_demo`
- `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`
