# 前端调试器

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

- 选择仓库内现有测试并加载
- 切换 `pipeline` / `functional` backend
- `Load / Run / Pause / Step Cycle / Step Commit / Reset`
- 实时查看：
  - 五级流水线当前状态
  - 最近周期时间线
  - 寄存器变化
  - 关键 CSR / Trap 状态
  - 最近一次总线访问
  - UART / CLINT / PLIC / Storage 状态

## 当前限制

- `Run` 目前由 Node 服务按定时器重复发送 `step_cycle`
- WebSocket 只用于服务端向浏览器推送快照
- 首版没有断点、条件暂停、差分对比和任意文件加载
