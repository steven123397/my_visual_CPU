# Phase 4 准备性设计

## 文档定位

本文档用于定义当前仓库进入 `Phase 4` 之前的准备性收口边界，并明确第一刀只开 `P4-prep-1`：`bus / memory region` 合同收口。

它重点回答：

- 当前为什么还不直接启动完整 `Phase 4`
- 哪些 `Phase 4` 准备项现在做有独立结构收益
- `P4-prep-1` 具体应该收窄到什么边界
- 它与后续 `cache / DMA / multicore / coherence` 的关系是什么

本文档不承担实时进度更新。当前主线优先级、当前是否进入实施，以及后续是否继续展开 `P4-prep-2 / P4-prep-3`，以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 相关设计：
  - [platform_mmio_contract.md](platform_mmio_contract.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [debug_frontend_integration.md](debug_frontend_integration.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`Phase 1` bring-up、`Phase 3` 的最小 `OoO execute`、`debug/frontend` 的教学演示链路，以及 `向量扩展 + ML workload` 的 `V-lite` `V0 ~ V4` 都已经接通。现阶段的主线方法论依然是：reference-first、小步推进、由真实 workload 或真实 bug 驱动更重结构。

在这样的语境下，直接把 `Phase 4` 理解成“马上做 cache / DMA / multicore / coherence”并不健康。原因不是这些方向不重要，而是当前还缺少足够稳定的 memory-level workload 信号，难以判断哪一刀最值、哪一刀只是把状态空间提前放大。

但这并不意味着 `Phase 4` 当前完全不能动。仓库里已经出现了一类适合提前做的事项：它们本身就有独立结构收益，又不会提前扩大 guest 可见语义面。例如更清晰的 `bus / memory region` 合同、更好的 memory 观测面，或为未来 DMA 预留的更干净总线边界。

因此，当前对 `Phase 4` 的健康打开方式，不是直接启动完整实现，而是先定义一组准备性切片，并把第一刀收窄到 `P4-prep-1`：`bus / memory region` 合同收口。

## 目标

- 把当前 `Phase 4` 的正式入口收窄成一组准备性工作，而不是一次性启动完整大专项。
- 明确第一刀只做 `P4-prep-1`：统一 `bus / memory region` 的分类、属性和查询合同。
- 为未来的 `cache`、shadow cache / memory observation、DMA initiator contract 提供单一事实来源。
- 在不改变当前 guest 可见语义的前提下，减少 RAM / MMIO / live side effect / unmapped 这类判断散落在各处的情况。
- 让后续 `Phase 4` 是否值得继续扩，能够建立在更稳定的 workload 与观测信号之上。

## 非目标

- 不在这一轮实现真实 `D-cache / I-cache`。
- 不在这一轮实现 DMA engine、异步完成路径或中断完成模型。
- 不在这一轮引入 multicore、coherence、NUMA、cache hierarchy 或 scratchpad 控制器。
- 不在这一轮改变 `platform_mmio_contract` 对 guest 暴露的地址、寄存器或访问宽度。
- 不把 `P4-prep-1` 包装成性能研究结论；本轮仍优先回答结构边界问题。

## 约束与边界

- `functional` reference path 的正确性和当前 fault 口径优先级最高。
- 现有 `RAM / MMIO / unmapped` 的 guest 可见行为必须保持不变；本轮允许收口内部事实来源，但不允许顺手改 guest 合同。
- `AddressSpace` 仍负责虚实地址翻译与权限 / fault 语义；region 分类应发生在物理地址层，而不是在翻译前复制另一套规则。
- `Bus` 应成为物理 region 属性的统一查询入口；不继续鼓励在执行路径里直接写 `MEM_BASE / MEM_SIZE` 这类 ad-hoc 判断。
- 当前准备性工作必须能独立解释收益：即使后续 `cache / DMA` 暂时不做，`P4-prep-1` 也应仍然值得保留。

## 方案

### 总体推进方式

当前把 `Phase 4` 的入口收窄成 3 类准备项，但只正式打开第一类：

1. `P4-prep-1`：`bus / memory region` 合同收口
2. `P4-prep-2`：workload 驱动的 memory observation / shadow cache
3. `P4-prep-3`：DMA-ready 的 initiator / transaction 合同

其中只有 `P4-prep-1` 进入当前主线计划；`P4-prep-2 / P4-prep-3` 仅作为后续候选方向保留。

### `P4-prep-1`：统一 region 分类与属性

当前仓库里已经存在多处与 memory region 相关的判断：

- `Bus` 负责把物理地址路由到 RAM 或各类 MMIO 设备
- `AddressSpace` 负责翻译虚拟地址并形成 page fault / access fault
- 某些执行路径为了实现更窄的 fail-closed 行为，会直接基于 `MEM_BASE / MEM_SIZE` 判断“是否属于 RAM”

这种方式在仓库仍小时问题不大，但一旦后续真的引入 `cache`、shadow cache、DMA 或更细粒度观测面，就会开始暴露两个问题：

1. **事实来源分散**：不同路径对“这是 RAM、这是 live MMIO、这里不能缓存、这里不能批量搬运”的理解可能不一致。
2. **后续扩面困难**：如果未来要回答“这个地址是否 cacheable / dma-visible / side-effectful”，当前没有统一的 region 属性接口可复用。

因此，`P4-prep-1` 的核心不是新功能，而是把这些判断收口成统一 contract。

### Region 模型建议

建议在物理地址层引入统一 region 描述，至少覆盖以下维度：

- `kind`
  - `ram`
  - `mmio`
  - `unmapped`
- `cacheable`
  - 当前只有普通 RAM 为 `true`
- `has_side_effect`
  - 当前 live MMIO 为 `true`
- `dma_visible`
  - 当前可先保守：RAM 为 `true`，MMIO 为 `false`
- `supports_burst`
  - 为未来 DMA / block transfer 预留；当前 RAM 可为 `true`
- `label`
  - 只读调试用名称，例如 `ram`、`uart`、`clint`、`plic`、`storage`

这里的目标不是一次做成完整设备树，而是先形成“后续所有 memory hierarchy / DMA 准备工作都能问同一个问题”的最小公共属性面。

### 建议接口边界

建议把统一查询入口放在 `Bus` 侧，例如提供类似：

- 对单地址或一段 span 的 region 查询
- 能返回统一 region 属性与所属设备 / RAM 信息
- 能回答“整段 span 是否都属于同一种 region / 是否都可 cache / 是否都允许 burst”

这样：

- `AddressSpace` 继续只管翻译与权限
- 执行路径在拿到物理地址后，可统一向 `Bus` 查询 region 属性
- 后续 `cache`、shadow cache、DMA contract、debug observation 都不需要再各写一套 `MEM_BASE / MEM_SIZE` 或设备判定

### 对现有路径的直接收益

即使不继续做真正 `Phase 4` 实现，`P4-prep-1` 也会立刻改善当前仓库：

- 向量 `vle.v / vse.v` 的整段 span 预校验，可以不再依赖硬编码 RAM 范围，而是走统一 region contract。
- 后续若还有标量访存、debug 观测或设备防御要做更窄 hardening，也能复用同一套 region 判断。
- `debug/frontend` 后续如果要增加更克制的 memory observation，也可以直接消费统一 region 标签，而不是在浏览器端猜测地址属于哪类设备。

### `P4-prep-2 / P4-prep-3` 的承接关系

`P4-prep-1` 完成后，后续才有健康空间继续评估：

- `P4-prep-2`：增加只读 memory observation / shadow cache，回答 workload 是否真有 cache 价值。
- `P4-prep-3`：增加未来 DMA 需要的 initiator / transaction 合同，但暂不引入真正 DMA 设备。

也就是说，`P4-prep-1` 不是完整 `Phase 4` 的替代品，而是后续每一刀都能复用的结构底座。

## 验证思路

`P4-prep-1` 真进入实现时，建议至少守住以下基线：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

如果改动集中在 memory / bus / vector memory boundary，建议额外盯住：

- `cd myCPU && make test-host-vector_vlite_smoke`
- `cd myCPU && make test-host-vector_cnn_smoke`
- `cd myCPU && make test-host-vector_pipeline_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`

验证重点不是“性能提升”，而是：

- RAM / MMIO / unmapped 的 fault 与 side effect 口径不回退
- 现有 debug snapshot / frontend 行为不被顺手污染
- 新的 region contract 能被多个路径复用，而不是只服务一个调用点

## 风险与取舍

- 先做 region contract，会让当前 `Phase 4` 看起来不够“有功能感”，但这是有意识的取舍：先统一事实来源，比提前做一个难以解释收益的 cache 原型更健康。
- 把 `cacheable / dma_visible / supports_burst` 这类属性提前引入，会让接口比当前需求稍宽，但它们都直接服务后续明确方向，不属于无约束预留。
- 如果后续 workload 证据仍不足，`P4-prep-1` 也依然有独立价值，因为它会减少 memory 边界判断的重复实现。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `Phase 4` 当前准备性入口的设计边界。
- 当前主线只正式打开 `P4-prep-1`；是否继续实施、做到哪一步，以及 `P4-prep-2 / P4-prep-3` 是否值得单开，以 [../status/mainline_status.md](../status/mainline_status.md) 与 [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。
