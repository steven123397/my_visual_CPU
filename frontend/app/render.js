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
        test: 'linux_proto_console',
        backend: 'functional',
        gated: true,
        gatedLabel: 'Runtime Image required',
        fallbackSummary: '配置 MYCPU_LINUX_PROTO_CONSOLE_IMAGE 后，可用受控 linux_proto runtime 打开 UART 串口 console。',
        marker: 'post-init reached',
        panels: ['terminal', 'Linux Image', 'DTB', 'virtio-blk'],
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
        status: 'ready',
        localTool: 'ai_tiny_model',
        fallbackSummary: '用服务器端白名单模板生成 bounded dynamic tiny model，观察输出和 timed-simple profile。',
        marker: 'server-generated graph',
        panels: ['shape controls', 'profile', 'op summary'],
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

function isLinuxConsoleDemo(demo) {
  return demo.test === 'linux_proto_console';
}

function demoCardStatusLabel(demo, available, selected) {
  if (demo.localTool === 'ai_tiny_model') {
    return 'Run profile';
  }
  if (isLinuxConsoleDemo(demo)) {
    return available ? 'Ready' : 'Not configured';
  }
  if (available) {
    return selected ? 'Selected' : 'Open demo';
  }
  return demo.gatedLabel ?? 'Coming soon';
}

function linuxConsoleDiagnosticLabel(status) {
  switch (status) {
    case 'not-found':
      return 'Image path missing';
    case 'not-file':
      return 'Image path is not a file';
    case 'not-readable':
      return 'Image path is not readable';
    case 'missing-env':
    default:
      return 'Runtime Image required';
  }
}

function isLinuxLoadPending(state) {
  return state.runState === 'loading' && state.loadProgress?.test === 'linux_proto_console';
}

function loadProgressElapsedSeconds(progress) {
  const startedAt =
    typeof progress?.startedAt === 'number' && Number.isFinite(progress.startedAt)
      ? progress.startedAt
      : Date.now();
  const now =
    typeof progress?.now === 'number' && Number.isFinite(progress.now)
      ? progress.now
      : Date.now();
  return Math.max(0, Math.floor((now - startedAt) / 1000));
}

function renderLinuxLoadProgress(state) {
  if (!isLinuxLoadPending(state)) {
    return '';
  }

  const progress = state.loadProgress;
  const backend = progress.backend ?? state.backend ?? 'functional';
  const waitingFor = progress.waitingFor ?? 'mycpu-linux# ';
  const elapsed = loadProgressElapsedSeconds(progress);
  return `
    <div class="demo-workspace__boot-progress" role="status">
      <span>Linux boot in progress</span>
      <strong>${elapsed}s</strong>
      <em>${escapeHtml(backend)}</em>
      <code>waiting for ${escapeHtml(waitingFor)}</code>
    </div>
  `;
}

function renderLinuxConsoleDiagnostic(diagnostic) {
  if (!diagnostic || diagnostic.ready) {
    return '';
  }

  const label = linuxConsoleDiagnosticLabel(diagnostic.status);
  const detail = diagnostic.message ?? 'No session will be created until the Image is configured.';
  const pathLine = diagnostic.path
    ? `<span>Path: <code>${escapeHtml(diagnostic.path)}</code></span>`
    : '';
  return `
    <strong>${escapeHtml(label)}</strong>
    <span>${escapeHtml(detail)}</span>
    ${pathLine}
  `;
}

function renderDemoGateNote(demo, available, state) {
  if (!demo.gated || available) {
    return '';
  }
  if (isLinuxConsoleDemo(demo)) {
    const diagnostic = state.diagnostics?.linuxConsole ?? null;
    const diagnosticBody = renderLinuxConsoleDiagnostic(diagnostic);
    return `
      <div class="demo-card__gate">
        <strong>${escapeHtml(demo.gatedLabel ?? 'Runtime Image required')}</strong>
        ${diagnosticBody}
        <span>Set <code>MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image</code> before starting the frontend server.</span>
        <span>No session will be created until the Image is configured.</span>
      </div>
    `;
  }
  return `
    <div class="demo-card__gate">
      <strong>${escapeHtml(demo.gatedLabel ?? 'Coming soon')}</strong>
    </div>
  `;
}

function renderDemoCard(demo, state) {
  const entry = demo.test ? findManifestEntry(state, demo.test) : null;
  const localToolAvailable = demo.localTool === 'ai_tiny_model';
  const available = Boolean(entry) || localToolAvailable;
  const selected = available && state.selectedTest === demo.test;
  const routeLabel = demo.title;
  const title = entry?.title ?? demo.title;
  const summary = entry?.summary ?? demo.fallbackSummary ?? '';
  const marker = entry?.workload?.expectedMarker ?? demo.marker ?? '';
  const badge = entry?.badge ?? demo.badge ?? null;
  const ops = entry?.workload?.ops ?? demo.panels ?? [];
  const classes = [
    'demo-card',
    selected ? 'is-selected' : '',
    available ? 'is-available' : 'is-soon',
    localToolAvailable ? 'is-local-tool' : '',
    demo.gated && !available ? 'is-gated' : '',
    isLinuxConsoleDemo(demo) && available ? 'is-ready' : '',
  ].filter(Boolean).join(' ');
  const attrs = available
    ? (
        localToolAvailable
          ? `data-demo-tool="${escapeHtml(demo.localTool)}" role="button" tabindex="0"`
          : `data-demo-test="${escapeHtml(demo.test)}" data-demo-backend="${escapeHtml(demo.backend ?? state.backend)}" role="button" tabindex="0"`
      )
    : 'aria-disabled="true"';
  const statusLabel = demoCardStatusLabel(demo, available, selected);
  const gateNote = renderDemoGateNote(demo, available, state);

  return `
    <article class="${classes}" ${attrs}>
      <div class="demo-card__topline">
        <span>${escapeHtml(routeLabel)}</span>
        <strong>${statusLabel}</strong>
      </div>
      <h4>${escapeHtml(title)}</h4>
      <p>${escapeHtml(summary)}</p>
      <div class="demo-card__meta">
        ${badge ? `<span>${escapeHtml(badge)}</span>` : ''}
        <span>${escapeHtml(demo.backend ?? 'planned')}</span>
        <span>${escapeHtml(marker)}</span>
      </div>
      <div class="demo-card__chips">
        ${ops.slice(0, 4).map((item) => `<span>${escapeHtml(item)}</span>`).join('')}
      </div>
      ${gateNote}
    </article>
  `;
}

function selectedAiTinyModelTemplate(state) {
  return state.aiTinyModel.templates.find(
    (item) => item.id === state.aiTinyModel.parameters.template,
  ) ?? state.aiTinyModel.templates[0] ?? null;
}

function renderAiTinyModelSelect(name, label, choices = [], selectedValue, choiceLabels = {}) {
  return `
    <label class="ai-tiny-model__field">
      <span>${escapeHtml(label)}</span>
      <select data-ai-param="${escapeHtml(name)}">
        ${choices.map((choice) => `
          <option value="${escapeHtml(choice)}" ${String(choice) === String(selectedValue) ? 'selected' : ''}>
            ${escapeHtml(choiceLabels?.[choice] ?? choice)}
          </option>
        `).join('')}
      </select>
    </label>
  `;
}

function renderAiTinyModelParameterFields(state, template) {
  const fields = [
    renderAiTinyModelSelect(
      'template',
      'Template',
      state.aiTinyModel.templates.map((item) => item.id),
      state.aiTinyModel.parameters.template,
      Object.fromEntries(
        state.aiTinyModel.templates.map((item) => [item.id, item.title ?? item.id]),
      ),
    ),
  ];
  for (const [name, definition] of Object.entries(template.parameters ?? {})) {
    fields.push(renderAiTinyModelSelect(
      name,
      definition.label ?? name,
      definition.choices ?? [],
      state.aiTinyModel.parameters[name],
      definition.choiceLabels ?? {},
    ));
  }
  return fields.join('');
}

function renderAiTinyModelResult(result) {
  if (!result) {
    return '';
  }
  const profile = result.profile ?? {};
  const aggregate = result.aggregate ?? {};
  const output = result.output ?? {};
  const ops = Array.isArray(result.ops) ? result.ops : [];
  const values = Array.isArray(output.values) ? output.values.join(', ') : '-';
  const expected = Array.isArray(output.expected) ? output.expected.join(', ') : '-';
  return `
    <div class="ai-tiny-model__result">
      <div class="ai-tiny-model__result-head">
        <span>Profile result</span>
        <strong>${escapeHtml(profile.progress ?? 'unknown')}</strong>
      </div>
      <div class="ai-tiny-model__metrics">
        <span><em>shape</em><strong>${escapeHtml(profile.shapeMode ?? '-')}</strong></span>
        <span><em>runtime</em><strong>${escapeHtml(profile.runtimeShapes ?? '-')}</strong></span>
        <span><em>device cycles</em><strong>${escapeHtml(profile.deviceCycles ?? 0)}</strong></span>
        <span><em>DMA cycles</em><strong>${escapeHtml(profile.dmaCycles ?? 0)}</strong></span>
        <span><em>compute</em><strong>${escapeHtml(profile.computeCycles ?? 0)}</strong></span>
        <span><em>stall</em><strong>${escapeHtml(profile.stallCycles ?? 0)}</strong></span>
        <span><em>bytes</em><strong>${escapeHtml(profile.bytesMoved ?? 0)}</strong></span>
        <span><em>util</em><strong>${escapeHtml(profile.utilization ?? 0)}</strong></span>
      </div>
      <div class="ai-tiny-model__output">
        <span>Output <code>${escapeHtml(output.dtype ?? 'fp32')} ${escapeHtml((output.shape ?? []).join('x'))}</code></span>
        <strong>${escapeHtml(values)}</strong>
        <em>expected ${escapeHtml(expected)}</em>
      </div>
      <div class="ai-tiny-model__ops">
        <span>op summary · ${escapeHtml(aggregate.opCount ?? ops.length)} ops</span>
        ${ops.map((op) => `
          <code>${escapeHtml(op.opIndex)}:${escapeHtml(op.opcode)} retired=${escapeHtml(op.retiredOps)} compute=${escapeHtml(op.computeCycles)} stall=${escapeHtml(op.stallCycles)}</code>
        `).join('')}
      </div>
    </div>
  `;
}

function valuesMatch(actual = [], expected = []) {
  if (!Array.isArray(actual) || !Array.isArray(expected)) {
    return false;
  }
  if (actual.length !== expected.length) {
    return false;
  }
  return actual.every((value, index) => String(value) === String(expected[index]));
}

function aiTinyModelParameterEvidence(template, parameters = {}) {
  return Object.entries(template?.parameters ?? {}).map(([name, definition]) => {
    const rawValue = parameters[name];
    const displayValue =
      definition?.choiceLabels?.[rawValue] ?? rawValue ?? definition?.default ?? '-';
    const label = definition?.label ?? name;
    return `${label} ${displayValue}`;
  });
}

function renderAiTinyModelObservedEvidence(state, template) {
  const result = state.aiTinyModel.result;
  const evidenceParameters = aiTinyModelParameterEvidence(template, state.aiTinyModel.parameters);
  if (!result) {
    return `
      <article class="ai-tiny-model__guide-card ai-tiny-model__guide-card--evidence is-pending" data-ai-evidence="pending">
        <span>Observed evidence</span>
        <strong>Run profile to collect runtime evidence</strong>
        <div class="ai-tiny-model__evidence-strip">
          ${evidenceParameters.map((item) => `<code>${escapeHtml(item)}</code>`).join('')}
        </div>
      </article>
    `;
  }

  const output = result.output ?? {};
  const actual = Array.isArray(output.values) ? output.values : [];
  const expected = Array.isArray(output.expected) ? output.expected : [];
  const matched = valuesMatch(actual, expected);
  const status = matched ? 'matched' : 'mismatch';
  const statusLabel = matched
    ? 'Matched expected output'
    : 'Mismatch: actual output diverges from expected';
  const runtimeShapes = result.profile?.runtimeShapes ?? 'none';
  const actualText = actual.length > 0 ? actual.join(', ') : '-';
  const expectedText = expected.length > 0 ? expected.join(', ') : '-';

  return `
    <article class="ai-tiny-model__guide-card ai-tiny-model__guide-card--evidence is-${status}" data-ai-evidence="${status}">
      <span>Observed evidence</span>
      <strong>${escapeHtml(statusLabel)}</strong>
      <div class="ai-tiny-model__evidence-strip">
        ${evidenceParameters.map((item) => `<code>${escapeHtml(item)}</code>`).join('')}
      </div>
      <div class="ai-tiny-model__evidence-lines">
        <span>runtime ${escapeHtml(runtimeShapes)}</span>
        <span>actual ${escapeHtml(actualText)}</span>
        <span>expected ${escapeHtml(expectedText)}</span>
      </div>
    </article>
  `;
}

function renderAiTinyModelDemoGuide(state, template) {
  const demo = template?.demo ?? {};
  const proves =
    Array.isArray(demo.proves) && demo.proves.length > 0
      ? demo.proves
      : ['This whitelist template keeps the profile path observable without opening arbitrary graph authoring.'];
  const boundaries =
    Array.isArray(demo.boundaries) && demo.boundaries.length > 0
      ? demo.boundaries
      : ['Execution stays inside the server-generated whitelist contract.'];
  const expectedMarker = demo.expectedMarker ?? 'Profile output and runtime shape should match the selected whitelist case.';
  return `
    <div class="ai-tiny-model__guide">
      <article class="ai-tiny-model__guide-card">
        <span>Expected marker</span>
        <strong>${escapeHtml(expectedMarker)}</strong>
      </article>
      ${renderAiTinyModelObservedEvidence(state, template)}
      <article class="ai-tiny-model__guide-card">
        <span>What this proves</span>
        <ul>
          ${proves.map((item) => `<li>${escapeHtml(item)}</li>`).join('')}
        </ul>
      </article>
      <article class="ai-tiny-model__guide-card">
        <span>Current boundary</span>
        <ul>
          ${boundaries.map((item) => `<li>${escapeHtml(item)}</li>`).join('')}
        </ul>
      </article>
    </div>
  `;
}

function renderAiTinyModelPanel(state) {
  const template = selectedAiTinyModelTemplate(state);
  if (!template) {
    return `
      <section class="ai-tiny-model">
        <div class="ai-tiny-model__intro">
          <span>AI Accelerator</span>
          <strong>Parameterized Tiny Model</strong>
          <p>正在读取服务器端白名单模板。</p>
        </div>
      </section>
    `;
  }

  const isRunning = state.aiTinyModel.runState === 'running';
  const customGraphDisabled = template.boundary?.allowsCustomGraph === false
    ? 'Custom graph upload is disabled; the server regenerates and validates the graph package.'
    : 'Server-side validation is required before execution.';
  return `
    <section class="ai-tiny-model" data-ai-template="${escapeHtml(template.id)}">
      <div class="ai-tiny-model__intro">
        <span>AI Accelerator</span>
        <strong>${escapeHtml(template.title ?? 'Parameterized Tiny Model')}</strong>
        <p>${escapeHtml(template.summary ?? 'Server-generated bounded tiny model profile.')}</p>
      </div>
      <div class="ai-tiny-model__controls">
        ${renderAiTinyModelParameterFields(state, template)}
        <button data-action="run-ai-tiny-model" ${isRunning ? 'disabled' : ''}>
          ${isRunning ? 'Running...' : 'Run profile'}
        </button>
      </div>
      ${renderAiTinyModelDemoGuide(state, template)}
      <div class="ai-tiny-model__chips">
        ${(template.opChain ?? []).map((item) => `<span>${escapeHtml(item)}</span>`).join('')}
        <span>${escapeHtml(template.shapeMode ?? 'dynamic_bounded')}</span>
        <span>${escapeHtml(customGraphDisabled)}</span>
      </div>
      ${state.aiTinyModel.error ? `<div class="ai-tiny-model__error">${escapeHtml(state.aiTinyModel.error)}</div>` : ''}
      ${renderAiTinyModelResult(state.aiTinyModel.result)}
    </section>
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
    ${renderLinuxLoadProgress(state)}
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
    ${renderAiTinyModelPanel(state)}
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
