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
    title: 'System Labs',
    summary: '从最小 monitor 到 supervisor 平台闭环，先验证系统可启动、可交互、可观察。',
    demos: [
      {
        title: 'interactive_os Monitor',
        test: 'guest_interactive_os_demo',
        backend: 'pipeline',
        fallbackSummary: '输入 help、regs、disk 和 pagewalk，直接体验浏览器终端到 guest monitor 的闭环。',
        marker: 'monitor> ',
        panels: ['terminal', 'registers', 'devices'],
        brief: 'Operate the guest monitor through the browser terminal and confirm command roundtrips before widening into larger OS bring-up flows.',
        primaryStage: 'Terminal + session summary',
        inspectorFocus: [
          'Terminal I/O and prompt settling',
          'Register and device state after monitor commands',
          'Pipeline timeline once the session is loaded',
        ],
        proves: [
          'The browser terminal, debug CLI session, and guest monitor form one closed interactive loop.',
          'A small but real OS-like control path is observable before Linux distro bring-up.',
        ],
        boundary: 'This route is a curated monitor session, not a general-purpose userland shell.',
      },
      {
        title: 'Supervisor Platform',
        test: 'guest_supervisor_demo',
        backend: 'pipeline',
        fallbackSummary: '沿 supervisor demo 观察 trap、MMIO、timer 和 storage platform smoke。',
        marker: 'supervisor demo',
        panels: ['pipeline', 'devices', 'events'],
        brief: 'Inspect the supervisor-platform checkpoint where traps, timer delivery, MMIO, and storage coordination converge in one guest path.',
        primaryStage: 'Pipeline board + platform inspector',
        inspectorFocus: [
          'Trap and interrupt-related pipeline events',
          'Platform device state and event flow',
          'Storage and timer side effects in one snapshot stream',
        ],
        proves: [
          'Supervisor-mode runtime and platform devices can complete a controlled smoke path together.',
          'The platform path remains observable from pipeline to device counters.',
        ],
        boundary: 'This is still a proof-oriented platform smoke, not a full multi-process operating system route.',
      },
    ],
  },
  {
    title: 'Linux Distro Labs',
    summary: '把 Linux 串口会话、发行版证据链和 capability 收口组织成一组连续实验。',
    demos: [
      {
        title: 'Linux Serial Console',
        test: 'linux_proto_console',
        backend: 'functional',
        gated: true,
        gatedLabel: 'Runtime Image required',
        fallbackSummary: '配置 MYCPU_LINUX_PROTO_CONSOLE_IMAGE 后，可用受控 linux_proto runtime 打开 UART 串口 console。',
        marker: 'post-init reached',
        panels: ['terminal', 'Linux Image', 'DTB', 'virtio-blk'],
        brief: 'Boot the curated Linux serial route, reach a live prompt over UART, and keep the distro-facing runtime contract visible beside the session.',
        primaryStage: 'Terminal + distro contract',
        inspectorFocus: [
          'UART output and shell prompt settling',
          'Linux runtime assets and backend selection',
          'Platform devices relevant to boot and console I/O',
        ],
        proves: [
          'The frontend can load a real Linux Image through the existing debug session flow.',
          'A controlled Linux shell path is available without hiding asset gating and runtime boundaries.',
        ],
        boundary: 'This is a curated serial shell route, not a general-purpose cloud VM or arbitrary distro launcher.',
        assetNote: 'Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image before starting the frontend server.',
      },
      {
        title: 'Alpine Distro Evidence',
        scenarioKey: 'linux_alpine_evidence',
        scenarioTest: 'linux_proto_console',
        backend: 'functional',
        fallbackSummary: '围绕 Alpine external rootfs 的 shell、filesystem、process 和 FS-state 正向证据整理专题视图。',
        marker: 'filesystem / process / fs-state',
        panels: ['shell', 'filesystem', 'process', 'FP roundtrip'],
        brief: 'Surface the external Alpine evidence chain as a guided lab instead of scattering it across status notes and one-off probes.',
        primaryStage: 'Evidence cards + terminal excerpts',
        inspectorFocus: [
          'Filesystem and process-control checkpoints',
          'FS-state roundtrip interpretation',
          'External asset contract and verification scope',
        ],
        proves: [
          'Linux distro support is presented as a sequence of verified contracts, not as a single shell screenshot.',
        ],
        boundary: 'This slice is planned; it should not claim complete distro compatibility before the underlying contracts are finished.',
      },
      {
        title: 'Capability & ISA Matrix',
        scenarioKey: 'linux_capability_isa_matrix',
        scenarioTest: 'linux_proto_console',
        backend: 'functional',
        fallbackSummary: '把 riscv,isa、hwcap、FCSR/FP 和 guest-visible capability 的收口做成专题观察面。',
        marker: 'capability closure',
        panels: ['riscv,isa', 'hwcap', 'FP / FCSR'],
        brief: 'Turn Linux capability and ISA closure into an explicit lab so the remaining platform gaps can be explained next to live evidence.',
        primaryStage: 'Capability matrix + probe evidence',
        inspectorFocus: [
          'Guest-visible capability advertisement',
          'FCSR and floating-point contract evidence',
          'Remaining distro-facing ISA gaps',
        ],
        proves: [
          'Linux capability closure is a platform contract problem, not just a shell-demo concern.',
        ],
        boundary: 'This lab remains planned until the guest-visible capability contract is further tightened.',
      },
    ],
  },
  {
    title: 'Machine Labs',
    summary: '围绕 pipeline、寄存器、CSR、memory 与 platform 观察面组织微架构实验。',
    demos: [
      {
        title: 'Pipeline Inspector',
        test: 'hello',
        backend: 'pipeline',
        fallbackSummary: '用最小 asm workload 快速观察五级 pipeline、commit、stall 和事件流。',
        marker: 'halt',
        panels: ['pipeline', 'timeline', 'registers'],
        brief: 'Use the smallest assembly workload to inspect pipeline stages, commit cadence, and control-flow events without workload noise.',
        primaryStage: 'Pipeline board + timeline',
        inspectorFocus: [
          'Stage occupancy and commit cadence',
          'Redirect, stall, and event annotations',
          'Register diffs beside pipeline snapshots',
        ],
        proves: [
          'The frontend can expose pipeline internals without inventing simulator state.',
          'Microarchitecture observation stays usable even on minimal workloads.',
        ],
        boundary: 'This route prioritizes observability over workload richness.',
      },
      {
        title: 'Memory & Platform',
        test: 'supervisor_platform_smoke',
        backend: 'pipeline',
        fallbackSummary: '观察 Sv39 / MMIO / storage 相关 platform smoke 的设备状态。',
        marker: 'platform smoke',
        panels: ['AddressSpace', 'CLINT', 'PLIC', 'virtio-blk'],
        brief: 'Trace memory-system and MMIO-facing platform behavior through snapshots, bus state, and device counters.',
        primaryStage: 'Platform inspector + bus/events',
        inspectorFocus: [
          'Bus access summary and MMIO outcome',
          'Platform devices and event stream',
          'Address-space-adjacent observations from the current snapshot',
        ],
        proves: [
          'Platform-side observability is available in the same workbench as execution state.',
        ],
        boundary: 'This route is still a guided platform smoke, not a full tracing environment.',
      },
    ],
  },
  {
    title: 'AI Labs',
    summary: '把 guest accelerator、白名单 tiny model 和后续 task-spec workload 组织成受控 AI 实验域。',
    demos: [
      {
        title: 'AI Accelerator Demo',
        test: 'guest_ai_accel_demo',
        backend: 'pipeline',
        fallbackSummary: '通过 MMIO 提交 graph package，并用 KMVAI 验证 guest 到设备闭环。',
        marker: 'KMVAI',
        panels: ['AI counters', 'DMA bytes', 'completion'],
        brief: 'Run the guest accelerator demo and correlate MMIO submission, DMA movement, completion, and device-side counters in one view.',
        primaryStage: 'Terminal + AI counters',
        inspectorFocus: [
          'Doorbell, DMA, completion, and utilization counters',
          'Guest-visible marker versus device-side profile data',
          'AI-specific workload guide and current runtime scope',
        ],
        proves: [
          'The accelerator path is observable from guest submission through device completion.',
        ],
        boundary: 'This route exercises a curated accelerator workload, not arbitrary user-supplied models.',
      },
      {
        title: 'Parameterized Tiny Model',
        status: 'ready',
        localTool: 'ai_tiny_model',
        fallbackSummary: '用服务器端白名单模板生成 bounded dynamic tiny model，观察输出和 timed-simple profile。',
        marker: 'server-generated graph',
        panels: ['shape controls', 'profile', 'op summary'],
        brief: 'Use a server-generated whitelist template to inspect bounded runtime shapes, expected output, and accelerator profile counters together.',
        primaryStage: 'Profile result + runtime counters',
        inspectorFocus: [
          'Template-scoped parameters and runtime shapes',
          'Observed output versus expected output',
          'DMA / compute / stall / utilization breakdown',
        ],
        proves: [
          'The AI lab can expose a constrained user-task entry point without opening arbitrary graph upload.',
        ],
        boundary: 'Execution stays inside whitelist templates and server-side validation.',
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
        brief: 'Inspect a fixed vector CNN workload with live vector-register context and workload-specific guide rails.',
        primaryStage: 'Vector register view + workload guide',
        inspectorFocus: [
          'Vector-register state and changed lanes',
          'CNN lane summary and workload-specific evidence',
          'Vector-flavored pipeline stages when visible',
        ],
        proves: [
          'Vector and ML-oriented runtime behavior can be explained in the same workbench as OS demos.',
        ],
        boundary: 'This route is a fixed workload sample, not a general vector-kernel laboratory.',
      },
      {
        title: 'JIT / DBT Runtime Labs',
        scenarioKey: 'jit_runtime_lab',
        scenarioTest: 'guest_vector_cnn_demo',
        backend: 'pipeline',
        fallbackSummary: '展示 opt-in runtime stats：hit、miss、emit、fallback、invalidate 和 differential mismatch。',
        marker: 'jit-dispatch',
        panels: ['stats', 'fallback', 'invalidate'],
        brief: 'Use a synced runtime-friendly workload to request the current JIT / DBT dry-run summary and keep it visible beside the live session.',
        primaryStage: 'Runtime evidence topic',
        inspectorFocus: [
          'hit / miss / emit / invalidate counters',
          'fallback and differential mismatch evidence',
          'Current runtime boundary versus future backend ambitions',
        ],
        proves: [
          'The runtime line can be presented as evidence-first instrumentation rather than as a hidden prototype.',
        ],
        boundary: 'Current output reflects the existing dry-run dispatch contract only; it does not enable a default JIT backend.',
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

function findDemoRecordByTest(testName) {
  for (const group of DEMO_GROUPS) {
    for (const demo of group.demos) {
      if (demo.test === testName) {
        return { group, demo };
      }
    }
  }
  return null;
}

function findDemoRecordByScenarioKey(scenarioKey) {
  for (const group of DEMO_GROUPS) {
    for (const demo of group.demos) {
      const demoKey = demo.scenarioKey ?? demo.test ?? demo.localTool ?? demo.title;
      if (demoKey === scenarioKey) {
        return { group, demo };
      }
    }
  }
  return null;
}

function isLinuxConsoleDemo(demo) {
  return demo.test === 'linux_proto_console';
}

function resolveDemoState(demo, state) {
  const entry = demo.test ? findManifestEntry(state, demo.test) : null;
  const scenarioEntry = demo.scenarioTest ? findManifestEntry(state, demo.scenarioTest) : null;
  const localToolAvailable = demo.localTool === 'ai_tiny_model';
  const topical = typeof demo.scenarioKey === 'string' && demo.scenarioKey.length > 0;
  const runtimeAvailable = Boolean(entry) || localToolAvailable || Boolean(scenarioEntry);
  const topicAvailable = topical;
  const available = runtimeAvailable || topicAvailable;
  const scenarioKey = demo.scenarioKey ?? demo.test ?? demo.localTool ?? demo.title;
  const selected = topical
    ? state.selectedScenario === scenarioKey
    : demo.test
      ? state.selectedTest === demo.test
      : state.selectedScenario === scenarioKey;
  return {
    entry,
    scenarioEntry,
    localToolAvailable,
    topical,
    topicAvailable,
    runtimeAvailable,
    available,
    selected,
    scenarioKey,
  };
}

function resolvedScenarioTest(demo, entry, state) {
  return demo.scenarioTest ?? demo.test ?? entry?.name ?? state.selectedScenarioTest ?? state.selectedTest ?? null;
}

function demoCardStatusLabel(demo, available, selected, topical = false) {
  if (topical) {
    return selected ? 'Viewing topic' : 'Open topic';
  }
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
  const {
    entry,
    localToolAvailable,
    topical,
    available,
    selected,
    scenarioKey,
  } = resolveDemoState(demo, state);
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
    topical ? 'is-topical' : '',
    demo.gated && !available ? 'is-gated' : '',
    isLinuxConsoleDemo(demo) && available ? 'is-ready' : '',
  ].filter(Boolean).join(' ');
  const attrs = available
    ? (
        localToolAvailable
          ? `data-demo-tool="${escapeHtml(demo.localTool)}" role="button" tabindex="0"`
          : topical
            ? `data-scenario-key="${escapeHtml(scenarioKey)}" data-scenario-test="${escapeHtml(demo.scenarioTest ?? '')}" data-scenario-backend="${escapeHtml(demo.backend ?? state.backend)}" role="button" tabindex="0"`
            : `data-demo-test="${escapeHtml(demo.test)}" data-demo-backend="${escapeHtml(demo.backend ?? state.backend)}" role="button" tabindex="0"`
      )
    : 'aria-disabled="true"';
  const statusLabel = demoCardStatusLabel(demo, available, selected, topical);
  const gateNote = renderDemoGateNote(demo, available, state);

  return `
    <article class="${classes}" data-route-label="${escapeHtml(routeLabel)}" ${attrs}>
      <div class="demo-card__title-row">
        <h4>${escapeHtml(title)}</h4>
        <strong class="demo-card__status">${statusLabel}</strong>
      </div>
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

function renderCompactDemoCard(demo, state) {
  const {
    entry,
    localToolAvailable,
    topical,
    available,
    selected,
    scenarioKey,
  } = resolveDemoState(demo, state);
  const routeLabel = demo.title;
  const title = entry?.title ?? demo.title;
  const summary = entry?.summary ?? demo.fallbackSummary ?? '';
  const marker = entry?.workload?.expectedMarker ?? demo.marker ?? '';
  const ops = entry?.workload?.ops ?? demo.panels ?? [];
  const classes = [
    'demo-card',
    'demo-card--compact',
    selected ? 'is-selected' : '',
    available ? 'is-available' : 'is-soon',
    localToolAvailable ? 'is-local-tool' : '',
    topical ? 'is-topical' : '',
    demo.gated && !available ? 'is-gated' : '',
    isLinuxConsoleDemo(demo) && available ? 'is-ready' : '',
  ].filter(Boolean).join(' ');
  const attrs = available
    ? (
        localToolAvailable
          ? `data-demo-tool="${escapeHtml(demo.localTool)}" role="button" tabindex="0"`
          : topical
            ? `data-scenario-key="${escapeHtml(scenarioKey)}" data-scenario-test="${escapeHtml(demo.scenarioTest ?? '')}" data-scenario-backend="${escapeHtml(demo.backend ?? state.backend)}" role="button" tabindex="0"`
            : `data-demo-test="${escapeHtml(demo.test)}" data-demo-backend="${escapeHtml(demo.backend ?? state.backend)}" role="button" tabindex="0"`
      )
    : 'aria-disabled="true"';
  const statusLabel = demoCardStatusLabel(demo, available, selected, topical);
  return `
    <article class="${classes}" data-route-label="${escapeHtml(routeLabel)}" ${attrs}>
      <div class="demo-card__title-row">
        <h4>${escapeHtml(title)}</h4>
        <strong class="demo-card__status">${statusLabel}</strong>
      </div>
      <div class="demo-card__details">
        <p>${escapeHtml(summary)}</p>
        <div class="demo-card__meta">
          <span>${escapeHtml(demo.backend ?? 'planned')}</span>
          <span>${escapeHtml(marker)}</span>
        </div>
        <div class="demo-card__chips">
          ${ops.slice(0, 4).map((item) => `<span>${escapeHtml(item)}</span>`).join('')}
        </div>
        ${renderDemoGateNote(demo, available, state)}
      </div>
    </article>
  `;
}

function selectedScenarioContext(state) {
  const selectedEntry = selectedTestEntry(state);
  const scenarioRecord = state.selectedScenario ? findDemoRecordByScenarioKey(state.selectedScenario) : null;
  const record = scenarioRecord?.demo?.scenarioKey
    ? scenarioRecord
    : findDemoRecordByTest(state.selectedTest)
      ?? (state.selectedScenarioTest ? findDemoRecordByTest(state.selectedScenarioTest) : null)
      ?? scenarioRecord;
  if (record) {
    const { demo, group } = record;
    const resolved = resolveDemoState(demo, state);
    const runtimeEntry = resolved.entry
      ?? resolved.scenarioEntry
      ?? findManifestEntry(state, demo.scenarioTest ?? '')
      ?? findManifestEntry(state, state.selectedScenarioTest ?? '');
    const entry = resolved.topical ? runtimeEntry : (resolved.entry ?? selectedEntry);
    const displayEntry = resolved.topical ? null : entry;
    const marker = entry?.workload?.expectedMarker ?? demo.marker ?? '-';
    const assetNote = entry?.workload?.assetNote ?? demo.assetNote ?? '';
    const scenarioTest = resolvedScenarioTest(demo, entry, state);
    const needsRuntimeManifest = resolved.topical && Boolean(demo.scenarioTest);
    const readyLabel = resolved.runtimeAvailable
      ? (resolved.localToolAvailable ? 'Host tool ready' : (resolved.topical ? 'Runtime manifest ready' : 'Manifest ready'))
      : (demo.gated || needsRuntimeManifest ? 'External asset required' : 'Planned');
    const sessionBackend = resolved.topical
      ? (state.selectedScenarioBackend ?? demo.backend ?? state.backend ?? '-')
      : (state.loadedSession?.backend ?? demo.backend ?? state.backend ?? '-');
    const sessionState = state.runState ?? 'idle';
    const expectedPrompt = marker && marker !== '-' ? marker : 'No prompt marker declared';
    let topicalNextAction = 'Read the topic, then switch to a runnable live scenario when you want runtime evidence.';
    if (group.title === 'Linux Distro Labs') {
      topicalNextAction = 'Read the distro evidence, then open the live shell route when you want UART-backed confirmation.';
    } else if ((demo.scenarioKey ?? '') === 'jit_runtime_lab') {
      topicalNextAction = 'Sync a runtime-friendly workload, then run the JIT probe to collect the current dry-run summary.';
    }
    const nextAction = resolved.runtimeAvailable
      ? (resolved.topical
        ? topicalNextAction
        : sessionState === 'idle'
        ? 'Load the scenario to create a session and watch the primary stage.'
        : sessionState === 'loading'
          ? 'Wait for the marker, then inspect the primary stage and evidence cards.'
          : 'Use Run, Pause, Reset, or Terminate to steer the live session.')
      : (demo.gated
        ? 'Prepare the required external asset, then reopen the route.'
        : 'This scenario is catalogued but not wired into a runnable path yet.');
    const sessionContract = [
      `Backend ${sessionBackend}`,
      `Run state ${sessionState}`,
      `Expected marker ${expectedPrompt}`,
    ];
    if (assetNote) {
      sessionContract.push(`Asset ${assetNote}`);
    }
    const observationHints = [
      `Primary stage: ${demo.primaryStage ?? 'Terminal + inspector'}`,
      ...(demo.inspectorFocus ?? []).slice(0, 2),
    ];
    return {
      groupTitle: group.title,
      demo,
      entry,
      readyLabel,
      title: displayEntry?.title ?? demo.title ?? state.selectedTest,
      summary: displayEntry?.summary ?? demo.fallbackSummary ?? 'No scenario summary yet.',
      brief: demo.brief ?? displayEntry?.summary ?? demo.fallbackSummary ?? 'No scenario brief yet.',
      primaryStage: demo.primaryStage ?? 'Terminal + inspector',
      inspectorFocus: demo.inspectorFocus ?? [],
      proves: demo.proves ?? [],
      boundary: demo.boundary ?? 'This scenario only claims the currently verified contract.',
      marker,
      assetNote,
      sessionBackend,
      sessionState,
      nextAction,
      sessionContract,
      observationHints,
      scenarioKey: resolved.scenarioKey,
      scenarioTest,
      scenarioBackend: sessionBackend,
      topical: resolved.topical,
      selected: resolved.selected,
      runtimeAvailable: resolved.runtimeAvailable,
    };
  }

  const fallbackBackend = state.loadedSession?.backend ?? state.backend ?? '-';
  const fallbackState = state.runState ?? 'idle';
  return {
    groupTitle: 'Ad-hoc scenario',
    demo: null,
    entry: selectedEntry,
    readyLabel: selectedEntry ? 'Manifest ready' : 'Unavailable',
    title: selectedEntry?.title ?? selectedEntry?.menuLabel ?? state.selectedTest,
    summary: selectedEntry?.summary ?? 'Current manifest entry does not provide a curated lab summary yet.',
    brief: selectedEntry?.summary ?? 'Current manifest entry is available but does not yet have a curated scenario brief.',
    primaryStage: 'Terminal + inspector',
    inspectorFocus: ['Terminal session', 'Snapshot summary', 'Platform and register panels'],
    proves: ['This route is available through the current manifest and debug-session pipeline.'],
    boundary: 'This scenario has not yet been rewritten into the new lab catalog.',
    marker: selectedEntry?.workload?.expectedMarker ?? '-',
    assetNote: selectedEntry?.workload?.assetNote ?? '',
    sessionBackend: fallbackBackend,
    sessionState: fallbackState,
    nextAction: selectedEntry
      ? 'Load or resume this manifest-backed route, then inspect the snapshot and platform panels.'
      : 'Choose a wired scenario from the navigator before launching a session.',
    sessionContract: [
      `Backend ${fallbackBackend}`,
      `Run state ${fallbackState}`,
      `Expected marker ${selectedEntry?.workload?.expectedMarker ?? 'No prompt marker declared'}`,
    ],
    observationHints: ['Terminal session', 'Snapshot summary', 'Platform and register panels'],
  };
}

function renderScenarioFocusList(items = []) {
  return `
    <ul class="demo-workspace__focus-list">
      ${items.map((item) => `<li>${escapeHtml(item)}</li>`).join('')}
    </ul>
  `;
}

function renderScenarioContract(context) {
  return `
    <article class="demo-workspace__brief-card demo-workspace__brief-card--contract">
      <span>Session contract</span>
      <strong>${escapeHtml(context.readyLabel)}</strong>
      ${renderScenarioFocusList(context.sessionContract)}
      <p>${escapeHtml(context.nextAction)}</p>
    </article>
  `;
}

function renderScenarioEvidence(context) {
  const markerLine = context.marker && context.marker !== '-'
    ? `Expected marker ${context.marker}`
    : 'Expected marker is not declared yet.';
  return `
    <article class="demo-workspace__brief-card demo-workspace__brief-card--evidence">
      <span>Evidence and boundary</span>
      <strong>${escapeHtml(markerLine)}</strong>
      ${renderScenarioFocusList(context.proves)}
      <p>${escapeHtml(context.boundary)}</p>
      ${context.assetNote ? `<code>${escapeHtml(context.assetNote)}</code>` : ''}
    </article>
  `;
}

function renderScenarioControls(context, state) {
  const runtimeAvailable = context.runtimeAvailable ?? Boolean(context.entry);
  const syncDisabled = !context.scenarioTest || !runtimeAvailable;
  const syncLabel = state.selectedTest === context.scenarioTest
    ? 'Session synced'
    : 'Sync session';
  const liveButton = context.groupTitle === 'Linux Distro Labs' && context.topical && context.scenarioTest && runtimeAvailable
    ? `
      <button
        id="scenario-open-live-button"
        type="button"
        class="demo-workspace__action-button demo-workspace__action-button--accent"
      data-action="open-scenario-live"
        data-scenario-test="${escapeHtml(context.scenarioTest)}"
        data-scenario-backend="${escapeHtml(context.scenarioBackend ?? context.sessionBackend)}"
      >
        Open live shell
      </button>
    `
    : '';
  const loadButton = runtimeAvailable
    ? `
        <button
          id="scenario-load-button"
          type="button"
          class="demo-workspace__action-button"
          data-action="load-current-session"
        >
          Load current scenario
        </button>
      `
    : '';
  const probeButton = context.scenarioKey === 'jit_runtime_lab'
    ? `
      <button
        type="button"
        class="demo-workspace__action-button demo-workspace__action-button--teal"
        data-action="run-jit-dispatch"
      >
        Run JIT probe
      </button>
    `
    : '';
  return `
    <article class="demo-workspace__brief-card demo-workspace__brief-card--actions">
      <span>Scenario controls</span>
      <strong>Keep the topic and the live session aligned.</strong>
      <div class="demo-workspace__action-row">
        <button
          id="scenario-sync-button"
          type="button"
          class="demo-workspace__action-button"
          data-action="sync-scenario-session"
          data-scenario-test="${escapeHtml(context.scenarioTest ?? '')}"
          data-scenario-backend="${escapeHtml(context.scenarioBackend ?? context.sessionBackend)}"
          ${syncDisabled ? 'disabled' : ''}
        >
          ${escapeHtml(syncLabel)}
        </button>
        ${loadButton}
        ${liveButton}
        ${probeButton}
      </div>
      <p>Scenario cards pick the narrative view; session controls below still own Load, Run, Pause, Reset, and Terminate.</p>
    </article>
  `;
}

function renderJitDispatchEvidence(state) {
  if (state.jitDispatch.runState === 'running') {
    return `
      <article class="demo-workspace__brief-card demo-workspace__brief-card--runtime is-pending">
        <span>Observed runtime dispatch</span>
        <strong>Collecting dry-run summary…</strong>
        <p>Run JIT probe is querying the current loaded session.</p>
      </article>
    `;
  }
  if (state.jitDispatch.runState === 'error') {
    return `
      <article class="demo-workspace__brief-card demo-workspace__brief-card--runtime is-error">
        <span>Observed runtime dispatch</span>
        <strong>Probe failed</strong>
        <p>${escapeHtml(state.jitDispatch.error ?? 'unknown error')}</p>
      </article>
    `;
  }
  if (!state.jitDispatch.summary) {
    return `
      <article class="demo-workspace__brief-card demo-workspace__brief-card--runtime">
        <span>Observed runtime dispatch</span>
        <strong>Sample runtime dispatch</strong>
        <p>Use Run JIT probe after syncing a runtime-friendly workload to collect the current dry-run summary.</p>
      </article>
    `;
  }

  const summary = state.jitDispatch.summary;
  const cacheLabel = summary.cache_state ? `cache ${summary.cache_state}` : 'cache unknown';
  return `
    <article class="demo-workspace__brief-card demo-workspace__brief-card--runtime">
      <span>Observed runtime dispatch</span>
      <strong>${escapeHtml(summary.action ?? 'unknown action')}</strong>
      ${renderScenarioFocusList([
        `source ${summary.source ?? 'unknown'}`,
        `candidate executions ${summary.candidate_executions ?? 0}`,
        `candidate retired ${summary.candidate_retired_instructions ?? 0}`,
        cacheLabel,
        `range ${(summary.start_pc ?? '0x0')} -> ${(summary.end_pc ?? '0x0')}`,
      ])}
      <p>reject ${escapeHtml(summary.reject_kind ?? 'none')} / reason ${escapeHtml(summary.reject_reason ?? 'none')}</p>
    </article>
  `;
}

function linuxLaneContext(context) {
  if (context.groupTitle !== 'Linux Distro Labs') {
    return null;
  }
  return {
    title: 'Linux distro lane',
    summary: 'Use one workbench to move from serial shell bring-up toward distro-facing process, filesystem, and ISA closure.',
    ladder: [
      'Boot -> Shell',
      'TTY / login semantics',
      'Process control',
      'Filesystem persistence',
      'Capability / ISA closure',
    ],
    matrix: [
      'Linux Serial Console -> live curated shell route',
      'Alpine Distro Evidence -> shell / filesystem / process / FS-state',
      'Capability & ISA Matrix -> riscv,isa / hwcap / FCSR / FP',
    ],
  };
}

function renderLinuxLane(context) {
  const lane = linuxLaneContext(context);
  if (!lane) {
    return '';
  }
  return `
    <section class="demo-workspace__lane demo-workspace__lane--linux">
      <div class="demo-workspace__lane-head">
        <span>${escapeHtml(lane.title)}</span>
        <strong>${escapeHtml(context.title)}</strong>
        <p>${escapeHtml(lane.summary)}</p>
      </div>
      <div class="demo-workspace__lane-grid">
        <article class="demo-workspace__brief-card demo-workspace__brief-card--linux">
          <span>Capability ladder</span>
          <strong>Track the platform claim from shell to ISA.</strong>
          ${renderScenarioFocusList(lane.ladder)}
        </article>
        <article class="demo-workspace__brief-card demo-workspace__brief-card--linux">
          <span>Curated distro matrix</span>
          <strong>Keep the current Linux family visible beside the live session.</strong>
          ${renderScenarioFocusList(lane.matrix)}
        </article>
      </div>
    </section>
  `;
}

function renderRuntimeLane(context, state) {
  if (context.scenarioKey !== 'jit_runtime_lab') {
    return '';
  }
  return `
    <section class="demo-workspace__lane demo-workspace__lane--runtime">
      <div class="demo-workspace__lane-head">
        <span>Runtime evidence topic</span>
        <strong>${escapeHtml(context.title)}</strong>
        <p>Keep one runtime-friendly workload synced while you inspect the current JIT / DBT dry-run contract.</p>
      </div>
      <div class="demo-workspace__lane-grid">
        <article class="demo-workspace__brief-card demo-workspace__brief-card--runtime">
          <span>Runtime focus</span>
          <strong>Sample runtime dispatch</strong>
          ${renderScenarioFocusList([
            'hit / miss / emit / fallback',
            'reject kind and fallback reason',
            'current dry-run scope versus default backend ambitions',
          ])}
        </article>
        ${renderJitDispatchEvidence(state)}
      </div>
    </section>
  `;
}

function renderLabNavigator(state) {
  return `
    <nav class="demo-workspace__navigator" aria-label="Lab navigator">
      <div class="demo-workspace__navigator-head">
        <span>Scenario navigator</span>
        <strong>Choose a lab route.</strong>
      </div>
      <div class="demo-workspace__navigator-groups">
        ${DEMO_GROUPS.map((group) => `
          <section class="demo-group">
            <div class="demo-group__header">
              <h3>${escapeHtml(group.title)}</h3>
              <p>${escapeHtml(group.summary)}</p>
            </div>
            <div class="demo-group__cards">
              ${group.demos.map((demo) => renderCompactDemoCard(demo, state)).join('')}
            </div>
          </section>
        `).join('')}
      </div>
    </nav>
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

function renderAuthPanel(state) {
  const auth = state.auth;
  if (!auth.required) {
    return `
      <div class="auth-panel auth-panel--disabled">
        <strong>Auth disabled</strong>
        <span>当前实例未启用登录控制。</span>
      </div>
    `;
  }

  if (!auth.authenticated) {
    return `
      <form class="auth-panel auth-panel--login" id="auth-login-form">
        <label>
          <span>用户名</span>
          <input id="auth-username" name="username" type="text" autocomplete="username" value="admin">
        </label>
        <label>
          <span>密码</span>
          <input id="auth-password" name="password" type="password" autocomplete="current-password">
        </label>
        <button type="submit" ${auth.loginPending ? 'disabled' : ''}>
          ${auth.loginPending ? '登录中...' : '登录'}
        </button>
        ${auth.loginError ? `<div class="auth-panel__error">${escapeHtml(auth.loginError)}</div>` : ''}
      </form>
    `;
  }

  const controllerText = auth.controllerUsername
    ? (auth.controllerSession ? '你正在控制当前会话。' : `${auth.controllerUsername} 正在控制当前会话。`)
    : '当前没有控制者，首次写操作会自动获取控制权。';
  return `
    <div class="auth-panel auth-panel--active">
      <div class="auth-panel__status">
        <strong>${escapeHtml(auth.username ?? 'unknown')}</strong>
        <span>${escapeHtml(auth.role ?? 'observer')}</span>
        <em>${escapeHtml(`${auth.activeSessions}/${auth.sessionLimit}`)} sessions</em>
      </div>
      <div class="auth-panel__hint">${escapeHtml(controllerText)}</div>
      <div class="auth-panel__actions">
        <button id="release-control-button" ${auth.controllerSession ? '' : 'disabled'}>释放控制权</button>
        <button id="logout-button">退出登录</button>
      </div>
    </div>
  `;
}

function renderDemoWorkspace(state) {
  return `
    <div class="console-sidebar__workspace">
      ${renderLinuxLoadProgress(state)}
      ${renderLabNavigator(state)}
    </div>
  `;
}

function renderGuidePanel(state) {
  const context = selectedScenarioContext(state);
  const syncControls = renderScenarioControls(context, state);
  const linuxLane = renderLinuxLane(context);
  const runtimeLane = renderRuntimeLane(context, state);
  return `
    <section class="guide-panel">
      <div class="guide-panel__head">
        <span>Guide</span>
        <strong>${escapeHtml(context.title)}</strong>
        <p>${escapeHtml(context.brief)}</p>
      </div>
      <div class="guide-panel__chips">
        <span>${escapeHtml(context.groupTitle)}</span>
        <span>${escapeHtml(context.sessionBackend)}</span>
        <span>${escapeHtml(context.sessionState)}</span>
        <span>${escapeHtml(context.marker)}</span>
      </div>
      <article class="guide-card">
        <span>Scenario</span>
        <strong>${escapeHtml(context.primaryStage)}</strong>
        <p>${escapeHtml(context.summary)}</p>
      </article>
      ${renderScenarioContract(context)}
      <article class="guide-card">
        <span>What to watch</span>
        <strong>Focus on the panels that matter for this route.</strong>
        ${renderScenarioFocusList(context.inspectorFocus)}
      </article>
      ${renderScenarioEvidence(context)}
      ${syncControls}
      ${linuxLane}
      ${runtimeLane}
    </section>
  `;
}

function shouldShowAiTinyModelPanel(state) {
  const context = selectedScenarioContext(state);
  return (
    context.groupTitle === 'AI Labs' ||
    state.aiTinyModel.templates.length > 0 ||
    state.aiTinyModel.runState !== 'idle' ||
    state.aiTinyModel.result !== null ||
    state.aiTinyModel.error !== null
  );
}

function renderAiLabPanel(state) {
  if (!shouldShowAiTinyModelPanel(state)) {
    return '';
  }
  return renderAiTinyModelPanel(state);
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
  if (elements.guide) {
    elements.guide.innerHTML = renderGuidePanel(state);
  }
  if (elements.authPanel) {
    elements.authPanel.innerHTML = renderAuthPanel(state);
  }
  elements.desktop.dataset.debugOpen = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.desktop.dataset.terminalCollapsed = state.layout.terminalCollapsed ? 'true' : 'false';
  elements.debugInspector.dataset.open = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.terminal.innerHTML = renderTerminal(state);
  elements.summary.innerHTML = renderSummary(snapshot, state.runState);
  if (elements.workload) {
    elements.workload.innerHTML = renderWorkloadPanel(currentTest, snapshot);
  }
  if (elements.aiLab) {
    elements.aiLab.innerHTML = renderAiLabPanel(state);
  }
  elements.predictor.innerHTML = renderPredictor(snapshot);
  elements.pipeline.innerHTML = snapshot
    ? `${renderPipelineBoard(snapshot)}${renderTimeline(timelineRows)}`
    : `${renderPipelineBoard(snapshot)}${renderTimeline(timelineRows)}<div class="empty-state empty-state-hint">选择测试用例并点击 <strong>Load</strong> 开始调试会话</div>`;
  elements.events.innerHTML = renderOooPanel(snapshot);
  if (elements.vector) {
    elements.vector.innerHTML = renderVectorPanel(snapshot, previous, currentTest, currentBackend);
  }
  elements.devices.innerHTML = renderPlatformGroup(snapshot);
  elements.registers.innerHTML = renderArchitectureGroup(snapshot, registers);
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
  const requiresAuth = state.auth.required;
  const canControl = !requiresAuth || (state.auth.authenticated && state.auth.canControl);
  const canObserve = !requiresAuth || state.auth.authenticated;
  elements.testSelect.innerHTML = state.tests.map((item) => `
    <option value="${item.name}" ${item.name === state.selectedTest ? 'selected' : ''}>
      ${item.menuLabel ?? item.name}${item.hasDisk ? ' [disk]' : ''}
    </option>
  `).join('');
  elements.backendSelect.value = state.backend;
  elements.statusBadge.textContent = state.runState;
  elements.testSelect.disabled = !canObserve;
  elements.backendSelect.disabled = !canObserve;
  document.querySelector('#load-button').disabled = !canControl;
  document.querySelector('#run-button').disabled = !canControl;
  document.querySelector('#pause-button').disabled = !canControl;
  document.querySelector('#reset-button').disabled = !canControl;
  document.querySelector('#terminate-button').disabled = !canControl;
  document.querySelector('#step-cycle-button').disabled = !canControl;
  document.querySelector('#step-commit-button').disabled = !canControl;
}
