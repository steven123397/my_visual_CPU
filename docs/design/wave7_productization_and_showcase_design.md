# Wave 7 产品化展示与在线控制台设计

## 文档定位

本文档记录主线 `Wave 7 / 产品化展示与在线调试平台收口` 的历史设计边界。

它回答在 `Wave 7` 首轮产品化阶段曾经采用过的站点壳层和展示目标：

- `Wave 7` 为什么不是继续堆核心执行功能，而是把已有能力整理成可访问、可体验、可解释的产品形态。
- 面向用户时，首页、控制台和产品文档应如何分工。
- 首页如何从 [../showcase/preview.html](../showcase/preview.html) 演化成真正产品官网。
- `frontend/` 控制台为什么最初会按 demo workspace 重新组织展示内容。
- `Wave 7` 和 Post-Wave 7 新主线之间的边界。

当前前端展示与 `/console` 的现行设计，已经统一迁移到
[post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md)。
本文档不再作为当前前端展示结构、导航分组、场景组织或 `/docs` 信息架构的权威来源。

本文档不记录执行 checklist。具体实施步骤写入 `docs/plan/`，当前状态以
[../status/mainline_status.md](../status/mainline_status.md) 为准。

## 当前有效性说明

- 历史语境有效：本文档保留 `Wave 7` 首轮产品官网壳层、首页叙事和 demo workspace v1 的设计背景。
- 当前前端现行设计入口：
  [post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md)
- 当前状态以 [../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md#mainline-wave7-product-website-shell-plan](../plan/history_plan.md#mainline-wave7-product-website-shell-plan)
- 相关设计：
  - [post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md)
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [npu_tpu_accelerator_direction_design.md](npu_tpu_accelerator_direction_design.md)
  - [wave6_jit_dbt_readiness_design.md](wave6_jit_dbt_readiness_design.md)

## 背景与问题

当前仓库已经是一个已可运行的 RISC-V 模拟器原型，具备 `functional / pipeline`
双后端、`interactive_os`、`xv6`、Linux checkpoint / probe、AI accelerator、向量 /
ML workload、L1D / shadow cache、JIT / DBT opt-in 原型和完整回归门禁。技术能力已经足够支撑一次阶段性展示，但现有入口仍更像“开发者调试台 + 汇报预览页”。

如果直接把当前 [../showcase/preview.html](../showcase/preview.html) 和 `frontend/`
暴露到公网，用户需要自己理解 Makefile、workload、backend、marker、probe 和状态文档，体验成本偏高。`Wave 7` 的核心问题不是“再做一个功能”，而是把这些能力整理成一个通过域名可访问的产品化站点：

```text
/
  产品首页
/console
  在线控制台
/docs
  产品文档
```

这个站点的第一目标用户是通过域名访问 myCPU 的普通产品用户。首页需要先展示用户能体验什么：打开控制台、启动系统 demo、观察 CPU 和内存状态、运行 AI accelerator workload、查看 JIT / DBT 与 L1D 的运行时数据。工程边界、安全限制和未完成项继续保留在设计 / 状态文档中，不作为首页卖点或导航主线。

## 目标

- 建立真正产品官网形态，而不是技术文档索引或一次性汇报页。
- 首页采用 Apple-style 产品叙事：第一屏极简，向下滚动后通过横向出现、跃入、浮现、数字递增、结构剖面和大幅模块展示完整工程能力。
- 保留现有米黄色 / 铜色 / 青绿色风格，但升级为“工程纸面 + 精密控制台”的产品气质。
- 把 `frontend/` 从调试台重构为 demo workspace：用户按演示目标进入 Linux、xv6、AI accelerator、pipeline、vector、JIT 等体验，而不是先面对一堆底层状态。
- 建立产品文档层，把现有 `design / status / showcase` 内容整理成面向用户的体验指南，而不是直接暴露设计文档森林。
- 固定公网部署前的 session、安全、资源、白名单 workload 和日志边界。

## 非目标

- 不在 `Wave 7` 中实现正式 `--backend jit`、persistent cache、multicore / coherence 或完整 NPU runtime。
- 不在 `Wave 7` 中承诺标准 Debian / Alpine 发行版镜像级兼容；这属于 Post-Wave 7 新主线。
- 不在 `Wave 7` 中开放任意用户 kernel / rootfs / AI 模型上传。
- 不把产品文档写成新的事实来源；工程真相仍以 `design / status / plan` 体系为准。
- 不把控制台扩成 IDE、通用调试器或 trace studio。
- 不迁移前端技术栈；当前继续基于原生 HTML / CSS / ESM 和 Node debug server。

## 约束与边界

- 首页第一屏不展示复杂功能清单，只放品牌、定位和两个主入口：`打开控制台`、`阅读产品文档`。
- 后续滚动内容可以高密度展示上层能力和底层基础设施，但必须按主题分段，不堆纯卡片目录。
- 控制台必须保持现有米黄色风格的辨识度，但展示内容和组织方式允许大重构。
- 控制台所有状态必须来自真实 debug snapshot、workload manifest、terminal session 或服务器端 profile，不能在浏览器端编造执行语义。
- Linux interactive console 是串口 shell demo，不是浏览器内运行 Linux，不提供图形桌面、网络或任意镜像支持。
- AI accelerator 在 `Wave 7` 首页和控制台主叙事中展示白名单 demo + 参数化小模型体验；任意 graph package 或模型导入的限制写入内部设计 / 状态文档，不放进对外卖点。
- 产品文档必须链接回原始 design / status 证据链，但面向读者重写结构和语言。

## 历史方案

### 结构设计

`Wave 7` 站点采用三层结构：

```text
Product Site
  -> Home
     -> Apple-style scroll storytelling
  -> Console
     -> demo-first workspace
  -> Docs
     -> product documentation
```

#### 1. 首页

首页从 [../showcase/preview.html](../showcase/preview.html) 演化，但不沿用“汇报 HTML 预览”的信息密度。推荐的叙事顺序是：

1. **Hero / First View**
   - `myCPU`
   - “一个可在浏览器中体验的 RISC-V 系统模拟器工作台。”
   - `打开控制台` / `阅读产品文档`
   - 背景只保留克制的终端光标、状态灯或控制台轮廓。
2. **Console Reveal**
   - 大号控制台界面从下方浮现，展示 terminal、pipeline 和 counters 的真实感。
3. **Run Real Systems**
   - Linux serial console、`xv6` shell、`interactive_os` monitor 以横向出现 / 大模块展示。
4. **Inspect the Machine**
   - 以内部剖面方式展示 `functional / pipeline`、register / CSR diff、memory observation、profile。
5. **Accelerate Workloads**
   - 深色区域展示 AI accelerator、DMA、scratchpad、compute、completion、参数化小模型和 vector CNN。
6. **Prototype the Future**
   - 展示 JIT / DBT opt-in 原型、L1D / shadow cache、Post-Wave 7 两条新主线。
7. **Built with Guardrails**
   - 用数字跃入展示测试、Spike differential、docs governance、`make test` / `make test-pipeline`。
8. **Final CTA**
   - 回到 `打开控制台` / `阅读产品文档`。

首页可以使用卡片，但不能退化成卡片堆叠页。卡片、大幅模块、横向滑入、浮现仪表、数字递增和路线图需要混合使用，形成产品发布页节奏。

#### 2. 控制台

以下内容描述的是 `Wave 7` 首轮 `demo workspace v1` 的组织方式，保留为历史设计语境。
当前 `/console` 的现行结构已经改为 `Lab workbench`，以
[post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md) 为准。

控制台从当前调试台重构为 demo workspace。推荐一级入口：

- `OS Bring-up`
  - `interactive_os`
  - `xv6 shell`
  - Linux serial console
- `Microarchitecture`
  - pipeline timeline
  - register / CSR diff
  - memory observation / profile
- `AI Accelerator`
  - guest AI demo
  - 白名单 demo
  - 参数化小模型
  - DMA / scratchpad / profile counters
- `Vector / ML`
  - vector operator demo
  - vector CNN
- `JIT / DBT`
  - opt-in stats
  - hit / miss / emit / fallback / invalidation

每个 demo 页面必须回答：

- 这个 demo 证明什么？
- 用户能执行什么动作？
- 预期看到什么 marker？
- 当前能力边界是什么？

控制台仍复用现有 `frontend/server/debug_server.mjs`、`debug_server_runtime.mjs`、
`tests_manifest.mjs`、terminal input pump 和 debug CLI session，但前端展示从“状态面板优先”调整为“demo 任务优先”。

#### 3. 产品文档

以下目录结构属于 `Wave 7` 首轮产品文档设想。当前前端产品解释层、`/console` 信息架构和后续
Linux 专题组织，不再以这里的目录分组作为现行约束。

`/docs` 是面向用户的产品文档，不是直接暴露 `docs/design`。推荐目录：

```text
Overview
Try the Console
Architecture
Execution Backends
OS Bring-up
AI Accelerator
Vector and ML Workloads
JIT / DBT Prototype
Verification
Roadmap
Design References
```

每页先解释产品能力和体验方式，再链接到对应 design / status 文档。这样用户先看到“能做什么、怎么体验”，需要深入时再进入工程证据链。

### 接口 / 数据 / 契约

- 首页可以静态渲染，但用于产品截图 / 预览的数据应来自现有 demo 的稳定 marker、snapshot 或手工 curated 文案，不伪造未实现能力。
- 控制台继续通过 Node debug server 调用 `mycpu --debug-cli`。
- 控制台 workload manifest 需要扩展为产品化 demo manifest，至少包含：
  - demo category
  - title / subtitle / summary
  - expected marker
  - required assets
  - supported actions
  - visible panels
  - internal notes / resource limits
  - resource limits
- Linux console session 必须具备 timeout、reset、terminate、terminal output trimming 和白名单 assets。
- AI 参数化小模型必须由服务器端生成 / 校验 graph package，不信任浏览器传入的 graph。
- 产品文档可以从 Markdown / curated metadata 生成，但必须保留原始 design / status 的链接。

### 验证思路

文档-only 更新至少运行：

- `git diff --check`

触达首页 / 控制台静态 UI 时，至少运行：

- `cd frontend && node --test`

触达 debug server / session / workload manifest 时，至少运行：

- `cd frontend && node --test`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`

触达 Linux interactive console 时，补充：

- 本地 CLI / debug server Linux prompt smoke
- terminal input / output / timeout / reset / terminate smoke

触达 AI 参数化小模型时，补充：

- `cd myCPU && make test-host-ai_accelerator_profile_smoke`
- 参数生成 / shape fail-closed / resource limit smoke

大范围控制台改造完成后，守住：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

## 风险与取舍

- Apple-style 叙事页容易变成视觉包装；必须持续把真实 demo marker、测试、文档和边界放进页面。
- 控制台大重构会触碰用户体验和现有测试，必须先拆站点壳层，再逐步重构 demo workspace。
- 产品文档如果直接复制设计文档，会让评审迷失；如果过度重写，又可能制造并行事实来源。解决方式是“产品解释 + design/status 证据链接”。
- 公网部署不能先于 session / 资源 / 白名单边界，否则 Linux 和 AI demo 都会变成安全风险。
- Post-Wave 7 两条新主线可以打破现有边界，但不能提前塞进 `Wave 7` 产品化收口。

## 历史收口说明

- `Wave 7` 首页壳层、产品文档 v1 和 `demo workspace v1` 已完成并归档。
- 首轮计划归档：
  [../plan/history_plan.md#mainline-wave7-product-website-shell-plan](../plan/history_plan.md#mainline-wave7-product-website-shell-plan)
