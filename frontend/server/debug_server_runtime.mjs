// 调试服务器运行时：管理 debug CLI 会话生命周期、run loop、终端 I/O 投影、
// Linux boot marker 等待与 session 代次状态机，供 debug_server.mjs 调用。
import {
  applyTerminalChunk,
  createTerminalProjectionState,
  DEFAULT_TERMINAL_MAX_LENGTH,
  resetTerminalProjectionState,
} from '../shared/terminal_projection.mjs';
import {
  DEFAULT_DEBUG_RUN_RATE_HZ,
  terminalLimits,
} from './debug_budget.mjs';
import { normalizeCliResponse } from './debug_cli_session.mjs';

// 运行时错误基类，携带 HTTP 状态码。
class DebugServerRuntimeError extends Error {
  constructor(message, statusCode = 500) {
    super(message);
    this.name = 'DebugServerRuntimeError';
    this.statusCode = statusCode;
  }
}

// 过期会话错误（代次不一致），固定状态码 409。
export class StaleSessionError extends DebugServerRuntimeError {
  constructor(message = 'stale session action') {
    super(message, 409);
    this.name = 'StaleSessionError';
  }
}

// 创建并返回调试服务器运行时实例（会话/run loop/终端/snapshot 全部封装在内）。
export function createDebugServerRuntime({
  createSession,
  wsHub,
} = {}) {
  let currentSession = null;
  let currentEntry = null;
  let currentBackend = 'pipeline';
  let currentSnapshot = null;
  let currentTerminalPrompt = null;
  let runTimer = null;
  let currentTerminalProjection = createTerminalProjectionState({
    maxLength: DEFAULT_TERMINAL_MAX_LENGTH,
  });
  let currentTerminalOffset = 0;
  let currentGeneration = 0;
  let sessionActionQueue = Promise.resolve();
  let runLoopToken = 0;

  // 串行化会话动作到队列执行，避免并发请求互相覆盖。
  function enqueueSessionAction(action) {
    const queued = sessionActionQueue.then(action, action);
    sessionActionQueue = queued.catch(() => {});
    return queued;
  }

  // 递增会话代次并返回新代次（用于失效旧请求）。
  function beginSessionGeneration() {
    currentGeneration += 1;
    return currentGeneration;
  }

  // 校验代次一致，否则抛 StaleSessionError。
  function assertGeneration(generation) {
    if (generation !== currentGeneration) {
      throw new StaleSessionError();
    }
  }

  // 停止 run loop 定时器并失效令牌。
  function stopRunLoop() {
    if (runTimer) {
      clearTimeout(runTimer);
      runTimer = null;
    }
    runLoopToken += 1;
  }

  // 校验会话已加载，否则抛 400。
  function requireSessionLoaded() {
    if (!currentSession) {
      throw new DebugServerRuntimeError('session not loaded', 400);
    }
  }

  // 重置终端投影与偏移跟踪。
  function resetTerminalTracking() {
    resetTerminalProjectionState(currentTerminalProjection);
    currentTerminalOffset = 0;
  }

  // 构造终端重置广播空消息。
  function buildTerminalResetMessage() {
    return {
      type: 'terminal',
      text: '',
      nextOffset: 0,
      reset: true,
    };
  }

  // 判断快照是否含断点事件。
  function snapshotHasBreakpoint(snapshot) {
    return Array.isArray(snapshot?.events) &&
      snapshot.events.some((event) => event?.kind === 'breakpoint');
  }

  // 读取会话 UART 输出并校验代次。
  async function readTerminalOutput(offset = currentTerminalOffset, generation = currentGeneration) {
    requireSessionLoaded();
    const chunk = normalizeCliResponse(await currentSession.uartOutput(offset), 'session uartOutput');
    assertGeneration(generation);
    return {
      text: chunk.text ?? '',
      nextOffset: chunk.nextOffset ?? chunk.next_offset ?? offset,
    };
  }

  // 调用会话方法并规范化响应。
  async function callSession(method, ...args) {
    requireSessionLoaded();
    const response = await currentSession[method](...args);
    return normalizeCliResponse(response, `session ${method}`);
  }

  // 归一化终端块并更新投影与偏移。
  function trackTerminalChunk(chunk, { reset = false } = {}) {
    const normalized = {
      type: 'terminal',
      text: chunk.text ?? '',
      nextOffset: chunk.nextOffset ?? chunk.next_offset ?? currentTerminalOffset,
      reset,
    };

    if (reset) {
      resetTerminalProjectionState(currentTerminalProjection);
      applyTerminalChunk(currentTerminalProjection, normalized.text);
    } else if (normalized.nextOffset > currentTerminalOffset) {
      applyTerminalChunk(currentTerminalProjection, normalized.text);
    }
    currentTerminalOffset = normalized.nextOffset;
    return normalized;
  }

  // 同步终端增量并按需广播。
  async function syncTerminalDelta({
    offset = currentTerminalOffset,
    reset = false,
    broadcast = false,
    generation = currentGeneration,
  } = {}) {
    const message = trackTerminalChunk(await readTerminalOutput(offset, generation), { reset });
    if (broadcast && (reset || message.text.length > 0)) {
      wsHub.broadcast(message);
    }
    return message;
  }

  // 步进提交直到终端有输出或运行停止。
  async function advanceUntilTerminalActivity({
    maxCommits = terminalLimits.stepCommitBudget,
    settleCommits = terminalLimits.ascii.settleCommits,
    idleCommitsWithoutOutput = null,
    shouldStop = null,
    generation = currentGeneration,
  } = {}) {
    const aggregate = {
      type: 'terminal',
      text: '',
      nextOffset: currentTerminalOffset,
      reset: false,
    };
    let quietCommits = 0;
    let sawOutput = false;
    let commits = 0;

    for (let i = 0; i < maxCommits; ++i) {
      assertGeneration(generation);
      currentSnapshot = await currentSession.stepCommit();
      assertGeneration(generation);
      commits += 1;

      const terminal = await syncTerminalDelta({ broadcast: false, generation });
      if (terminal.text.length > 0) {
        aggregate.text += terminal.text;
        aggregate.nextOffset = terminal.nextOffset;
        sawOutput = true;
        quietCommits = 0;
        if (shouldStop?.({ aggregate, currentSnapshot, currentTerminalBuffer: currentTerminalProjection.text })) {
          break;
        }
      } else {
        quietCommits += 1;
        if (sawOutput) {
          if (quietCommits >= settleCommits) {
            break;
          }
        } else if (idleCommitsWithoutOutput != null &&
                   quietCommits >= idleCommitsWithoutOutput) {
          break;
        }
      }

      if (currentSnapshot.summary?.halted) {
        break;
      }
      if (snapshotHasBreakpoint(currentSnapshot)) {
        break;
      }
    }

    return {
      commits,
      terminal: aggregate,
    };
  }

  // 依据输入文本生成终端推进计划。
  function buildTerminalAdvancePlan(text) {
    if (!text) {
      return null;
    }

    let visibleAsciiCount = 0;
    let controlCount = 0;

    for (const char of text) {
      if (char === '\r' || char === '\n') {
        controlCount += 1;
        continue;
      }
      if (char === '\b' || char === '\x7f') {
        controlCount += 1;
        continue;
      }
      if (/^[\x20-\x7e]$/.test(char)) {
        visibleAsciiCount += 1;
      }
    }

    if (visibleAsciiCount === 0 && controlCount === 0) {
      return null;
    }

    const { newline, control, ascii, textCommitScale } = terminalLimits;

    if (text.includes('\r') || text.includes('\n')) {
      return {
        maxCommits: newline.maxCommits,
        settleCommits: newline.settleCommits,
        idleCommitsWithoutOutput: newline.idleCommitsWithoutOutput,
        shouldStop: ({ aggregate, currentSnapshot, currentTerminalBuffer: terminalBuffer }) =>
          aggregate.text.length > 0
          && (
            currentSnapshot.summary?.halted
            || (currentTerminalPrompt && terminalBuffer.endsWith(currentTerminalPrompt))
          ),
      };
    }

    if (controlCount > 0) {
      return {
        maxCommits: Math.max(control.minMaxCommits, (visibleAsciiCount + controlCount) * textCommitScale),
        settleCommits: control.settleCommits,
        idleCommitsWithoutOutput: control.idleCommitsWithoutOutput,
      };
    }

    if (visibleAsciiCount > 0) {
      return {
        maxCommits: Math.max(ascii.minMaxCommits, (visibleAsciiCount + controlCount) * textCommitScale),
        settleCommits: ascii.settleCommits,
        idleCommitsWithoutOutput: ascii.idleCommitsWithoutOutput,
        shouldStop: ({ aggregate }) => aggregate.text.length >= visibleAsciiCount,
      };
    }
  }

  // 生成命令等待计划直到匹配文本。
  function buildTerminalCommandWaitPlan(text) {
    if (!currentEntry?.commandUntilUartText) {
      return null;
    }
    if (!text.includes('\r') && !text.includes('\n')) {
      return null;
    }
    return {
      text: currentEntry.commandUntilUartText,
      maxSteps: currentEntry.commandMaxSteps ?? 0,
      timeoutMs: currentEntry.commandRequestTimeoutMs,
    };
  }

  // 启动周期性步进 run loop 定时器。
  function startRunLoop(intervalMs, generation) {
    stopRunLoop();
    const token = runLoopToken;

    const scheduleNextTick = () => {
      if (token !== runLoopToken || generation !== currentGeneration || !currentSession) {
        return;
      }

      runTimer = setTimeout(async () => {
        runTimer = null;
        try {
          await enqueueSessionAction(async () => {
            assertGeneration(generation);
            if (!currentSession || token !== runLoopToken) {
              return;
            }

            currentSnapshot = await callSession('stepCycle');
            assertGeneration(generation);
            wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
            await syncTerminalDelta({ broadcast: true, generation });
            if (currentSnapshot.summary?.halted || snapshotHasBreakpoint(currentSnapshot)) {
              stopRunLoop();
            }
          });
        } catch (error) {
          if (error instanceof StaleSessionError) {
            return;
          }
          wsHub.broadcast({ type: 'error', message: error.message });
          stopRunLoop();
          return;
        }

        if (
          token === runLoopToken &&
          generation === currentGeneration &&
          !currentSnapshot?.summary?.halted &&
          !snapshotHasBreakpoint(currentSnapshot)
        ) {
          scheduleNextTick();
        }
      }, intervalMs);
    };

    scheduleNextTick();
  }

  // 通过会话动作队列执行并返回 Promise。
  function runQueued(action) {
    return enqueueSessionAction(action);
  }

  // 加载会话并初始化快照与终端（含 Linux boot marker 等待）。
  async function initializeSessionState(session, entry, backend) {
    const terminalPrompt = entry.terminalPrompt ?? null;
    await normalizeCliResponse(await session.load(entry, backend), 'session load');
    for (const payload of entry.payloads ?? []) {
      await normalizeCliResponse(
        await session.loadPayload(payload.image, payload.addr),
        'session loadPayload',
      );
    }
    for (const seed of entry.gprSeeds ?? []) {
      await normalizeCliResponse(
        await session.setGpr(seed.reg, seed.value),
        'session setGpr',
      );
    }
    const snapshot = entry.bootUntilUartText
      ? normalizeCliResponse(
          await session.runUntilUartContains(
            entry.bootUntilUartText,
            entry.bootMaxSteps ?? 0,
            { timeoutMs: entry.bootRequestTimeoutMs },
          ),
          'session runUntilUartContains',
        )
      : normalizeCliResponse(await session.snapshot(), 'session snapshot');
    const terminalChunk = normalizeCliResponse(
      await session.uartOutput(0),
      'session uartOutput',
    );
    const terminalProjection = createTerminalProjectionState({
      maxLength: DEFAULT_TERMINAL_MAX_LENGTH,
    });
    const terminal = {
      type: 'terminal',
      text: terminalChunk.text ?? '',
      nextOffset: terminalChunk.nextOffset ?? terminalChunk.next_offset ?? 0,
      reset: true,
    };

    resetTerminalProjectionState(terminalProjection);
    applyTerminalChunk(terminalProjection, terminal.text);

    return {
      session,
      snapshot,
      terminalPrompt,
      terminalProjection,
      terminalOffset: terminal.nextOffset,
      terminal,
    };
  }

  // 复位后等待 boot marker 重新武装会话。
  async function rearmSessionStateAfterReset(session, entry) {
    const snapshot = entry.bootUntilUartText
      ? normalizeCliResponse(
          await session.runUntilUartContains(
            entry.bootUntilUartText,
            entry.bootMaxSteps ?? 0,
            { timeoutMs: entry.bootRequestTimeoutMs },
          ),
          'session runUntilUartContains',
        )
      : normalizeCliResponse(await session.snapshot(), 'session snapshot');
    const terminalChunk = normalizeCliResponse(
      await session.uartOutput(0),
      'session uartOutput',
    );
    const terminal = {
      type: 'terminal',
      text: terminalChunk.text ?? '',
      nextOffset: terminalChunk.nextOffset ?? terminalChunk.next_offset ?? 0,
      reset: true,
    };
    return {
      snapshot,
      terminal,
      terminalOffset: terminal.nextOffset,
    };
  }

  // 判断复位后是否需重新武装会话（Linux console 等带 payload 入口）。
  function shouldRearmSessionOnReset(entry) {
    return (entry?.payloads?.length ?? 0) > 0
      || (entry?.gprSeeds?.length ?? 0) > 0
      || Boolean(entry?.bootUntilUartText);
  }

  return {
    // 创建并加载新会话替换旧会话。
    async load(entry, backend = 'pipeline') {
      const generation = beginSessionGeneration();
      stopRunLoop();
      return runQueued(async () => {
        let nextState = null;
        let nextSession = null;
        const previousSession = currentSession;

        assertGeneration(generation);
        try {
          nextSession = await createSession();
          nextState = await initializeSessionState(nextSession, entry, backend);
          assertGeneration(generation);
        } catch (error) {
          if (nextSession) {
            try {
              await nextSession.close();
            } catch {}
          }
          throw error;
        }

        currentSession = nextState.session;
        currentEntry = entry;
        currentBackend = backend;
        currentSnapshot = nextState.snapshot;
        currentTerminalPrompt = nextState.terminalPrompt;
        currentTerminalProjection = nextState.terminalProjection;
        currentTerminalOffset = nextState.terminalOffset;

        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        wsHub.broadcast(nextState.terminal);

        if (previousSession) {
          try {
            await previousSession.close();
          } catch {}
          assertGeneration(generation);
        }

        return { ok: true, snapshot: currentSnapshot, terminal: nextState.terminal };
      });
    },

    // 请求并返回当前快照。
    async snapshot() {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        currentSnapshot = await callSession('snapshot');
        assertGeneration(generation);
        return { snapshot: currentSnapshot };
      });
    },

    // 单周期步进并广播快照与终端。
    async stepCycle() {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        currentSnapshot = await callSession('stepCycle');
        assertGeneration(generation);
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({ broadcast: true, generation });
        return { snapshot: currentSnapshot, terminal };
      });
    },

    // 单提交步进并广播快照与终端。
    async stepCommit() {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        currentSnapshot = await callSession('stepCommit');
        assertGeneration(generation);
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({ broadcast: true, generation });
        return { snapshot: currentSnapshot, terminal };
      });
    },

    // 分发内存/CSR/断点调试命令。
    async debugCommand(command = {}) {
      const generation = currentGeneration;
      stopRunLoop();
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        let response;
        switch (command?.cmd) {
        case 'set_memory':
          response = await callSession(
            'setMemory',
            command.addr,
            command.value,
            command.size ?? 8,
            command.virtual === true,
          );
          break;
        case 'set_csr':
          response = await callSession('setCsr', command.csr, command.value);
          break;
        case 'break_at':
          response = await callSession('breakAt', command.addr);
          break;
        default:
          throw new DebugServerRuntimeError(`unsupported debug command: ${command?.cmd ?? '<missing>'}`, 400);
        }
        currentSnapshot = await callSession('snapshot');
        assertGeneration(generation);
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        return { ok: true, response, snapshot: currentSnapshot };
      });
    },

    // 复位会话并按需重新武装终端（Linux console rearm）。
    async reset() {
      const generation = beginSessionGeneration();
      stopRunLoop();
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        currentSnapshot = await callSession('reset');
        assertGeneration(generation);
        resetTerminalTracking();
        if (shouldRearmSessionOnReset(currentEntry)) {
          const nextState = await rearmSessionStateAfterReset(currentSession, currentEntry, currentBackend);
          assertGeneration(generation);
          currentSnapshot = nextState.snapshot;
          currentTerminalOffset = nextState.terminalOffset;
          resetTerminalProjectionState(currentTerminalProjection);
          applyTerminalChunk(currentTerminalProjection, nextState.terminal.text);
          wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
          wsHub.broadcast(nextState.terminal);
          return { snapshot: currentSnapshot, terminal: nextState.terminal };
        }
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({
          offset: 0,
          reset: true,
          broadcast: true,
          generation,
        });
        return { snapshot: currentSnapshot, terminal };
      });
    },

    // 终止会话并清理全部状态。
    async terminate() {
      const generation = beginSessionGeneration();
      stopRunLoop();
      return runQueued(async () => {
        const session = currentSession;
        assertGeneration(generation);
        requireSessionLoaded();
        currentSession = null;
        currentEntry = null;
        currentBackend = 'pipeline';
        currentSnapshot = null;
        currentTerminalPrompt = null;
        resetTerminalTracking();
        const terminal = buildTerminalResetMessage();
        wsHub.broadcast(terminal);
        try {
          await session.close();
        } catch {}
        assertGeneration(generation);
        return { ok: true, snapshot: null, terminal };
      });
    },

    // 以指定频率启动 run loop。
    async run(rateHz = DEFAULT_DEBUG_RUN_RATE_HZ) {
      const intervalMs = Math.max(20, Math.floor(1000 / Math.max(1, rateHz)));
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        startRunLoop(intervalMs, generation);
        return { ok: true };
      });
    },

    // 处理终端输入并推进输出（等待 prompt settling）。
    async terminalInput(text = '') {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        await callSession('uartInput', text);
        assertGeneration(generation);
        const advancePlan = buildTerminalAdvancePlan(text);
        const commandWaitPlan = buildTerminalCommandWaitPlan(text);
        let terminal;
        let shouldBroadcastSnapshot = false;
        if (runTimer) {
          terminal = await syncTerminalDelta({ broadcast: true, generation });
        } else if (commandWaitPlan) {
          const waitedTerminal = await callSession(
            'runUntilNewUartContains',
            currentTerminalOffset,
            commandWaitPlan.text,
            commandWaitPlan.maxSteps,
            { timeoutMs: commandWaitPlan.timeoutMs },
          );
          terminal = trackTerminalChunk(waitedTerminal, { reset: false });
          currentSnapshot = await callSession('snapshot');
          wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
          if (terminal.text.length > 0) {
            wsHub.broadcast(terminal);
          }
        } else if (advancePlan) {
          const result = await advanceUntilTerminalActivity({
            ...advancePlan,
            generation,
          });
          terminal = result.terminal;
          shouldBroadcastSnapshot = result.commits > 0;
          if (shouldBroadcastSnapshot) {
            wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
          }
          if (terminal.text.length > 0) {
            wsHub.broadcast(terminal);
          }
        } else {
          terminal = await syncTerminalDelta({ broadcast: true, generation });
        }
        return {
          ok: true,
          text: terminal.text,
          nextOffset: terminal.nextOffset,
        };
      });
    },

    // 读取指定偏移起的终端输出。
    async terminalOutput(offset = 0) {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        return readTerminalOutput(offset, generation);
      });
    },

    // 调用会话 JIT 派发并返回摘要。
    async jitDispatch() {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        const summary = await callSession('jitDispatch');
        assertGeneration(generation);
        return { summary };
      });
    },

    // 停止 run loop 并返回当前快照。
    async pause() {
      stopRunLoop();
      return runQueued(async () => ({ ok: true, snapshot: currentSnapshot }));
    },

    // 停止运行时并关闭底层会话。
    async close() {
      stopRunLoop();
      if (currentSession) {
        await currentSession.close();
        currentSession = null;
      }
      currentEntry = null;
      currentBackend = 'pipeline';
    },
  };
}
