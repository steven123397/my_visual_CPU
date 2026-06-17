# myCPU 结题汇报 PPT 设计方案

## 目标

做一份能讲 10 分钟的课程结题 PPT。主线分三部分：

1. 项目实现的底层：CPU / 内存 / 特权 / 外设 / guest / 前端调试链路。
2. 创新点：不是只写解释器，而是做成可运行、可观察、可验证、可扩展的系统平台。
3. 三人分工与 Git 协作：按子系统负责，主线集成，测试门禁和文档同步。

风格沿用 `preview.html`：

- 背景：米黄色工程纸面 `#f5eddc` / `#e4d4b8`
- 主色：铜色 `#b35d2f` / 深铜 `#72371d`
- 强调色：青绿色 `#1c7780` / 深青 `#0e4f56`
- 深色面板：近黑蓝绿 `#08151a` / `#172126`
- 字体气质：中文宋体/思源宋体做大标题，黑体/思源黑体做正文，等宽字体做命令和 marker
- 画面语言：工程纸面 + 精密控制台；不堆大段文字，但允许每页用多组短句、卡片、对照表、流程图、终端输出和证据截图承载更多信息

## 已有截图素材评估

`docs/showcase` 当前已有 4 张新截图，整体可以直接进入 PPT，但放法需要按画幅调整：

### 1. `ppt_screenshot_console_overview.png`

- 尺寸：`2507 x 1450`，接近 16:9，清晰度足够。
- 内容：Lab workbench 首页 + 会话选择 + terminal + guide/contract，能体现产品化展示和工作台组织方式。
- 问题：右侧有浏览器滚动条；画面信息比较多，直接全屏会显得拥挤。
- 推荐用途：第 2 页“汇报内容”右侧证据图，或第 7 页“可视化调试与产品化展示”主图。
- PPT 放法：
  - 第 2 页：放右侧 55% 宽，裁掉最右侧滚动条，左侧保留三栏路线图。
  - 第 7 页：做全宽主图时，建议只保留中下部工作台区域，顶部大标题区域可裁掉一部分。
  - 可加半透明深色标签：`Lab Navigator`、`Session Bar`、`Terminal`、`Scenario Inspector`。

### 2. `ppt_screenshot_pipeline.png`

- 尺寸：`1317 x 913`，不是 16:9，但主体非常清楚。
- 内容：五级流水线卡片 + 最近周期表，适合证明 pipeline 可观察性。
- 问题：右侧边缘有一个小红点/边缘元素；下方表格只露出两行，完整全屏展示意义不大。
- 推荐用途：第 5 页“底层实现重点”右侧主图。
- PPT 放法：
  - 不建议全屏，建议作为 55% 到 60% 宽的浅色截图卡片。
  - 裁掉右侧 1% 到 2% 边缘，去掉小红点。
  - 左侧配 3 个短卡片：`共享语义`、`pipeline 后端`、`可观察提交`。

### 3. `ppt_screenshot_terminal.png`

- 尺寸：`1277 x 855`，接近 3:2，深色终端观感很好。
- 内容：`interactive_os terminal`、`guest_interactive_os_demo · pipeline`、`help` 输出和 prompt。
- 问题：画面里有一次 `timee` 拼写错误导致 `unknown command`，但这反而可以解释终端是实时交互，不是静态 mock。
- 推荐用途：第 6 页“系统软件 bring-up”主图，或第 12 页总结页右侧终端证据。
- PPT 放法：
  - 可作为右侧 60% 宽深色主视觉。
  - 如果担心 `timee` 干扰，裁掉终端正文上半部分，只保留 `help` 输出和 `monitor>` prompt。
  - 左侧放证据链：`KMVPETDS`、`interactive_os`、`xv6 shell`、`Linux-facing`。

### 4. `ppt_screenshot_ai_or_vecto.png`

- 尺寸：`1531 x 1403`，偏竖向，不适合整张直接塞进 16:9。
- 内容：上半部分是 AI Accelerator Demo 说明，下半部分是 Parameterized Tiny Model 控件和证据卡片。
- 问题：信息量很大；整图缩小后文字会太小。
- 推荐用途：第 9 页“创新点展开：AI / Vector / JIT”。
- PPT 放法：
  - 优先裁下半部分 `Parameterized Tiny Model` 区域，作为右侧主图。
  - 上半部分 `Scenario inspector` 可作为小缩略图或不用。
  - 文件名少了末尾 `r`，方案按实际文件名 `ppt_screenshot_ai_or_vecto.png` 引用；如果想统一命名，可手动改成 `ppt_screenshot_ai_or_vector.png`。

## 四段生成图片的 AI 提示词

下面 4 段用于生成 PPT 中的非截图类示意图。建议生成 16:9 横图，分辨率至少 `1600x900`。如果使用的图片模型不擅长中文文字，要求它少写字、留出标题空位，文字在 PPT 里后期手动加。

### 1. 系统总体架构图生成提示词

```text
生成一张 16:9 横版系统架构图，用于课程结题 PPT。主题是 myCPU RISC-V 系统模拟器。
视觉风格：米黄色工程纸面背景，细网格纹理，铜色和青绿色强调，深色精密控制台面板，不要科幻霓虹，不要紫色渐变。
画面结构：四层分层架构，从下到上分别是 Host Simulator / Platform MMIO / Guest Runtime / Browser Lab。
Host Simulator 层包含 Decode、InstructionSemantics、functional reference、pipeline backend、JIT/DBT prototype。
Platform MMIO 层包含 RAM、UART、CLINT、PLIC、block storage、AI accelerator。
Guest Runtime 层包含 boot、PMM、Sv39、trap、kernel_alpha、interactive_os、xv6、Linux-facing probes。
Browser Lab 层包含 Node debug server、terminal、pipeline view、register/CSR inspector、profile panels。
要求：像工程蓝图或产品发布会技术剖面图，有轻微 3D 层叠感，线条清晰，文字少而大，留出左上角标题区域。
不要生成密密麻麻的小字，不要生成真实芯片照片，不要生成抽象光球。
```

### 2. 底层执行链路图生成提示词

```text
生成一张 16:9 横版技术流程图，用于解释 myCPU 中“一条 RISC-V 指令如何变成系统行为”。
视觉风格：工程纸面 + 精密控制台，米黄色背景，铜色箭头，青绿色状态节点，少量深色终端面板。
流程从左到右：Fetch / Decode -> InstructionSemantics -> Core State(GPR/CSR/privilege/trap) -> Sv39/TLB -> AddressSpace/Bus -> RAM/MMIO Devices。
在下方加两条分支概念：functional reference 用于正确性基线；pipeline backend 用于 rename / ROB / LSQ / OoO observation。
重点表达：共享语义，不复制第二套 ISA；debug snapshot 只读观察，不改变执行语义。
要求：节点清晰，适合 PPT 投影，文字少而大，避免复杂公式，避免真实代码截图。
```

### 3. 创新点地图生成提示词

```text
生成一张 16:9 横版创新点地图，用于 myCPU 结题汇报。
中心是 myCPU RISC-V 系统模拟器实验平台，周围六个创新点环绕：共享语义 + 多后端、系统级 bring-up、浏览器 Lab 工作台、分层验证体系、AI Accelerator 原型、JIT/DBT 研究路径。
视觉风格：米黄色工程纸面背景，铜色/青绿色节点，深色中心圆或控制台核心，细网格和连接线，有工程评审感。
每个节点只保留短标签，不要大段文字；节点之间用细线连接，像技术路线图。
要求整体简洁、有层级、适合第 8 页作为全页主图；不要使用通用商业图标堆叠，不要使用紫色科技风。
```

### 4. 三人分工与 Git 协作图生成提示词

```text
生成一张 16:9 横版团队协作图，用于课程项目结题 PPT。
视觉风格沿用米黄色工程纸面、铜色和青绿色强调、深色终端风格。
画面上半部分是三人分工卡片：
梁家琦：总体架构、ISA/CSR/内存/平台设备、guest runtime、前端调试链路、文档整理与最终联调。
杨皓宇：pipeline backend、分支预测、rename、ROB、LSQ、flush/rollback、可观察状态。
余健超：asm 指令级测试、unit/host/guest smoke、pipeline 回归、frontend 测试、Spike differential。
画面下半部分是 Git 协作时间线：main baseline -> feature/wave branch -> test gate -> merge -> docs sync。
要求：像工程项目协作看板，不要人物照片，不要卡通头像；文字区域要大，便于 PPT 后期替换和微调。
```

## PPT 页结构

## 内容密度原则

这版不再要求“每页正文不超过 6 行”。新的判断标准是：

- 可以超过 6 行，但必须拆成卡片、标签、流程步骤、对照表或终端输出。
- 避免整段正文；单个文字块尽量控制在 1 到 3 行。
- 每页保留一个主视觉：截图、架构图、流程图或终端卡片，不要让文字铺满整页。
- 信息多的页面分成两层：PPT 页面放短句和证据，演讲者备注放展开讲稿。
- 技术页可以密一点，封面、总结和转场页要更克制。

### 第 1 页：封面

标题：

```text
myCPU：基于 RISC-V 的系统级模拟器设计与实现
```

副标题：

```text
计算机系统结构课程项目结题汇报
```

成员：

```text
梁家琦 · 杨皓宇 · 余健超
```

画面：

- 米黄色工程纸面背景
- 右侧放一个深色终端卡片：

```text
$ kernel_alpha_demo
KMVPETDS
$ xv6-riscv
init: starting sh
```

讲稿提示：

> 我们做的不是一个只能跑几条指令的解释器，而是一套已经可运行、可调试、可验证的 RISC-V 系统模拟器。

### 第 2 页：10 分钟路线图

标题：

```text
汇报内容
```

内容三栏：

1. 底层实现
   - ISA / CSR / Sv39 / MMIO
   - functional + pipeline
   - guest 与前端
2. 创新点
   - 系统级 bring-up
   - 浏览器 Lab
   - AI / JIT / 验证体系
3. 协作方式
   - 三人分工
   - Git 分支与主线
   - 测试门禁

配图：

- 右侧可放 `ppt_screenshot_console_overview.png`
- 如果页面显得太满，只裁右下工作台区域，不放整张网页长截图

讲稿提示：

> 我会先解释底层怎么搭起来，再讲超出基础解释器的创新点，最后说明我们三个人如何分工和用 Git 保持工程可控。

### 第 3 页：系统总体架构

标题：

```text
从 ISA 到浏览器：四层闭环
```

主图：

```text
使用“系统总体架构图生成提示词”生成的 16:9 架构图
```

右下角短句：

```text
Host Simulator → Platform MMIO → Guest Runtime → Browser Lab
```

讲稿提示：

> 最底层是 C++17 模拟器主体，中间是 MMIO 平台和 guest runtime，最上层是 Node debug server 与浏览器 Lab。运行时证据从底层产生，再被前端只读展示。

### 第 4 页：底层执行链路

标题：

```text
一条指令如何变成系统行为
```

主图：

```text
使用“底层执行链路图生成提示词”生成的 16:9 流程图
```

讲稿提示：

> 指令先经过 fetch/decode，再进入共享 InstructionSemantics。它会更新 GPR、CSR、privilege 和 trap 状态；如果访问内存，会经过 Sv39、TLB、Bus，再到 RAM 或 MMIO 设备。

补充短句：

```text
共享语义是正确性基线；pipeline 和 JIT 原型不复制第二套 ISA。
```

### 第 5 页：底层实现重点：CPU / 内存 / 特权

标题：

```text
从解释器走向系统模拟器
```

布局：

- 左侧 3 个短卡片
- 右侧放 `ppt_screenshot_pipeline.png`
- 图片建议裁掉最右侧小边缘元素，保留五级流水线卡片和最近周期表

卡片内容：

```text
ISA 语义
RV64I / RV64M，InstructionSemantics 作为真值来源
functional 先保证正确性，pipeline / JIT 原型只消费共享语义

特权与 trap
M / S / U，trap delegation，mret / sret
CSR、异常、中断和 privilege 切换串成系统路径

Sv39 与平台
三级页表，TLB，page fault，UART / CLINT / PLIC / block
CPU 访存统一走 AddressSpace → Bus → RAM / MMIO
```

讲稿提示：

> 这一页重点讲“底层不是堆 if-else”。我们把语义、状态、地址空间、设备和后端拆开，让功能级正确性和微结构观察能同时存在。

演讲者备注可展开：

- 为什么需要 `functional`：它是 reference path，用来对齐后续 pipeline、JIT/DBT、Spike 差分和 guest 运行证据。
- 为什么需要 Sv39：没有页表、TLB 和 fault，就只能跑裸机，不能解释 OS bring-up 中的地址空间、权限和异常。
- 为什么设备要走统一 Bus：UART、timer、PLIC、storage、AI accelerator 都以 MMIO 方式接入，guest 看到的是同一套平台合同。

### 第 6 页：系统软件 bring-up

标题：

```text
能跑系统，才算系统级模拟器
```

布局：

- 左侧纵向证据链
- 右侧放 `ppt_screenshot_terminal.png`
- 如果担心 `timee` 拼写错误分散注意力，裁掉终端正文上半部分，只保留 `help` 输出和 `monitor>` prompt

证据链：

```text
kernel_alpha
KMVPETDS：boot / PMM / Sv39 / interrupt / storage
这证明自制内核已经跨过内存、页表、中断和存储最小路径

interactive_os
浏览器 terminal 可输入命令并观察 UART 回显
这证明串口输入输出和前端交互闭环可用

xv6-riscv
真实 virtio-blk board path 稳定到 shell
这证明平台不只服务自制 guest，也能承载外部 workload

Linux-facing
flat image + DTB + rootfs + serial console 路径
这证明项目已进入更真实的 Linux bring-up 方向
```

讲稿提示：

> 这部分说明项目已经越过裸机程序阶段。自制内核、interactive monitor、xv6 和 Linux-facing probe 都在同一个平台抽象上运行。

演讲者备注可展开：

- `KMVPETDS` 每个字母可以快速解释：kernel、memory、virtual memory、PLIC、external interrupt、timer、disk、storage signature。
- xv6 的重点不是“名字好听”，而是它走真实 board path 和 virtio-blk，能暴露更多平台 bug。
- Linux-facing 这里要讲边界：已有 boot/probe 和 serial console 路线，但不说完整发行版兼容已经完成。

### 第 7 页：可视化调试与产品化展示

标题：

```text
把模拟器变成可观察的实验台
```

布局：

- 大图：`ppt_screenshot_console_overview.png`
- 建议裁掉浏览器右侧滚动条，优先展示中下部工作台区域
- 图上叠 4 个标签：

```text
Terminal
Pipeline
Registers / CSR
Device / AI profile
```

可加 4 个小注释：

```text
Load / Run / Pause / Reset / Terminate
scenario 控制会话流转

Terminal
展示 UART 输出和 guest prompt

Inspector
展示寄存器、CSR、pipeline、device counters

Evidence / Boundary
说明 demo 证明什么，以及当前不能夸大什么
```

讲稿提示：

> 前端不是装饰，它解决的是系统级项目最难的可观察性问题。我们可以在浏览器里看到终端输出、pipeline 阶段、寄存器变化、外设计数器和 profile 数据。

### 第 8 页：创新点总览

标题：

```text
创新点：可运行、可观察、可验证、可扩展
```

主图：

```text
使用“创新点地图生成提示词”生成的 16:9 创新点地图
```

讲稿提示：

> 创新点不是某一个孤立功能，而是把课程项目做成一个平台。共享语义、多后端、系统 bring-up、浏览器 Lab、AI accelerator、JIT/DBT 和验证体系共同支撑这个平台。

### 第 9 页：创新点展开：AI / Vector / JIT

标题：

```text
面向后续研究的扩展能力
```

布局：

- 左侧三张卡片
- 右侧放 `ppt_screenshot_ai_or_vecto.png`
- 建议只裁下半部分 `Parameterized Tiny Model` 区域，避免整图缩小后文字过小

卡片内容：

```text
AI Accelerator
MMIO 设备、scratchpad、DMA、profile summary、simulated cycles
亮点是把 AI 任务做成设备级路径，而不是 CPU 侧函数调用

Vector / ML
V-lite 指令子集，固定 conv→relu CNN demo，marker 为 V3OK
亮点是把向量状态和 ML workload 放进同一观察体系

JIT / DBT
hot-path、IR dry-run、host emitter、opt-in runtime harness
亮点是先做 translation contract 和 guardrail，不急着替换默认 backend
```

边界提示：

```text
不夸大：当前不声称完整商用 NPU、默认 JIT backend 或完整 Linux 发行版兼容。
```

讲稿提示：

> 这些方向展示了项目的扩展性，但我们保持边界诚实：课程结题时它们是有证据的原型和研究路径，不包装成已经完整商用的功能。

演讲者备注可展开：

- AI accelerator 的创新点是“设备化”：MMIO doorbell、submission queue、scratchpad、DMA 和 profile summary 形成硬件边界感。
- Vector / ML 的创新点是“workload 化”：不是只验证单条向量指令，而是用固定 CNN demo 证明一组算子能闭环。
- JIT / DBT 的创新点是“工程化推进”：先做 dry-run、IR、lowering、host emitter 和 opt-in harness，默认语义仍回到 reference path。

### 第 10 页：验证体系

标题：

```text
如何避免“能跑但不可信”
```

布局：

- 四层金字塔或四列卡片

内容：

```text
Asm / Unit
指令、loader、MMU、device、helper
锁定最小语义和模块边界

Host Smoke
debug CLI、pipeline、JIT、AI profile
验证宿主侧组合路径

Guest Smoke
kernel_alpha、interactive_os、xv6、Linux-facing probe
验证系统级运行链路

External Oracle
Spike differential 验证高风险架构语义
用外部参考实现降低自证风险
```

底部命令条：

```bash
cd myCPU && make test
cd myCPU && make test-pipeline
cd frontend && node --test
```

讲稿提示：

> 系统模拟器不能只靠“演示时跑通”。我们把验证拆成多层，既测底层语义，也测 guest 系统和前端服务。

演讲者备注可展开：

- 单元测试适合防止局部退化，guest smoke 适合证明跨层路径仍能跑通。
- Spike differential 的价值是引入外部 oracle，避免模拟器只和自己的预期互相验证。
- 前端也有 Node 测试，因为可视化调试台已经是项目交付的一部分，不是临时页面。

### 第 11 页：三人分工与 Git 协作

标题：

```text
三人分工：子系统负责，主线集成
```

主图：

```text
使用“三人分工与 Git 协作图生成提示词”生成的 16:9 协作图
```

讲稿提示：

> 我们按子系统分工：梁家琦负责总体架构、平台、guest 和前端集成；杨皓宇负责 pipeline 和微结构；余健超负责测试、验证和差分。协作上以 main 为稳定主线，功能通过分支或阶段性 checkpoint 合入，合入前跑对应测试。

可补充数字：

```text
当前快照：246 commits
主线约定：design / plan / status 分离
main 保持可运行，feature / wave 小步推进
合并前先跑对应测试，再同步文档口径
```

页面可加一个小型 Git 口径表：

```text
分支策略：main 稳定，功能线独立推进
提交粒度：按可验证 checkpoint 收口
冲突处理：先保主线事实源，再合入子系统改动
验收方式：代码、测试、截图、状态文档一起闭环
```

### 第 12 页：总结与答辩收束

标题：

```text
结题总结
```

三句话：

```text
1. 我们完成了一套已可运行的 RISC-V 系统模拟器原型。
2. 它不只解释指令，还能支撑特权级、虚拟内存、外设、guest OS、真实 workload 和浏览器调试。
3. 后续可继续沿 Linux 发行版平台、AI accelerator、JIT/DBT、cache / 多核方向演进。
```

右侧终端：

```text
$ make test
PASS
$ xv6-riscv
init: starting sh
$ kernel_alpha_demo
KMVPETDS
```

讲稿提示：

> 如果用一句话总结，myCPU 已经从课程里的指令集模拟器，成长为一个可以继续做系统结构实验的平台。

## AI 生成 PPT 的完整提示词

可以把下面整段交给 Gamma、Kimi PPT、WPS AI、Canva、通义或其他 PPT 生成器。生成后再手动替换截图。

```text
请生成一份 12 页、16:9 横版 PPT，主题是《myCPU：基于 RISC-V 的系统级模拟器设计与实现》，用于《计算机系统结构》课程结题汇报，目标讲 10 分钟。

整体风格：
- 使用“工程纸面 + 精密控制台”的视觉风格。
- 背景为米黄色工程纸面，主色为铜色和青绿色，深色区域使用近黑蓝绿终端风格。
- 颜色参考：#f5eddc、#e4d4b8、#b35d2f、#72371d、#1c7780、#0e4f56、#08151a、#172126。
- 大标题用中文宋体/思源宋体风格，正文用黑体/思源黑体，命令和 marker 用等宽字体。
- 不要堆大段文字；每页可以有较多信息，但必须拆成短句、卡片、流程图、对照表、终端输出、证据图或演讲者备注。
- 图形元素使用细网格、纸面纹理、圆角 8-24px、深色终端卡片、铜色/青绿色强调线。

项目事实：
- myCPU 是一套已经可运行的 RISC-V 系统模拟器原型，不是纯设计稿。
- 项目从指令解释扩展到系统级平台，包含 RV64I/RV64M、M/S/U 特权级、Sv39 虚拟内存、UART/CLINT/PLIC/块设备、functional 参考后端、pipeline 微结构后端、guest runtime、浏览器调试台、AI accelerator 原型、JIT/DBT 原型和验证体系。
- 方法论是 reference-first：InstructionSemantics + functional backend 是 ISA 语义真值来源，pipeline 和 JIT/DBT 原型消费共享语义，不复制第二套 ISA。
- 运行证据包括 kernel_alpha 的 KMVPETDS、xv6 shell、Linux-facing probe、interactive_os terminal、Vector CNN 的 V3OK、AI accelerator profile summary。
- 当前不夸大：不声称完整 Linux 发行版兼容，不声称正式默认 JIT backend，不声称完整商用 NPU，不声称 multicore/coherence 已完成。
- 三人分工：梁家琦负责总体架构、ISA/CSR/内存/平台设备、guest runtime、前端调试链路、文档整理与最终联调；杨皓宇负责 pipeline backend、分支预测、rename、ROB、LSQ、提交边界、flush/rollback 与可观察状态；余健超负责 asm 指令级测试、unit/host/guest smoke、pipeline 回归、frontend 测试和 Spike differential。
- Git 协作口径：main 保持稳定可运行，功能按 wave/feature 小步推进，合并前跑对应测试，文档保持 design/plan/status 分离。当前快照可标注 246 commits。

PPT 结构：
1. 封面：myCPU：基于 RISC-V 的系统级模拟器设计与实现。副标题：计算机系统结构课程项目结题汇报。成员：梁家琦、杨皓宇、余健超。右侧放深色终端卡片，内容为 `$ kernel_alpha_demo / KMVPETDS / $ xv6-riscv / init: starting sh`。
2. 汇报内容：底层实现、创新点、三人分工与 Git 协作。三栏卡片，右侧使用 `ppt_screenshot_console_overview.png` 的工作台局部截图。
3. 系统总体架构：使用 AI 生成的 3D 分层图，四层为 Host Simulator、Platform/MMIO、Guest Runtime、Debug Frontend/Lab Workbench。
4. 底层执行链路：使用 AI 生成的流程图，从 Fetch/Decode 到 InstructionSemantics、Core State、Sv39/TLB、Bus/MMIO。
5. 底层实现重点：ISA 语义、特权与 trap、Sv39 与平台。右侧使用 `ppt_screenshot_pipeline.png`，裁掉右侧小边缘元素。
6. 系统软件 bring-up：kernel_alpha、interactive_os、xv6-riscv、Linux-facing。右侧使用 `ppt_screenshot_terminal.png`，必要时裁掉上半部分拼写错误行。
7. 可视化调试与产品化展示：浏览器 Lab 工作台，展示 terminal、pipeline、register/CSR、device/AI profile。使用 `ppt_screenshot_console_overview.png` 的中下部工作台区域。
8. 创新点总览：可运行、可观察、可验证、可扩展。使用 AI 生成的创新点地图。
9. 创新点展开：AI Accelerator、Vector/ML、JIT/DBT。右侧使用 `ppt_screenshot_ai_or_vecto.png` 的下半部分 Tiny Model 区域，并加“边界诚实”提示。
10. 验证体系：Asm/Unit、Host Smoke、Guest Smoke、External Oracle 四层。底部命令条：`cd myCPU && make test`、`cd myCPU && make test-pipeline`、`cd frontend && node --test`。
11. 三人分工与 Git 协作：使用 AI 生成的协作图，强调子系统负责、主线集成、测试门禁、文档同步。
12. 总结：三句话收束。右侧放终端卡片：`$ make test PASS`、`$ xv6-riscv init: starting sh`、`$ kernel_alpha_demo KMVPETDS`。

注意：
- 不限制每页 6 行；技术页可以信息更密，但不要出现整段论文式正文。
- 单个文字块尽量 1 到 3 行；信息多时拆成卡片、表格、流程步骤或讲稿备注。
- 每页保留一个主视觉，文字围绕主视觉解释，不要铺满整页。
- 截图文件已经存在于 `docs/showcase`：`ppt_screenshot_console_overview.png`、`ppt_screenshot_pipeline.png`、`ppt_screenshot_terminal.png`、`ppt_screenshot_ai_or_vecto.png`。
- 对截图做必要裁剪：console 裁滚动条和顶部冗余，pipeline 裁右侧边缘，terminal 可裁掉拼写错误行，AI 图裁下半部分 Tiny Model 区域。
- 架构图、执行链路图、创新点图、协作图用本文档里的 4 段图片生成提示词生成，不引用已删除的 SVG。
```

## 现场 10 分钟讲稿节奏

- 0:00-0:40：封面和项目定位。强调“已可运行的系统模拟器原型”。
- 0:40-1:20：三部分路线图。告诉老师后面按底层、创新、协作讲。
- 1:20-2:30：系统总体架构。讲四层闭环。
- 2:30-3:40：底层执行链路。讲共享语义、core state、Sv39、MMIO。
- 3:40-4:40：CPU/内存/特权重点。讲 reference-first 与 pipeline 解耦。
- 4:40-5:50：系统软件 bring-up。讲 kernel_alpha、interactive_os、xv6、Linux-facing。
- 5:50-6:40：浏览器 Lab。讲可观察性，不是纯展示页。
- 6:40-7:50：创新点。讲平台化、AI、JIT、验证体系，同时说明边界。
- 7:50-8:50：验证体系。讲多层测试和 Spike external oracle。
- 8:50-9:40：三人分工与 Git 协作。
- 9:40-10:00：总结。回到“课程项目已经完成，并具备后续研究平台价值”。

## PPT 插图文件清单

当前截图已经放在根目录，PPT 工具引用这些文件即可：

```text
ppt_screenshot_console_overview.png
ppt_screenshot_pipeline.png
ppt_screenshot_terminal.png
ppt_screenshot_ai_or_vecto.png
```

如果最终 PPT 文件也放在根目录，图片路径关系最简单；如果 PPT 放到其他目录，插图后最好选择“嵌入图片”，不要只链接外部文件。非截图类架构图建议用本文档的 4 段 AI 提示词重新生成，再作为 PNG 插入。
