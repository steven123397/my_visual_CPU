import {
  getAuthSession,
  jitDispatch,
  listTests,
  listAiTinyModelTemplates,
  login,
  runAiTinyModel,
  loadSession,
  logout,
  releaseControl,
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
  setLoadedSession,
  setLoadProgress,
  setAiTinyModelTemplates,
  setAiTinyModelParameters,
  setAiTinyModelRunState,
  setAiTinyModelResult,
  setJitDispatchRunState,
  setJitDispatchResult,
  setAuthState,
  setLoginError,
  setLoginPending,
  setTerminalPendingInput,
  setTests,
  selectDemo,
  selectScenario,
  syncScenarioSessionSelection,
  togglePanelCollapsed,
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
  aiLab: document.querySelector('#ai-lab-slot'),
  predictor: document.querySelector('#predictor-slot'),
  pipeline: document.querySelector('#pipeline-slot'),
  events: document.querySelector('#events-slot'),
  vector: document.querySelector('#vector-slot'),
  devices: document.querySelector('#devices-slot'),
  registers: document.querySelector('#registers-slot'),
  csrs: document.querySelector('#csrs-slot'),
  bus: document.querySelector('#bus-slot'),
  guide: document.querySelector('#guide-slot'),
  notice: document.querySelector('#notice'),
  authPanel: document.querySelector('#auth-panel'),
};
let snapshotSocket = null;

function shouldConnectRealtime() {
  return !state.auth.required || state.auth.authenticated;
}

function applyPanelCollapses() {
  const collapsed = new Set(state.layout.collapsedPanels);
  document.querySelectorAll('.panel').forEach((panel) => {
    const panelClass = Array.from(panel.classList).find((c) => c.startsWith('panel-')) ?? '';
    const isCollapsed = collapsed.has(panelClass);
    panel.dataset.collapsed = String(isCollapsed);

    const header = panel.querySelector('.panel-header');
    if (!header) {
      return;
    }
    let toggle = header.querySelector('.panel-toggle');
    if (!toggle) {
      toggle = document.createElement('button');
      toggle.type = 'button';
      toggle.className = 'panel-toggle';
      toggle.dataset.panelClass = panelClass;
      header.appendChild(toggle);
    }
    toggle.textContent = isCollapsed ? '+' : '−';
    toggle.setAttribute('aria-label', isCollapsed ? '展开面板' : '折叠面板');
  });
}

function paint() {
  updateControls(elements, state);
  renderApp(elements, state);
  applyPanelCollapses();
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

async function refreshAuthState() {
  const response = await getAuthSession();
  setAuthState(state, response.auth);
  if (response.auth?.authenticated) {
    setLoginError(state, null);
  } else {
    setTests(state, [], {});
    clearLoadedSession(state);
  }
  paint();
}

async function handleLogin(form) {
  const formData = new FormData(form);
  const username = String(formData.get('username') ?? '');
  const password = String(formData.get('password') ?? '');
  setLoginPending(state, true);
  setLoginError(state, null);
  paint();
  try {
    const response = await login(username, password);
    setAuthState(state, response.auth);
    setLoginPending(state, false);
    setLoginError(state, null);
    paint();
    showNotice(`已登录 ${response.auth.username}。`, 'success');
    await initDataAfterAuth();
    connectRealtime();
  } catch (error) {
    setLoginPending(state, false);
    setLoginError(state, error.message);
    paint();
    showNotice(error.message, 'error');
  }
}

async function handleLogout() {
  const response = await logout();
  setAuthState(state, response.auth);
  setTests(state, [], {});
  clearLoadedSession(state);
  paint();
  showNotice('已退出登录。', 'success');
  connectRealtime();
}

async function handleReleaseControl() {
  const response = await releaseControl();
  setAuthState(state, response.auth);
  paint();
  showNotice('已释放控制权。', 'success');
}

async function initDataAfterAuth() {
  const testsResponse = await listTests();
  setTests(state, testsResponse.tests, testsResponse.diagnostics);
  paint();
  await loadAiTinyModelTemplates();
}

function connectRealtime() {
  snapshotSocket?.close?.();
  snapshotSocket = null;
  if (!shouldConnectRealtime()) {
    return;
  }
  snapshotSocket = connectSnapshotSocket(
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

async function handleRunJitDispatch() {
  setJitDispatchRunState(state, 'running', null);
  paint();
  try {
    const result = await jitDispatch();
    setJitDispatchResult(state, result.summary ?? null);
    paint();
    showNotice('JIT / DBT runtime probe 已刷新。', 'success');
  } catch (error) {
    setJitDispatchRunState(state, 'error', error.message);
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
  paint();
  await refreshAuthState();
  if (state.auth.required && !state.auth.authenticated) {
    showNotice('请输入管理员账号登录后再操作调试服务。');
  } else {
    await initDataAfterAuth();
    showNotice('本地调试服务已连接，先选择测试并点击 Load。');
  }
  connectRealtime();

  elements.testSelect.addEventListener('change', (event) => {
    selectDemo(state, event.target.value, state.backend);
    paint();
  });
  elements.backendSelect.addEventListener('change', (event) => {
    state.backend = event.target.value;
  });

  document.addEventListener('click', async (event) => {
    let needsPaint = false;
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

    const scenarioCard = event.target.closest?.('.demo-card[data-scenario-key]');
    if (scenarioCard) {
      event.preventDefault();
      const didSelect = selectScenario(
        state,
        scenarioCard.dataset.scenarioKey,
        scenarioCard.dataset.scenarioTest,
        scenarioCard.dataset.scenarioBackend,
      );
      if (didSelect) {
        showNotice(`已切到 ${scenarioCard.dataset.scenarioKey} 专题，可先阅读工作台说明。`, 'success');
        needsPaint = true;
      }
    }

    const syncScenarioButton = event.target.closest?.('[data-action="sync-scenario-session"]');
    if (syncScenarioButton) {
      event.preventDefault();
      const targetTest = syncScenarioButton.dataset.scenarioTest;
      const targetBackend = syncScenarioButton.dataset.scenarioBackend;
      const didSync = syncScenarioSessionSelection(state, targetTest, targetBackend);
      if (didSync) {
        showNotice(`已同步到 ${targetTest}，点击 Load 启动当前场景。`, 'success');
        paint();
      }
    }

    const scenarioLoadButton = event.target.closest?.('[data-action="load-current-session"]');
    if (scenarioLoadButton) {
      event.preventDefault();
      document.querySelector('#load-button')?.click();
    }

    const openLiveButton = event.target.closest?.('[data-action="open-scenario-live"]');
    if (openLiveButton) {
      event.preventDefault();
      const targetTest = openLiveButton.dataset.scenarioTest;
      const targetBackend = openLiveButton.dataset.scenarioBackend;
      const didSelect = selectDemo(state, targetTest, targetBackend);
      if (didSelect) {
        showNotice(`已切回 ${targetTest} live shell，可直接 Load。`, 'success');
        paint();
      }
    }

    const aiRunButton = event.target.closest?.('[data-action="run-ai-tiny-model"]');
    if (aiRunButton) {
      event.preventDefault();
      await handleRunAiTinyModel();
      return;
    }

    const jitRunButton = event.target.closest?.('[data-action="run-jit-dispatch"]');
    if (jitRunButton) {
      event.preventDefault();
      await handleRunJitDispatch();
      return;
    }

    const terminalToggle = event.target.closest?.('[data-action="toggle-terminal-collapsed"]');
    if (terminalToggle) {
      event.preventDefault();
      setTerminalCollapsed(state, !state.layout.terminalCollapsed);
      needsPaint = true;
    }

    const panelToggle = event.target.closest?.('.panel-toggle');
    if (panelToggle) {
      event.preventDefault();
      event.stopPropagation();
      const panelClass = panelToggle.dataset.panelClass;
      if (panelClass) {
        togglePanelCollapsed(state, panelClass);
        paint();
      }
      return;
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

  document.addEventListener('submit', async (event) => {
    const loginForm = event.target.closest?.('#auth-login-form');
    if (!loginForm) {
      return;
    }
    event.preventDefault();
    await handleLogin(loginForm);
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

    const scenarioCard = event.target.closest?.('.demo-card[data-scenario-key]');
    if (scenarioCard && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      const didSelect = selectScenario(
        state,
        scenarioCard.dataset.scenarioKey,
        scenarioCard.dataset.scenarioTest,
        scenarioCard.dataset.scenarioBackend,
      );
      if (didSelect) {
        showNotice(`已切到 ${scenarioCard.dataset.scenarioKey} 专题，可先阅读工作台说明。`, 'success');
        paint();
      }
      return;
    }

    const syncScenarioButton = event.target.closest?.('[data-action="sync-scenario-session"]');
    if (syncScenarioButton && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      const didSync = syncScenarioSessionSelection(
        state,
        syncScenarioButton.dataset.scenarioTest,
        syncScenarioButton.dataset.scenarioBackend,
      );
      if (didSync) {
        showNotice(`已同步到 ${syncScenarioButton.dataset.scenarioTest}，点击 Load 启动当前场景。`, 'success');
        paint();
      }
      return;
    }

    const scenarioLoadButton = event.target.closest?.('[data-action="load-current-session"]');
    if (scenarioLoadButton && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      document.querySelector('#load-button')?.click();
      return;
    }

    const openLiveButton = event.target.closest?.('[data-action="open-scenario-live"]');
    if (openLiveButton && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      const didSelect = selectDemo(
        state,
        openLiveButton.dataset.scenarioTest,
        openLiveButton.dataset.scenarioBackend,
      );
      if (didSelect) {
        showNotice(`已切回 ${openLiveButton.dataset.scenarioTest} live shell，可直接 Load。`, 'success');
        paint();
      }
      return;
    }

    const jitRunButton = event.target.closest?.('[data-action="run-jit-dispatch"]');
    if (jitRunButton && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      await handleRunJitDispatch();
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

  document.addEventListener('click', async (event) => {
    const logoutButton = event.target.closest?.('#logout-button');
    if (logoutButton) {
      event.preventDefault();
      try {
        await handleLogout();
      } catch (error) {
        showNotice(error.message, 'error');
      }
      return;
    }
    const releaseButton = event.target.closest?.('#release-control-button');
    if (releaseButton) {
      event.preventDefault();
      try {
        await handleReleaseControl();
      } catch (error) {
        showNotice(error.message, 'error');
      }
    }
  });

}

init().catch((error) => {
  showNotice(error.message, 'error');
});
