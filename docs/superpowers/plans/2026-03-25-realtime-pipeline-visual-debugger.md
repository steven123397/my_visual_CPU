# 实时流水线可视化调试前端实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 为当前模拟器新增一个可本地运行的实时调试前端，能够加载现有测试、驱动 `pipeline` 后端执行，并在浏览器中可视化流水线、寄存器、CSR、总线访问和设备状态。

**架构：** 采用三层结构：浏览器前端、Node 本地调试服务、C++ 调试会话。Node 服务负责 HTTP/WS 协议、测试清单和运行控制；C++ 调试会话负责单步执行、快照采集和 JSON 协议；前端只负责渲染和交互。

**技术栈：** C++17、现有 `myCPU` 构建系统、Node.js 内置 `http`/`child_process`、原生 HTML/CSS/ES 模块前端、shell/host smoke tests

---

### 任务 1：补出调试快照与单步会话核心

**文件：**
- 创建：`myCPU/src/debug/debug_snapshot.h`
- 创建：`myCPU/src/debug/debug_session.h`
- 创建：`myCPU/src/debug/debug_session.cpp`
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
- 测试：`myCPU/tests/host/debug_session_smoke.cpp`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：编写失败的 host 测试**

```cpp
int main() {
    DebugSession session;
    session.load_elf("tests/asm/hello.elf", BackendKind::Pipeline, nullptr);

    const DebugSnapshot before = session.snapshot();
    session.step_cycle();
    const DebugSnapshot after = session.snapshot();

    expect(after.summary.cycle == before.summary.cycle + 1, "step_cycle should advance cycle");
    expect(after.pipeline.if_stage.valid || after.pipeline.id_stage.valid, "pipeline snapshot should expose inflight stages");

    while (!session.snapshot().summary.halted) {
        session.step_commit();
    }

    const DebugSnapshot halted = session.snapshot();
    expect(halted.summary.halted, "step_commit loop should reach halt");
    expect(!halted.events.empty(), "debug snapshots should include recent events");
}
```

- [ ] **步骤 2：运行测试验证失败**

运行：`cd myCPU && make test-host-debug_session`
预期：FAIL，报错缺少 `DebugSession` / `DebugSnapshot` 或对应接口

- [ ] **步骤 3：编写最少实现代码**

实现要点：
- 为 `ExecutionBackend` 增加只读调试快照接口，默认可返回空的阶段数据
- 为 `PipelineBackend` 暴露阶段寄存器、最近的 `stall` / `redirect` / `trap` / `commit` 事件
- 为 `Machine` 增加：
  - `step()`
  - `loaded()`
  - `cpu() / bus() / uart() / clint() / plic() / storage()`
- 为 `Bus` 记录最近一次访存/MMIO
- 为设备对象补只读状态导出接口
- 新增 `DebugSession`：
  - 负责加载 ELF
  - 负责 `step_cycle()` / `step_commit()`
  - 负责组装统一快照

- [ ] **步骤 4：运行测试验证通过**

运行：`cd myCPU && make test-host-debug_session`
预期：PASS

- [ ] **步骤 5：回归验证**

运行：`cd myCPU && make test-host-pipeline_backend test-host-backend_differential`
预期：PASS

### 任务 2：增加 CLI 调试协议与测试清单

**文件：**
- 创建：`myCPU/src/debug/debug_protocol.h`
- 创建：`myCPU/src/debug/debug_protocol.cpp`
- 修改：`myCPU/src/main.cpp`
- 修改：`myCPU/src/platform/machine.h`
- 修改：`myCPU/src/platform/machine.cpp`
- 创建：`myCPU/tests/host/debug_cli.sh`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：编写失败的协议测试**

```sh
printf '%s\n' \
  '{"cmd":"load","image":"tests/asm/hello.elf","backend":"pipeline"}' \
  '{"cmd":"snapshot"}' \
  '{"cmd":"step_cycle"}' \
  '{"cmd":"snapshot"}' \
  '{"cmd":"quit"}' \
  | ./mycpu --debug-cli
```

校验点：
- 第一条 `snapshot` 返回 JSON，含 `summary.cycle = 0`
- 第二条 `snapshot` 返回 JSON，含 `summary.cycle = 1`
- 返回里包含 `pipeline`、`devices`、`bus` 字段

- [ ] **步骤 2：运行测试验证失败**

运行：`cd myCPU && sh tests/host/debug_cli.sh mycpu`
预期：FAIL，`--debug-cli` 未实现

- [ ] **步骤 3：实现最少 CLI 协议**

协议约束：
- 输入：每行一个 JSON 命令
- 输出：每行一个 JSON 响应
- 首版命令只实现：
  - `load`
  - `snapshot`
  - `step_cycle`
  - `step_commit`
  - `reset`
  - `quit`
- 解析不依赖第三方 JSON 库，允许实现受控的最小 JSON 读写器

- [ ] **步骤 4：运行测试验证通过**

运行：`cd myCPU && sh tests/host/debug_cli.sh mycpu`
预期：PASS

- [ ] **步骤 5：基础回归**

运行：`cd myCPU && make test-backend-cli test-host-debug_session`
预期：PASS

### 任务 3：实现 Node 本地调试服务

**文件：**
- 创建：`frontend/server/debug_server.mjs`
- 创建：`frontend/server/tests_manifest.mjs`
- 创建：`frontend/server/ws.mjs`
- 创建：`frontend/tests/debug_server.test.mjs`
- 创建：`frontend/package.json`

- [ ] **步骤 1：编写失败的 Node 服务测试**

```js
test('GET /api/tests returns built-in test manifest', async () => {
  const server = await startServer({ port: 0, spawnBackend: fakeBackend });
  const response = await fetch(`${server.baseUrl}/api/tests`);
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.ok(body.tests.some((item) => item.name === 'hello'));
});

test('POST /api/session/step-cycle returns updated snapshot', async () => {
  const server = await startServer({ port: 0, spawnBackend: fakeBackend });
  await post(server, '/api/session/load', { test: 'hello', backend: 'pipeline' });
  const response = await post(server, '/api/session/step-cycle', {});
  assert.equal(response.snapshot.summary.cycle, 1);
});
```

- [ ] **步骤 2：运行测试验证失败**

运行：`node --test frontend/tests/debug_server.test.mjs`
预期：FAIL，服务模块不存在

- [ ] **步骤 3：实现最少 Node 服务**

实现要点：
- 使用 Node 内置模块：
  - `http`
  - `fs`
  - `path`
  - `child_process`
  - `crypto`
- 提供：
  - `GET /api/tests`
  - `POST /api/session/load`
  - `POST /api/session/snapshot`
  - `POST /api/session/step-cycle`
  - `POST /api/session/step-commit`
  - `POST /api/session/run`
  - `POST /api/session/pause`
  - `POST /api/session/reset`
  - `GET /ws`
- `run/pause` 在 Node 层用定时器驱动反复 `step_cycle`
- 静态服务 `frontend/app/*`

- [ ] **步骤 4：运行测试验证通过**

运行：`node --test frontend/tests/debug_server.test.mjs`
预期：PASS

### 任务 4：实现静态前端工作台

**文件：**
- 创建：`frontend/app/index.html`
- 创建：`frontend/app/styles.css`
- 创建：`frontend/app/app.js`
- 创建：`frontend/app/state.js`
- 创建：`frontend/app/render.js`
- 创建：`frontend/app/api.js`
- 创建：`frontend/app/components/pipeline.js`
- 创建：`frontend/app/components/panels.js`

- [ ] **步骤 1：编写失败的前端纯逻辑测试**

```js
test('buildTimelineRows highlights stalls and redirects', () => {
  const rows = buildTimelineRows([
    { cycle: 1, pipeline: { flags: { stalled: true, redirected: false } } },
    { cycle: 2, pipeline: { flags: { stalled: false, redirected: true } } },
  ]);

  assert.equal(rows[0].flag, 'stall');
  assert.equal(rows[1].flag, 'redirect');
});
```

- [ ] **步骤 2：运行测试验证失败**

运行：`node --test frontend/tests/ui_state.test.mjs`
预期：FAIL，前端状态/渲染模块不存在

- [ ] **步骤 3：实现最少前端**

界面必须包含：
- 顶部控制栏：
  - 测试选择
  - 后端选择
  - `Load / Run / Pause / Step Cycle / Step Commit / Reset`
- 主流水线区：
  - 5 级流水线当前状态
  - 最近 N 拍时间线
- 右侧面板：
  - 事件流
  - 设备状态
  - 运行摘要
- 底部状态区：
  - GPR
  - CSR / Trap
  - 最近一次总线访问

视觉要求：
- 使用明确的色彩分层
- 页面背景不使用纯平单色
- 支持桌面和窄屏布局

- [ ] **步骤 4：运行测试验证通过**

运行：`node --test frontend/tests/ui_state.test.mjs`
预期：PASS

- [ ] **步骤 5：端到端联调验证**

运行：
- `cd myCPU && make`
- `node frontend/server/debug_server.mjs`
- 在浏览器打开本地地址，加载 `hello` 或 `timer_interrupt`

预期：
- 前端可以连上服务
- 可以成功 `Load`
- `Step Cycle` 会推进 `cycle`
- `Step Commit` 会推进到下一个提交边界
- 页面能显示流水线、寄存器、CSR、设备状态

### 任务 5：文档与启动方式收尾

**文件：**
- 修改：`README.md`
- 创建：`frontend/README.md`

- [ ] **步骤 1：补启动说明**

补充：
- 如何构建 `myCPU/mycpu`
- 如何启动 `frontend/server/debug_server.mjs`
- 默认访问地址
- 当前首版限制

- [ ] **步骤 2：验证命令**

运行：
- `cd myCPU && make`
- `node --test frontend/tests/debug_server.test.mjs frontend/tests/ui_state.test.mjs`

预期：PASS

- [ ] **步骤 3：最终回归**

运行：`cd myCPU && make test-host-debug_session test-backend-cli`
预期：PASS
