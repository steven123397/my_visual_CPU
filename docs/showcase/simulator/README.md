# myCPU 模拟器展示材料

> 本目录保存原有 myCPU 模拟器展示材料，包括课程结题、PPT、截图、演示脚本和对外展示材料。本轮操作系统课程展示材料另放在 [../course-os](../course-os)。这里服务展示，不承担实时状态职责；当前工程事实以 [../../status/mainline_status.md](../../status/mainline_status.md) 为准。

## 文件清单

| 文件 | 用途 |
|---|---|
| [myCPU_结题汇报.pptx](myCPU_结题汇报.pptx) | 最终结题 PPT。 |
| [myCPU_结题汇报_十分钟演讲稿.md](myCPU_结题汇报_十分钟演讲稿.md) | 按 PPT 页序组织的 10 分钟演讲稿。 |
| [结题报告-梁家琦-20231071332-电计2304.md](结题报告-梁家琦-20231071332-电计2304.md) | 课程结题报告正文。 |
| [结题汇报PPT设计方案.md](结题汇报PPT设计方案.md) | PPT 设计方案、页面结构和图片提示词。 |
| [preview.html](preview.html) | 沿用结题汇报配色和版式的 HTML 展示页。 |
| [ppt_screenshot_console_overview.png](ppt_screenshot_console_overview.png) | 当前 Lab workbench 总览截图。 |
| [ppt_screenshot_pipeline.png](ppt_screenshot_pipeline.png) | Pipeline 观察截图。 |
| [ppt_screenshot_terminal.png](ppt_screenshot_terminal.png) | `interactive_os` terminal 截图。 |
| [ppt_screenshot_ai_or_vecto.png](ppt_screenshot_ai_or_vecto.png) | AI / Vector 相关工作台截图。 |
| [post_wave7_ai_demo_v1_guide.md](post_wave7_ai_demo_v1_guide.md) | Post-Wave 7 AI demo v1 的最短演示路径。 |

旧版展示截图已被这 4 张当前截图替换，不再作为展示入口。

## 展示口径

myCPU 当前应表述为：

- 一套已经可运行的 RISC-V 系统模拟器原型，而不是纯设计稿。
- 一套从 C 原型演进到模块化 C++17 架构的工程。
- 一套以 `reference-first` 为核心方法的模拟平台：共享 `InstructionSemantics + functional backend` 是 ISA 真值来源。
- 一套具备浏览器 Lab 工作台的系统结构实验平台，支持 terminal、pipeline、寄存器、CSR、设备计数器和 AI profile 观察。

结题汇报时建议把能力边界讲清楚：

- 已完成：`kernel_alpha = KMVPETDS`、`interactive_os`、`xv6-riscv` shell、Linux-facing console/probe、V-lite / AI accelerator demo、JIT/DBT opt-in 原型、分层验证体系。
- 不夸大：不声称默认 JIT backend、完整 Linux 发行版兼容、任意 AI 模型上传、完整商用 NPU runtime 或 multicore / coherence 已完成。

## 推荐演示顺序

1. 打开 [preview.html](preview.html) 或 PPT 封面，说明项目定位：从指令模拟器推进到系统级模拟器。
2. 展示 `ppt_screenshot_console_overview.png`，说明 `/console` Lab workbench 的会话组织方式。
3. 展示 `ppt_screenshot_pipeline.png`，讲 `functional + pipeline`、共享语义和可观察提交。
4. 展示 `ppt_screenshot_terminal.png`，用 `interactive_os` terminal 证明 guest / UART / 浏览器闭环。
5. 展示 `ppt_screenshot_ai_or_vecto.png` 或运行 [post_wave7_ai_demo_v1_guide.md](post_wave7_ai_demo_v1_guide.md) 中的命令，说明 AI accelerator / task spec / profile 边界。
6. 结束时回到 [../../status/mainline_status.md](../../status/mainline_status.md)，说明课程结题后继续开发的主线。

## 常用命令

启动浏览器 Lab：

```bash
cd myCPU && make
cd ..
node frontend/server/debug_server.mjs --port=4173
```

基础验证：

```bash
cd myCPU
make test
make test-pipeline

cd ../frontend
node --test
```

AI demo v1：

```bash
cd myCPU
python3 workloads/ai_proto/run_demo_v1.py --out-dir workloads/ai_proto/generated/demo_v1
```

## 文档边界

- 模拟器展示材料放在本目录，Course OS 展示材料放在 [../course-os](../course-os)。
- 当前状态只写在 [../../status/mainline_status.md](../../status/mainline_status.md) 和必要的专项状态文档。
- 长期设计边界写在 [../../design](../../design)。
- 执行计划和完成归档写在 [../../plan](../../plan)。
