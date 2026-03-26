# Debug / Frontend 集成实现计划

> **文档状态：** 已完成

> **完成态说明：** 本文档对应的接入工作已经完成，继续保留在 `plan/` 作为历史计划记录。当前结果以 [debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)、[status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md) 和 [readme.md](/home/liangjiaqi/projects/my_visual_CPU/readme.md) 为准。下文中的“本轮”“待办”等表述均按当时计划语境理解。

> **面向 AI 代理的工作者：** 如需重演类似工作，仍应使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans。下文复选框结果仅保留历史执行记录。

**目标：** 在不改变当前 `functional` / `pipeline` 执行语义的前提下，把最小 `debug_session/protocol` 与前端教学演示链路正式接入主线。

**架构：** 先为 backend、`Machine`、`Bus` 和关键设备补最小只读调试面，再接入 `DebugSession` 与 `--debug-cli`，随后接入本地 Node 服务和浏览器前端，最后补齐 README、状态文档和验证门禁。

**技术栈：** C++17、GNU Make、Node.js 内置 `node:test`、原生 HTML / CSS / ESM。

## 关联文档

- 来源设计：
  - [design/debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)
- 目标状态：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)

---

## 参考文档

- [debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)
- [pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)
- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
- [docs/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md)
- [readme.md](/home/liangjiaqi/projects/my_visual_CPU/readme.md)

## 文件结构

### 新增文件

- `myCPU/src/debug/debug_snapshot.h`
  统一调试快照值对象。
- `myCPU/src/debug/debug_session.h`
  调试会话声明。
- `myCPU/src/debug/debug_session.cpp`
  调试会话实现。
- `myCPU/src/debug/debug_protocol.h`
  CLI 协议入口声明。
- `myCPU/src/debug/debug_protocol.cpp`
  JSON line 命令解析与输出。
- `myCPU/tests/host/debug_cli_smoke.cpp`
  最小 host-side debug CLI smoke。
- `frontend/package.json`
  Node 测试入口。
- `frontend/README.md`
  前端本地运行说明。
- `frontend/app/*`
  浏览器端页面、状态与渲染。
- `frontend/server/*`
  本地 HTTP / WebSocket 服务与测试清单。
- `frontend/tests/*`
  Node 内置测试。

### 重点修改文件

- `myCPU/src/exec/backend.h`
  补 `debug_snapshot()` 最小只读接口。
- `myCPU/src/exec/functional_backend.*`
  返回 functional 调试快照。
- `myCPU/src/exec/pipeline_backend.*`
  导出 stage 文本与 pipeline flags。
- `myCPU/src/platform/machine.*`
  暴露 debug session 所需只读 getter 与 reset helper。
- `myCPU/src/mem/bus.*`
  记录最近一次总线访问。
- `myCPU/src/devices/uart16550.*`
  暴露 UART buffer / mirror 开关。
- `myCPU/src/devices/clint.*`
  暴露 `mtimecmp()` / timer pending 状态。
- `myCPU/src/devices/plic.*`
  暴露 source / context 状态 helper。
- `myCPU/src/devices/simple_storage.*`
  暴露 storage 状态与清理镜像 helper。
- `myCPU/src/main.cpp`
  新增 `--debug-cli`。
- `myCPU/Makefile`
  接 debug 源文件与 `test-host-debug_cli_smoke`。
- `myCPU/AGENTS.md`
  更新 debug/frontend 已接入后的实现基线与验证门禁。
- `readme.md`
  补本地演示运行方式。

## 任务 1：补齐最小调试快照与 introspection

**文件：**

- 创建：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/exec/backend.h`
- 修改：`myCPU/src/exec/functional_backend.h`
- 修改：`myCPU/src/exec/functional_backend.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/src/platform/machine.h`
- 修改：`myCPU/src/platform/machine.cpp`
- 修改：`myCPU/src/mem/bus.h`
- 修改：`myCPU/src/mem/bus.cpp`
- 修改：`myCPU/src/devices/uart16550.h`
- 修改：`myCPU/src/devices/uart16550.cpp`
- 修改：`myCPU/src/devices/clint.h`
- 修改：`myCPU/src/devices/plic.h`
- 修改：`myCPU/src/devices/simple_storage.h`

- [x] **步骤 1：先写一个失败的 host smoke，要求能读到 backend / pipeline / bus / device 快照**

- [x] **步骤 2：运行该 smoke，确认因缺少调试接口而失败**

- [x] **步骤 3：补 `DebugSnapshot`、backend `debug_snapshot()`、`Machine` getter、`Bus::last_access()` 与设备 helper**

- [x] **步骤 4：重新运行 smoke，确认通过**

- [x] **步骤 5：Commit**

## 任务 2：接入 `DebugSession`、`debug_protocol` 与 `--debug-cli`

**文件：**

- 创建：`myCPU/src/debug/debug_session.h`
- 创建：`myCPU/src/debug/debug_session.cpp`
- 创建：`myCPU/src/debug/debug_protocol.h`
- 创建：`myCPU/src/debug/debug_protocol.cpp`
- 创建：`myCPU/tests/host/debug_cli_smoke.cpp`
- 修改：`myCPU/src/main.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：先写 `debug_cli_smoke.cpp`，覆盖 `load`、`snapshot`、`step_cycle`、`step_commit`、`reset` 基本路径**

- [x] **步骤 2：运行 `cd myCPU && make test-host-debug_cli_smoke`，确认失败**

- [x] **步骤 3：实现 `DebugSession` 与 `run_debug_cli()`，并在 `main.cpp` 接入 `--debug-cli`**

- [x] **步骤 4：重新运行 `cd myCPU && make test-host-debug_cli_smoke`，确认通过**

- [x] **步骤 5：Commit**

## 任务 3：接入前端服务、页面和 Node tests

**文件：**

- 创建：`frontend/package.json`
- 创建：`frontend/README.md`
- 创建：`frontend/app/index.html`
- 创建：`frontend/app/app.js`
- 创建：`frontend/app/render.js`
- 创建：`frontend/app/state.js`
- 创建：`frontend/app/api.js`
- 创建：`frontend/app/components/pipeline.js`
- 创建：`frontend/app/components/panels.js`
- 创建：`frontend/app/styles.css`
- 创建：`frontend/server/debug_server.mjs`
- 创建：`frontend/server/tests_manifest.mjs`
- 创建：`frontend/server/ws.mjs`
- 创建：`frontend/tests/debug_server.test.mjs`
- 创建：`frontend/tests/ui_state.test.mjs`

- [x] **步骤 1：先写 Node tests，守住 tests manifest、session API 和前端纯状态逻辑**

- [x] **步骤 2：运行 `cd frontend && node --test`，确认失败**

- [x] **步骤 3：接入前端文件，按当前主线测试清单补齐 `kernel_alpha` 系列 demo**

- [x] **步骤 4：重新运行 `cd frontend && node --test`，确认通过**

- [x] **步骤 5：Commit**

## 任务 4：补文档并跑完整验证

**文件：**

- 修改：`myCPU/AGENTS.md`
- 修改：`readme.md`
- 修改：`docs/status/code_self_review_status.md`
- 修改：`docs/status/kernel_alpha_status.md`

- [x] **步骤 1：更新实现基线、调试前端运行方式与当前状态说明**

- [x] **步骤 2：运行 `cd myCPU && make test-host-debug_cli_smoke`**

- [x] **步骤 3：运行 `cd frontend && node --test`**

- [x] **步骤 4：运行 `cd myCPU && make test-pipeline`**

- [x] **步骤 5：运行 `cd myCPU && make test`**

- [x] **步骤 6：Commit**
