import { listTests, loadSession, stepCycle, stepCommit, resetSession, runSession, pauseSession, connectSnapshotSocket } from './api.js';
import { createAppState, pushSnapshot, resetHistory, setTests } from './state.js';
import { renderApp, updateControls } from './render.js';

const state = createAppState();

const elements = {
  testSelect: document.querySelector('#test-select'),
  backendSelect: document.querySelector('#backend-select'),
  statusBadge: document.querySelector('#status-badge'),
  summary: document.querySelector('#summary-slot'),
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

async function handleLoad() {
  state.runState = 'loading';
  paint();
  const response = await loadSession(state.selectedTest, state.backend);
  resetHistory(state);
  pushSnapshot(state, response.snapshot);
  state.runState = 'paused';
  paint();
  showNotice(`已加载 ${state.selectedTest}`, 'success');
}

async function handleAction(action, label) {
  const response = await action();
  if (response?.snapshot) {
    pushSnapshot(state, response.snapshot);
  }
  state.runState = response?.snapshot?.summary?.halted ? 'halted' : 'paused';
  paint();
  showNotice(label, 'success');
}

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

  document.querySelector('#load-button').addEventListener('click', async () => {
    try {
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
  );
}

init().catch((error) => {
  showNotice(error.message, 'error');
});
