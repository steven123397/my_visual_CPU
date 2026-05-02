import test from 'node:test';
import assert from 'node:assert/strict';

import {
  buildTimelineRows,
  classifyEventTone,
  classifyInstructionFlavor,
  clearLoadedSession,
  createAppState,
  appendTerminalOutput,
  selectDemo,
  setDiagnostics,
  setLoadedSession,
  setTests,
  diffRegisters,
  diffVectorRegisters,
  pushSnapshot,
  shouldAutoScrollToBottom,
} from '../app/state.js';

test('buildTimelineRows highlights stalls and redirects', () => {
  const rows = buildTimelineRows([
    {
      summary: { cycle: 1 },
      pipeline: {
        if: { text: 'lw' },
        id: { text: 'add' },
        ex: { text: '' },
        mem: { text: '' },
        wb: { text: '' },
        flags: {
          stalled: true,
          stall_reason: 'memory_path_busy',
          redirected: false,
        },
      },
    },
    {
      summary: { cycle: 2 },
      pipeline: { if: { text: 'jal' }, id: { text: 'nop' }, ex: { text: 'beq' }, mem: { text: '' }, wb: { text: '' }, flags: { stalled: false, redirected: true } },
    },
  ]);

  assert.equal(rows[0].flag, 'stall');
  assert.equal(rows[0].flagLabel, 'stall: memory_path_busy');
  assert.equal(rows[1].flag, 'redirect');
  assert.equal(rows[1].flagLabel, 'redirect');
  assert.equal(rows[0].cycle, 1);
  assert.equal(rows[1].stages.ex, 'beq');
});

test('diffRegisters assigns layered emphasis for value changes', () => {
  const registers = diffRegisters(
    { gpr: ['0x0', '0x0', '0x8'] },
    { gpr: ['0x0', '0x3', '0x0'] },
  );

  assert.equal(registers[0].emphasis, 'steady');
  assert.equal(registers[1].emphasis, 'rise');
  assert.equal(registers[2].emphasis, 'clear');
});

test('classifyEventTone groups event kinds for clearer highlighting', () => {
  assert.equal(classifyEventTone('stall'), 'control');
  assert.equal(classifyEventTone('redirect'), 'control');
  assert.equal(classifyEventTone('trap'), 'trap');
  assert.equal(classifyEventTone('store'), 'memory');
  assert.equal(classifyEventTone('halt'), 'lifecycle');
  assert.equal(classifyEventTone('unknown'), 'neutral');
});

test('classifyInstructionFlavor marks vector cfg / mem / alu instructions', () => {
  assert.deepEqual(classifyInstructionFlavor('vsetcfg'), { family: 'vector', kind: 'config', label: 'vector cfg' });
  assert.deepEqual(classifyInstructionFlavor('vle.v v1, (a0)'), { family: 'vector', kind: 'memory', label: 'vector mem' });
  assert.deepEqual(classifyInstructionFlavor('vdot.vv v3, v1, v2'), { family: 'vector', kind: 'dot', label: 'vector dot' });
  assert.equal(classifyInstructionFlavor('addi x0, x0, 0'), null);
});

test('diffVectorRegisters tracks changes and idle vector registers', () => {
  const registers = diffVectorRegisters(
    { vector: { registers: ['0x0', '0x01000000000000000000000000000000'] } },
    { vector: { registers: ['0x0', '0x02000000000000000000000000000000'] } },
  );

  assert.equal(registers[0].emphasis, 'idle');
  assert.equal(registers[1].changed, true);
  assert.equal(registers[1].emphasis, 'mutate');
});

test('shouldAutoScrollToBottom only sticks when user is near the latest event', () => {
  assert.equal(shouldAutoScrollToBottom(null), true);
  assert.equal(shouldAutoScrollToBottom({ scrollTop: 360, clientHeight: 240, scrollHeight: 600 }), true);
  assert.equal(shouldAutoScrollToBottom({ scrollTop: 100, clientHeight: 240, scrollHeight: 600 }), false);
});

test('selectDemo updates the workload and backend only for manifest-backed demos', () => {
  const state = {
    tests: [{ name: 'guest_ai_accel_demo' }],
    selectedTest: 'hello',
    backend: 'functional',
  };

  assert.equal(selectDemo(state, 'guest_ai_accel_demo', 'pipeline'), true);
  assert.equal(state.selectedTest, 'guest_ai_accel_demo');
  assert.equal(state.backend, 'pipeline');

  assert.equal(selectDemo(state, 'linux_shell_demo', 'pipeline'), false);
  assert.equal(state.selectedTest, 'guest_ai_accel_demo');
  assert.equal(state.backend, 'pipeline');
});

test('setTests stores read-only diagnostics alongside the workload manifest', () => {
  const state = createAppState();
  const diagnostics = {
    linuxConsole: {
      status: 'not-found',
      ready: false,
      envVar: 'MYCPU_LINUX_PROTO_CONSOLE_IMAGE',
      path: '/missing/Image',
      message: 'Image path does not exist: /missing/Image',
    },
  };

  setTests(state, [{ name: 'hello' }], diagnostics);

  assert.deepEqual(state.diagnostics.linuxConsole, diagnostics.linuxConsole);
  assert.equal(state.selectedTest, 'hello');
});

test('setDiagnostics ignores malformed diagnostics payloads', () => {
  const state = createAppState();

  setDiagnostics(state, null);

  assert.deepEqual(state.diagnostics, {});
});

test('clearLoadedSession resets snapshot history, terminal state, and active session identity', () => {
  const state = createAppState();
  setLoadedSession(state, {
    test: 'linux_proto_console',
    backend: 'pipeline',
  });
  pushSnapshot(state, {
    summary: {
      cycle: 9,
      halted: false,
    },
  });
  appendTerminalOutput(state, {
    text: 'mycpu-linux# ',
    nextOffset: 'mycpu-linux# '.length,
    reset: true,
  });
  state.terminal.focused = true;
  state.terminal.pendingInput = true;
  state.runState = 'running';

  clearLoadedSession(state);

  assert.equal(state.loadedSession, null);
  assert.equal(state.currentSnapshot, null);
  assert.deepEqual(state.history, []);
  assert.equal(state.terminal.connected, false);
  assert.equal(state.terminal.buffer, '');
  assert.equal(state.terminal.nextOffset, 0);
  assert.equal(state.terminal.focused, false);
  assert.equal(state.terminal.pendingInput, false);
  assert.equal(state.runState, 'idle');
});
