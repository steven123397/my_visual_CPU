# Pipeline 投机执行与提交契约

## 文档定位

本文档只回答当前 `pipeline` 的一类核心问题：哪些结果可以在投机阶段产生，哪些动作必须等到 architected commit boundary 才能对外生效。

与 `phase3_ooo_execution_model_design.md` 的分工如下：

- `phase3_ooo_execution_model_design.md`
  - 解释当前执行模型、`rename / ROB / LSQ` 边界、分支预测和后续取舍。
- 本文档
  - 解释 architected state、side effect、rollback、precise exception / interrupt 的正式 contract。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关设计：
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
- 已完成计划归档：
  - [../plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan)
  - [../plan/history_plan.md#phase3-ooo-readiness-plan](../plan/history_plan.md#phase3-ooo-readiness-plan)

## 背景与问题

当前 `pipeline` 已经不是纯 in-order 五级流水，而是带有最小 `rename + ROB + LSQ +` 真实 `OoO execute` 的执行后端。随着 execute 与 architected 提交逐步分离，真正需要长期保留的，不再是“某一轮实现时怎么接线”，而是“哪些结果允许先算出来、哪些副作用绝不能提前泄漏”。

如果这条边界不清楚，就容易在后续调整中反复引入同一类问题：

- younger 指令被 squash 后仍污染 RAM、MMIO 或 CSR
- trap / interrupt 没有在 precise 的 commit boundary 被观察到
- `satp` / `sfence.vma` / `mret` / `sret` 在错误阶段提前生效
- 向量或标量路径各自偷偷形成第二套 side effect 口径

## 目标

- 明确当前 speculative result 与 architected side effect 的边界。
- 明确 precise exception / interrupt 的统一观察口径。
- 明确 RAM、MMIO、CSR、trap-return、TLB、向量状态在投机与 squash 下的正式合同。
- 为后续 host smoke / differential / debug snapshot 提供稳定解释基础。

## 非目标

- 不展开更激进的 replay / recovery 机制设计。
- 不把 `functional` 改造成 speculative backend。
- 不在本文档中规定具体 predictor 算法、`ROB` 深度或 `LSQ` 容量。

## 当前契约

### 1. speculative result 与 architected side effect 的边界

当前 `pipeline` 允许在 commit 之前先 materialize 或暂存的结果，包括：

- 标量 ALU 结果
- 分支决议结果
- load 读取结果
- CSR 指令计算出的待写入值
- store 的地址 / 数据准备状态
- non-memory vector ALU 的 materialized result

但以下动作默认必须等到 architected commit boundary 才能生效：

- architected GPR / CSR 最终写入
- RAM store 真正落内存
- MMIO load / store 对设备产生可观察副作用
- halt
- `mret / sret` 的 privilege / PC 切换
- `satp` 与 `sfence.vma` 驱动的 architected TLB 可见性变化
- architected vector state 最终落地

### 2. precise exception / interrupt 合同

当前 `pipeline` 必须继续满足 precise exception / interrupt：

- architecturally older 的 fault / trap 之前，younger 指令不得留下可见副作用。
- interrupt 只能在 architected commit boundary 被正式递送。
- 若指令在 retirement 前被 squash，它的异常、CSR 写、store、MMIO side effect 都必须一起失效。
- rollback / flush 统一裁剪 younger speculative state，而不是按“谁先执行完”裁剪。

### 3. RAM store 合同

当前普通 RAM store 的正式语义是：

- store 可以提前形成地址与数据，并进入 `LSQ` 暂存。
- store 在 commit 之前不得真正写入 RAM。
- store 若在 commit 前被 squash，RAM 内容必须保持旧值。
- store 一旦在 commit boundary 退休，其结果必须立刻成为新的 architected 可见值。

### 4. MMIO 合同

当前 MMIO 比 RAM 更严格：

- MMIO load / store 不得在投机阶段对设备生效。
- 被 squash 的 younger MMIO store 不得命中设备。
- 当前 `LSQ` 不把 MMIO 当作可自由投机的普通 memory 请求。
- device side effect 必须统一沿 commit-boundary 理解，而不是在局部执行阶段绕开 contract。

这条边界当前直接保护 UART、CLINT、PLIC、`SimpleStorage` 以及 `debug/frontend` 可观察状态。

### 5. CSR、trap-return 与 halt 合同

当前 CSR 与控制类指令遵守如下合同：

- CSR 写在投机阶段最多形成“待提交写入值”。
- 被 squash 的 younger CSR 写不得影响后续 architected 观察。
- `mret / sret` 的 privilege / EPC / PC 切换只在 commit boundary 生效。
- halt 也只在 commit boundary 生效；被 squash 的 younger halt 不得提前停机。

### 6. `satp`、`sfence.vma` 与 TLB 合同

当前地址空间相关动作也必须围绕 architected commit boundary 理解：

- `satp` 写入只有在 commit 后才成为新的 architected address-space 配置。
- `sfence.vma` 的 flush 只有在 commit 后才真正影响后续 architected fetch / load / store。
- 被 squash 的 younger `satp` / `sfence.vma` 不得污染 TLB 或 page-walk 可见性。

### 7. 当前向量执行合同

当前向量路径也必须服从同一套提交口径：

- non-memory vector ALU 可以 execute 先 materialize，commit 再落地 architected vector state。
- `vsetcfg / vle.v / vse.v` 当前仍保持保守 serializing 处理。
- 被 squash 的 younger vector 指令不得污染 architected vector state。
- 向量 memory path 当前不得绕开现有 MMIO / RAM / fault 合同。

### 8. rollback 与观测合同

当前 rollback / flush 的正式要求是：

- younger speculative `rename / ROB / phys / LSQ` state 可以被统一回滚。
- 已被 squash 的 younger 指令不得继续出现在 retire trace 中。
- debug snapshot / CLI 只能暴露当前真实、仍可解释的 speculative state，而不能把已被裁掉的 younger 行为伪装成 architected 结果。

## 验证思路

当前与本 contract 直接相关的正式基线至少包括：

- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-pipeline_rename_commit_smoke`
- `cd myCPU && make test-host-backend_differential_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-vector_pipeline_smoke`

后续若继续调整 commit-boundary、rollback 或 vector / memory 交界，优先补最窄专项回归，而不是扩大到新的大而宽 smoke。

## 风险与取舍

- 当前把大量动作都推迟到 commit boundary，会让 backend 内部实现比纯 in-order 更繁琐，但这是 precise exception、可调试性和统一可观察性的必要成本。
- 当前继续把 MMIO 视为 non-speculative，会限制一部分表面灵活度，但这对当前仓库的设备模型和调试链路是必要取舍。
- 当前向量路径仍刻意不扩成独立 memory speculation 模型，这会推迟更像真实硬件的表现，但能显著降低 side effect 边界失控的风险。

## 当前有效性说明

- 当前有效：本文档作为当前 `pipeline` 投机执行、commit boundary 与 side effect 可见性的正式 contract。
- 当前实现状态和后续取舍，以 [../status/mainline_status.md](../status/mainline_status.md) 为准。
