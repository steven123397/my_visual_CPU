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
  linuxPromptMode = false,
  loadError = null,
  snapshotError = null,
  jitDispatchSummary = null,
} = {}) {
  let cycle = 0;
  let terminal = `boot:${label}\r\n> `;
  let closed = false;
  let pendingEcho = '';
  let stepCycleCount = 0;
  const loadCalls = [];
  const payloadLoads = [];
  const gprSeeds = [];
  const runUntilCalls = [];
  const runUntilNewCalls = [];
  const callLog = [];
  return {
    async load(entry, backend) {
      if (loadError) {
        throw loadError;
      }
      callLog.push('load');
      loadCalls.push({ entry, backend });
      cycle = 0;
      stepCycleCount = 0;
      terminal = linuxPromptMode ? `boot:${label}\r\nmycpu-linux# ` : `boot:${label}\r\n> `;
      return { ok: true };
    },
    async loadPayload(image, addr) {
      callLog.push(`loadPayload:${addr}`);
      payloadLoads.push({ image, addr });
      return { ok: true };
    },
    async setGpr(reg, value) {
      callLog.push(`setGpr:${reg}`);
      gprSeeds.push({ reg, value });
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
      } else if (linuxPromptMode && pendingEcho === 'help\r') {
        terminal += 'help\r\ncommands: help uptime exit\r\nmycpu-linux# ';
        pendingEcho = '';
      }
      return makeSnapshot(cycle, label);
    },
    async reset() {
      callLog.push('reset');
      cycle = 0;
      stepCycleCount = 0;
      terminal = 'reset\r\n> ';
      return makeSnapshot(cycle, label);
    },
    async runUntilUartContains(text, maxSteps, options = {}) {
      callLog.push('runUntilUartContains');
      runUntilCalls.push({ text, maxSteps, timeoutMs: options.timeoutMs });
      cycle = 4;
      terminal = linuxPromptMode ? `boot:${label}\r\nready\r\nmycpu-linux# ` : `boot:${label}\r\nready\r\n> `;
      return makeSnapshot(cycle, label);
    },
    async runUntilNewUartContains(offset, text, maxSteps, options = {}) {
      callLog.push('runUntilNewUartContains');
      runUntilNewCalls.push({ offset, text, maxSteps, timeoutMs: options.timeoutMs });
      cycle += 2;
      if (linuxPromptMode && pendingEcho === 'help\r') {
        terminal += 'help\r\ncommands: help uptime exit\r\nmycpu-linux# ';
        pendingEcho = '';
      }
      return {
        type: 'uart_output',
        offset,
        next_offset: terminal.length,
        text: terminal.slice(offset),
      };
    },
    async uartInput(text) {
      if (echoInputChunkSize || linuxPromptMode) {
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
    async jitDispatch() {
      callLog.push('jitDispatch');
      return jitDispatchSummary ?? {
        type: 'jit_dispatch',
        ok: false,
        source: 'hot-path-profile',
        action: 'reference-fallback',
        start_pc: '0x80200000',
        end_pc: '0x80200008',
        cache_state: 'miss',
        planned: true,
        translated: true,
        lowered: false,
        fallback_to_reference: true,
        lowered_instruction_count: 0,
        candidate_executions: 3,
        candidate_retired_instructions: 6,
        reject_kind: 'control-flow',
        reject_reason: 'fallback-required',
        helper_replay_kind: 'none',
        host_code: false,
        executable_memory: false,
        guest_execution: false,
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
    get loadCalls() {
      return loadCalls;
    },
    get payloadLoads() {
      return payloadLoads;
    },
    get gprSeeds() {
      return gprSeeds;
    },
    get runUntilCalls() {
      return runUntilCalls;
    },
    get runUntilNewCalls() {
      return runUntilNewCalls;
    },
    get callLog() {
      return callLog;
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

test('createDebugServerRuntime terminate closes the session, clears terminal tracking, and rejects later steps', async () => {
  const session = createFakeSession();
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  await runtime.load({
    name: 'linux_proto_console',
    image: 'workloads/linux_proto/linux_sbi_shim.bin',
    terminalPrompt: 'mycpu-linux# ',
  }, 'pipeline');
  wsHub.messages.length = 0;

  const result = await runtime.terminate();

  assert.equal(result.ok, true);
  assert.equal(session.closed, true);
  assert.equal(result.snapshot, null);
  assert.deepEqual(result.terminal, {
    type: 'terminal',
    text: '',
    nextOffset: 0,
    reset: true,
  });
  assert.deepEqual(wsHub.messages, [result.terminal]);
  await assert.rejects(runtime.stepCycle(), /session not loaded/);

  await runtime.close();
});

test('createDebugServerRuntime applies Linux boot payloads and register seeds before boot marker wait', async () => {
  const session = createFakeSession({ label: 'linux-session' });
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  const result = await runtime.load({
    name: 'linux_proto_console',
    image: 'workloads/linux_proto/linux_sbi_shim.bin',
    imageFormat: 'flat',
    loadAddr: '0x80000000',
    blockTransport: 'virtio-blk',
    disk: 'workloads/linux_proto/rootfs.ext4',
    payloads: [
      { image: 'external/linux-riscv/arch/riscv/boot/Image', addr: '0x80200000' },
      { image: 'workloads/linux_proto/mycpu_virt.dtb', addr: '0x87f00000' },
    ],
    gprSeeds: [
      { reg: 'a0', value: '0x0' },
      { reg: 'a1', value: '0x87f00000' },
      { reg: 'a2', value: '0x80200000' },
    ],
    bootUntilUartText: 'mycpu-linux# ',
    bootMaxSteps: 300000000,
    bootRequestTimeoutMs: 120000,
    terminalPrompt: 'mycpu-linux# ',
  }, 'pipeline');

  assert.equal(result.ok, true);
  assert.equal(session.loadCalls.length, 1);
  assert.equal(session.loadCalls[0].backend, 'pipeline');
  assert.equal(session.loadCalls[0].entry.imageFormat, 'flat');
  assert.deepEqual(session.payloadLoads, [
    { image: 'external/linux-riscv/arch/riscv/boot/Image', addr: '0x80200000' },
    { image: 'workloads/linux_proto/mycpu_virt.dtb', addr: '0x87f00000' },
  ]);
  assert.deepEqual(session.gprSeeds, [
    { reg: 'a0', value: '0x0' },
    { reg: 'a1', value: '0x87f00000' },
    { reg: 'a2', value: '0x80200000' },
  ]);
  assert.deepEqual(session.runUntilCalls, [
    { text: 'mycpu-linux# ', maxSteps: 300000000, timeoutMs: 120000 },
  ]);
  assert.equal(result.snapshot.summary.cycle, 4);
  assert.match(result.terminal.text, /ready/);

  await runtime.close();
});

test('createDebugServerRuntime reset lets C++ replay Linux payloads and only waits for prompt', async () => {
  const session = createFakeSession({ label: 'linux-reset', linuxPromptMode: true });
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });
  const entry = {
    name: 'linux_proto_console',
    image: 'workloads/linux_proto/linux_sbi_shim.bin',
    imageFormat: 'flat',
    loadAddr: '0x80000000',
    blockTransport: 'virtio-blk',
    disk: 'workloads/linux_proto/rootfs.ext4',
    payloads: [
      { image: 'external/linux-riscv/arch/riscv/boot/Image', addr: '0x80200000' },
      { image: 'workloads/linux_proto/mycpu_virt.dtb', addr: '0x87f00000' },
    ],
    gprSeeds: [
      { reg: 'a0', value: '0x0' },
      { reg: 'a1', value: '0x87f00000' },
      { reg: 'a2', value: '0x80200000' },
    ],
    bootUntilUartText: 'mycpu-linux# ',
    bootMaxSteps: 300000000,
    bootRequestTimeoutMs: 120000,
    terminalPrompt: 'mycpu-linux# ',
  };

  await runtime.load(entry, 'pipeline');
  wsHub.messages.length = 0;
  session.callLog.length = 0;

  const result = await runtime.reset();

  assert.equal(result.snapshot.summary.cycle, 4);
  assert.match(result.terminal.text, /mycpu-linux# /);
  assert.deepEqual(session.callLog, [
    'reset',
    'runUntilUartContains',
  ]);
  assert.equal(session.payloadLoads.length, 2);
  assert.equal(session.gprSeeds.length, 3);
  assert.deepEqual(session.runUntilCalls.slice(-1), [
    { text: 'mycpu-linux# ', maxSteps: 300000000, timeoutMs: 120000 },
  ]);
  assert.deepEqual(
    wsHub.messages.map((message) => message.type),
    ['snapshot', 'terminal'],
  );

  await runtime.close();
});

test('createDebugServerRuntime repeated Linux resets do not append duplicate post-load actions', async () => {
  const session = createFakeSession({ label: 'linux-reset-repeat', linuxPromptMode: true });
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub: createWsHub(),
  });
  const entry = {
    name: 'linux_proto_console',
    image: 'workloads/linux_proto/linux_sbi_shim.bin',
    payloads: [
      { image: 'external/linux-riscv/arch/riscv/boot/Image', addr: '0x80200000' },
      { image: 'workloads/linux_proto/mycpu_virt.dtb', addr: '0x87f00000' },
    ],
    gprSeeds: [
      { reg: 'a0', value: '0x0' },
      { reg: 'a1', value: '0x87f00000' },
      { reg: 'a2', value: '0x80200000' },
    ],
    bootUntilUartText: 'mycpu-linux# ',
    bootMaxSteps: 300000000,
    bootRequestTimeoutMs: 120000,
    terminalPrompt: 'mycpu-linux# ',
  };

  await runtime.load(entry, 'functional');
  await runtime.reset();
  await runtime.reset();

  assert.equal(session.payloadLoads.length, 2);
  assert.equal(session.gprSeeds.length, 3);
  assert.deepEqual(
    session.callLog.filter((item) => item.startsWith('loadPayload') || item.startsWith('setGpr')),
    [
      'loadPayload:0x80200000',
      'loadPayload:0x87f00000',
      'setGpr:a0',
      'setGpr:a1',
      'setGpr:a2',
    ],
  );
  assert.equal(session.runUntilCalls.length, 3);

  await runtime.close();
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

test('createDebugServerRuntime terminalInput waits for Linux prompt command output to settle', async () => {
  const session = createFakeSession({
    label: 'linux-console',
    linuxPromptMode: true,
  });
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  await runtime.load({
    name: 'linux_proto_console',
    image: 'workloads/linux_proto/linux_sbi_shim.bin',
    bootUntilUartText: 'mycpu-linux# ',
    bootMaxSteps: 300000000,
    commandUntilUartText: 'mycpu-linux# ',
    commandMaxSteps: 50000000,
    commandRequestTimeoutMs: 30000,
    terminalPrompt: 'mycpu-linux# ',
  }, 'pipeline');
  wsHub.messages.length = 0;

  const response = await runtime.terminalInput('help\r');
  const terminalMessages = wsHub.messages.filter((message) => message.type === 'terminal');

  assert.equal(response.ok, true);
  assert.match(response.text, /commands: help uptime exit/);
  assert.match(response.text, /mycpu-linux# $/);
  assert.equal(session.runUntilNewCalls.length, 1);
  assert.ok(session.runUntilNewCalls[0].offset > 0);
  assert.equal(session.runUntilNewCalls[0].text, 'mycpu-linux# ');
  assert.equal(session.runUntilNewCalls[0].maxSteps, 50000000);
  assert.equal(session.runUntilNewCalls[0].timeoutMs, 30000);
  assert.equal(terminalMessages.length, 1);
  assert.match(terminalMessages[0].text, /commands: help uptime exit/);
  assert.match(terminalMessages[0].text, /mycpu-linux# $/);

  await runtime.close();
});

test('createDebugServerRuntime jitDispatch returns the current dry-run summary for a loaded session', async () => {
  const session = createFakeSession({
    label: 'jit-session',
    jitDispatchSummary: {
      type: 'jit_dispatch',
      ok: true,
      source: 'hot-path-profile',
      action: 'lowered-ready',
      start_pc: '0x80000000',
      end_pc: '0x80000020',
      cache_state: 'hit',
      planned: true,
      translated: true,
      lowered: true,
      fallback_to_reference: false,
      lowered_instruction_count: 5,
      candidate_executions: 18,
      candidate_retired_instructions: 72,
      reject_kind: 'none',
      reject_reason: 'none',
      helper_replay_kind: 'none',
      host_code: false,
      executable_memory: false,
      guest_execution: false,
    },
  });
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  await runtime.load({
    name: 'guest_vector_cnn_demo',
    image: 'guest/vector_cnn_demo.elf',
    terminalPrompt: '> ',
  }, 'pipeline');

  const response = await runtime.jitDispatch();

  assert.deepEqual(response.summary, {
    type: 'jit_dispatch',
    ok: true,
    source: 'hot-path-profile',
    action: 'lowered-ready',
    start_pc: '0x80000000',
    end_pc: '0x80000020',
    cache_state: 'hit',
    planned: true,
    translated: true,
    lowered: true,
    fallback_to_reference: false,
    lowered_instruction_count: 5,
    candidate_executions: 18,
    candidate_retired_instructions: 72,
    reject_kind: 'none',
    reject_reason: 'none',
    helper_replay_kind: 'none',
    host_code: false,
    executable_memory: false,
    guest_execution: false,
  });
  assert.ok(session.callLog.includes('jitDispatch'));

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
