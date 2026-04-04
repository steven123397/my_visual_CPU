import {
  applyTerminalChunk,
  createTerminalProjectionState,
  DEFAULT_TERMINAL_MAX_LENGTH,
  resetTerminalProjectionState,
} from '../shared/terminal_projection.mjs';
import { normalizeCliResponse } from './debug_cli_session.mjs';

class DebugServerRuntimeError extends Error {
  constructor(message, statusCode = 500) {
    super(message);
    this.name = 'DebugServerRuntimeError';
    this.statusCode = statusCode;
  }
}

export class StaleSessionError extends DebugServerRuntimeError {
  constructor(message = 'stale session action') {
    super(message, 409);
    this.name = 'StaleSessionError';
  }
}

export function createDebugServerRuntime({
  createSession,
  wsHub,
} = {}) {
  let currentSession = null;
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

  function enqueueSessionAction(action) {
    const queued = sessionActionQueue.then(action, action);
    sessionActionQueue = queued.catch(() => {});
    return queued;
  }

  function beginSessionGeneration() {
    currentGeneration += 1;
    return currentGeneration;
  }

  function assertGeneration(generation) {
    if (generation !== currentGeneration) {
      throw new StaleSessionError();
    }
  }

  function stopRunLoop() {
    if (runTimer) {
      clearTimeout(runTimer);
      runTimer = null;
    }
    runLoopToken += 1;
  }

  function requireSessionLoaded() {
    if (!currentSession) {
      throw new DebugServerRuntimeError('session not loaded', 400);
    }
  }

  function resetTerminalTracking() {
    resetTerminalProjectionState(currentTerminalProjection);
    currentTerminalOffset = 0;
  }

  async function readTerminalOutput(offset = currentTerminalOffset, generation = currentGeneration) {
    requireSessionLoaded();
    const chunk = normalizeCliResponse(await currentSession.uartOutput(offset), 'session uartOutput');
    assertGeneration(generation);
    return {
      text: chunk.text ?? '',
      nextOffset: chunk.nextOffset ?? chunk.next_offset ?? offset,
    };
  }

  async function callSession(method, ...args) {
    requireSessionLoaded();
    const response = await currentSession[method](...args);
    return normalizeCliResponse(response, `session ${method}`);
  }

  function trackTerminalChunk(chunk, { reset = false } = {}) {
    const normalized = {
      type: 'terminal',
      text: chunk.text ?? '',
      nextOffset: chunk.nextOffset ?? currentTerminalOffset,
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

  async function advanceUntilTerminalActivity({
    maxCommits = 4096,
    settleCommits = 256,
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
    }

    return {
      commits,
      terminal: aggregate,
    };
  }

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

    if (text.includes('\r') || text.includes('\n')) {
      return {
        maxCommits: 16384,
        settleCommits: 1024,
        idleCommitsWithoutOutput: 128,
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
        maxCommits: Math.max(1024, (visibleAsciiCount + controlCount) * 1024),
        settleCommits: 64,
        idleCommitsWithoutOutput: 64,
      };
    }

    if (visibleAsciiCount > 0) {
      return {
        maxCommits: Math.max(2048, (visibleAsciiCount + controlCount) * 1024),
        settleCommits: 256,
        idleCommitsWithoutOutput: 64,
        shouldStop: ({ aggregate }) => aggregate.text.length >= visibleAsciiCount,
      };
    }
  }

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
            if (currentSnapshot.summary?.halted) {
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

        if (token === runLoopToken && generation === currentGeneration && !currentSnapshot?.summary?.halted) {
          scheduleNextTick();
        }
      }, intervalMs);
    };

    scheduleNextTick();
  }

  function runQueued(action) {
    return enqueueSessionAction(action);
  }

  return {
    async load(entry, backend = 'pipeline') {
      const generation = beginSessionGeneration();
      stopRunLoop();
      return runQueued(async () => {
        assertGeneration(generation);
        if (currentSession) {
          await currentSession.close();
          assertGeneration(generation);
        }
        currentSession = await createSession();
        currentTerminalPrompt = entry.terminalPrompt ?? null;
        await callSession('load', entry, backend);
        assertGeneration(generation);
        currentSnapshot = entry.bootUntilUartText
          ? await callSession(
              'runUntilUartContains',
              entry.bootUntilUartText,
              entry.bootMaxSteps ?? 0,
            )
          : await callSession('snapshot');
        assertGeneration(generation);
        resetTerminalTracking();
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({
          offset: 0,
          reset: true,
          broadcast: true,
          generation,
        });
        return { ok: true, snapshot: currentSnapshot, terminal };
      });
    },

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

    async reset() {
      const generation = beginSessionGeneration();
      stopRunLoop();
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        currentSnapshot = await callSession('reset');
        assertGeneration(generation);
        resetTerminalTracking();
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

    async run(rateHz = 8) {
      const intervalMs = Math.max(20, Math.floor(1000 / Math.max(1, rateHz)));
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        startRunLoop(intervalMs, generation);
        return { ok: true };
      });
    },

    async terminalInput(text = '') {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        await callSession('uartInput', text);
        assertGeneration(generation);
        const advancePlan = buildTerminalAdvancePlan(text);
        let terminal;
        let shouldBroadcastSnapshot = false;
        if (runTimer) {
          terminal = await syncTerminalDelta({ broadcast: true, generation });
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

    async terminalOutput(offset = 0) {
      const generation = currentGeneration;
      return runQueued(async () => {
        assertGeneration(generation);
        requireSessionLoaded();
        return readTerminalOutput(offset, generation);
      });
    },

    async pause() {
      stopRunLoop();
      return runQueued(async () => ({ ok: true, snapshot: currentSnapshot }));
    },

    async close() {
      stopRunLoop();
      if (currentSession) {
        await currentSession.close();
        currentSession = null;
      }
    },
  };
}
