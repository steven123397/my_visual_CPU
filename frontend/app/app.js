import {
  listTests,
  listAiTinyModelTemplates,
  runAiTinyModel,
  loadSession,
  stepCycle,
  stepCommit,
  resetSession,
  terminateSession,
  runSession,
  pauseSession,
  terminalInput,
  terminalOutput,
  connectSnapshotSocket,
} from './api.js';
import {
  appendTerminalOutput,
  clearLoadProgress,
  clearLoadedSession,
  createAppState,
  normalizeTerminalInput,
  pushSnapshot,
  setTerminalCollapsed,
  setTerminalFocus,
  setInspectorGroupOpen,
  setLoadedSession,
  setLoadProgress,
  setAiTinyModelTemplates,
  setAiTinyModelParameters,
  setAiTinyModelRunState,
  setAiTinyModelResult,
  setTerminalPendingInput,
  setTests,
  selectDemo,
} from './state.js';
import { formatLoadErrorMessage } from './load_error_message.js';
import { createTerminalInputPump } from './terminal_input_pump.js';
import { renderApp, updateControls } from './render.js';

const state = createAppState();

const elements = {
  testSelect: document.querySelector('#test-select'),
  backendSelect: document.querySelector('#backend-select'),
  statusBadge: document.querySelector('#status-badge'),
  desktop: document.querySelector('#desktop-shell'),
  debugInspector: document.querySelector('#debug-inspector'),
  demoWorkspace: document.querySelector('#demo-workspace-slot'),
  terminal: document.querySelector('#terminal-slot'),
  summary: document.querySelector('#summary-slot'),
  workload: document.querySelector('#workload-slot'),
  predictor: document.querySelector('#predictor-slot'),
  pipeline: document.querySelector('#pipeline-slot'),
  events: document.querySelector('#events-slot'),
  vector: document.querySelector('#vector-slot'),
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
  const requestedTest = state.selectedTest;
  const requestedBackend = state.backend;
  state.runState = 'loading';
  clearLoadedSession(state);
  state.runState = 'loading';
  setLoadProgress(state, {
    test: requestedTest,
    backend: requestedBackend,
    startedAt: Date.now(),
    waitingFor: requestedTest === 'linux_proto_console' ? 'mycpu-linux# ' : null,
    label: requestedTest === 'linux_proto_console' ? 'Booting Linux' : 'Loading demo',
  });
  paint();
  const response = await loadSession(requestedTest, requestedBackend);
  clearLoadProgress(state);
  setLoadedSession(state, {
    test: requestedTest,
    backend: response?.snapshot?.summary?.backend ?? requestedBackend,
  });
  pushSnapshot(state, response.snapshot);
  mergeTerminal(response.terminal, true);
  if (!response.terminal) {
    await syncTerminal(0, true);
  }
  state.runState = 'paused';
  paint();
  showNotice(`已加载 ${requestedTest}`, 'success');
}

async function handleTerminate() {
  await terminateSession();
  clearLoadedSession(state);
  paint();
  showNotice('已结束当前会话。', 'success');
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

async function loadAiTinyModelTemplates() {
  const response = await listAiTinyModelTemplates();
  setAiTinyModelTemplates(state, response.templates);
  paint();
}

async function handleRunAiTinyModel() {
  setAiTinyModelRunState(state, 'running', null);
  paint();
  try {
    const result = await runAiTinyModel(state.aiTinyModel.parameters);
    setAiTinyModelResult(state, result);
    paint();
    showNotice('AI tiny model profile 已完成。', 'success');
  } catch (error) {
    setAiTinyModelRunState(state, 'error', error.message);
    paint();
    showNotice(error.message, 'error');
  }
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
  setTests(state, testsResponse.tests, testsResponse.diagnostics);
  paint();
  await loadAiTinyModelTemplates();
  showNotice('本地调试服务已连接，先选择测试并点击 Load。');

  elements.testSelect.addEventListener('change', (event) => {
    state.selectedTest = event.target.value;
  });
  elements.backendSelect.addEventListener('change', (event) => {
    state.backend = event.target.value;
  });

  document.addEventListener('click', async (event) => {
    let needsPaint = false;
    const summary = event.target.closest?.('.panel-group__summary[data-layout-key]');
    if (summary) {
      const layoutKey = summary.dataset.layoutKey;
      event.preventDefault();
      setInspectorGroupOpen(state, layoutKey, !state.layout[layoutKey]);
      needsPaint = true;
    }

    const demoCard = event.target.closest?.('.demo-card[data-demo-test]');
    if (demoCard) {
      event.preventDefault();
      const didSelect = selectDemo(state, demoCard.dataset.demoTest, demoCard.dataset.demoBackend);
      if (didSelect) {
        showNotice(`已选择 ${state.selectedTest}，点击 Load 启动。`, 'success');
        needsPaint = true;
      }
    }

    const aiToolCard = event.target.closest?.('.demo-card[data-demo-tool="ai_tiny_model"]');
    if (aiToolCard) {
      event.preventDefault();
      showNotice('已打开 AI 参数化小模型面板，可直接运行 host profile。', 'success');
      document.querySelector('.ai-tiny-model')?.scrollIntoView?.({ behavior: 'smooth', block: 'center' });
      needsPaint = true;
    }

    const aiRunButton = event.target.closest?.('[data-action="run-ai-tiny-model"]');
    if (aiRunButton) {
      event.preventDefault();
      await handleRunAiTinyModel();
      return;
    }

    const terminalToggle = event.target.closest?.('[data-action="toggle-terminal-collapsed"]');
    if (terminalToggle) {
      event.preventDefault();
      setTerminalCollapsed(state, !state.layout.terminalCollapsed);
      needsPaint = true;
    }

    const shouldFocusTerminal =
      !state.layout.terminalCollapsed &&
      state.terminal.connected &&
      elements.terminal.contains(event.target) &&
      !event.target.closest?.('[data-action="toggle-terminal-collapsed"]');
    if (!state.terminal.connected && elements.terminal.contains(event.target) && !terminalToggle) {
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
    const demoCard = event.target.closest?.('.demo-card[data-demo-test]');
    if (demoCard && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      const didSelect = selectDemo(state, demoCard.dataset.demoTest, demoCard.dataset.demoBackend);
      if (didSelect) {
        showNotice(`已选择 ${state.selectedTest}，点击 Load 启动。`, 'success');
        paint();
      }
      return;
    }

    const aiToolCard = event.target.closest?.('.demo-card[data-demo-tool="ai_tiny_model"]');
    if (aiToolCard && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      showNotice('已打开 AI 参数化小模型面板，可直接运行 host profile。', 'success');
      document.querySelector('.ai-tiny-model')?.scrollIntoView?.({ behavior: 'smooth', block: 'center' });
      return;
    }

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

  document.addEventListener('change', (event) => {
    const param = event.target.dataset?.aiParam;
    if (!param) {
      return;
    }
    const template = state.aiTinyModel.templates.find(
      (item) => item.id === state.aiTinyModel.parameters.template,
    ) ?? state.aiTinyModel.templates[0] ?? null;
    const definition =
      param === 'template'
        ? { choices: state.aiTinyModel.templates.map((item) => item.id), default: state.aiTinyModel.parameters.template }
        : template?.parameters?.[param];
    const numericChoices = Array.isArray(definition?.choices) && definition.choices.every((choice) => typeof choice === 'number');
    const value = numericChoices ? Number(event.target.value) : event.target.value;
    setAiTinyModelParameters(state, { [param]: value });
    if (param === 'template') {
      setAiTinyModelTemplates(state, state.aiTinyModel.templates);
    }
    paint();
  });

  document.querySelector('#load-button').addEventListener('click', async () => {
    try {
      terminalInputPump.reset();
      await handleLoad();
    } catch (error) {
      const loadContext = state.loadProgress ?? {
        test: state.selectedTest,
        backend: state.backend,
      };
      state.runState = 'error';
      clearLoadProgress(state);
      paint();
      showNotice(formatLoadErrorMessage(error, loadContext), 'error');
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

  document.querySelector('#terminate-button').addEventListener('click', async () => {
    try {
      terminalInputPump.reset();
      await handleTerminate();
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
      if (
        !state.loadedSession &&
        terminal?.reset === true &&
        (terminal.text ?? '').length === 0 &&
        (terminal.nextOffset ?? terminal.next_offset ?? 0) === 0
      ) {
        clearLoadedSession(state);
        paint();
        return;
      }
      mergeTerminal(terminal);
      paint();
    },
  );
}

init().catch((error) => {
  showNotice(error.message, 'error');
});
