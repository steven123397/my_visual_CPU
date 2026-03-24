# AGENTS.md

## 适用范围

本文件适用于 [myCPU/guest](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest) 子树下的 guest supervisor runtime、VM、trap、runtime、user task/program 和 demo / smoke orchestration。

## 当前实现基线

guest 侧当前不是单纯 demo 代码，而是一条已经接通的最小 bring-up 路径，包含：

- linker-backed early allocator
- bitmap PMM
- guest-side Sv39 page-table builder
- `vm_address_space_t` / `vm_process_t`
- `vm_user_region_t` / `vm_object_t`
- `trap_context_t` / `trap_user_runtime_t`
- `user_task_t`
- `user_task_bootstrap_t`
- `user_program_t`
- `user_program_smoke_t`
- `supervisor_demo_smoke`

当前已经能完成：

- S-mode 最小 runtime bring-up
- U-mode enter / return
- delegated user page-fault recovery
- delegated user `ecall`
- delegated timer / external interrupt return
- 单用户生命周期和清理 smoke

## 关键边界

当前 guest 侧分层应理解为：

- `memory.c`
  早期内存布局、early allocator、linker symbol 边界。
- `pmm.c`
  物理页管理。
- `vm.c`
  address space / process / region / object / fault policy。
- `trap.c`
  trap dispatch、trap context、user runtime 生命周期。
- `runtime_context.c`
  当前活跃 process / address_space / trap_context 记录。
- `user_task.c`
  单用户 task 层生命周期封装。
- `user_task_bootstrap.c`
  标准用户地址布局、对象和 region 装配。
- `user_program.c`
  当前标准 user image / task / bootstrap 汇总层。
- `user_program_smoke.c`
  user-program smoke orchestration helper。
- `supervisor_demo_smoke.c`
  顶层 demo runner 和 session orchestration。

## 当前 smoke 编排边界

这是最近一轮收口后的正式基线，后续不要重新打散：

- [supervisor_demo/main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/supervisor_demo/main.c)
  只负责基础初始化和调用 `supervisor_demo_smoke_run()`。
- [include/supervisor_demo_smoke.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/include/supervisor_demo_smoke.h)
  只暴露单一 public demo runner。
- [include/user_program_smoke.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/include/user_program_smoke.h)
  对外只保留 3 个阶段 helper：
  - `user_program_smoke_validate_lifecycle()`
  - `user_program_smoke_prepare_standard()`
  - `user_program_smoke_enter_round()`
- `invalid_region` / `remap_region` / `fault-skip` / `resume` 这类更细的 orchestration 保持在 helper 内部，不再回流到 `main.c`。

## 本子树的局部规则

- 不要重新把 demo 逻辑堆回 `guest/supervisor_demo/main.c`。
- 新的标准生命周期逻辑优先放到：
  - `user_program`
  - `user_task_bootstrap`
  - `trap`
  - `vm`
  而不是直接塞进 smoke/demo。
- smoke 的 public surface 应尽量小，细碎 orchestration 尽量内部化、`static` 化。
- 保持对象所有权和生命周期边界明确：
  - address space
  - process
  - region
  - object
  - trap context
  - user runtime
- 对 README 中 guest 相关描述保持简洁；实现细节写在本文件或专门文档，不要把 README 写成长流水账。

## 当前已知问题

本子树当前最明显的结构性问题不是 public API，而是内部实现仍偏重：

- [kernel/vm.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm.c)
  仍同时承担 address space、process、region/object 和 fault policy。
- [kernel/trap.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap.c)
  同时承担 dispatch、policy 和 user runtime lifecycle。
- [kernel/user_program_smoke.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/user_program_smoke.c)
  虽然 public surface 已收口，但内部体量仍大。
- [kernel/supervisor_demo_smoke.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/supervisor_demo_smoke.c)
  仍承载较多顶层 orchestration。

此外，当前实现还有一些阶段性固定上限：

- `VM_MAX_ADDRESS_SPACES`
- `VM_MAX_USER_REGIONS`
- `VM_PROCESS_MAX_USER_REGIONS`
- `VM_MAX_FAULT_ACTIONS`
- `TRAP_MAX_INTERRUPT_CAUSE`
- `TRAP_MAX_EXCEPTION_CAUSE`

这些限制在当前单用户 bring-up 路径可接受，但真正进入小 OS / kernel bring-up 时需要优先关注。

## 本子树下一步工作

当前“收口”工作可以视为已基本完成，后续重点转为功能缺口与实现层继续拆分：

1. 继续 Phase 1 guest/runtime gap closure，而不是继续做 `main.c` 清理。
2. 继续推进 process / runtime refinement。
3. 继续补更多 user interrupt / trap coverage，不止当前 timer + external + user-`ecall`。
4. 把 [kernel/vm.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm.c) 和 [kernel/trap.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap.c) 继续拆小。
5. 为第一次真正的小 OS / kernel bring-up 清掉基础障碍。

和 guest 路径直接相关的最新系统性审查结果见：

- [docs/code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/code_self_review_2026-03-24.md)

## 验证要求

只要触及 guest runtime / demo / smoke 路径，默认至少关注：

- `cd myCPU && make test-guest-supervisor_demo`

通常仍应回归：

- `cd myCPU && make test`
