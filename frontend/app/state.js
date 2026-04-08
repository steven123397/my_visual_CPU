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

export function createAppState() {
  return {
    tests: [],
    selectedTest: 'hello',
    backend: 'pipeline',
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

export function setTests(state, tests) {
  state.tests = tests;
  if (!tests.some((item) => item.name === state.selectedTest) && tests[0]) {
    state.selectedTest = tests[0].name;
  }
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
