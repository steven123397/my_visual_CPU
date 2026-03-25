# kernel_alpha bring-up 历史记录（2026-03-25）

## 文档定位

本文档只保留独立 `kernel_alpha` bring-up 路线的简要实现过程记录。

当前主维护入口已经切换到：

- [docs/status/kernel_alpha_bringup_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_bringup_status.md)

因此这里不再维护实时状态，只保留少量历史脉络，避免主状态文档重新膨胀成执行流水账。下文里的“1 条正向 + 6 条负向”等描述，只反映归档当时的中间节点，不代表当前总数。

## 简要实现过程

1. 先从 `guest_supervisor_demo` 之外拆出独立 `kernel_alpha_demo`，把第一次真正的小 kernel alpha bring-up 收口成 `K -> M -> V -> P -> E -> T -> D -> S` 正向主路径。
2. 在正向基线稳定后，先补 `guest_kernel_alpha_fault_demo`，固定“VM 已开启但 CLINT 未映射时会进入 fault / panic”的最小负向合同。
3. 随后围绕 `SimpleStorage` 的 metadata / readiness / error contract，补上 `guest_kernel_alpha_storage_no_media_demo`、`guest_kernel_alpha_storage_bad_block_count_demo`、`guest_kernel_alpha_storage_lba_range_demo` 和 `guest_kernel_alpha_storage_bad_command_demo`。
4. 为支撑这批 storage 负向回归，guest 侧同步补出 `storage_read_info()`、`storage_probe()`、`storage_status()`、`storage_error()`、`storage_clear_error()` 和 `storage_read_block_with_count()` 这些最小 helper。
5. 在 storage 错误合同之后，继续转向 non-storage device readiness，新增 `guest_kernel_alpha_plic_not_ready_demo`，固定“PLIC 已映射但未初始化时，UART THRE 不会到达 supervisor external interrupt，bring-up 会在 deadline 超时后进入 panic”的负向合同，输出为 `KMVPX`。

## 当前归档结论

到这一步为止，独立 `kernel_alpha` 已经形成：

- 1 条正向 bring-up 主路径：`KMVPETDS`
- 6 条负向回归路径：`KMVX`、`KMVPX`、`KMVNX`、`KMVBX`、`KMVLX`、`KMVCX`

下一步实时规划请以 [docs/status/kernel_alpha_bringup_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_bringup_status.md) 为准。
