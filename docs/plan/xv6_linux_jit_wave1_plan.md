# xv6 / Linux / JIT Wave 1 实现计划

> **文档状态：** 执行中（`2026-04-21` 已完成主线切换设计、状态文档、4 个 worktree 编排与第一轮 handoff 收集；`2026-04-22` 已按 `A -> B -> C -> D` 完成第一轮主工作树整合，并完成首轮 B / C post-integration follow-up：PLIC source split、`Machine` / CLI / debug CLI block transport 选择、`mycpu_virt` board 切到 `virtio-blk`；同日进一步的 A / B bug-driven follow-up 已把 `xv6` 推到 shell，并落下 Linux-facing `flat/payload/set_gpr + linux_proto` foundation；`2026-04-24` 又修正了 `linux_proto` 在缺 `rootfs.cpio` 时会被 Make 依赖提前卡死的问题，并补上 repo-generated 最小 `rootfs.cpio` `/init` fallback；同日 `linux_sbi_shim` 也补上 early-trap UART 诊断，functional 路径补上最小 `RV64C`，modern `virtio-mmio` 也补齐 `VIRTIO_F_VERSION_1`，本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 已可稳定走到 `/init reached`；`2026-04-25` 又补上 `LINUX_PROTO_ROOTFS_MODE=block` 与 repo-generated `rootfs.ext4` fallback，并把 repo-generated block-rootfs `/init` 收口到 `mycpu linux initrd: stage=console-opened`、`stage=rootfs-rw-ok`、`stage=proc-readable`、`stage=sys-readable`、`stage=execve-post-init` 与 `mycpu linux userland: post-init reached`。原 4 个专项 worktree / branch 已清理，当前剩余任务收敛到：在这个最小 block-rootfs `post-init reached` baseline 之上冻结下一处更后 userland checkpoint）

## 文档定位

本文档记录当前 `xv6 / Linux / JIT` 主线切换的 Wave 1 如何落地，包括：

- 4 个独立 worktree / 分支的职责划分
- 每条 workstream 的文件 ownership、交付边界和验证要求
- 协调者的合并顺序
- 可以直接复制到新对话里的 agent 启动 prompt

## 关联文档

- 来源设计：
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
- 目标状态：
  - [../status/xv6_linux_jit_status.md](../status/xv6_linux_jit_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 把当前主线正式切到 `RV64A + virtio + CSR / privilege 补全 + xv6-riscv`。
- 在不放弃默认延续线 guardrail 的前提下，为未来 `Linux` 和 `JIT / DBT` 铺 durable foundation。
- 首轮通过 4 个独立对话、4 个独立 branch / worktree 并行推进，并尽量避免共享文件冲突；当前并行阶段已经完成，剩余 follow-up 直接在 `main` 工作区收口。

## 完成定义

- 协调者已经写清当前主线切换设计、专项状态和当前 wave 1 计划。
- 已经为 4 条 workstream 创建独立 branch / worktree。
- 每条 workstream 都有明确的 ownership、验证基线和不可越界修改范围。
- 每条 workstream 都有可直接复制的新对话 prompt。
- `docs/status/mainline_status.md`、`docs/status/project_priority_roadmap.md` 和 `docs/index.md` 已与当前主线切换口径对齐。
- 各 agent 产出已经按 `A/B -> C -> D` 的顺序被协调者整合，且首轮 B / C follow-up 也已回收到 `main`。
- `xv6` board profile 已切到真实 `virtio-blk`，并能通过 `xv6_boot_smoke` / `run-workload-xv6` 验证。
- 当前剩余完成定义收敛为：把 `xv6` shell 守成稳定 guardrail，同时把真实 Linux bring-up 在 block-rootfs 下最小 `console-opened -> rootfs-rw-ok -> proc-readable -> sys-readable -> /init reached -> post-init reached` 之后的下一处稳定 checkpoint 一起冻结下来。

## 首轮 Worktree / Branch 矩阵（已执行，当前仅保留留档）

| Agent | Branch | Worktree | 核心职责 |
|------|------|------|------|
| A | `feat/xv6-foundation-rv64a-csr` | `.worktrees/xv6-foundation-rv64a-csr` | `RV64A`、CSR / privilege foundation |
| B | `feat/xv6-foundation-virtio-platform` | `.worktrees/xv6-foundation-virtio-platform` | `virtio-mmio`、`virtqueue`、`virtio-blk`、平台接线 |
| C | `feat/xv6-bringup-workload-harness` | `.worktrees/xv6-bringup-workload-harness` | 外部 workload harness、`xv6-riscv` boot / smoke / gap audit |
| D | `feat/linux-jit-observation-foundation` | `.worktrees/linux-jit-observation-foundation` | execution profile / observation、默认延续线 guardrail |

> 并行整合阶段已完成；上述 worktree / branch 已按清理请求收口，本表只保留首轮 ownership 历史。后续 blocker 仍按同一 ownership 分类，但直接在 `main` 工作区推进。

## 协作硬规则

- 协调者维护共享 docs：`docs/status/*`、`docs/index.md`、共享 design / plan 文档正文默认由协调者维护。
- 各 agent 默认不改共享 docs；需要状态回写时，在对话里向协调者汇报。
- A 负责 ISA / privilege contract；C 如果发现这类 blocker，只记录并回报，不在 bring-up 分支里顺手修。
- B 负责 `virtio / platform`；C 如果发现设备 / MMIO / 中断路由 blocker，只记录并回报，不在 bring-up 分支里顺手修。
- D 负责读侧 observation / profile 和 guardrail，不拥有 A/B/C 的 architected 语义修改权。
- 任何 workstream 如果发现自己需要大幅修改别人的 ownership 文件，必须停下来先向协调者汇报。

## 当前执行进展

- `2026-04-21` 已收齐 4 份 `workstream_handoff.md`。
- `2026-04-22` 已按默认顺序把 A / B / C / D 第一轮 foundation 整合进主工作树。
- A：共享 `AtomicRequest` contract、`RV64A`、`misa.A`、`mhartid`、`wfi` 与对应 asm / host smoke 已进入主线，并通过 `make test-host-atomic_semantics_smoke test-atomic_basic test-atomic_ordering_smoke` 验证。
- B：`virtio-mmio + virtqueue + virtio_device + virtio-blk` foundation 与 DMA helper 已进入主线；随后也已完成首轮平台 follow-up：PLIC 现在按 `xv6` 约定把 `virtio=1`、`UART=10` 分开接线，`Machine` / CLI / debug CLI 已支持 block transport 选择。
- C：external workload harness、vendored `xv6-riscv`、board profile、`build/run/smoke-workload-xv6` 与 `xv6_boot_smoke` 已进入主线；当前 `mycpu_virt` board profile 已切到 `virtio-blk`，`xv6_shell_smoke` 与 `run-workload-xv6` 已把真实 `virtio` board path 守到 shell，当前 bring-up 近端重心已切到 Linux。
- D：`execution_profile`、debug CLI profile 导出与代表性 workload smoke 已进入主线，`test-host-execution_profile_smoke` 也已补进默认 `make test` / `make test-pipeline`。
- `2026-04-24` Linux harness follow-up：`linux_proto` 的 repo-generated `dtb` 生成已改成“缺 `initrd` 也能先生成占位 `chosen`”，并补上 repo-generated 最小 `rootfs.cpio` `/init` fallback；因此默认工作区的 `make run-workload WORKLOAD_NAME=linux_proto` 现在会继续进入 probe 入口，并把缺失输入收窄成 `Image` 单项，而不再被 Make 依赖错误提前中断。同日 functional 路径也已补上最小 `RV64C`，modern `virtio-mmio` 已补齐 `VIRTIO_F_VERSION_1`，本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 已能稳定走到 repo-generated initramfs `/init reached`。
- `2026-04-25` Linux block-rootfs follow-up：`linux_proto` 现已支持 `LINUX_PROTO_ROOTFS_MODE=block` 与 repo-generated `rootfs.ext4` fallback，block 模式也会把 `chosen.initrd` 收口成零长度占位。基于本地 `CONFIG_RISCV_ISA_C=y` Linux `Image`，当前 block-rootfs 路径已稳定推进到 `EXT4-fs (vda)`、`VFS: Mounted root`、`devtmpfs: mounted`、`Run /init as init process`、`mycpu linux initrd: stage=console-opened`、`mycpu linux initrd: stage=rootfs-rw-ok`、`mycpu linux initrd: stage=proc-readable`、`mycpu linux initrd: stage=sys-readable`、`mycpu linux initrd: /init reached`、`mycpu linux initrd: stage=execve-post-init` 与 `mycpu linux userland: post-init reached`；当前剩余任务因此进一步收敛成 `post-init reached` 之后的下一处用户态 checkpoint，而不再是“有没有一份非空磁盘镜像”或“最小 `/init` console 输出是否闭环”。
- 这一轮回归已覆盖 `make test-unit-mmio_contract_matrix`、`make test-host-debug_protocol_command_smoke`、`make test-host-virtio_blk_smoke`、`make test-host-xv6_boot_smoke`、`make run-workload-xv6`、`make test`、`make test-pipeline` 与 `cd frontend && node --test`；原 4 个专项 worktree / branch 也已完成清理。

## 任务

### 任务 1：主线切换与协调基线

**文件：**
- 创建：
  - `docs/design/xv6_linux_jit_mainline_design.md`
  - `docs/status/xv6_linux_jit_status.md`
  - `docs/plan/xv6_linux_jit_wave1_plan.md`
- 修改：
  - `docs/status/mainline_status.md`
  - `docs/status/project_priority_roadmap.md`
  - `docs/design/future_expansion_roadmap_design.md`
  - `docs/index.md`

- [x] **步骤 1：** 把 `future_expansion_roadmap_design.md` 中的候选切换线正式提升为当前主线，并写入独立 design / status / plan。
- [x] **步骤 2：** 明确 4 条 workstream 的 branch / worktree / ownership / merge order。
- [x] **步骤 3：** 准备每个新对话的 prompt，并要求各 agent 通过协调者工作树里的绝对路径读取权威 docs。
- [ ] **步骤 4：** 后续随着各 workstream 落地，持续由协调者回写 `xv6_linux_jit_status.md`、`mainline_status.md` 和 `history_plan.md`。

### 任务 2：Workstream A — RV64A + CSR / privilege foundation

**文件：**
- 创建：
  - `myCPU/tests/asm/atomic_basic.S`
  - `myCPU/tests/asm/atomic_ordering_smoke.S`
  - `myCPU/tests/host/atomic_semantics_smoke.cpp`
  - 如有必要，新增原子 contract helper（例如 `myCPU/src/isa/atomic_contract.{h,cpp}`）
- 修改：
  - `myCPU/src/decode.c`
  - `myCPU/src/decode.h`
  - `myCPU/src/isa/instruction_semantics.cpp`
  - `myCPU/src/isa/execution_context.*`
  - `myCPU/src/exec/memory_ops.cpp`
  - `myCPU/src/exec/system_ops.cpp`
  - `myCPU/src/exec/functional_backend.cpp`
  - `myCPU/src/arch/csr_file.*`
  - `myCPU/src/cpu.cpp`
  - `myCPU/src/trap.cpp`
  - `myCPU/Makefile`

- [x] **步骤 1：** 先做 `xv6 / Linux` 相关 ISA、CSR、privilege gap audit，列出必须补的 architected contract，不要一上来就铺实现。
- [x] **步骤 2：** 设计并落地共享的 atomic / reservation contract，确保 `InstructionSemantics` 仍是单一语义来源。
- [x] **步骤 3：** 在 `functional` backend 跑通 `RV64A` 完整 architected 语义，并补 asm / host smoke。
- [x] **步骤 4：** 为 `pipeline` 预留保守但 durable 的消费方式；如果需要先走串行化路径，也必须复用同一份 contract，而不是另写一套最小特判。
- [x] **步骤 5：** 跑窄验证，再守住 `cd myCPU && make test` 与相关 `CSR / trap / privilege` 门禁。

### 任务 3：Workstream B — virtio / platform foundation

**文件：**
- 创建：
  - `myCPU/src/devices/virtio_mmio.{h,cpp}`
  - `myCPU/src/devices/virtqueue.{h,cpp}`
  - `myCPU/src/devices/virtio_device.{h,cpp}`
  - `myCPU/src/devices/virtio_blk.{h,cpp}`
  - `myCPU/tests/unit/virtio_mmio_contract.cpp`
  - `myCPU/tests/unit/virtqueue_smoke.cpp`
  - `myCPU/tests/host/virtio_blk_smoke.cpp`
- 修改：
  - `myCPU/src/platform/address_map.h`
  - `myCPU/src/platform/machine.cpp`
  - `myCPU/src/devices/device.h`
  - `myCPU/src/mem/bus.*`
  - `myCPU/Makefile`

- [x] **步骤 1：** 先定义通用 `virtio-mmio + virtqueue + virtio_device` 分层，不要把 `virtio-blk` 直接写成 `Machine` 里的设备特判。
- [x] **步骤 2：** 实现 `virtio-blk` 第一刀，并把中断路由、MMIO window、queue state 接到现有平台。
- [x] **步骤 3：** 为后续 `virtio-console` / `virtio-net` 留统一 backend 扩展点，避免 `virtio-blk` 占用 transport 层细节。
- [x] **步骤 4：** 补 unit / host smoke，并守住现有 `bus / device / platform` 回归。
- [x] **步骤 5：** 与 C 线对齐 xv6 预期的块设备接口、地址布局和启动参数。

### 任务 4：Workstream C — external guest workload harness + xv6 bring-up

**文件：**
- 创建：
  - `myCPU/external/xv6-riscv/`（或当前确认的 vendored workload 入口）
  - `myCPU/workloads/xv6/`（build / run / image / boot glue）
  - `myCPU/tests/host/xv6_boot_smoke.cpp` 或等价 harness smoke
  - `myCPU/tests/data/xv6/`（如需要的镜像 / 磁盘工件说明）
- 修改：
  - `myCPU/Makefile`
  - `myCPU/src/main.cpp`
  - `myCPU/src/platform/machine.cpp`
  - 与 workload 启动直接相关的最少 loader / CLI glue

- [x] **步骤 1：** 先完成外部 guest workload harness 设计，避免把 `xv6` 写成一条一次性 demo target。
- [x] **步骤 2：** 盘清 `xv6-riscv` 的 boot、trap、timer、interrupt、block device、memory map 和 boot arg 依赖，形成 gap list。
- [x] **步骤 3：** 在不越权修改 A/B ownership 的前提下，把 build / image / run / smoke harness 先搭起来。
- [ ] **步骤 4：** 等 A/B 第一轮 contract 站稳后，逐步推进真实 boot smoke；当前 `xv6` 已稳定到真实 `virtio` board path 的 shell，`linux_proto` 在缺资产场景下也已走到 probe fail-closed，而在本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 上已能稳定走到 repo-generated initramfs `/init reached`，以及 repo-generated block-rootfs 的 `mycpu linux initrd: stage=console-opened`、`stage=rootfs-rw-ok`、`stage=proc-readable`、`stage=sys-readable`、`stage=execve-post-init` 与 `mycpu linux userland: post-init reached`。当前剩余任务收敛为：在这个最小 block-rootfs `/init` + rootfs-rw + procfs + sysfs + first post-init `exec` baseline 之上，冻结随之暴露的下一处用户态 checkpoint。
- [x] **步骤 5：** 把 `xv6` 入口设计成未来可并列接入 `Linux` workload 的外部 guest profile，而不是只保留 `xv6` 特例。

### 任务 5：Workstream D — observation / profile foundation + default-line guardrail

**文件：**
- 创建：
  - `myCPU/src/exec/execution_profile.{h,cpp}`
  - `myCPU/tests/host/execution_profile_smoke.cpp`
- 修改：
  - `myCPU/src/debug/debug_snapshot.h`
  - `myCPU/src/debug/debug_protocol_response.cpp`
  - `myCPU/src/debug/debug_session.cpp`
  - `myCPU/src/exec/pipeline_sequence.*`
  - `myCPU/src/exec/pipeline_backend*.cpp`
  - `myCPU/tests/host/debug_cli_smoke.cpp`
  - `myCPU/tests/host/pipeline_backend_smoke.cpp`
  - `myCPU/tests/host/vector_cnn_smoke.cpp`
  - `myCPU/tests/host/vector_pipeline_smoke.cpp`
  - `myCPU/Makefile`

- [x] **步骤 1：** 定义面向 `Linux / JIT / DBT` 的稳定 execution profile / observation 读侧合同，而不是新增一次性 log。
- [x] **步骤 2：** 补代表性 workload 的 hot-path / trap / memory-region 观测信号，并让 debug / CLI 可读取这些信号。
- [x] **步骤 3：** 继续把 `V4`、`kernel_alpha`、`interactive_os`、`pipeline` 作为本轮 guardrail workload，而不是因为主线切换就停止维护。
- [x] **步骤 4：** 如果 profile 需要新统计维度，优先复用既有 `pipeline_sequence`、`debug snapshot`、`memory_region` 等现成边界，不另起并行事实来源。
- [x] **步骤 5：** 跑 `debug_cli_smoke`、代表性 workload smoke 和主门禁，确保观测面不反向污染执行语义。

## 推荐合并顺序

1. **第一轮整合顺序已经执行完毕：A -> B -> C -> D。**
   - A / B 已先把 architected contract 与 `virtio` foundation 立成主线事实来源。
   - C 随后已基于 A/B 新主线刷新 `xv6_boot_smoke` 到新的 early-boot checkpoint。
   - D 最后整合，并把 `execution_profile` guardrail 补进默认主门禁。
2. **首轮 B / C follow-up 也已经执行完毕。**
   - PLIC 已按 `xv6` 约定拆开 `virtio` / UART source，`Machine` / CLI / debug CLI 已支持 block transport 选择。
   - `xv6` board profile 已从 `simple_storage` 切到真实 `virtio-blk` contract，`xv6_boot_smoke` / `run-workload-xv6` 已开始消费这条路径。
3. **当前剩余顺序改为：先推进 C 类真实 Linux bring-up，再按暴露缺口回派 A / B / D。**
   - C 先把 `xv6` shell 守成 guardrail，同时冻结 `linux_proto` 在 `devtmpfs/initramfs/xor` 之后的下一处稳定 checkpoint 或 blocker。
   - A / B / D 只随着新 blocker 进入 bug-driven 支撑，不反向抢占 C 的 bring-up ownership。
4. **每一轮 follow-up 仍由协调者统一回写 status/docs。**

## Agent 启动 prompt

> 以下 prompt 保留为首轮并行阶段的留档样例。若后续再次拆分专项，需要先按最新 branch / worktree 名称和当前 blocker 改写后再复用。

### Agent A prompt

```text
你当前工作在 branch `feat/xv6-foundation-rv64a-csr`、worktree `.worktrees/xv6-foundation-rv64a-csr`。

先阅读这些文件：
1. `/home/liangjiaqi/projects/my_visual_CPU/AGENTS.md`
2. `/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md`
3. `/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md`
4. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/future_expansion_roadmap_design.md`
5. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/xv6_linux_jit_mainline_design.md`
6. `/home/liangjiaqi/projects/my_visual_CPU/docs/status/xv6_linux_jit_status.md`
7. `/home/liangjiaqi/projects/my_visual_CPU/docs/plan/xv6_linux_jit_wave1_plan.md`

你的唯一职责：
- 落地 `RV64A` 与 `xv6 / Linux` 近端必需的 `CSR / privilege` contract foundation。
- 保持 `InstructionSemantics` 是单一语义来源。
- 优先定义 durable atomic / reservation contract，供 `functional`、后续保守 `pipeline` 路径和未来 `JIT / DBT` 复用。

你的 ownership：
- `myCPU/src/decode.*`
- `myCPU/src/isa/*`
- `myCPU/src/arch/csr_file.*`
- `myCPU/src/cpu.cpp`
- `myCPU/src/trap.cpp`
- `myCPU/src/exec/memory_ops.cpp`
- `myCPU/src/exec/system_ops.cpp`
- `myCPU/src/exec/functional_backend.cpp`
- 你新增的 `atomic` 相关 asm / host smoke
- `myCPU/Makefile` 中与你测试 target 直接相关的最小改动

你不要做的事：
- 不实现 `virtio` 设备
- 不改 `xv6` harness / workload glue
- 不改 `docs/status/*`、`docs/index.md`、共享 design / plan 文档正文
- 不把 `RV64A` 写成只服务某个 backend 的一次性特判

工作方式：
- 先执行 gap audit，再给出你准备改哪些文件、准备怎么设计 atomic / reservation contract。
- 如果发现 blocker 落在 `virtio / platform` 或 `xv6 harness`，只汇报，不越权修改。
- 默认不自动 commit。

完成前至少汇报：
- 改动文件列表
- 新增 / 修改的测试
- 哪些 contract 已站稳
- 哪些缺口需要协调者转给其他 workstream
```

### Agent B prompt

```text
你当前工作在 branch `feat/xv6-foundation-virtio-platform`、worktree `.worktrees/xv6-foundation-virtio-platform`。

先阅读这些文件：
1. `/home/liangjiaqi/projects/my_visual_CPU/AGENTS.md`
2. `/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md`
3. `/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md`
4. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/future_expansion_roadmap_design.md`
5. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/xv6_linux_jit_mainline_design.md`
6. `/home/liangjiaqi/projects/my_visual_CPU/docs/status/xv6_linux_jit_status.md`
7. `/home/liangjiaqi/projects/my_visual_CPU/docs/plan/xv6_linux_jit_wave1_plan.md`

你的唯一职责：
- 建立通用 `virtio-mmio + virtqueue + virtio_device + virtio-blk` 平台基础。
- 保证这套分层现在服务 `xv6`，后续还能自然延伸到 `Linux` 与更多 `virtio` 设备。

你的 ownership：
- `myCPU/src/devices/*` 中你新增的 `virtio*` 文件
- `myCPU/src/platform/address_map.h`
- `myCPU/src/platform/machine.cpp`
- `myCPU/src/mem/bus.*`
- `myCPU/src/devices/device.h`
- 你新增的 `virtio` unit / host smoke
- `myCPU/Makefile` 中与你测试 target 直接相关的最小改动

你不要做的事：
- 不修改 ISA / CSR / privilege 语义
- 不推进 `xv6` workload harness 逻辑
- 不改共享 docs 正文
- 不把 `virtio-blk` 写成只服务单一设备的一次性分支逻辑

工作方式：
- 先输出 transport / queue / device backend 的层次设计和计划修改文件。
- 如果发现 blocker 属于 ISA / CSR / privilege，汇报给协调者，不越权修复。
- 默认不自动 commit。

完成前至少汇报：
- `virtio` 分层结构
- 变更文件列表
- 新增 / 修改的测试
- 对 `xv6` bring-up 侧需要暴露的设备 contract
```

### Agent C prompt

```text
你当前工作在 branch `feat/xv6-bringup-workload-harness`、worktree `.worktrees/xv6-bringup-workload-harness`。

先阅读这些文件：
1. `/home/liangjiaqi/projects/my_visual_CPU/AGENTS.md`
2. `/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md`
3. `/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md`
4. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/future_expansion_roadmap_design.md`
5. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/xv6_linux_jit_mainline_design.md`
6. `/home/liangjiaqi/projects/my_visual_CPU/docs/status/xv6_linux_jit_status.md`
7. `/home/liangjiaqi/projects/my_visual_CPU/docs/plan/xv6_linux_jit_wave1_plan.md`

你的唯一职责：
- 把 `xv6-riscv` 接入为外部 guest workload。
- 搭出后续 `Linux` 也能复用的 workload harness / board profile / smoke 入口。
- 形成清晰的 `xv6` boot / trap / timer / interrupt / block device gap list。

你的 ownership：
- `myCPU/external/xv6-riscv/` 或当前确认的外部 workload 入口
- `myCPU/workloads/xv6/` 或等价 build / run glue
- `myCPU/tests/host/xv6_boot_smoke.cpp` 或等价 harness smoke
- `myCPU/Makefile` 中与 workload 构建、镜像、smoke 直接相关的最小改动
- `myCPU/src/main.cpp` 与 `myCPU/src/platform/machine.cpp` 中仅与 boot glue / CLI / image loading 直接相关的最小改动

你不要做的事：
- 不顺手修 ISA / CSR / privilege contract
- 不顺手实现 `virtio` 设备
- 不改共享 docs 正文
- 不把 `xv6` 写成一次性 demo target；要优先形成通用 external workload harness

工作方式：
- 第一步先做 gap audit 和 harness 结构设计，再动实现。
- 遇到 ISA / CSR / `virtio` blocker 时只汇报，不越权修别人的 ownership。
- 默认不自动 commit。

完成前至少汇报：
- 你决定采用的 workload harness 目录结构
- 当前 `xv6` 缺口清单
- 构建 / 启动 / smoke 路径
- 哪些 blocker 需要协调者转给 A/B
```

### Agent D prompt

```text
你当前工作在 branch `feat/linux-jit-observation-foundation`、worktree `.worktrees/linux-jit-observation-foundation`。

先阅读这些文件：
1. `/home/liangjiaqi/projects/my_visual_CPU/AGENTS.md`
2. `/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md`
3. `/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md`
4. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/future_expansion_roadmap_design.md`
5. `/home/liangjiaqi/projects/my_visual_CPU/docs/design/xv6_linux_jit_mainline_design.md`
6. `/home/liangjiaqi/projects/my_visual_CPU/docs/status/xv6_linux_jit_status.md`
7. `/home/liangjiaqi/projects/my_visual_CPU/docs/plan/xv6_linux_jit_wave1_plan.md`

你的唯一职责：
- 继续守住默认延续线 guardrail。
- 建立面向 `Linux / JIT / DBT` 的 execution profile / observation foundation。
- 让 debug / CLI / smoke workload 能提供更稳定的 hot-path、trap、memory-region 观测，而不反向污染执行语义。

你的 ownership：
- `myCPU/src/exec/execution_profile.*`（如新增）
- `myCPU/src/debug/*`
- `myCPU/src/exec/pipeline_sequence.*`
- `myCPU/src/exec/pipeline_backend*.cpp` 中仅与 profile / observation 直接相关的改动
- `myCPU/tests/host/debug_cli_smoke.cpp`
- `myCPU/tests/host/pipeline_backend_smoke.cpp`
- `myCPU/tests/host/vector_cnn_smoke.cpp`
- `myCPU/tests/host/vector_pipeline_smoke.cpp`
- `myCPU/Makefile` 中与你测试 target 直接相关的最小改动

你不要做的事：
- 不改 ISA / CSR architected contract
- 不实现 `virtio` 设备
- 不推进 `xv6` harness 本身
- 不把观测层写成一次性日志打印或调试特判
- 不改共享 docs 正文

工作方式：
- 先定义哪些 profile / observation 信号值得作为稳定合同，再决定文件落点。
- 优先复用现有 `debug snapshot`、`pipeline_sequence`、`memory_region` 等边界，不另造平行事实来源。
- 默认不自动 commit。

完成前至少汇报：
- 你准备引入的 profile / observation 合同
- 变更文件列表
- 新增 / 修改的测试
- 这些信号未来如何服务 `Linux` 与 `JIT / DBT`
```

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
