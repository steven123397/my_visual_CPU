import test from 'node:test';
import assert from 'node:assert/strict';

import { createDebugServerRuntime } from '../server/debug_server_runtime.mjs';

function makeSnapshot(cycle, label = 'session-1') {
  return {
    type: 'snapshot',
    summary: {
      cycle,
      instret: cycle,
      pc: '0x80000000',
      halted: false,
      privilege: 'S',
      backend: 'pipeline',
    },
    pipeline: {
      flags: {
        committed: cycle > 0,
        trap_flush: false,
      },
    },
    devices: {
      uart: {
        recent_output: label,
        output_size: label.length,
      },
    },
    events: [],
  };
}

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function createFakeSession({
  label = 'session-1',
  stepCycleTerminalPrefix = '',
  blockFirstStepCycle = null,
  echoInputChunkSize = null,
  loadError = null,
  snapshotError = null,
} = {}) {
  let cycle = 0;
  let terminal = `boot:${label}\r\n> `;
  let closed = false;
  let pendingEcho = '';
  let stepCycleCount = 0;
  return {
    async load() {
      if (loadError) {
        throw loadError;
      }
      cycle = 0;
      stepCycleCount = 0;
      terminal = `boot:${label}\r\n> `;
      return { ok: true };
    },
    async snapshot() {
      if (snapshotError) {
        throw snapshotError;
      }
      return makeSnapshot(cycle, label);
    },
    async stepCycle() {
      stepCycleCount += 1;
      if (blockFirstStepCycle && stepCycleCount === 1) {
        blockFirstStepCycle.onStart?.();
        await blockFirstStepCycle.wait();
      }
      cycle += 1;
      if (stepCycleTerminalPrefix) {
        terminal += `${stepCycleTerminalPrefix}${cycle}\r\n`;
      }
      return makeSnapshot(cycle, label);
    },
    async stepCommit() {
      cycle += 1;
      if (echoInputChunkSize && pendingEcho.length > 0) {
        terminal += pendingEcho.slice(0, echoInputChunkSize);
        pendingEcho = pendingEcho.slice(echoInputChunkSize);
      }
      return makeSnapshot(cycle, label);
    },
    async reset() {
      cycle = 0;
      stepCycleCount = 0;
      terminal = 'reset\r\n> ';
      return makeSnapshot(cycle, label);
    },
    async runUntilUartContains() {
      cycle = 4;
      terminal = `boot:${label}\r\nready\r\n> `;
      return makeSnapshot(cycle, label);
    },
    async uartInput(text) {
      if (echoInputChunkSize) {
        pendingEcho += text;
      } else {
        terminal += text;
      }
      return { ok: true };
    },
    async uartOutput(offset = 0) {
      return {
        text: terminal.slice(offset),
        nextOffset: terminal.length,
      };
    },
    async close() {
      closed = true;
    },
    get closed() {
      return closed;
    },
    get stepCycleCount() {
      return stepCycleCount;
    },
  };
}

function createWsHub() {
  return {
    messages: [],
    broadcast(message) {
      this.messages.push(message);
    },
  };
}

test('createDebugServerRuntime load resets terminal tracking and broadcasts initial snapshot', async () => {
  const session = createFakeSession();
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  const result = await runtime.load({
    name: 'hello',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
    bootUntilUartText: 'ready',
    bootMaxSteps: 64,
  }, 'pipeline');

  assert.equal(result.ok, true);
  assert.equal(result.snapshot.summary.cycle, 4);
  assert.equal(result.terminal.reset, true);
  assert.equal(result.terminal.text, 'boot:session-1\r\nready\r\n> ');
  assert.deepEqual(
    wsHub.messages.map((message) => message.type),
    ['snapshot', 'terminal'],
  );

  await runtime.close();
  assert.equal(session.closed, true);
});

test('createDebugServerRuntime run keeps broadcasting snapshots and terminal deltas until pause', async () => {
  const session = createFakeSession({
    label: 'run-session',
    stepCycleTerminalPrefix: 'tick:',
  });
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  await runtime.load({
    name: 'hello',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
  }, 'pipeline');
  wsHub.messages.length = 0;

  await runtime.run(1000);
  await wait(90);
  const paused = await runtime.pause();
  const messageCountAfterPause = wsHub.messages.length;
  await wait(60);

  const snapshotMessages = wsHub.messages.filter((message) => message.type === 'snapshot');
  const terminalMessages = wsHub.messages.filter((message) => message.type === 'terminal');

  assert.ok(snapshotMessages.length >= 1, 'run should emit at least one snapshot update');
  assert.ok(snapshotMessages.at(-1).snapshot.summary.cycle >= 1, 'run should advance the cycle counter');
  assert.ok(terminalMessages.some((message) => /tick:/.test(message.text)), 'run should surface incremental terminal output');
  assert.ok(paused.snapshot.summary.cycle >= 1, 'pause should return the latest observed snapshot');
  assert.equal(wsHub.messages.length, messageCountAfterPause, 'pause should stop further run-loop broadcasts');

  await runtime.close();
});

test('createDebugServerRuntime can resume a long-running session across repeated run/pause cycles without leaking stale broadcasts', async () => {
  const session = createFakeSession({
    label: 'loop-session',
    stepCycleTerminalPrefix: 'tick:',
  });
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  await runtime.load({
    name: 'hello',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
  }, 'pipeline');

  let lastPausedCycle = 0;
  for (let round = 0; round < 3; ++round) {
    wsHub.messages.length = 0;
    await runtime.run(1000);
    await wait(80);
    const paused = await runtime.pause();
    const messageCountAfterPause = wsHub.messages.length;
    await wait(60);

    const snapshotMessages = wsHub.messages.filter((message) => message.type === 'snapshot');
    const terminalMessages = wsHub.messages.filter((message) => message.type === 'terminal');

    assert.ok(snapshotMessages.length >= 1, `round ${round + 1} should emit at least one snapshot update while running`);
    assert.ok(terminalMessages.some((message) => /tick:/.test(message.text)), `round ${round + 1} should keep surfacing terminal deltas while running`);
    assert.ok(paused.snapshot.summary.cycle > lastPausedCycle, `round ${round + 1} should continue advancing the live session instead of restarting it`);
    assert.equal(wsHub.messages.length, messageCountAfterPause, `round ${round + 1} pause should stop later broadcasts`);

    lastPausedCycle = paused.snapshot.summary.cycle;
  }

  await runtime.close();
});

test('createDebugServerRuntime load replacement suppresses stale run-loop output from the previous session', async () => {
  let releaseFirstStepCycle;
  const firstStepCycleGate = new Promise((resolve) => {
    releaseFirstStepCycle = resolve;
  });
  let firstStepCycleStarted;
  const firstStepCycleStartedPromise = new Promise((resolve) => {
    firstStepCycleStarted = resolve;
  });
  const sessions = [
    createFakeSession({
      label: 'session-1',
      stepCycleTerminalPrefix: 'stale:',
      blockFirstStepCycle: {
        onStart: () => firstStepCycleStarted(),
        wait: () => firstStepCycleGate,
      },
    }),
    createFakeSession({
      label: 'session-2',
      stepCycleTerminalPrefix: 'fresh:',
    }),
  ];
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => sessions.shift(),
    wsHub,
  });

  await runtime.load({
    name: 'hello',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
  }, 'pipeline');
  wsHub.messages.length = 0;

  await runtime.run(1000);
  await firstStepCycleStartedPromise;

  const replacementLoadPromise = runtime.load({
    name: 'replacement',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
  }, 'pipeline');
  await wait(10);
  releaseFirstStepCycle();

  const replacement = await replacementLoadPromise;
  assert.equal(replacement.ok, true);
  assert.equal(replacement.snapshot.devices.uart.recent_output, 'session-2');

  const snapshotMessages = wsHub.messages.filter((message) => message.type === 'snapshot');
  const terminalMessages = wsHub.messages.filter((message) => message.type === 'terminal');

  assert.ok(snapshotMessages.some((message) => message.snapshot.devices.uart.recent_output === 'session-2'));
  assert.ok(!snapshotMessages.some((message) => message.snapshot.devices.uart.recent_output === 'session-1' && message.snapshot.summary.cycle > 0));
  assert.ok(!terminalMessages.some((message) => /stale:/.test(message.text)));

  await runtime.close();
});

test('createDebugServerRuntime terminalInput coalesces a larger echoed batch into one response and one terminal broadcast', async () => {
  const session = createFakeSession({
    label: 'burst-session',
    echoInputChunkSize: 16,
  });
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  await runtime.load({
    name: 'interactive',
    image: 'guest/interactive_os.elf',
    terminalPrompt: '> ',
  }, 'pipeline');
  wsHub.messages.length = 0;

  const burst = 'help '.repeat(32);
  const response = await runtime.terminalInput(burst);

  const snapshotMessages = wsHub.messages.filter((message) => message.type === 'snapshot');
  const terminalMessages = wsHub.messages.filter((message) => message.type === 'terminal');

  assert.equal(response.ok, true);
  assert.equal(response.text, burst);
  assert.equal(snapshotMessages.length, 1);
  assert.equal(terminalMessages.length, 1);
  assert.equal(terminalMessages[0].text, burst);

  await runtime.close();
});

test('createDebugServerRuntime load keeps the previous session live when replacement init fails', async () => {
  const stableSession = createFakeSession({ label: 'stable-session' });
  const brokenSession = createFakeSession({
    label: 'broken-session',
    loadError: new Error('replacement load failed'),
  });
  const sessions = [stableSession, brokenSession];
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => sessions.shift(),
    wsHub,
  });

  await runtime.load({
    name: 'hello',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
  }, 'pipeline');
  wsHub.messages.length = 0;

  await assert.rejects(
    runtime.load({
      name: 'replacement',
      image: 'tests/asm/hello.elf',
      terminalPrompt: '> ',
    }, 'pipeline'),
    /replacement load failed/,
  );

  const snapshot = await runtime.snapshot();
  assert.equal(snapshot.snapshot.devices.uart.recent_output, 'stable-session');
  assert.equal(brokenSession.closed, true, 'failed replacement session should be closed');
  assert.equal(wsHub.messages.length, 0, 'failed replacement load should not broadcast partial state');

  await runtime.close();
  assert.equal(stableSession.closed, true);
});

test('createDebugServerRuntime load keeps the previous session live when replacement snapshot fails', async () => {
  const stableSession = createFakeSession({ label: 'stable-session' });
  const brokenSession = createFakeSession({
    label: 'snapshot-broken',
    snapshotError: new Error('replacement snapshot failed'),
  });
  const sessions = [stableSession, brokenSession];
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => sessions.shift(),
    wsHub,
  });

  await runtime.load({
    name: 'hello',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
  }, 'pipeline');
  wsHub.messages.length = 0;

  await assert.rejects(
    runtime.load({
      name: 'replacement',
      image: 'tests/asm/hello.elf',
      terminalPrompt: '> ',
    }, 'pipeline'),
    /replacement snapshot failed/,
  );

  const snapshot = await runtime.snapshot();
  assert.equal(snapshot.snapshot.devices.uart.recent_output, 'stable-session');
  assert.equal(brokenSession.closed, true, 'failed snapshot replacement session should be closed');
  assert.equal(wsHub.messages.length, 0, 'failed replacement snapshot should not broadcast partial state');

  await runtime.close();
  assert.equal(stableSession.closed, true);
});
