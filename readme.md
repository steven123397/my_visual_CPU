# myCPU — RISC-V 模拟器

`myCPU` 当前已经是一个已可运行的模拟器原型，而不是纯设计稿。主线已经形成 `phase1-stable`（`283aee6`）这一 Phase 1 冻结基线，并在此后继续接入了 `pipeline` 执行后端、本地 `debug_session/protocol`、浏览器前端教学演示链路，以及 `Phase 3-A` 的首轮分支预测增强。

项目当前的工程重点不是“再证明它能跑”，而是继续稳住 reference path 的 correctness、维护 `kernel_alpha` 与 `interactive_os` 这两条 guest 路线，并把 `pipeline` / `debug/frontend` 收口成更稳定的已接入能力。

## 当前定位

- host simulator：C / C++17
- guest runtime：C11 + RISC-V assembly
- 默认 reference path：`functional`
- 可选执行后端：`pipeline`
- 当前 guest 主线：
  - `guest_supervisor_demo`
  - `kernel_alpha` 正向 + 负向回归
  - `interactive_os` 最小交互 monitor

## 仓库结构

```text
my_visual_CPU/
├── myCPU/      # 模拟器主体、guest runtime、测试与 Makefile
├── frontend/   # 本地调试服务、浏览器前端与 Node 测试
├── docs/       # background / design / plan / status 正式文档
└── readme.md
```

更细的模块边界和局部规则请直接看：

- [AGENTS.md](AGENTS.md)
- [myCPU/AGENTS.md](myCPU/AGENTS.md)
- [myCPU/guest/AGENTS.md](myCPU/guest/AGENTS.md)

## 构建

依赖：

```bash
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
```

构建模拟器：

```bash
cd myCPU
make
```

## 运行

常用入口：

```bash
cd myCPU

# 运行 ELF
./mycpu <program.elf>

# 显式选择 pipeline backend
./mycpu --backend pipeline <program.elf>

# 运行平坦二进制（指定加载地址）
./mycpu -b 80000000 <program.bin>

# 启动本地 debug CLI 协议进程
./mycpu --debug-cli
```

## 本地前端教学演示

先构建模拟器，再从仓库根目录启动本地服务：

```bash
cd myCPU
make

cd ..
node frontend/server/debug_server.mjs
```

默认地址：

```text
http://127.0.0.1:4173
```

当前前端支持：

- 选择仓库内现有 asm / guest / `kernel_alpha` demo
- 切换 `functional` / `pipeline`
- `Load / Run / Pause / Step Cycle / Step Commit / Reset`
- `interactive_os terminal`：点击后接管键盘，把 ASCII、`Enter`、`Backspace` 送入 guest monitor
- Debug inspector：查看 snapshot、pipeline、events、registers、CSRs、bus 与设备状态

## `interactive_os` 最小交互演示

`interactive_os` 不是图形桌面，而是一条“浏览器终端壳 + guest 串口 monitor”最小闭环。当前 monitor 命令集至少包含：

- `help`
- `echo`
- `time`
- `uptime`
- `disk info`
- `disk read <lba>`
- `regs`
- `peek <addr> [1|2|4|8]`
- `pagewalk <addr>`
- `pte dump <addr>`
- `halt`

常用验证入口：

```bash
cd myCPU
make test-unit-monitor_commands
make test-host-interactive_terminal_smoke
make test-guest-interactive_os_demo
make test-pipeline-guest-interactive_os_demo
```

## 测试

主回归入口：

```bash
cd myCPU
make test
make test-pipeline

cd ../frontend
node --test
```

常用定向验证：

```bash
cd myCPU
make test-host-debug_cli_smoke
make test-host-interactive_terminal_smoke
make test-guest-kernel_alpha_demo
make test-guest-kernel_alpha_fault_demo
```

说明：

- `make test` 守默认 `functional` reference path。
- `make test-pipeline` 守 `pipeline` backend 的 asm / host / guest / debug 门禁。
- `node --test` 守本地调试服务、terminal API 和前端纯状态逻辑。
- 更细的目标名称以 [myCPU/Makefile](myCPU/Makefile) 为准。

## 文档入口

想快速了解当前状态，建议按下面顺序看：

1. [docs/index.md](docs/index.md)
2. [docs/status/mainline_status.md](docs/status/mainline_status.md)
3. [docs/status/kernel_alpha_status.md](docs/status/kernel_alpha_status.md)
4. [docs/status/code_self_review_status.md](docs/status/code_self_review_status.md)

设计边界和回归收口标准见：

- [docs/design/cpp_refactor_design.md](docs/design/cpp_refactor_design.md)
- [docs/design/debug_frontend_integration.md](docs/design/debug_frontend_integration.md)
- [docs/design/minimal_interactive_os_design.md](docs/design/minimal_interactive_os_design.md)
- [docs/design/platform_mmio_contract.md](docs/design/platform_mmio_contract.md)
- [docs/design/regression_completion_criteria.md](docs/design/regression_completion_criteria.md)
