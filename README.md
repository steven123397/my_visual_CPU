# myCPU — RISC-V 模拟器原型

`my_visual_CPU` 是一个围绕 `myCPU` 演进的 RISC-V 模拟器项目。它不是课程作业式的一次性原型，而是一套已经可运行、可测试、可继续扩展的代码基线：仓库同时提供以 ISA 正确性为主的 `functional` 执行路径、用于执行模型实验的 `pipeline` 后端、能够支撑最小 supervisor / kernel bring-up 的 guest runtime，以及一条本地 `debug_session/protocol + frontend` 教学演示链路。

这个项目的目标，不只是“跑通几个 demo”，而是持续维护一套统一语义来源下的模拟器原型，并在此基础上逐步推进特权级、虚存、平台设备、guest runtime、pipeline 与更复杂微架构模型的实验与验证。

## 当前定位

- host simulator：C / C++17
- guest runtime：C11 + RISC-V assembly
- ISA / execution：当前以 `RV64I / RV64M` 为基础，默认执行路径为 `functional`，并提供 `pipeline` 后端
- privilege / memory：已覆盖最小 `M/S/U` 特权路径、`Sv39` 与基础 trap / page fault 语义
- platform：已接入 `UART`、`CLINT`、`PLIC` 与 MMIO block storage
- guest demos：`guest_supervisor_demo`、`kernel_alpha` 正负回归、`interactive_os` 最小交互 monitor
- debug tooling：本地 `--debug-cli` 协议、Node 调试服务与浏览器前端演示页

## 仓库结构

```text
my_visual_CPU/
├── myCPU/      # 模拟器主体、guest runtime、测试与 Makefile
├── frontend/   # 本地调试服务、浏览器前端与 Node 测试
├── docs/       # background / design / plan / status 正式文档
└── README.md
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
make test-host-pipeline_rename_commit_smoke
make test-host-pipeline_speculation_contracts_smoke
make test-host-interactive_terminal_smoke
make test-guest-kernel_alpha_demo
make test-guest-kernel_alpha_fault_demo
```

说明：

- `make test` 守默认 `functional` reference path。
- `make test-pipeline` 守 `pipeline` backend 的 asm / host / guest / debug 门禁。
- `node --test` 守本地调试服务、terminal API 和前端纯状态逻辑。
- 更细的目标名称以 [myCPU/Makefile](myCPU/Makefile) 为准。

### 可选：Spike 外部差分验证

Spike 外部差分是独立离线能力，用来给一批 host 微场景提供 `myCPU vs Spike` 的外部 oracle。它当前不属于默认主门禁，所以：

- 没装 Spike 时，`make test` 和 `make test-pipeline` 仍然可以正常跑
- 只有你主动运行 Spike 差分入口时，才要求本机能找到 `spike`

最小使用方式：

```bash
cd myCPU

# 只跑本地 helper / parser 自测，不要求安装 Spike
make test-host-spike_differential_smoke

# 真实运行 myCPU vs Spike final-state differential
make test-host-spike_differential
```

如果 `spike` 不在默认 `PATH`，可以显式指定：

```bash
cd myCPU
SPIKE_PATH=/path/to/spike make test-host-spike_differential
```

也可以先把 Spike 放进当前 shell 的 `PATH`：

```bash
export PATH=/path/to/spike/bin:$PATH
cd myCPU
make test-host-spike_differential
```

当前这条差分默认会跑一组已接入的 host 微场景。若要本地只看单个场景，可以先编译目标，再直接执行：

```bash
cd myCPU
make tests/host/spike_differential_smoke
./tests/host/spike_differential_smoke --run-differential trap_return
```

如果需要保留临时 ELF 和 Spike debug script 便于排障，可以加：

```bash
cd myCPU
SPIKE_DIFF_KEEP_TEMPS=1 make test-host-spike_differential
```

## 文档入口

想快速了解当前状态，建议按下面顺序看：

1. [docs/index.md](docs/index.md)
2. [docs/status/mainline_status.md](docs/status/mainline_status.md)
3. [docs/status/project_priority_roadmap.md](docs/status/project_priority_roadmap.md)
4. [docs/status/kernel_alpha_status.md](docs/status/kernel_alpha_status.md)

设计边界和回归收口标准见：

- [docs/design/cpp_refactor_design.md](docs/design/cpp_refactor_design.md)
- [docs/design/debug_frontend_integration.md](docs/design/debug_frontend_integration.md)
- [docs/design/minimal_interactive_os_design.md](docs/design/minimal_interactive_os_design.md)
- [docs/design/platform_mmio_contract.md](docs/design/platform_mmio_contract.md)
- [docs/design/regression_completion_criteria.md](docs/design/regression_completion_criteria.md)
- [docs/design/spike_differential_validation_design.md](docs/design/spike_differential_validation_design.md)
