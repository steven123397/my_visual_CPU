# 展示材料

本目录只做展示材料入口，不承担实时状态职责。当前工程事实仍以
[../status/mainline_status.md](../status/mainline_status.md) 和
[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。

## 目录

- [course-os](course-os)：本轮操作系统课程最终总结与展示材料，覆盖课程 OS 基线、交互
  shell、架构增强、Linux compat Plus、OSComp 外部验证和明确不声明的边界；后续 PPT、
  讲稿、截图、演示脚本和 HTML 预览页优先放在这里。
- [simulator](simulator)：原有 myCPU 模拟器展示材料，包括既有结题 PPT、讲稿、报告、
  截图、HTML 预览页和 AI demo v1 展示脚本。

## 维护口径

- `course-os` 目录用于操作系统课程展示，不把 Linux compat Plus / OSComp 外部验证写成
  Stage 1-4 课程 OS 基线完成条件。
- `simulator` 目录保留原有模拟器展示内容和路径语义，正式引用应使用
  `docs/showcase/simulator/...`。
