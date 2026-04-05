import {
  listTests,
  loadSession,
  stepCycle,
  stepCommit,
  resetSession,
  runSession,
  pauseSession,
  terminalInput,
  terminalOutput,
  connectSnapshotSocket,
} from './api.js';
import {
  appendTerminalOutput,
  createAppState,
  normalizeTerminalInput,
  pushSnapshot,
  resetHistory,
  resetTerminalState,
  setTerminalFocus,
  setInspectorGroupOpen,
  setTerminalPendingInput,
  setTests,
} from './state.js';
import { createTerminalInputPump } from './terminal_input_pump.js';
import { renderApp, updateControls } from './render.js';

const state = createAppState();

const elements = {
  testSelect: document.querySelector('#test-select'),
  backendSelect: document.querySelector('#backend-select'),
  statusBadge: document.querySelector('#status-badge'),
  desktop: document.querySelector('#desktop-shell'),
  debugInspector: document.querySelector('#debug-inspector'),
  terminal: document.querySelector('#terminal-slot'),
  summary: document.querySelector('#summary-slot'),
  predictor: document.querySelector('#predictor-slot'),
  pipeline: document.querySelector('#pipeline-slot'),
  events: document.querySelector('#events-slot'),
  devices: document.querySelector('#devices-slot'),
  registers: document.querySelector('#registers-slot'),
  csrs: document.querySelector('#csrs-slot'),
  bus: document.querySelector('#bus-slot'),
  notice: document.querySelector('#notice'),
};

function paint() {
  updateControls(elements, state);
  renderApp(elements, state);
}

function showNotice(message, kind = 'info') {
  elements.notice.textContent = message;
  elements.notice.dataset.kind = kind;
}

function mergeTerminal(payload, reset = false) {
  if (!payload) {
    return;
  }
  appendTerminalOutput(state, {
    text: payload.text ?? '',
    nextOffset: payload.nextOffset ?? payload.next_offset ?? state.terminal.nextOffset,
    reset: payload.reset ?? reset,
  });
}

async function syncTerminal(offset = state.terminal.nextOffset, reset = false) {
  const payload = await terminalOutput(offset);
  mergeTerminal(payload, reset);
}

async function handleLoad() {
  state.runState = 'loading';
  resetTerminalState(state);
  paint();
  const response = await loadSession(state.selectedTest, state.backend);
  resetHistory(state);
  pushSnapshot(state, response.snapshot);
  mergeTerminal(response.terminal, true);
  if (!response.terminal) {
    await syncTerminal(0, true);
  }
  state.runState = 'paused';
  paint();
  showNotice(`已加载 ${state.selectedTest}`, 'success');
}

async function handleAction(action, label) {
  const response = await action();
  if (response?.snapshot) {
    pushSnapshot(state, response.snapshot);
  }
  mergeTerminal(response?.terminal);
  state.runState = response?.snapshot?.summary?.halted ? 'halted' : 'paused';
  paint();
  showNotice(label, 'success');
}

async function handleTerminalInput(text) {
  terminalInputPump.enqueue(text);
}

const terminalInputPump = createTerminalInputPump({
  sendInput: terminalInput,
  onResponse: (response) => {
    mergeTerminal(response);
    paint();
  },
  onError: (error) => {
    showNotice(error.message, 'error');
  },
  onPendingChange: (pending) => {
    setTerminalPendingInput(state, pending);
    paint();
  },
});

async function init() {
  const testsResponse = await listTests();
  setTests(state, testsResponse.tests);
  paint();
  showNotice('本地调试服务已连接，先选择测试并点击 Load。');

  elements.testSelect.addEventListener('change', (event) => {
    state.selectedTest = event.target.value;
  });
  elements.backendSelect.addEventListener('change', (event) => {
    state.backend = event.target.value;
  });

  document.addEventListener('click', (event) => {
    let needsPaint = false;
    const summary = event.target.closest?.('.panel-group__summary[data-layout-key]');
    if (summary) {
      const layoutKey = summary.dataset.layoutKey;
      event.preventDefault();
      setInspectorGroupOpen(state, layoutKey, !state.layout[layoutKey]);
      needsPaint = true;
    }

    const shouldFocusTerminal = state.terminal.connected && elements.terminal.contains(event.target);
    if (!state.terminal.connected && elements.terminal.contains(event.target)) {
      showNotice('先点击 Load，建立一个 interactive_os 会话。');
    }
    if (state.terminal.focused !== shouldFocusTerminal) {
      setTerminalFocus(state, shouldFocusTerminal);
      needsPaint = true;
    }

    if (needsPaint) {
      paint();
    }
  });

  document.addEventListener('keydown', async (event) => {
    if (!state.terminal.focused) {
      return;
    }

    const text = normalizeTerminalInput(event);
    if (!text) {
      return;
    }

    event.preventDefault();

    try {
      await handleTerminalInput(text);
    } catch (error) {
      showNotice(error.message, 'error');
    }
  });

  document.querySelector('#load-button').addEventListener('click', async () => {
    try {
      terminalInputPump.reset();
      await handleLoad();
    } catch (error) {
      state.runState = 'error';
      paint();
      showNotice(error.message, 'error');
    }
  });

  document.querySelector('#step-cycle-button').addEventListener('click', async () => {
    try {
      await handleAction(stepCycle, '已前进 1 个 cycle');
    } catch (error) {
      showNotice(error.message, 'error');
    }
  });

  document.querySelector('#step-commit-button').addEventListener('click', async () => {
    try {
      await handleAction(stepCommit, '已前进到下一个提交边界');
    } catch (error) {
      showNotice(error.message, 'error');
    }
  });

  document.querySelector('#reset-button').addEventListener('click', async () => {
    try {
      terminalInputPump.reset();
      await handleAction(resetSession, '已重置当前会话');
    } catch (error) {
      showNotice(error.message, 'error');
    }
  });

  document.querySelector('#run-button').addEventListener('click', async () => {
    try {
      await runSession(8);
      state.runState = 'running';
      paint();
      showNotice('连续运行中，可随时 Pause。', 'success');
    } catch (error) {
      showNotice(error.message, 'error');
    }
  });

  document.querySelector('#pause-button').addEventListener('click', async () => {
    try {
      const response = await pauseSession();
      if (response?.snapshot) {
        pushSnapshot(state, response.snapshot);
      }
      mergeTerminal(response?.terminal);
      state.runState = 'paused';
      paint();
      showNotice('已暂停。', 'success');
    } catch (error) {
      showNotice(error.message, 'error');
    }
  });

  connectSnapshotSocket(
    (snapshot) => {
      pushSnapshot(state, snapshot);
      state.runState = snapshot.summary?.halted ? 'halted' : state.runState;
      paint();
    },
    (message) => showNotice(message, 'error'),
    (terminal) => {
      mergeTerminal(terminal);
      paint();
    },
  );
}

init().catch((error) => {
  showNotice(error.message, 'error');
});
