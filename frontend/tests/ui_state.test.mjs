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
  setLoadProgress,
  setDiagnostics,
  setLoadedSession,
  setTests,
  setAiTinyModelTemplates,
  setAiTinyModelParameters,
  setAiTinyModelRunState,
  setAiTinyModelResult,
  clearLoadProgress,
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

test('AI tiny model state stores whitelist templates, bounded parameters, and run result', () => {
  const state = createAppState();

  setAiTinyModelTemplates(state, [
    {
      id: 'dynamic_tiny_model',
      title: 'Parameterized Tiny Model',
      parameters: {
        batch: { choices: [1, 2], default: 1 },
        inputPreset: { choices: ['balanced', 'negative_clamp'], default: 'balanced' },
      },
    },
  ]);
  setAiTinyModelParameters(state, {
    template: 'dynamic_tiny_model',
    batch: 2,
    inputPreset: 'negative_clamp',
  });
  setAiTinyModelRunState(state, 'running', null);
  setAiTinyModelResult(state, {
    output: {
      values: [0, 2.5],
      expected: [0, 2.5],
    },
    profile: {
      deviceCycles: 31,
    },
  });

  assert.equal(state.aiTinyModel.templates.length, 1);
  assert.equal(state.aiTinyModel.parameters.template, 'dynamic_tiny_model');
  assert.equal(state.aiTinyModel.parameters.batch, 2);
  assert.equal(state.aiTinyModel.parameters.inputPreset, 'negative_clamp');
  assert.equal(state.aiTinyModel.runState, 'completed');
  assert.equal(state.aiTinyModel.error, null);
  assert.deepEqual(state.aiTinyModel.result.output.values, [0, 2.5]);
});

test('AI tiny model state reselects template-specific defaults when switching to another whitelist template', () => {
  const state = createAppState();

  setAiTinyModelTemplates(state, [
    {
      id: 'dynamic_tiny_model',
      title: 'Parameterized Tiny Model',
      parameters: {
        batch: { choices: [1, 2], default: 1 },
        inputPreset: { choices: ['balanced', 'negative_clamp'], default: 'balanced' },
      },
    },
    {
      id: 'dynamic_gemm',
      title: 'Dynamic GEMM Profile',
      parameters: {
        runtimeShape: { choices: ['two_rows_identity_tail', 'single_row_identity_head'], default: 'two_rows_identity_tail' },
      },
    },
  ]);

  setAiTinyModelParameters(state, {
    template: 'dynamic_tiny_model',
    batch: 2,
    inputPreset: 'negative_clamp',
  });
  setAiTinyModelTemplates(state, state.aiTinyModel.templates);
  setAiTinyModelParameters(state, {
    template: 'dynamic_gemm',
  });
  setAiTinyModelTemplates(state, state.aiTinyModel.templates);

  assert.equal(state.aiTinyModel.parameters.template, 'dynamic_gemm');
  assert.equal(state.aiTinyModel.parameters.runtimeShape, 'two_rows_identity_tail');
  assert.equal(state.aiTinyModel.parameters.batch, undefined);
  assert.equal(state.aiTinyModel.parameters.inputPreset, undefined);
});

test('AI tiny model state records run errors without preserving stale results', () => {
  const state = createAppState();

  setAiTinyModelResult(state, {
    output: {
      values: [2.5],
    },
  });
  setAiTinyModelRunState(state, 'error', 'batch must be one of: 1, 2');

  assert.equal(state.aiTinyModel.runState, 'error');
  assert.equal(state.aiTinyModel.error, 'batch must be one of: 1, 2');
  assert.equal(state.aiTinyModel.result, null);
});

test('load progress records Linux boot wait metadata and can be cleared', () => {
  const state = createAppState();
  const startedAt = 1700000000000;

  setLoadProgress(state, {
    test: 'linux_proto_console',
    backend: 'functional',
    startedAt,
    waitingFor: 'mycpu-linux# ',
    label: 'Booting Linux',
  });

  assert.deepEqual(state.loadProgress, {
    test: 'linux_proto_console',
    backend: 'functional',
    startedAt,
    waitingFor: 'mycpu-linux# ',
    label: 'Booting Linux',
  });

  clearLoadProgress(state);

  assert.equal(state.loadProgress, null);
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
  state.loadProgress = {
    test: 'linux_proto_console',
    backend: 'functional',
    startedAt: 1700000000000,
    waitingFor: 'mycpu-linux# ',
    label: 'Booting Linux',
  };
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
  assert.equal(state.loadProgress, null);
  assert.equal(state.runState, 'idle');
});
