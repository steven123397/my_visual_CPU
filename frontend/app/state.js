const MAX_HISTORY = 32;

export function createAppState() {
  return {
    tests: [],
    selectedTest: 'hello',
    backend: 'pipeline',
    runState: 'idle',
    currentSnapshot: null,
    history: [],
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

export function buildTimelineRows(history) {
  return history.map((snapshot) => {
    const flags = snapshot.pipeline?.flags ?? {};
    let flag = 'normal';
    if (flags.stalled) {
      flag = 'stall';
    } else if (flags.redirected) {
      flag = 'redirect';
    } else if (flags.trap_flush) {
      flag = 'flush';
    } else if (flags.committed) {
      flag = 'commit';
    }

    return {
      cycle: snapshot.summary?.cycle ?? 0,
      flag,
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
