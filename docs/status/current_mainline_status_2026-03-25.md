# 当前主线状态（2026-03-25，更新到 2026-03-26）

## 文档定位

本文档用于记录 `phase1-stable` 冻结后、`pipeline core` 与 `debug/frontend` 已完成正式接入之后，当前主线仍需继续推进的工程任务。

它面向下一轮实现工作，重点回答：

- Phase 1 近期主线还剩什么
- 已接入的 Phase 2 能力当前按什么方式继续推进
- 新对话继续工作时，应该优先看哪些入口文档和验证门禁

## 当前状态

当前主线已经稳定成立的事实如下：

- 仓库当前已经是一个已可运行的模拟器原型，而不是纯设计稿。
- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成。
- 默认 `functional` reference path、独立 `kernel_alpha` 正向与九条负向回归、`make test` 主门禁均已打通。
- `pipeline core`、`--backend pipeline`、`make test-pipeline`、`debug_session/protocol`、本地 Node 调试服务与浏览器前端教学演示链路都已经正式接入。

这意味着当前主线不再把 `pipeline` 与 `debug/frontend` 视为“待合入功能”，而是把它们视为已经落地、需要继续稳定化的现有能力。

关于当前主线中“回归相关工作做到什么程度可认为阶段性收口”的统一判断口径，见：

- [regression_completion_criteria_2026-03-26.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/regression_completion_criteria_2026-03-26.md)

## 2026-03-26 补充进展

本轮主线已经完成一批新的 hardening 回归扩充：

- `tests/asm/illegal_integer_encodings.S` 已扩展更多非法整数编码样本。
- `tests/asm/mmio_access_faults.S` 已把 CPU 侧非法 MMIO 访问的 access-fault 合同接入 asm 门禁。
- `tests/unit/elf_loader_segments.cpp`、`tests/unit/elf_loader_rejects.cpp` 和 `tests/unit/elf_loader_header_rejects.cpp` 已补上更真实的 ELF segment/layout 与 malformed-input reject 回归。
- `tests/unit/bus_device_guards.cpp` 与 `tests/unit/mmio_contract_matrix.cpp` 已把 host-side MMIO guard / contract 做到第一轮矩阵化。
- `tests/asm/csr_illegal_matrix.S` 已把 CSR 非法访问、跨特权级访问和只读 CSR 写入的 trap 合同补成第一轮矩阵。
- `regression_completion_criteria_2026-03-26.md` 已成为当前 Phase 1 / Phase 2 回归收口的正式判断口径。

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
4. 继续推进 guest runtime 的 process / runtime refinement 与大文件拆分，尤其守住 `vm*`、`trap*`、`kernel_runtime`、`kernel_bringup` 当前已经形成的边界。

这些工作仍然是近期主线，不应因为 `pipeline` / `debug/frontend` 已接入而被搁置。

## Phase 2 当前安排

当前对 Phase 2 的理解和安排如下：

- `pipeline core` 与 `debug/frontend` 的正式接入工作已经完成。
- 近期不再把 Phase 2 理解成“继续搬运更多旧分支代码”，而是进入稳定化和验证补强阶段。
- 当前最重要的 Phase 2 工程问题，不是继续扩 UI 或继续引入新模型，而是按已新增的回归收口标准把出门条件落实到差分和快照门禁。

在这个前提下，Phase 2 近期优先级如下：

1. 按 [regression_completion_criteria_2026-03-26.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/regression_completion_criteria_2026-03-26.md) 落实当前仓库对 Phase 2 的最小完成标准。
2. 继续补强 `pipeline` 的 correctness / differential / robustness 验证。
3. 继续把 `debug/frontend` 限定在“教学演示可用”的最小范围，重点守住快照结构、协议输出和本地测试门禁。
4. 在上述工作稳定之前，不急着把更多执行模型或更大的调试功能面并入当前主线。

## 当前仍然有效的风险 / 限制

- reference robustness 回归已经完成第一轮系统扩充，但 `privilege / Sv39 / pipeline differential` 仍未完全形成闭环。
- guest runtime 虽已完成第一轮拆分，但 `vm*`、`trap*`、`kernel_runtime`、`kernel_bringup` 仍需要继续守住边界，避免回退到单个大文件。
- `kernel_alpha` 已经达到 Phase 1 核心完成态，但更多 device readiness / fault / panic / runtime refinement 仍属于 post-Phase1 hardening。
- `pipeline` 已经正式可用，但后续 privileged / trap / interrupt / MMIO 行为的一致性验证仍应继续补强。
- `debug/frontend` 已经正式接入，但仍应避免膨胀成断点 / 条件暂停 / 任意文件加载的通用调试器。

## 下一步

1. 先沿 reference path 继续补 `privilege / Sv39` 等仍未闭环的边界，并保持已落地的 illegal / MMIO / ELF / CSR hardening 矩阵稳定可回归。
2. 继续把 `kernel_alpha` 十条回归和 `guest_supervisor_demo` 守在稳定输出上。
3. 继续推进 guest runtime 的 process / runtime refinement 与大文件拆分，但避免破坏现有层次边界。
4. 已有 Phase 2 出门标准文档，下一步按 [regression_completion_criteria_2026-03-26.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/regression_completion_criteria_2026-03-26.md) 把 `pipeline` 差分矩阵继续做实，而不是停留在原则层。
5. 在不扩功能面的前提下，继续补强 `pipeline` 差分与 `debug/frontend` 稳定性验证。

## 建议入口

新对话如果要继续推进当前主线工作，建议优先阅读：

- [AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/AGENTS.md)
- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
- [myCPU/guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)
- [regression_completion_criteria_2026-03-26.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/regression_completion_criteria_2026-03-26.md)
- [code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_2026-03-24.md)
- [kernel_alpha_bringup_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_bringup_status.md)

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
