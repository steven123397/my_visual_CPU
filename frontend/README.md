# 前端调试器

当前这套前端 / Node 服务已经正式接入主线，但范围仍限定在“教学演示可用”的最小集合，不是通用调试器。

## 目录说明

- `app/`
  - 浏览器端静态页面与渲染逻辑
- `server/`
  - 本地调试服务、测试清单、WebSocket 广播
- `tests/`
  - Node 内置测试

## 启动方式

先构建模拟器：

```bash
cd myCPU
make
```

再回到仓库根目录启动本地服务：

```bash
node frontend/server/debug_server.mjs
```

默认地址：

```text
http://127.0.0.1:4173
```

## 当前首版能力

- 选择仓库内现有 asm、`guest_supervisor_demo` 和 `kernel_alpha` 正负 demo 并加载
- 直接选择 `guest_vector_demo` 与 `guest_vector_cnn_demo`
- 切换 `pipeline` / `functional` backend
- `Load / Run / Pause / Step Cycle / Step Commit / Reset`
- 实时查看：
  - 当前 workload 说明卡
  - 五级流水线当前状态
  - 向量指令在 pipeline / timeline 里的 `config / memory / ALU` 高亮
  - 最近周期时间线
  - `Vector State`：`SEW / VL` 与 `v0..v31` 的最小寄存器 dump / diff
  - 固定 `conv -> relu` demo 的专题卡
  - 寄存器变化
  - 关键 CSR / Trap 状态
  - 最近一次总线访问
  - UART / CLINT / PLIC / Storage 状态

## 当前限制

- `Run` 目前由 Node 服务按定时器重复发送 `step_cycle`
- WebSocket 只用于服务端向浏览器推送快照
- 首版没有断点、条件暂停、差分对比和任意文件加载
- 向量 / CNN 可视化当前只服务于已经落地的固定 demo，不是通用模型可视化器

## 验证

```bash
cd frontend
node --test
```
