import { buildTimelineRows, diffRegisters, shouldAutoScrollToBottom } from './state.js';
import { renderTerminal } from './components/terminal.js';
import { renderPipelineBoard, renderTimeline } from './components/pipeline.js';
import {
  renderSummary,
  renderWorkloadPanel,
  renderPredictor,
  renderVectorPanel,
  renderOooPanel,
  renderArchitectureGroup,
  renderPlatformGroup,
} from './components/panels.js';

const DEMO_GROUPS = [
  {
    title: 'OS Bring-up',
    summary: '启动 guest monitor、xv6 和 Linux 路线，观察 boot log、UART 和系统 marker。',
    demos: [
      {
        title: 'interactive_os Monitor',
        test: 'guest_interactive_os_demo',
        backend: 'pipeline',
        fallbackSummary: '输入 help、regs、disk 和 pagewalk，直接体验浏览器终端到 guest monitor 的闭环。',
        marker: 'monitor> ',
        panels: ['terminal', 'registers', 'devices'],
      },
      {
        title: 'Supervisor Platform',
        test: 'guest_supervisor_demo',
        backend: 'pipeline',
        fallbackSummary: '沿 supervisor demo 观察 trap、MMIO、timer 和 storage platform smoke。',
        marker: 'supervisor demo',
        panels: ['pipeline', 'devices', 'events'],
      },
      {
        title: 'Linux Serial Console',
        status: 'soon',
        fallbackSummary: 'Wave 7 后续会接入受控 Linux 串口 shell；当前首页和文档先展示 bring-up 路线。',
        marker: 'timerfd-one-shot-readback-ok',
        panels: ['terminal', 'session limits'],
      },
    ],
  },
  {
    title: 'Machine Inspector',
    summary: '把 pipeline、register、CSR、memory 和 platform device 状态放进同一个观察面。',
    demos: [
      {
        title: 'Pipeline Inspector',
        test: 'hello',
        backend: 'pipeline',
        fallbackSummary: '用最小 asm workload 快速观察五级 pipeline、commit、stall 和事件流。',
        marker: 'halt',
        panels: ['pipeline', 'timeline', 'registers'],
      },
      {
        title: 'Memory & Platform',
        test: 'supervisor_platform_smoke',
        backend: 'pipeline',
        fallbackSummary: '观察 Sv39 / MMIO / storage 相关 platform smoke 的设备状态。',
        marker: 'platform smoke',
        panels: ['AddressSpace', 'CLINT', 'PLIC', 'virtio-blk'],
      },
    ],
  },
  {
    title: 'AI Accelerator',
    summary: '运行 guest AI 设备 demo，查看 MMIO doorbell、DMA、completion 和 profile counters。',
    demos: [
      {
        title: 'AI Accelerator Demo',
        test: 'guest_ai_accel_demo',
        backend: 'pipeline',
        fallbackSummary: '通过 MMIO 提交 graph package，并用 KMVAI 验证 guest 到设备闭环。',
        marker: 'KMVAI',
        panels: ['AI counters', 'DMA bytes', 'completion'],
      },
      {
        title: 'Parameterized Tiny Model',
        status: 'soon',
        fallbackSummary: 'Wave 7 后续加入白名单参数化小模型，用受限 shape 和 dtype 生成可运行 graph。',
        marker: 'generated graph',
        panels: ['shape controls', 'profile'],
      },
    ],
  },
  {
    title: 'Runtime Labs',
    summary: '体验向量 workload、L1D / shadow cache 和 JIT / DBT opt-in runtime 数据。',
    demos: [
      {
        title: 'Vector CNN',
        test: 'guest_vector_cnn_demo',
        backend: 'pipeline',
        fallbackSummary: '固定输入和卷积核的 conv -> relu 样本，用 V3OK 验证 vector CNN 闭环。',
        marker: 'V3OK',
        panels: ['vector registers', 'CNN lanes'],
      },
      {
        title: 'JIT / DBT Runtime Labs',
        status: 'soon',
        fallbackSummary: '展示 opt-in runtime stats：hit、miss、emit、fallback、invalidate 和 differential mismatch。',
        marker: 'jit-dispatch',
        panels: ['stats', 'fallback', 'invalidate'],
      },
    ],
  },
];

function queryEventList(...slots) {
  for (const slot of slots) {
    const list = slot?.querySelector?.('.event-list');
    if (list) {
      return list;
    }
  }
  return null;
}

function loadedTestEntry(state) {
  return state.tests.find((item) => item.name === state.loadedSession?.test) ?? null;
}

function selectedTestEntry(state) {
  return state.tests.find((item) => item.name === state.selectedTest) ?? null;
}

function escapeHtml(text = '') {
  return String(text)
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

function findManifestEntry(state, testName) {
  return state.tests.find((item) => item.name === testName) ?? null;
}

function renderDemoCard(demo, state) {
  const entry = demo.test ? findManifestEntry(state, demo.test) : null;
  const available = Boolean(entry);
  const selected = available && state.selectedTest === demo.test;
  const routeLabel = demo.title;
  const title = entry?.title ?? demo.title;
  const summary = entry?.summary ?? demo.fallbackSummary ?? '';
  const marker = entry?.workload?.expectedMarker ?? demo.marker ?? '';
  const ops = entry?.workload?.ops ?? demo.panels ?? [];
  const classes = [
    'demo-card',
    selected ? 'is-selected' : '',
    available ? 'is-available' : 'is-soon',
  ].filter(Boolean).join(' ');
  const attrs = available
    ? `data-demo-test="${escapeHtml(demo.test)}" data-demo-backend="${escapeHtml(demo.backend ?? state.backend)}" role="button" tabindex="0"`
    : 'aria-disabled="true"';
  const statusLabel = available ? (selected ? 'Selected' : 'Open demo') : 'Coming soon';

  return `
    <article class="${classes}" ${attrs}>
      <div class="demo-card__topline">
        <span>${escapeHtml(routeLabel)}</span>
        <strong>${statusLabel}</strong>
      </div>
      <h4>${escapeHtml(title)}</h4>
      <p>${escapeHtml(summary)}</p>
      <div class="demo-card__meta">
        <span>${escapeHtml(demo.backend ?? 'planned')}</span>
        <span>${escapeHtml(marker)}</span>
      </div>
      <div class="demo-card__chips">
        ${ops.slice(0, 4).map((item) => `<span>${escapeHtml(item)}</span>`).join('')}
      </div>
    </article>
  `;
}

function renderDemoWorkspace(state) {
  const selected = selectedTestEntry(state);
  return `
    <div class="demo-workspace__intro">
      <span>Demo workspace</span>
      <strong>选择 demo 卡片后，再用下方控制条启动。</strong>
    </div>
    <div class="demo-workspace__status">
      <span>Selected route</span>
      <strong>${escapeHtml(selected?.title ?? selected?.menuLabel ?? state.selectedTest)}</strong>
      <em>${escapeHtml(state.backend)}</em>
    </div>
    <div class="demo-workspace__grid">
      ${DEMO_GROUPS.map((group) => `
        <section class="demo-group">
          <div class="demo-group__header">
            <h3>${escapeHtml(group.title)}</h3>
            <p>${escapeHtml(group.summary)}</p>
          </div>
          <div class="demo-group__cards">
            ${group.demos.map((demo) => renderDemoCard(demo, state)).join('')}
          </div>
        </section>
      `).join('')}
    </div>
  `;
}

export function renderApp(elements, state) {
  const snapshot = state.currentSnapshot;
  const previous = state.history.length > 1 ? state.history[state.history.length - 2] : null;
  const registers = diffRegisters(previous, snapshot);
  const timelineRows = buildTimelineRows(state.history).slice().reverse();
  const currentTest = loadedTestEntry(state);
  const currentBackend = snapshot?.summary?.backend ?? state.loadedSession?.backend ?? null;
  const previousEventList = queryEventList(elements.devices, elements.events);
  const keepEventsPinned = shouldAutoScrollToBottom(previousEventList);
  const previousTerminal = elements.terminal.querySelector('.terminal-scrollport');
  const keepTerminalPinned = shouldAutoScrollToBottom(previousTerminal);
  const previousTerminalScrollTop = previousTerminal?.scrollTop ?? 0;

  if (elements.demoWorkspace) {
    elements.demoWorkspace.innerHTML = renderDemoWorkspace(state);
  }
  elements.desktop.dataset.debugOpen = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.desktop.dataset.terminalCollapsed = state.layout.terminalCollapsed ? 'true' : 'false';
  elements.debugInspector.dataset.open = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.terminal.innerHTML = renderTerminal(state);
  elements.summary.innerHTML = renderSummary(snapshot, state.runState);
  if (elements.workload) {
    elements.workload.innerHTML = renderWorkloadPanel(currentTest, snapshot);
  }
  elements.predictor.innerHTML = renderPredictor(snapshot);
  elements.pipeline.innerHTML = snapshot
    ? `${renderPipelineBoard(snapshot)}${renderTimeline(timelineRows)}`
    : `${renderPipelineBoard(snapshot)}${renderTimeline(timelineRows)}<div class="empty-state empty-state-hint">选择测试用例并点击 <strong>Load</strong> 开始调试会话</div>`;
  elements.events.innerHTML = renderOooPanel(snapshot);
  if (elements.vector) {
    elements.vector.innerHTML = renderVectorPanel(snapshot, previous, currentTest, currentBackend);
  }
  elements.devices.innerHTML = renderPlatformGroup(snapshot, state.layout.platformGroupOpen);
  elements.registers.innerHTML = renderArchitectureGroup(snapshot, registers, state.layout.architectureGroupOpen);
  elements.csrs.innerHTML = '';
  elements.bus.innerHTML = '';

  const nextEventList = queryEventList(elements.devices, elements.events);
  if (nextEventList && keepEventsPinned) {
    nextEventList.scrollTop = nextEventList.scrollHeight;
  }

  const nextTerminal = elements.terminal.querySelector('.terminal-scrollport');
  if (nextTerminal) {
    if (keepTerminalPinned) {
      nextTerminal.scrollTop = nextTerminal.scrollHeight;
    } else {
      nextTerminal.scrollTop = previousTerminalScrollTop;
    }
  }
}

export function updateControls(elements, state) {
  elements.testSelect.innerHTML = state.tests.map((item) => `
    <option value="${item.name}" ${item.name === state.selectedTest ? 'selected' : ''}>
      ${item.menuLabel ?? item.name}${item.hasDisk ? ' [disk]' : ''}
    </option>
  `).join('');
  elements.backendSelect.value = state.backend;
  elements.statusBadge.textContent = state.runState;
}
