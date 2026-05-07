const MAX_HISTORY = 32;
export const MAX_TERMINAL_BUFFER = 32768;

import {
  applyTerminalChunk,
  createTerminalProjectionState,
  resetTerminalProjectionState,
} from '../shared/terminal_projection.mjs';

function createTerminalState() {
  const terminal = {
    buffer: '',
    nextOffset: 0,
    focused: false,
    connected: false,
    pendingInput: false,
  };
  Object.defineProperty(terminal, 'projection', {
    value: createTerminalProjectionState({ maxLength: MAX_TERMINAL_BUFFER }),
    writable: true,
    enumerable: false,
  });
  return terminal;
}

function createJitDispatchState() {
  return {
    runState: 'idle',
    error: null,
    summary: null,
  };
}

export function createAppState() {
  return {
    auth: {
      required: false,
      authenticated: false,
      username: null,
      role: null,
      activeSessions: 0,
      sessionLimit: 0,
      controllerUsername: null,
      controllerSession: false,
      canControl: true,
      loginPending: false,
      loginError: null,
    },
    tests: [],
    diagnostics: {},
    aiTinyModel: {
      templates: [],
      parameters: {
        template: 'dynamic_tiny_model',
        batch: 1,
        inputPreset: 'balanced',
      },
      runState: 'idle',
      error: null,
      result: null,
    },
    jitDispatch: createJitDispatchState(),
    selectedTest: 'hello',
    selectedScenario: 'hello',
    selectedScenarioTest: 'hello',
    selectedScenarioBackend: 'pipeline',
    backend: 'pipeline',
    loadedSession: null,
    loadProgress: null,
    runState: 'idle',
    currentSnapshot: null,
    history: [],
    terminal: createTerminalState(),
    layout: {
      debugPanelOpen: true,
      architectureGroupOpen: false,
      platformGroupOpen: false,
      terminalCollapsed: false,
    },
  };
}

export function setAuthState(state, auth = {}) {
  state.auth = {
    ...state.auth,
    required: Boolean(auth.required),
    authenticated: Boolean(auth.authenticated),
    username: auth.username ?? null,
    role: auth.role ?? null,
    activeSessions: Number.isInteger(auth.activeSessions) ? auth.activeSessions : 0,
    sessionLimit: Number.isInteger(auth.sessionLimit) ? auth.sessionLimit : 0,
    controllerUsername: auth.controllerUsername ?? null,
    controllerSession: Boolean(auth.controllerSession),
    canControl: auth.canControl ?? !auth.required,
    loginPending: state.auth.loginPending,
    loginError: state.auth.loginError,
  };
}

export function setLoginPending(state, pending) {
  state.auth.loginPending = pending;
}

export function setLoginError(state, error) {
  state.auth.loginError = error ?? null;
}

export function setAiTinyModelTemplates(state, templates) {
  state.aiTinyModel.templates = Array.isArray(templates) ? templates : [];
  const selected = state.aiTinyModel.templates.find(
    (item) => item.id === state.aiTinyModel.parameters.template,
  ) ?? state.aiTinyModel.templates[0] ?? null;
  if (!selected) {
    return;
  }

  const nextParameters = {
    template: selected.id,
  };
  for (const [name, definition] of Object.entries(selected.parameters ?? {})) {
    const choices = Array.isArray(definition?.choices) ? definition.choices : [];
    const currentValue = state.aiTinyModel.parameters[name];
    if (choices.some((choice) => String(choice) === String(currentValue))) {
      nextParameters[name] = currentValue;
      continue;
    }
    nextParameters[name] = definition?.default ?? choices[0];
  }
  state.aiTinyModel.parameters = nextParameters;
}

export function setAiTinyModelParameters(state, parameters = {}) {
  state.aiTinyModel.parameters = {
    ...state.aiTinyModel.parameters,
    ...parameters,
  };
}

export function setAiTinyModelRunState(state, runState, error = null) {
  state.aiTinyModel.runState = runState;
  state.aiTinyModel.error = error;
  if (runState === 'running' || runState === 'error') {
    state.aiTinyModel.result = null;
  }
}

export function setAiTinyModelResult(state, result) {
  state.aiTinyModel.result = result ?? null;
  state.aiTinyModel.error = null;
  state.aiTinyModel.runState = result ? 'completed' : 'idle';
}

export function setDiagnostics(state, diagnostics) {
  state.diagnostics =
    diagnostics && typeof diagnostics === 'object'
      ? diagnostics
      : {};
}

export function setTests(state, tests, diagnostics = state.diagnostics) {
  state.tests = Array.isArray(tests) ? tests : [];
  setDiagnostics(state, diagnostics);
  if (!state.tests.some((item) => item.name === state.selectedTest) && state.tests[0]) {
    state.selectedTest = state.tests[0].name;
    state.selectedScenario = state.tests[0].name;
    state.selectedScenarioTest = state.tests[0].name;
    state.selectedScenarioBackend = state.tests[0].backend ?? state.backend;
  } else if (!state.selectedScenario) {
    state.selectedScenario = state.selectedTest;
    state.selectedScenarioTest = state.selectedTest;
    state.selectedScenarioBackend = state.backend;
  }
}

export function selectDemo(state, testName, backend = state.backend) {
  if (!state.tests.some((item) => item.name === testName)) {
    return false;
  }

  state.selectedTest = testName;
  state.selectedScenario = testName;
  state.selectedScenarioTest = testName;
  if (typeof backend === 'string' && backend.length > 0) {
    state.backend = backend;
    state.selectedScenarioBackend = backend;
  }
  return true;
}

export function selectScenario(state, scenarioKey, scenarioTest = null, scenarioBackend = state.backend) {
  if (typeof scenarioKey !== 'string' || scenarioKey.length === 0) {
    return false;
  }
  state.selectedScenario = scenarioKey;
  state.selectedScenarioTest =
    typeof scenarioTest === 'string' && scenarioTest.length > 0
      ? scenarioTest
      : null;
  state.selectedScenarioBackend =
    typeof scenarioBackend === 'string' && scenarioBackend.length > 0
      ? scenarioBackend
      : state.backend;
  return true;
}

export function syncScenarioSessionSelection(state, testName, backend = state.backend) {
  if (!state.tests.some((item) => item.name === testName)) {
    return false;
  }

  state.selectedTest = testName;
  state.selectedScenarioTest = testName;
  if (typeof backend === 'string' && backend.length > 0) {
    state.backend = backend;
    state.selectedScenarioBackend = backend;
  }
  return true;
}

export function setLoadedSession(state, session) {
  if (!session || typeof session.test !== 'string' || session.test.length === 0) {
    state.loadedSession = null;
    return;
  }

  state.loadedSession = {
    test: session.test,
    backend:
      typeof session.backend === 'string' && session.backend.length > 0
        ? session.backend
        : null,
  };
}

export function setLoadProgress(state, progress) {
  if (!progress || typeof progress.test !== 'string' || progress.test.length === 0) {
    state.loadProgress = null;
    return;
  }

  state.loadProgress = {
    test: progress.test,
    backend:
      typeof progress.backend === 'string' && progress.backend.length > 0
        ? progress.backend
        : null,
    startedAt:
      typeof progress.startedAt === 'number' && Number.isFinite(progress.startedAt)
        ? progress.startedAt
        : Date.now(),
    waitingFor:
      typeof progress.waitingFor === 'string' && progress.waitingFor.length > 0
        ? progress.waitingFor
        : null,
    label:
      typeof progress.label === 'string' && progress.label.length > 0
        ? progress.label
        : 'Loading',
  };

  if (typeof progress.now === 'number' && Number.isFinite(progress.now)) {
    state.loadProgress.now = progress.now;
  }
}

export function clearLoadProgress(state) {
  state.loadProgress = null;
}

export function pushSnapshot(state, snapshot) {
  state.currentSnapshot = snapshot;
  state.history = [...state.history, snapshot].slice(-MAX_HISTORY);
  state.runState = snapshot?.summary?.halted ? 'halted' : state.runState;
}

export function resetHistory(state) {
  state.currentSnapshot = null;
  state.history = [];
}

export function resetTerminalState(state) {
  state.terminal = createTerminalState();
}

export function clearLoadedSession(state) {
  state.loadedSession = null;
  clearLoadProgress(state);
  state.runState = 'idle';
  resetHistory(state);
  resetTerminalState(state);
  clearJitDispatch(state);
}

export function appendTerminalOutput(state, payload = {}) {
  const text = payload.text ?? '';
  const nextOffset =
    typeof payload.nextOffset === 'number'
      ? payload.nextOffset
      : (typeof payload.next_offset === 'number' ? payload.next_offset : state.terminal.nextOffset);
  const reset = Boolean(payload.reset);

  if (!reset && nextOffset <= state.terminal.nextOffset) {
    state.terminal.connected = true;
    return;
  }

  if (reset) {
    resetTerminalProjectionState(state.terminal.projection);
  }
  applyTerminalChunk(state.terminal.projection, text);
  state.terminal.buffer = state.terminal.projection.text;
  state.terminal.nextOffset = nextOffset;
  state.terminal.connected = true;
}

export function setTerminalFocus(state, focused) {
  state.terminal.focused = focused;
}

export function setTerminalPendingInput(state, pending) {
  state.terminal.pendingInput = pending;
}

export function setJitDispatchRunState(state, runState, error = null) {
  state.jitDispatch.runState = runState;
  state.jitDispatch.error = error ?? null;
  if (runState === 'running' || runState === 'error') {
    state.jitDispatch.summary = null;
  }
}

export function setJitDispatchResult(state, summary) {
  state.jitDispatch.summary = summary ?? null;
  state.jitDispatch.error = null;
  state.jitDispatch.runState = summary ? 'completed' : 'idle';
}

export function clearJitDispatch(state) {
  state.jitDispatch = createJitDispatchState();
}

export function setDebugPanelOpen(state, open) {
  state.layout.debugPanelOpen = open;
}

export function setTerminalCollapsed(state, collapsed) {
  state.layout.terminalCollapsed = collapsed;
}

export function setInspectorGroupOpen(state, key, open) {
  if (!(key in state.layout)) {
    return;
  }
  state.layout[key] = open;
}

export function normalizeTerminalInput(event) {
  if (!event || event.ctrlKey || event.metaKey || event.altKey) {
    return null;
  }

  if (event.key === 'Enter') {
    return '\r';
  }

  if (event.key === 'Backspace') {
    return '\b';
  }

  if (typeof event.key !== 'string' || event.key.length !== 1) {
    return null;
  }

  const code = event.key.charCodeAt(0);
  return code >= 0x20 && code <= 0x7e ? event.key : null;
}

export function buildTimelineRows(history) {
  return history.map((snapshot) => {
    const flags = snapshot.pipeline?.flags ?? {};
    let flag = 'normal';
    let flagLabel = 'normal';
    if (flags.stalled) {
      flag = 'stall';
      flagLabel =
        typeof flags.stall_reason === 'string' && flags.stall_reason.length > 0 && flags.stall_reason !== 'none'
          ? `stall: ${flags.stall_reason}`
          : 'stall';
    } else if (flags.redirected) {
      flag = 'redirect';
      flagLabel = 'redirect';
    } else if (flags.trap_flush) {
      flag = 'flush';
      flagLabel = 'flush';
    } else if (flags.committed) {
      flag = 'commit';
      flagLabel = 'commit';
    }

    return {
      cycle: snapshot.summary?.cycle ?? 0,
      flag,
      flagLabel,
      stages: {
        if: snapshot.pipeline?.if?.text ?? '',
        id: snapshot.pipeline?.id?.text ?? '',
        ex: snapshot.pipeline?.ex?.text ?? '',
        mem: snapshot.pipeline?.mem?.text ?? '',
        wb: snapshot.pipeline?.wb?.text ?? '',
      },
    };
  });
}

export function classifyInstructionFlavor(text = '') {
  const normalized = String(text).trim().toLowerCase();
  if (!normalized) {
    return null;
  }

  const mnemonic = normalized.split(/[ ,\t]+/, 1)[0];
  if (!mnemonic.startsWith('v')) {
    return null;
  }

  if (mnemonic === 'vsetcfg') {
    return {
      family: 'vector',
      kind: 'config',
      label: 'vector cfg',
    };
  }

  if (mnemonic.startsWith('vle') || mnemonic.startsWith('vse')) {
    return {
      family: 'vector',
      kind: 'memory',
      label: 'vector mem',
    };
  }

  if (mnemonic.startsWith('vdot')) {
    return {
      family: 'vector',
      kind: 'dot',
      label: 'vector dot',
    };
  }

  if (mnemonic.startsWith('vmax')) {
    return {
      family: 'vector',
      kind: 'relu',
      label: 'vector relu',
    };
  }

  if (mnemonic.startsWith('vadd') || mnemonic.startsWith('vmul')) {
    return {
      family: 'vector',
      kind: 'alu',
      label: 'vector alu',
    };
  }

  return {
    family: 'vector',
    kind: 'generic',
    label: 'vector',
  };
}

function hexToBigInt(value) {
  try {
    return BigInt(value ?? '0x0');
  } catch {
    return 0n;
  }
}

export function diffRegisters(previous, current) {
  const before = previous?.gpr ?? [];
  const after = current?.gpr ?? [];
  return after.map((value, index) => ({
    index,
    value,
    changed: before[index] !== undefined && before[index] !== value,
    emphasis:
      before[index] === undefined || before[index] === value
        ? 'steady'
        : (hexToBigInt(before[index]) === 0n && hexToBigInt(value) !== 0n)
            ? 'rise'
            : (hexToBigInt(before[index]) !== 0n && hexToBigInt(value) === 0n)
                ? 'clear'
                : 'mutate',
  }));
}

export function diffVectorRegisters(previous, current) {
  const before = previous?.vector?.registers ?? [];
  const after = current?.vector?.registers ?? [];
  return after.map((value, index) => {
    const normalized = typeof value === 'string' ? value : '0x00000000000000000000000000000000';
    const changed = before[index] !== undefined && before[index] !== normalized;
    const empty = /^0x0+$/.test(normalized);
    return {
      index,
      value: normalized,
      changed,
      empty,
      emphasis:
        !changed ? (empty ? 'idle' : 'steady')
          : empty ? 'clear'
            : /^0x0+$/.test(before[index] ?? '0x0') ? 'rise'
              : 'mutate',
    };
  });
}

export function classifyEventTone(kind) {
  switch (kind) {
  case 'stall':
  case 'redirect':
  case 'commit':
  case 'flush':
    return 'control';
  case 'trap':
  case 'fetch-fault':
    return 'trap';
  case 'load':
  case 'store':
    return 'memory';
  case 'halt':
    return 'lifecycle';
  default:
    return 'neutral';
  }
}

export function shouldAutoScrollToBottom(metrics, threshold = 48) {
  if (!metrics) {
    return true;
  }
  return metrics.scrollTop + metrics.clientHeight >= metrics.scrollHeight - threshold;
}
