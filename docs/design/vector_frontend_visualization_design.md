# 向量 / 神经网络前端可视化设计

## 文档定位

本文档用于说明当前 `frontend + debug_session/protocol` 如何把已经落地的 `V-lite` 向量扩展与最小 `conv -> relu` guest workload 变成可观察、可教学演示的前端可视化能力。

它承接以下既有事实来源：

- 向量 / ML workload 路线已经被确认是长期候选主线之一
- `V0 / V1`、`V2`、`V3`、`V3 hardening`、`V4` 与一轮更窄的 `V4 hardening` 都已落地
- 当前前端仍定位为“教学演示可用”的最小 debug 壳层，不扩成通用调试器

本文档不承担实时进度更新。当前实现进度、当前活跃风险和后续下一步，以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 相关计划：
  - [../plan/history_plan.md#vector-frontend-visualization-plan](../plan/history_plan.md#vector-frontend-visualization-plan)
- 相关设计：
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [debug_frontend_ui_refresh_design.md](debug_frontend_ui_refresh_design.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [vector_v2_operator_guest_design.md](vector_v2_operator_guest_design.md)
  - [vector_v3_minimal_cnn_guest_design.md](vector_v3_minimal_cnn_guest_design.md)
  - [vector_v4_minimal_vector_pipeline_design.md](vector_v4_minimal_vector_pipeline_design.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。向量这条线已经不再只是“未来可能做”的方向：`guest_vector_demo` 已经能稳定覆盖 `dot / GEMM / Conv / ReLU`，`guest_vector_cnn_demo` 也已经把固定 `conv -> relu` 的最小 CNN-style 闭环跑通；同时 `pipeline` 侧也已经具备 non-memory vector ALU 的最小 vector-aware execute / commit 边界。

但这些成果当前主要存在于：

- `docs/design/*` 的文字描述
- `make test*` / host smoke 的回归门禁
- UART marker（`V2OK` / `V3OK`）和底层 debug snapshot

这意味着：仓库虽然已经有了向量和最小神经网络 workload 的工程成果，但浏览器端还不能把“目前到底做到了什么、哪些是配置指令、哪些是向量 ALU、CNN demo 当前在跑哪一段、向量寄存器当前长什么样”直观展示出来。

因此，这一轮不应该去发明新的大而全调试器协议，也不应该顺势扩大为新的前端产品线，而应该围绕当前已落地的 `V2 / V3 / V4` 边界，补一层最小但有教学价值的前端可视化。

## 目标

- 让 `frontend` 测试清单直接暴露当前可运行的 `guest_vector_demo` 与 `guest_vector_cnn_demo`。
- 在浏览器端提供面向 workload 的说明卡，明确每条 demo 当前覆盖什么内容。
- 让五级流水线 / 最近周期时间线能直接看出哪些指令是向量指令，以及它们属于 `config / memory / ALU` 哪一类。
- 在 `DebugSnapshot` 中新增最小向量状态观测：`sew_bytes`、`vl` 与 `v0..v31`。
- 在前端补上向量寄存器 / 向量执行边界面板，并对 `guest_vector_cnn_demo` 提供固定 `conv -> relu` 的专题可视化。
- 保持这一切继续服务“教学演示可用”的边界，不把当前前端扩大成通用 waveform / profiler / trace studio。

## 非目标

- 不新增断点、条件暂停、可编辑 memory watch 或任意表达式求值。
- 不为了前端可视化新增第二套向量语义来源或独立执行模型。
- 不把 `frontend` 扩成性能分析器，不做 lane 级吞吐图、统一 trace 下载或逐提交 diff viewer。
- 不在这一轮里扩到 `Pool / FC`、模型文件加载、向量 load/store path 可视化或更重的 `Phase 4` memory hierarchy 展示。
- 不新增新的浏览器框架；继续保持原生 HTML / CSS / ESM。

## 约束与边界

- `functional` 仍然是参考语义真值来源；`pipeline` 只暴露自身真实状态，不复制一套向量解释器给前端。
- 前端新增的向量面板必须建立在 `DebugSnapshot` 的只读快照之上，不允许通过浏览器端推导反向控制执行路径。
- 向量快照只提供当前实现真正稳定存在的数据：
  - `sew_bytes`
  - `vl`
  - 32 个 16-byte 向量寄存器原始内容
- `frontend` 里关于 CNN 的可视化必须明确它是“固定 `conv -> relu` 教学示意”，而不是通用模型执行器。
- 当前 `pipeline` 的向量边界必须被如实表达：
  - non-memory vector ALU 已经脱离统一 serializing fallback
  - `vsetcfg / vle.v / vse.v` 仍保守 serializing
  - 这不是 lane / latency / vector rename 级模型

## 方案

### 结构设计

本轮前端可视化按用户确认的顺序拆成 4 层，并保持每层都能单独成立：

#### `P0`：入口与语义提示层

- `frontend/server/tests_manifest.mjs` 为 `guest_vector_demo` 与 `guest_vector_cnn_demo` 增加可展示元信息。
- 浏览器端增加 workload 说明卡，明确：
  - 这条 demo 的定位
  - 当前覆盖的算子 / 网络片段
  - 对应的 UART 成功 marker
- 五级流水线与 timeline 直接高亮向量指令，并把它们粗分为：
  - `vector cfg`
  - `vector mem`
  - `vector alu`

`P0` 的目标是：即使没有新增后端快照字段，浏览器也能让用户一眼认出“这是一条向量 / CNN 演示路径”。

#### `P1`：向量状态层

- `DebugSnapshot` 新增 `vector` 部分：
  - `sew_bytes`
  - `vl`
  - `registers[32]`
- CLI / JSON 协议序列化这部分数据。
- 前端新增 `Vector State` 面板：
  - 展示 `SEW / VL`
  - 用 diff 高亮最近变化的向量寄存器
  - 以当前 `sew_bytes + vl` 解释前若干 lane 的有符号值
  - 同时保留原始 16-byte dump 作为底层视图

`P1` 的目标是：让“向量寄存器与标量寄存器并存，但受 `SEW / VL` 共同约束”这件事可见、可讲解。

#### `P2`：最小 CNN workload 专题层

针对 `guest_vector_cnn_demo`，在前端增加固定 `conv -> relu` 的专题卡：

- 静态展示：
  - 输入 `conv_input`
  - 卷积核 `conv_kernel`
  - 期望 `conv` 输出 `[7, -9, 7]`
  - 期望 `relu` 输出 `[7, 0, 7]`
- 动态展示：
  - 若快照中已经能从 `v4 / v5` 读出对应结果，则同步展示 live lane
  - 若尚未运行到该阶段，则明确标为“待产生 / 尚未可见”

`P2` 不追求通用神经网络可视化，而是把当前仓库已经稳定落地的 `conv -> relu` 闭环准确讲清楚。

#### `P3`：当前向量执行边界提示层

在同一张前端面板里，把当前 `functional / pipeline` 的向量边界变成开发者可读提示：

- 当前活动向量指令所在 stage
- 当前是 `cfg / mem / alu` 哪一类
- 当前 backend 下向量执行边界的说明：
  - `functional`：共享语义直接生效
  - `pipeline`：non-memory vector ALU 可进入最小 vector-aware path；config / memory 仍 serializing
- 当 `stall_reason == vector_state_busy` 时，明确给出向量依赖阻塞提示

`P3` 的目标不是做全套微架构分析，而是让浏览器前端能如实表达当前 `V4` 的结构边界。

### 接口 / 数据 / 契约

#### 测试清单元信息

前端测试清单在保留现有 `name / kind / hasDisk` 的同时，可额外暴露只读展示字段，例如：

- `title`
- `summary`
- `badge`
- `workload`
  - `category`
  - `expectedMarker`
  - `ops`
  - `pipelineNote`
  - 对 `vector_cnn_demo` 额外暴露固定输入 / kernel / 期望输出

这些字段只服务前端展示，不参与 session load 协议语义。

#### 向量快照格式

新增的 `vector` 数据面保持最小：

```json
{
  "vector": {
    "sew_bytes": 4,
    "vl": 3,
    "registers": ["0x07000000f7ffffff0700000000000000", "..."]
  }
}
```

其中：

- `registers[i]` 保持固定 16-byte 原始 byte dump
- 前端负责基于 `sew_bytes` 和 `vl` 做 lane 解释
- 不在协议里额外引入“解释后数字数组”作为第二事实来源

#### 前端模块边界

- `frontend/server/tests_manifest.mjs`
  - 维护 demo 元信息
- `frontend/server/debug_server.mjs`
  - 把这些元信息透传给浏览器
- `frontend/app/render.js`
  - 装配 workload / vector / pipeline UI
- `frontend/app/components/pipeline.js`
  - 负责向量指令分类与 stage/timeline 高亮
- `frontend/app/components/panels.js`
  - 负责 workload 卡、vector state、CNN 可视化与当前执行边界提示
- `frontend/app/state.js`
  - 只维护最小 UI 推导 helper，例如 diff / instruction classification

### 验证思路

本轮至少守住：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd frontend && node --test`

并重点补以下验证：

- Node 测试验证 manifest 已暴露两个 vector demo 及其元信息
- Node 渲染测试验证 workload 卡、vector state 与 CNN 专题卡
- C++ host smoke 验证 debug snapshot JSON 已包含向量快照字段

## 风险与取舍

- 让前端知道 `conv_input / kernel / expected outputs` 会增加一点静态元信息，但这比为了可视化去发明通用模型协议更克制。
- 向量寄存器全部透出会让快照略变大，但当前每次只多 32 个 16-byte dump，仍处于“本地教学演示可接受”的量级。
- 把 `sew_bytes / vl` 的 lane 解释留在前端，能避免协议里重复维护一份“解释后数值数组”；代价是浏览器端需要承担少量解释逻辑，但这更符合单一事实来源原则。
- `P3` 只做当前执行边界提示，不做真正的跨 backend diff viewer；这会让高级分析能力继续受限，但更符合当前仓库“先把已落地边界讲清楚”的目标。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，记录 `vector / NN frontend visualization` 这一轮的正式边界。
- 当前结果以 [../status/mainline_status.md](../status/mainline_status.md) 与 [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。
