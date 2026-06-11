import test from 'node:test';
import assert from 'node:assert/strict';

import { renderApp } from '../app/render.js';
import { createAppState, pushSnapshot } from '../app/state.js';

function createSlot() {
  return {
    innerHTML: '',
    dataset: {},
    querySelector() {
      return null;
    },
  };
}

function createElements() {
  return {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlot(),
    devices: createSlot(),
    registers: createSlot(),
    csrs: createSlot(),
    bus: createSlot(),
    demoWorkspace: createSlot(),
    guide: createSlot(),
    aiLab: createSlot(),
    authPanel: createSlot(),
    workload: createSlot(),
    vector: createSlot(),
  };
}

test('renderApp propagates failed MMIO bus details into the grouped platform inspector', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.selectedTest = 'hello';
  state.backend = 'functional';
  state.terminal.connected = true;
  state.layout.debugPanelOpen = true;

  pushSnapshot(state, {
    summary: {
      cycle: 7,
      instret: 3,
      pc: '0x80000084',
      halted: false,
      privilege: 'M',
      backend: 'functional',
    },
    pipeline: {
      if: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      id: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: true,
        replay_flush: false,
        committed: false,
      },
      ooo: {
        rob_depth: 0,
        rob_head_sequence_id: 0,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: '-',
        total_predictions: 0,
        correct_predictions: 0,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x7',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 7, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: true,
      success: false,
      write: true,
      mmio: true,
      device: 'uart',
      addr: '0x10000000',
      value: '0x00000005',
      size: 4,
      detail: 'invalid MMIO access',
    },
    events: [
      { kind: 'store', cycle: 7, detail: 'uart write 0x10000000 failed: invalid MMIO access' },
    ],
  });

  const elements = {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlot(),
    devices: createSlot(),
    registers: createSlot(),
    csrs: createSlot(),
    bus: createSlot(),
  };

  renderApp(elements, state);

  assert.equal(elements.desktop.dataset.debugOpen, 'true');
  assert.equal(elements.debugInspector.dataset.open, 'true');
  assert.match(elements.devices.innerHTML, /平台与 I\/O/);
  assert.match(elements.devices.innerHTML, /<span>device<\/span><strong>uart<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>status<\/span><strong>failed<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>detail<\/span><strong>invalid MMIO access<\/strong>/);
  assert.match(elements.devices.innerHTML, /uart write 0x10000000 failed: invalid MMIO access/);
});

test('renderApp shows a login form when auth is required and no session exists', () => {
  const state = createAppState();
  state.auth.required = true;
  state.auth.authenticated = false;

  const elements = createElements();
  renderApp(elements, state);

  assert.match(elements.authPanel.innerHTML, /auth-login-form/);
  assert.match(elements.authPanel.innerHTML, /用户名/);
  assert.match(elements.authPanel.innerHTML, /密码/);
});

test('renderApp shows controller status and actions for an authenticated session', () => {
  const state = createAppState();
  state.auth = {
    ...state.auth,
    required: true,
    authenticated: true,
    username: 'admin',
    role: 'admin',
    activeSessions: 2,
    sessionLimit: 3,
    controllerUsername: 'admin',
    controllerSession: true,
    canControl: true,
  };

  const elements = createElements();
  renderApp(elements, state);

  assert.match(elements.authPanel.innerHTML, /admin/);
  assert.match(elements.authPanel.innerHTML, /2\/3 sessions/);
  assert.match(elements.authPanel.innerHTML, /释放控制权/);
  assert.match(elements.authPanel.innerHTML, /退出登录/);
});

test('renderApp surfaces unified observation events in the guide evidence drawer', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.tests = [
    {
      name: 'hello',
      title: 'Pipeline Inspector',
      backend: 'pipeline',
      summary: 'Small pipeline workload',
      workload: {
        expectedMarker: 'halt',
      },
    },
  ];
  state.selectedTest = 'hello';
  state.selectedScenario = 'hello';
  state.loadedSession = {
    test: 'hello',
    backend: 'pipeline',
  };

  pushSnapshot(state, {
    summary: {
      cycle: 12,
      instret: 8,
      pc: '0x80000020',
      halted: false,
      privilege: 'S',
      backend: 'pipeline',
    },
    observation_event: {
      schema_version: 1,
      source: 'execution-profile',
      phase: 'snapshot-summary',
      effect: 'observed',
      subject: {
        backend: 'pipeline',
        pc: '0x80000020',
        privilege: 'S',
      },
      timestamp_or_step: {
        cycle: 12,
        instret: 8,
      },
      payload: {
        total_retirements: 8,
        total_memory_observations: 2,
        top_hot_path: {
          start_pc: '0x80000020',
          end_pc: '0x80000030',
          executions: 3,
        },
      },
      evidence_ref: {
        debug_json: 'snapshot.profile',
      },
    },
    pipeline: {
      if: { valid: false, text: '' },
      flags: {},
    },
    gpr: [],
    csrs: {},
    devices: {},
    bus: {},
    events: [],
  });

  const elements = createElements();
  renderApp(elements, state);

  assert.match(elements.guide.innerHTML, /Observation event/);
  assert.match(elements.guide.innerHTML, /execution-profile/);
  assert.match(elements.guide.innerHTML, /snapshot-summary/);
  assert.match(elements.guide.innerHTML, /snapshot\.profile/);
  assert.match(elements.guide.innerHTML, /top hot path 0x80000020 -&gt; 0x80000030/);
});

test('renderApp shows predictor accuracy using resolved branch statistics contract', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.selectedTest = 'predictor';
  state.backend = 'pipeline';
  state.terminal.connected = true;
  state.layout.debugPanelOpen = true;

  pushSnapshot(state, {
    summary: {
      cycle: 19,
      instret: 8,
      pc: '0x80000090',
      halted: false,
      privilege: 'S',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      id: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: true,
      },
      ooo: {
        rob_depth: 1,
        rob_head_sequence_id: 8,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 4,
        correct_predictions: 3,
        mispredictions: 1,
        last_prediction_valid: false,
        last_mispredict_valid: true,
        last_mispredict_pc: '0x80000088',
        last_mispredict_target: '0x80000094',
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 0, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlot(),
    devices: createSlot(),
    registers: createSlot(),
    csrs: createSlot(),
    bus: createSlot(),
  };

  renderApp(elements, state);

  assert.match(elements.predictor.innerHTML, /<span class="stat-label">已解析分支<\/span>/);
  assert.match(elements.predictor.innerHTML, /<strong class="stat-value stat-accuracy">75\.0%<\/strong>/);
});

test('renderApp shows stall attribution, OoO metrics, and grouped inspector sections', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.selectedTest = 'interactive_os';
  state.backend = 'pipeline';
  state.terminal.connected = true;
  state.layout.debugPanelOpen = true;

  pushSnapshot(state, {
    summary: {
      cycle: 42,
      instret: 17,
      pc: '0x80000110',
      halted: false,
      privilege: 'S',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: true, pc: '0x80000110', raw: '0x00000013', text: 'addi x0, x0, 0' },
      id: { valid: true, pc: '0x8000010c', raw: '0x00052083', text: 'lw x1, 0(x10)' },
      ex: { valid: true, pc: '0x80000108', raw: '0x00150023', text: 'sb x1, 0(x10)' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: true,
        stall_reason: 'memory_path_busy',
        redirected: false,
        trap_flush: false,
        replay_flush: true,
        committed: false,
      },
      ooo: {
        rob_depth: 6,
        rob_head_sequence_id: 19,
        lsq_depth: 2,
        lsq_head_sequence_id: 18,
        lsq_load_state: 'blocked_by_overlapping_store',
        lsq_load_sequence_id: 21,
        lsq_store_sequence_id: 18,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 6,
        correct_predictions: 4,
        mispredictions: 2,
      },
    },
    gpr: Array.from({ length: 32 }, (_, index) => `0x${index.toString(16)}`),
    csrs: {
      mstatus: '0x0',
      sstatus: '0x2',
      mepc: '0x0',
      sepc: '0x80000104',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x8000000000000001',
    },
    devices: {
      uart: { ier: 1, output_size: 32 },
      clint: { mtime: 42, mtimecmp: 80, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: true, lba: 7 },
    },
    bus: {
      valid: true,
      success: true,
      write: false,
      mmio: false,
      device: 'ram',
      addr: '0x80000110',
      value: '0x00000013',
      size: 4,
      detail: '',
    },
    events: [
      { kind: 'stall', cycle: 42, detail: 'memory path occupied by older memory op' },
    ],
  });

  const elements = {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlot(),
    devices: createSlot(),
    registers: createSlot(),
    csrs: createSlot(),
    bus: createSlot(),
  };

  renderApp(elements, state);

  assert.match(elements.pipeline.innerHTML, /stall_reason/);
  assert.match(elements.pipeline.innerHTML, /memory_path_busy/);
  assert.match(elements.pipeline.innerHTML, /stall: memory_path_busy/);
  assert.match(elements.pipeline.innerHTML, /blocked_by_overlapping_store/);
  assert.match(elements.events.innerHTML, /OoO \/ 微架构/);
  assert.match(elements.events.innerHTML, /rob_depth/);
  assert.match(elements.events.innerHTML, /replay_flush/);
  assert.match(elements.registers.innerHTML, /架构状态/);
  assert.match(elements.devices.innerHTML, /平台与 I\/O/);
});

test('renderApp keeps grouped inspector sections expanded when layout state requests it', () => {
  const state = createAppState();
  state.runState = 'running';
  state.backend = 'pipeline';
  state.layout.debugPanelOpen = true;
  state.layout.architectureGroupOpen = true;
  state.layout.platformGroupOpen = true;

  pushSnapshot(state, {
    summary: {
      cycle: 3,
      instret: 1,
      pc: '0x80000080',
      halted: false,
      privilege: 'M',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: true, pc: '0x80000080', raw: '0x00000013', text: 'addi x0, x0, 0' },
      id: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: true,
      },
      ooo: {
        rob_depth: 0,
        rob_head_sequence_id: 0,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 1,
        correct_predictions: 1,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 3, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlot(),
    devices: createSlot(),
    registers: createSlot(),
    csrs: createSlot(),
    bus: createSlot(),
  };

  renderApp(elements, state);

  assert.match(elements.registers.innerHTML, /panel-architecture-grid/);
  assert.match(elements.devices.innerHTML, /panel-platform-grid/);
  assert.doesNotMatch(elements.registers.innerHTML, /<details/);
  assert.doesNotMatch(elements.devices.innerHTML, /<details/);
});

test('renderApp marks the desktop layout when terminal is collapsed', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.selectedTest = 'hello';
  state.backend = 'pipeline';
  state.terminal.connected = true;
  state.terminal.buffer = 'boot\nmonitor> help';
  state.layout.debugPanelOpen = true;
  state.layout.terminalCollapsed = true;

  pushSnapshot(state, {
    summary: {
      cycle: 9,
      instret: 4,
      pc: '0x80000090',
      halted: false,
      privilege: 'S',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      id: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: true,
      },
      ooo: {
        rob_depth: 0,
        rob_head_sequence_id: 0,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 1,
        correct_predictions: 1,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 9, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlot(),
    devices: createSlot(),
    registers: createSlot(),
    csrs: createSlot(),
    bus: createSlot(),
  };

  renderApp(elements, state);

  assert.equal(elements.desktop.dataset.terminalCollapsed, 'true');
  assert.match(elements.terminal.innerHTML, /terminal-window is-collapsed/);
  assert.match(elements.terminal.innerHTML, /展开 terminal/);
});

test('renderApp renders the redesigned three-column console surfaces', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_interactive_os_demo',
      menuLabel: 'guest_interactive_os_demo · interactive OS',
      kind: 'guest',
      backend: 'pipeline',
      title: 'interactive_os Monitor',
      summary: 'interactive monitor',
      workload: {
        category: 'guest',
        expectedMarker: 'monitor> ',
        ops: ['help', 'regs'],
      },
    },
    {
      name: 'guest_ai_accel_demo',
      menuLabel: 'guest_ai_accel_demo · AI accel MMIO',
      kind: 'guest',
      backend: 'pipeline',
      title: 'AI Accelerator Demo',
      summary: 'AI accel smoke',
      workload: {
        category: 'ai-accelerator-demo',
        expectedMarker: 'KMVAI',
        ops: ['MMIO doorbell', 'DMA load/store'],
      },
    },
  ];

  const elements = createElements();
  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Scenario navigator/);
  assert.match(elements.demoWorkspace.innerHTML, /System Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /Linux Distro Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /AI Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /data-demo-test="guest_ai_accel_demo"/);
  assert.match(elements.guide.innerHTML, /Guide/);
  assert.match(elements.guide.innerHTML, /Scenario/);
  assert.match(elements.guide.innerHTML, /Evidence and boundary/);
});

test('renderApp exposes Course OS Shell as a selectable system lab', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_course_os_shell_demo',
      menuLabel: 'guest_course_os_shell_demo · Course OS shell',
      kind: 'guest',
      backend: 'pipeline',
      title: 'Course OS Shell',
      badge: 'Course OS',
      summary: 'Stage 4 course shell terminal',
      workload: {
        stage: 'Course OS Stage 4',
        category: 'course-os-shell',
        expectedMarker: 'course-os> ',
        ops: ['terminal', 'procfs', 'FD / FS', 'COW'],
      },
    },
  ];
  state.selectedTest = 'guest_course_os_shell_demo';
  state.loadedSession = {
    test: 'guest_course_os_shell_demo',
    backend: 'pipeline',
  };

  const elements = createElements();
  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Course OS Shell/);
  assert.match(elements.demoWorkspace.innerHTML, /course-os&gt; /);
  assert.match(elements.demoWorkspace.innerHTML, /procfs/);
  assert.match(elements.demoWorkspace.innerHTML, /COW/);
  assert.match(elements.demoWorkspace.innerHTML, /data-demo-test="guest_course_os_shell_demo"/);
  assert.match(elements.guide.innerHTML, /Course OS shell/);
  assert.match(elements.guide.innerHTML, /not a Linux shell|不是 Linux shell/);
});

test('renderApp presents Linux serial console as a gated route until runtime assets are configured', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_interactive_os_demo',
      menuLabel: 'guest_interactive_os_demo',
      kind: 'guest',
      title: 'interactive_os Monitor',
      summary: 'interactive monitor',
    },
  ];

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Linux Serial Console/);
  assert.match(elements.demoWorkspace.innerHTML, /Not configured/);
  assert.match(elements.demoWorkspace.innerHTML, /Runtime Image required/);
  assert.match(elements.demoWorkspace.innerHTML, /MYCPU_LINUX_PROTO_CONSOLE_IMAGE/);
  assert.match(elements.demoWorkspace.innerHTML, /No session will be created until the Image is configured/);
  assert.match(elements.demoWorkspace.innerHTML, /aria-disabled="true"/);
  assert.doesNotMatch(elements.demoWorkspace.innerHTML, /data-demo-test="linux_proto_console"/);
});

test('renderApp explains the Linux serial console Image path diagnostic without enabling the route', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_interactive_os_demo',
      menuLabel: 'guest_interactive_os_demo',
      kind: 'guest',
      title: 'interactive_os Monitor',
      summary: 'interactive monitor',
    },
  ];
  state.diagnostics = {
    linuxConsole: {
      status: 'not-found',
      ready: false,
      envVar: 'MYCPU_LINUX_PROTO_CONSOLE_IMAGE',
      path: '/tmp/missing/Image',
      message: 'Image path does not exist: /tmp/missing/Image',
    },
  };

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Image path missing/);
  assert.match(elements.demoWorkspace.innerHTML, /\/tmp\/missing\/Image/);
  assert.match(elements.demoWorkspace.innerHTML, /Image path does not exist/);
  assert.match(elements.demoWorkspace.innerHTML, /aria-disabled="true"/);
  assert.doesNotMatch(elements.demoWorkspace.innerHTML, /data-demo-test="linux_proto_console"/);
});

test('renderApp lets the Linux serial console route select a configured linux_proto_console workload', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'linux_proto_console',
      menuLabel: 'linux_proto_console · Linux serial',
      kind: 'linux',
      backend: 'functional',
      title: 'Linux Serial Console',
      badge: 'Linux runtime',
      summary: '启动受控 linux_proto runtime，进入 UART 串口 console。',
      workload: {
        stage: 'Wave 7',
        category: 'linux-serial-console',
        expectedMarker: 'mycpu-linux# ',
        ops: ['flat SBI shim', 'Linux Image payload', 'DTB', 'virtio-blk rootfs'],
        pipelineNote: '配置本机 Linux Image 后才可运行；前端只桥接 UART 与现有 debug session。',
        assetNote: 'Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image before starting the frontend server.',
      },
    },
  ];
  state.selectedTest = 'linux_proto_console';
  state.loadedSession = {
    test: 'linux_proto_console',
    backend: 'functional',
  };
  state.backend = 'pipeline';

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /data-demo-test="linux_proto_console"/);
  assert.match(elements.demoWorkspace.innerHTML, /data-demo-backend="functional"/);
  assert.match(elements.demoWorkspace.innerHTML, /Ready/);
  assert.match(elements.guide.innerHTML, /Linux Distro Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /mycpu-linux# /);
  assert.match(elements.workload.innerHTML, /virtio-blk rootfs/);
  assert.match(elements.guide.innerHTML, /Session contract/);
  assert.match(elements.guide.innerHTML, /Asset Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE/);
  assert.match(elements.guide.innerHTML, /Load the scenario to create a session and watch the primary stage/);
  assert.match(elements.guide.innerHTML, /Linux distro lane/);
  assert.match(elements.guide.innerHTML, /Capability ladder/);
  assert.match(elements.guide.innerHTML, /Curated distro matrix/);
  assert.match(elements.demoWorkspace.innerHTML, /Alpine Distro Evidence/);
  assert.match(elements.demoWorkspace.innerHTML, /Capability &amp; ISA Matrix/);
  assert.match(elements.demoWorkspace.innerHTML, /is-selected/);
  assert.doesNotMatch(elements.demoWorkspace.innerHTML, /Runtime Image required/);
  assert.doesNotMatch(elements.demoWorkspace.innerHTML, /Not configured/);
});

test('renderApp can focus Linux distro evidence topics without requiring a runnable session', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'linux_proto_console',
      menuLabel: 'linux_proto_console · Linux serial',
      kind: 'linux',
      backend: 'functional',
      title: 'Linux Serial Console',
      badge: 'Linux runtime',
      summary: '启动受控 linux_proto runtime，进入 UART 串口 console。',
      workload: {
        stage: 'Wave 7',
        category: 'linux-serial-console',
        expectedMarker: 'mycpu-linux# ',
        ops: ['flat SBI shim', 'Linux Image payload', 'DTB', 'virtio-blk rootfs'],
      },
    },
  ];
  state.selectedTest = 'linux_proto_console';
  state.selectedScenario = 'linux_alpine_evidence';
  state.selectedScenarioTest = null;
  state.backend = 'functional';

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Alpine Distro Evidence/);
  assert.match(elements.guide.innerHTML, /Runtime manifest ready/);
  assert.match(elements.guide.innerHTML, /Read the distro evidence, then open the live shell route/);
  assert.match(elements.demoWorkspace.innerHTML, /Viewing topic|Open topic/);
  assert.match(elements.guide.innerHTML, /Open live shell/);
  assert.match(elements.guide.innerHTML, /data-action="open-scenario-live"/);
  assert.match(elements.guide.innerHTML, /data-scenario-backend="pipeline"/);
  assert.match(elements.guide.innerHTML, /Linux distro lane/);
  assert.match(elements.guide.innerHTML, /Capability ladder/);
  assert.match(elements.guide.innerHTML, /Curated distro matrix/);
});

test('renderApp keeps Linux distro topics readable but disables live controls without a runtime manifest', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_interactive_os_demo',
      menuLabel: 'guest_interactive_os_demo',
      kind: 'guest',
      backend: 'pipeline',
      title: 'interactive_os Monitor',
      summary: 'interactive monitor',
    },
  ];
  state.selectedTest = 'guest_interactive_os_demo';
  state.selectedScenario = 'linux_alpine_evidence';
  state.selectedScenarioTest = null;
  state.loadedSession = {
    test: 'guest_interactive_os_demo',
    backend: 'pipeline',
  };

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Alpine Distro Evidence/);
  assert.match(elements.guide.innerHTML, /External asset required|Runtime Image required/);
  assert.doesNotMatch(elements.guide.innerHTML, /Topic ready/);
  assert.doesNotMatch(elements.guide.innerHTML, /Open live shell/);
  assert.doesNotMatch(elements.guide.innerHTML, /data-action="open-scenario-live"/);
  assert.doesNotMatch(elements.guide.innerHTML, /Load current scenario/);
  assert.doesNotMatch(elements.guide.innerHTML, /data-action="load-current-session"/);
  assert.match(elements.guide.innerHTML, /Sync session/);
  assert.match(elements.guide.innerHTML, /disabled/);
});

test('renderApp shows a Linux boot progress strip while linux_proto_console is loading', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'linux_proto_console',
      menuLabel: 'linux_proto_console · Linux serial',
      kind: 'linux',
      backend: 'functional',
      title: 'Linux Serial Console',
      badge: 'Linux runtime',
      summary: '启动受控 linux_proto runtime，进入 UART 串口 console。',
      workload: {
        stage: 'Wave 7',
        category: 'linux-serial-console',
        expectedMarker: 'mycpu-linux# ',
        ops: ['flat SBI shim', 'Linux Image payload', 'DTB', 'virtio-blk rootfs'],
      },
    },
  ];
  state.selectedTest = 'linux_proto_console';
  state.backend = 'functional';
  state.runState = 'loading';
  state.loadProgress = {
    test: 'linux_proto_console',
    backend: 'functional',
    startedAt: 1700000000000,
    now: 1700000038000,
    waitingFor: 'mycpu-linux# ',
    label: 'Booting Linux',
  };

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Linux boot in progress/);
  assert.match(elements.demoWorkspace.innerHTML, /waiting for mycpu-linux# /);
  assert.match(elements.demoWorkspace.innerHTML, /functional/);
  assert.match(elements.demoWorkspace.innerHTML, /38s/);
  assert.match(elements.terminal.innerHTML, /Linux boot in progress/);
  assert.match(elements.terminal.innerHTML, /42s|38s/);
});

test('renderApp keeps normal demo loading free of Linux boot copy', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_interactive_os_demo',
      menuLabel: 'guest_interactive_os_demo',
      title: 'interactive_os Monitor',
      badge: 'OS Bring-up',
      summary: '输入 help 观察 guest monitor。',
      workload: {
        expectedMarker: 'monitor> ',
        ops: ['UART input'],
      },
    },
  ];
  state.selectedTest = 'guest_interactive_os_demo';
  state.backend = 'pipeline';
  state.runState = 'loading';
  state.loadProgress = {
    test: 'guest_interactive_os_demo',
    backend: 'pipeline',
    startedAt: 1700000000000,
    now: 1700000003000,
    waitingFor: 'monitor> ',
    label: 'Loading demo',
  };

  const elements = createElements();

  renderApp(elements, state);

  assert.doesNotMatch(elements.demoWorkspace.innerHTML, /Linux boot in progress/);
  assert.doesNotMatch(elements.terminal.innerHTML, /Linux boot in progress/);
});

test('renderApp shows Linux console workload asset note for a loaded Linux session', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.selectedTest = 'linux_proto_console';
  state.loadedSession = {
    test: 'linux_proto_console',
    backend: 'pipeline',
  };
  state.tests = [
    {
      name: 'linux_proto_console',
      menuLabel: 'linux_proto_console · Linux serial',
      kind: 'linux',
      title: 'Linux Serial Console',
      badge: 'Linux runtime',
      summary: '启动受控 linux_proto runtime，进入 UART 串口 console。',
      workload: {
        stage: 'Wave 7',
        expectedMarker: 'mycpu-linux# ',
        ops: ['flat SBI shim', 'Linux Image payload', 'DTB', 'virtio-blk rootfs'],
        pipelineNote: '配置本机 Linux Image 后才可运行；前端只桥接 UART 与现有 debug session。',
        assetNote: 'Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image before starting the frontend server.',
      },
    },
  ];

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.workload.innerHTML, /Linux Serial Console/);
  assert.match(elements.workload.innerHTML, /MYCPU_LINUX_PROTO_CONSOLE_IMAGE/);
  assert.match(elements.workload.innerHTML, /mycpu-linux# /);
  assert.match(elements.workload.innerHTML, /virtio-blk rootfs/);
});

test('renderApp turns JIT runtime labs into a runtime evidence topic instead of a placeholder', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_vector_cnn_demo',
      menuLabel: 'guest_vector_cnn_demo · Vector CNN',
      kind: 'guest',
      backend: 'pipeline',
      title: 'Minimal CNN Demo',
      badge: 'Vector + ML',
      summary: '固定 conv -> relu workload。',
      workload: {
        expectedMarker: 'V3OK',
        ops: ['vector registers', 'CNN lanes'],
      },
    },
  ];
  state.selectedTest = 'guest_vector_cnn_demo';
  state.selectedScenario = 'jit_runtime_lab';
  state.selectedScenarioTest = 'guest_vector_cnn_demo';
  state.backend = 'pipeline';

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /JIT \/ DBT Runtime Labs/);
  assert.match(elements.guide.innerHTML, /Runtime evidence topic/);
  assert.match(elements.guide.innerHTML, /Sample runtime dispatch/);
  assert.match(elements.guide.innerHTML, /Run JIT probe/);
  assert.match(elements.guide.innerHTML, /data-action="run-jit-dispatch"/);
  assert.match(elements.guide.innerHTML, /Sync session|Session synced/);
  assert.match(elements.guide.innerHTML, /Sync a runtime-friendly workload, then run the JIT probe/);
  assert.match(elements.demoWorkspace.innerHTML, /data-scenario-key="jit_runtime_lab"[\s\S]*Viewing topic/);
});

test('renderApp shows collected JIT dispatch evidence inside the runtime topic', () => {
  const state = createAppState();
  state.tests = [
    {
      name: 'guest_vector_cnn_demo',
      menuLabel: 'guest_vector_cnn_demo · Vector CNN',
      kind: 'guest',
      backend: 'pipeline',
      title: 'Minimal CNN Demo',
      badge: 'Vector + ML',
      summary: '固定 conv -> relu workload。',
      workload: {
        expectedMarker: 'V3OK',
        ops: ['vector registers', 'CNN lanes'],
      },
    },
  ];
  state.selectedTest = 'guest_vector_cnn_demo';
  state.selectedScenario = 'jit_runtime_lab';
  state.selectedScenarioTest = 'guest_vector_cnn_demo';
  state.backend = 'pipeline';
  state.jitDispatch = {
    runState: 'completed',
    error: null,
    summary: {
      type: 'jit_dispatch',
      ok: true,
      source: 'hot-path-profile',
      action: 'lowered-ready',
      start_pc: '0x80000000',
      end_pc: '0x80000020',
      cache_state: 'hit',
      planned: true,
      translated: true,
      lowered: true,
      fallback_to_reference: false,
      lowered_instruction_count: 5,
      candidate_executions: 18,
      candidate_retired_instructions: 72,
      reject_kind: 'none',
      reject_reason: 'none',
      helper_replay_kind: 'none',
      host_code: false,
      executable_memory: false,
      guest_execution: false,
    },
  };

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.guide.innerHTML, /Observed runtime dispatch/);
  assert.match(elements.guide.innerHTML, /lowered-ready/);
  assert.match(elements.guide.innerHTML, /hot-path-profile/);
  assert.match(elements.guide.innerHTML, /candidate executions 18/);
  assert.match(elements.guide.innerHTML, /cache hit/);
});


test('renderApp does not let stale closed DOM state override requested group expansion', () => {
  const state = createAppState();
  state.runState = 'running';
  state.backend = 'pipeline';
  state.layout.debugPanelOpen = true;
  state.layout.architectureGroupOpen = true;
  state.layout.platformGroupOpen = true;

  pushSnapshot(state, {
    summary: {
      cycle: 5,
      instret: 2,
      pc: '0x80000088',
      halted: false,
      privilege: 'S',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: true, pc: '0x80000088', raw: '0x00000013', text: 'addi x0, x0, 0' },
      id: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: true,
      },
      ooo: {
        rob_depth: 0,
        rob_head_sequence_id: 0,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 1,
        correct_predictions: 1,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 5, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.registers.innerHTML, /panel-architecture-grid/);
  assert.match(elements.devices.innerHTML, /panel-platform-grid/);
  assert.doesNotMatch(elements.registers.innerHTML, /data-layout-key="architectureGroupOpen"/);
  assert.doesNotMatch(elements.devices.innerHTML, /data-layout-key="platformGroupOpen"/);
});

test('renderApp shows vector workload guide, CNN panel, and live vector registers', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.backend = 'pipeline';
  state.loadedSession = {
    test: 'guest_vector_cnn_demo',
    backend: 'pipeline',
  };
  state.tests = [
    {
      name: 'guest_vector_cnn_demo',
      menuLabel: 'guest_vector_cnn_demo · conv->relu',
      kind: 'guest',
      title: 'Minimal CNN Demo',
      badge: 'Vector + NN',
      summary: '固定输入与固定卷积核的 conv -> relu 样本。',
      workload: {
        stage: 'P0-P3',
        expectedMarker: 'V3OK',
        ops: ['vsetcfg', 'vle.v', 'vdot.vv', 'vmax.vv', 'vse.v'],
        pipelineNote: 'non-memory vector ALU 可进入最小 vector-aware path；config / memory 仍 serializing。',
        registerFocus: [3, 4, 5],
        cnn: {
          input: [2, -1, 3, 4, -2, 1],
          kernel: [1, 0, -1, 2],
          conv: [7, -9, 7],
          relu: [7, 0, 7],
          liveConvReg: 4,
          liveReluReg: 5,
        },
      },
    },
  ];
  state.selectedTest = 'guest_vector_cnn_demo';
  state.terminal.connected = true;
  state.layout.debugPanelOpen = true;

  const baseVector = Array.from({ length: 32 }, () => '0x00000000000000000000000000000000');
  const nextVector = [...baseVector];
  nextVector[4] = '0x07000000f7ffffff0700000000000000';
  nextVector[5] = '0x07000000000000000700000000000000';

  pushSnapshot(state, {
    summary: {
      cycle: 10,
      instret: 6,
      pc: '0x8000008c',
      halted: false,
      privilege: 'M',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      id: { valid: true, pc: '0x80000088', raw: '0x0', text: 'vsetcfg' },
      ex: { valid: true, pc: '0x80000084', raw: '0x0', text: 'vdot.vv v3, v1, v2' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: true,
      },
      ooo: {
        rob_depth: 1,
        rob_head_sequence_id: 6,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 0,
        correct_predictions: 0,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    vector: {
      sew_bytes: 1,
      vl: 0,
      registers: baseVector,
    },
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 10, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  pushSnapshot(state, {
    summary: {
      cycle: 18,
      instret: 12,
      pc: '0x800000a8',
      halted: false,
      privilege: 'M',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: true, pc: '0x800000a8', raw: '0x0', text: 'vmax.vv v5, v4, v0' },
      id: { valid: true, pc: '0x800000a4', raw: '0x0', text: 'vse.v v5, (s0)' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: true,
        stall_reason: 'vector_state_busy',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: false,
      },
      ooo: {
        rob_depth: 2,
        rob_head_sequence_id: 12,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 0,
        correct_predictions: 0,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    vector: {
      sew_bytes: 4,
      vl: 3,
      registers: nextVector,
    },
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 18, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.workload.innerHTML, /Minimal CNN Demo/);
  assert.match(elements.workload.innerHTML, /V3OK/);
  assert.match(elements.pipeline.innerHTML, /vector relu/);
  assert.match(elements.pipeline.innerHTML, /vector mem/);
  assert.match(elements.vector.innerHTML, /Vector State/);
  assert.match(elements.vector.innerHTML, /vector_state_busy/);
  assert.match(elements.vector.innerHTML, /expected conv/);
  assert.match(elements.vector.innerHTML, /live v5 \/ relu/);
  assert.match(elements.vector.innerHTML, /7 · -9 · 7/);
  assert.match(elements.vector.innerHTML, /7 · 0 · 7/);
});

test('renderApp binds workload and vector panels to the loaded session instead of pending selector state', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.selectedTest = 'hello';
  state.backend = 'functional';
  state.loadedSession = {
    test: 'guest_vector_cnn_demo',
    backend: 'pipeline',
  };
  state.tests = [
    {
      name: 'hello',
      menuLabel: 'hello',
      kind: 'asm',
      title: 'Hello',
      summary: 'plain asm smoke',
    },
    {
      name: 'guest_vector_cnn_demo',
      menuLabel: 'guest_vector_cnn_demo · conv->relu',
      kind: 'guest',
      title: 'Minimal CNN Demo',
      badge: 'Vector + NN',
      summary: '固定输入与固定卷积核的 conv -> relu 样本。',
      workload: {
        stage: 'P0-P3',
        expectedMarker: 'V3OK',
        ops: ['vsetcfg', 'vle.v', 'vdot.vv', 'vmax.vv', 'vse.v'],
        pipelineNote: 'non-memory vector ALU 可进入最小 vector-aware path；config / memory 仍 serializing。',
        registerFocus: [4, 5],
        cnn: {
          input: [2, -1, 3, 4, -2, 1],
          kernel: [1, 0, -1, 2],
          conv: [7, -9, 7],
          relu: [7, 0, 7],
          liveConvReg: 4,
          liveReluReg: 5,
        },
      },
    },
  ];
  state.terminal.connected = true;
  state.layout.debugPanelOpen = true;

  const registers = Array.from({ length: 32 }, () => '0x00000000000000000000000000000000');
  registers[4] = '0x07000000f7ffffff0700000000000000';
  registers[5] = '0x07000000000000000700000000000000';

  pushSnapshot(state, {
    summary: {
      cycle: 18,
      instret: 12,
      pc: '0x800000a8',
      halted: false,
      privilege: 'M',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: true, pc: '0x800000a8', raw: '0x0', text: 'vmax.vv v5, v4, v0' },
      id: { valid: true, pc: '0x800000a4', raw: '0x0', text: 'vse.v v5, (s0)' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: true,
        stall_reason: 'vector_state_busy',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: false,
      },
      ooo: {
        rob_depth: 2,
        rob_head_sequence_id: 12,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 0,
        correct_predictions: 0,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    vector: {
      sew_bytes: 4,
      vl: 3,
      registers,
    },
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 18, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.workload.innerHTML, /Minimal CNN Demo/);
  assert.match(elements.vector.innerHTML, /backend<\/span>\s*<strong>pipeline<\/strong>/);
  assert.doesNotMatch(elements.workload.innerHTML, /plain asm smoke/);
  assert.doesNotMatch(elements.vector.innerHTML, /backend<\/span>\s*<strong>functional<\/strong>/);
});

test('renderApp preserves 64-bit vector lane precision in the register summary', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.selectedTest = 'guest_vector_demo';
  state.loadedSession = {
    test: 'guest_vector_demo',
    backend: 'pipeline',
  };
  state.tests = [
    {
      name: 'guest_vector_demo',
      menuLabel: 'guest_vector_demo · V-lite ops',
      kind: 'guest',
      title: 'V-lite Operator Demo',
      summary: 'vector precision smoke',
      workload: {
        stage: 'P0-P3',
        registerFocus: [3],
      },
    },
  ];
  state.terminal.connected = true;
  state.layout.debugPanelOpen = true;

  const registers = Array.from({ length: 32 }, () => '0x00000000000000000000000000000000');
  registers[3] = '0x01000000000000200000000000000000';

  pushSnapshot(state, {
    summary: {
      cycle: 5,
      instret: 1,
      pc: '0x80000080',
      halted: false,
      privilege: 'M',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      id: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: true,
      },
      ooo: {
        rob_depth: 0,
        rob_head_sequence_id: 0,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 0,
        correct_predictions: 0,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    vector: {
      sew_bytes: 8,
      vl: 1,
      registers,
    },
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 0 },
      clint: { mtime: 5, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.vector.innerHTML, /2305843009213693953/);
  assert.doesNotMatch(elements.vector.innerHTML, /2305843009213694000/);
});

test('renderApp shows AI accelerator workload guide and aggregate counters', () => {
  const state = createAppState();
  state.runState = 'halted';
  state.backend = 'pipeline';
  state.loadedSession = {
    test: 'guest_ai_accel_demo',
    backend: 'pipeline',
  };
  state.tests = [
    {
      name: 'guest_ai_accel_demo',
      menuLabel: 'guest_ai_accel_demo · AI accel MMIO',
      kind: 'guest',
      title: 'AI Accelerator Demo',
      badge: 'AI Accelerator',
      summary: '通过 MMIO 提交一个最小 graph package，并用 KMVAI 验证 guest 到设备的闭环。',
      workload: {
        stage: 'Wave 4',
        expectedMarker: 'KMVAI',
        ops: ['graph package', 'MMIO doorbell', 'DMA load/store', 'timed-simple profile'],
        pipelineNote: '当前 frontend 只展示 debug snapshot 中的 aggregate counters；op summary 和真实 DMA overlap 后移到后续专项阶段。',
        progress: [
          ['Queue', '单 entry submission / completion queue'],
          ['DMA', 'load/store bytes 来自 debug snapshot'],
          ['Compute', 'timed-simple compute / stall attribution'],
          ['Profile', '只读 aggregate counters'],
        ],
      },
    },
  ];
  state.selectedTest = 'guest_ai_accel_demo';
  state.terminal.connected = true;
  state.layout.debugPanelOpen = true;
  state.layout.platformGroupOpen = true;

  pushSnapshot(state, {
    summary: {
      cycle: 128,
      instret: 64,
      pc: '0x80000120',
      halted: true,
      privilege: 'M',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      id: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      ex: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      mem: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      wb: { valid: false, pc: '0x0', raw: '0x0', text: '' },
      flags: {
        stalled: false,
        stall_reason: 'none',
        redirected: false,
        trap_flush: false,
        replay_flush: false,
        committed: true,
      },
      ooo: {
        rob_depth: 0,
        rob_head_sequence_id: 0,
        lsq_depth: 0,
        lsq_head_sequence_id: 0,
        lsq_load_state: 'none',
        lsq_load_sequence_id: 0,
        lsq_store_sequence_id: 0,
      },
      predictor: {
        mode: 'bimodal-2bit',
        total_predictions: 0,
        correct_predictions: 0,
        mispredictions: 0,
      },
    },
    gpr: Array.from({ length: 32 }, () => '0x0'),
    csrs: {
      mstatus: '0x0',
      sstatus: '0x0',
      mepc: '0x0',
      sepc: '0x0',
      mcause: '0x0',
      scause: '0x0',
      mie: '0x0',
      mip: '0x0',
      satp: '0x0',
    },
    devices: {
      uart: { ier: 0, output_size: 5, recent_output: 'KMVAI' },
      clint: { mtime: 128, mtimecmp: 0, timer_interrupt_pending: false },
      plic: { pending: false, level: false },
      storage: { attached: false, lba: 0 },
      ai_accelerator: {
        present: true,
        queue_depth: 0,
        doorbell_count: 1,
        last_fault: 0,
        completion_count: 1,
        engine_busy: false,
        scratchpad_occupancy_bytes: 0,
        dma_load_bytes: 12,
        dma_store_bytes: 4,
        device_cycles: 8,
        dma_cycles: 6,
        compute_cycles: 1,
        stall_cycles: 1,
        busy_cycles: 10,
        queue_cycles: 1,
        completion_cycles: 1,
        effective_ops_per_cycle: 3,
        utilization: 10,
      },
    },
    bus: {
      valid: false,
      success: true,
      write: false,
      mmio: false,
      device: '',
      addr: '0x0',
      value: '0x0',
      size: 0,
      detail: '',
    },
    events: [],
  });

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.workload.innerHTML, /AI Accelerator Demo/);
  assert.match(elements.workload.innerHTML, /KMVAI/);
  assert.match(elements.workload.innerHTML, /graph package/);
  assert.match(elements.workload.innerHTML, /timed-simple profile/);
  assert.match(elements.workload.innerHTML, /Queue/);
  assert.match(elements.devices.innerHTML, /AI accelerator/);
  assert.match(elements.devices.innerHTML, /<span>queue_depth<\/span><strong>0<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>engine_busy<\/span><strong>idle<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>dma_load_bytes<\/span><strong>12<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>dma_store_bytes<\/span><strong>4<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>compute_cycles<\/span><strong>1<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>stall_cycles<\/span><strong>1<\/strong>/);
  assert.match(elements.devices.innerHTML, /<span>utilization<\/span><strong>10<\/strong>/);
});

test('renderApp shows a demo-first workspace with selectable workloads and future labs', () => {
  const state = createAppState();
  state.runState = 'idle';
  state.backend = 'pipeline';
  state.selectedTest = 'guest_interactive_os_demo';
  state.tests = [
    {
      name: 'guest_interactive_os_demo',
      menuLabel: 'guest_interactive_os_demo',
      title: 'interactive_os Monitor',
      badge: 'OS Bring-up',
      summary: '输入 help、regs、disk 和 pagewalk 观察 guest monitor。',
      workload: {
        category: 'os-bringup',
        expectedMarker: 'monitor> ',
        ops: ['UART input', 'monitor commands'],
      },
    },
    {
      name: 'guest_ai_accel_demo',
      menuLabel: 'guest_ai_accel_demo · AI accel MMIO',
      title: 'AI Accelerator Demo',
      badge: 'AI Accelerator',
      summary: '通过 MMIO 提交一个最小 graph package。',
      workload: {
        category: 'ai-accelerator-demo',
        expectedMarker: 'KMVAI',
        ops: ['MMIO doorbell', 'DMA load/store'],
      },
    },
    {
      name: 'guest_vector_cnn_demo',
      menuLabel: 'guest_vector_cnn_demo · conv->relu',
      title: 'Minimal CNN Demo',
      badge: 'Vector + NN',
      summary: '固定输入与固定卷积核的 conv -> relu 样本。',
      workload: {
        category: 'vector-cnn-demo',
        expectedMarker: 'V3OK',
        ops: ['vsetcfg', 'vdot.vv', 'vmax.vv'],
      },
    },
  ];

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.demoWorkspace.innerHTML, /Scenario navigator/);
  assert.match(elements.demoWorkspace.innerHTML, /System Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /Linux Distro Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /Machine Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /AI Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /Runtime Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /interactive_os Monitor/);
  assert.match(elements.guide.innerHTML, /Scenario/);
  assert.match(elements.guide.innerHTML, /Operate the guest monitor through the browser terminal/);
  assert.match(elements.guide.innerHTML, /Session contract/);
  assert.match(elements.guide.innerHTML, /What to watch/);
  assert.match(elements.guide.innerHTML, /Evidence and boundary/);
  assert.match(elements.demoWorkspace.innerHTML, /AI Accelerator/);
  assert.match(elements.demoWorkspace.innerHTML, /AI Accelerator Demo/);
  assert.match(elements.demoWorkspace.innerHTML, /Vector CNN/);
  assert.match(elements.demoWorkspace.innerHTML, /Minimal CNN Demo/);
  assert.match(elements.demoWorkspace.innerHTML, /JIT \/ DBT Runtime Labs/);
  assert.match(elements.demoWorkspace.innerHTML, /Open topic/);
  assert.match(elements.guide.innerHTML, /Terminal \+ session summary/);
  assert.match(elements.guide.innerHTML, /Load the scenario to create a session and watch the primary stage/);
  assert.match(elements.guide.innerHTML, /Scenario controls/);
  assert.match(elements.guide.innerHTML, /Sync session|Session synced/);
  assert.match(elements.demoWorkspace.innerHTML, /data-demo-test="guest_ai_accel_demo"/);
  assert.match(elements.demoWorkspace.innerHTML, /data-demo-backend="pipeline"/);
  assert.match(elements.demoWorkspace.innerHTML, /is-selected/);
});

test('renderApp shows the AI parameterized tiny model controls and profile result', () => {
  const state = createAppState();
  state.aiTinyModel.templates = [
    {
      id: 'dynamic_tiny_model',
      title: 'Parameterized Tiny Model',
      summary: 'Server-generated dynamic tiny model profile.',
      opChain: ['gemm', 'relu', 'pool_max'],
      demo: {
        expectedMarker: 'balanced returns 2.5, 5.5 and negative_clamp returns 0, 2.5',
        proves: [
          'Server regenerates bounded graph package, runtime shape table, inputs and expected output.',
          'The profile path stays aligned with mycpu --ai-profile-manifest.',
        ],
        boundaries: [
          'No custom graph upload or arbitrary model import.',
          'Only approved batch and input preset values are allowed.',
        ],
      },
      parameters: {
        batch: { label: 'Batch', choices: [1, 2], default: 1 },
        inputPreset: {
          label: 'Input preset',
          choices: ['balanced', 'negative_clamp'],
          default: 'balanced',
          choiceLabels: {
            balanced: 'Balanced activations',
            negative_clamp: 'ReLU clamp path',
          },
        },
      },
      boundary: {
        allowsCustomGraph: false,
      },
    },
  ];
  state.aiTinyModel.parameters = {
    template: 'dynamic_tiny_model',
    batch: 2,
    inputPreset: 'negative_clamp',
  };
  state.aiTinyModel.runState = 'completed';
  state.aiTinyModel.result = {
    output: {
      dtype: 'fp32',
      shape: [2, 1],
      values: [0, 2.5],
      expected: [0, 2.5],
    },
    profile: {
      progress: 'completed',
      shapeMode: 'dynamic_bounded',
      runtimeShapes: 't0:2x3,t2:2x2,t3:2x2,t4:2x1',
      bytesMoved: 72,
      retiredOps: 12,
      deviceCycles: 33,
      dmaCycles: 12,
      computeCycles: 9,
      stallCycles: 6,
      utilization: 27,
    },
    ops: [
      { opIndex: 0, opcode: 'gemm', retiredOps: 8, computeCycles: 4, stallCycles: 2, tileCount: 1 },
      { opIndex: 1, opcode: 'eltwise_relu', retiredOps: 2, computeCycles: 2, stallCycles: 2, tileCount: 1 },
      { opIndex: 2, opcode: 'pool_max', retiredOps: 2, computeCycles: 3, stallCycles: 2, tileCount: 1 },
    ],
  };
  state.tests = [
    {
      name: 'guest_ai_accel_demo',
      menuLabel: 'guest_ai_accel_demo · AI accel MMIO',
      title: 'AI Accelerator Demo',
      badge: 'AI Accelerator',
      summary: '通过 MMIO 提交一个最小 graph package。',
      workload: {
        category: 'ai-accelerator-demo',
        expectedMarker: 'KMVAI',
        ops: ['MMIO doorbell', 'DMA load/store'],
      },
    },
  ];

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.aiLab.innerHTML, /Parameterized Tiny Model/);
  assert.match(elements.aiLab.innerHTML, /data-ai-template="dynamic_tiny_model"/);
  assert.match(elements.aiLab.innerHTML, /Expected marker/);
  assert.match(elements.aiLab.innerHTML, /What this proves/);
  assert.match(elements.aiLab.innerHTML, /Current boundary/);
  assert.match(elements.aiLab.innerHTML, /Observed evidence/);
  assert.match(elements.aiLab.innerHTML, /data-ai-evidence="matched"/);
  assert.match(elements.aiLab.innerHTML, /Matched expected output/);
  assert.match(elements.aiLab.innerHTML, /Batch 2/);
  assert.match(elements.aiLab.innerHTML, /ReLU clamp path/);
  assert.match(elements.aiLab.innerHTML, /balanced returns 2\.5, 5\.5 and negative_clamp returns 0, 2\.5/);
  assert.match(elements.aiLab.innerHTML, /No custom graph upload or arbitrary model import/);
  assert.match(elements.aiLab.innerHTML, /data-ai-param="batch"/);
  assert.match(elements.aiLab.innerHTML, /data-ai-param="inputPreset"/);
  assert.match(elements.aiLab.innerHTML, /negative_clamp/);
  assert.match(elements.aiLab.innerHTML, /Run profile/);
  assert.match(elements.aiLab.innerHTML, /dynamic_bounded/);
  assert.match(elements.aiLab.innerHTML, /t0:2x3,t2:2x2,t3:2x2,t4:2x1/);
  assert.match(elements.aiLab.innerHTML, /0, 2\.5/);
  assert.match(elements.aiLab.innerHTML, /gemm/);
  assert.match(elements.aiLab.innerHTML, /pool_max/);
  assert.match(elements.aiLab.innerHTML, /Custom graph upload is disabled/);
  assert.match(elements.aiLab.innerHTML, /data-ai-custom-graph-editor/);
  assert.match(elements.aiLab.innerHTML, /data-ai-custom-graph-json/);
  assert.match(elements.aiLab.innerHTML, /Run custom graph/);
  assert.match(elements.aiLab.innerHTML, /ai_custom_graph_v1/);
  assert.match(elements.aiLab.innerHTML, /bounded_dynamic_gemm_v1/);
  assert.match(elements.aiLab.innerHTML, /balanced_rows/);
});

test('renderApp shows the AI whitelist template selector and template-specific controls', () => {
  const state = createAppState();
  state.aiTinyModel.templates = [
    {
      id: 'dynamic_tiny_model',
      title: 'Parameterized Tiny Model',
      summary: 'Server-generated dynamic tiny model profile.',
      opChain: ['gemm', 'relu', 'pool_max'],
      demo: {
        expectedMarker: 'dynamic tiny model marker',
        proves: ['bounded dynamic tiny model contract'],
        boundaries: ['no custom upload'],
      },
      parameters: {
        batch: { choices: [1, 2], default: 1 },
        inputPreset: { choices: ['balanced', 'negative_clamp'], default: 'balanced' },
      },
      boundary: {
        allowsCustomGraph: false,
      },
    },
    {
      id: 'dynamic_gemm',
      title: 'Dynamic GEMM Profile',
      summary: 'Server-generated bounded dynamic GEMM profile.',
      opChain: ['gemm'],
      demo: {
        expectedMarker: 'single_row_identity_head returns 1, 2, 3, 8',
        proves: ['runtime shape gating proves bounded dynamic GEMM path'],
        boundaries: ['no arbitrary matrix sizes outside whitelist'],
      },
      parameters: {
        runtimeShape: { choices: ['two_rows_identity_tail', 'single_row_identity_head'], default: 'two_rows_identity_tail' },
      },
      boundary: {
        allowsCustomGraph: false,
      },
    },
    {
      id: 'dynamic_cnn',
      title: 'Dynamic CNN Profile',
      summary: 'Bounded dynamic conv2d -> relu -> transpose -> reduce profile.',
      opChain: ['conv2d', 'eltwise_relu', 'layout_transpose', 'reduce_sum'],
      demo: {
        expectedMarker: 'compact_2x2 returns 15, 31',
        proves: ['conv2d -> relu -> transpose -> reduce stays observable under bounded runtime shapes'],
        boundaries: ['no free-form CNN graph authoring'],
      },
      parameters: {
        runtimeShape: { choices: ['compact_2x2', 'full_3x3'], default: 'compact_2x2' },
      },
      boundary: {
        allowsCustomGraph: false,
      },
    },
    {
      id: 'tiny_attention_static',
      title: 'Tiny Attention Static',
      summary: 'Static attention-like profile with softmax in the middle.',
      opChain: ['gemm', 'softmax', 'gemm'],
      demo: {
        expectedMarker: 'uniform_query returns 2',
        proves: ['softmax stays visible as a fixed static graph profile'],
        boundaries: ['not a general transformer runtime'],
      },
      parameters: {
        inputPreset: { choices: ['uniform_query', 'biased_query'], default: 'uniform_query' },
      },
      boundary: {
        allowsCustomGraph: false,
      },
    },
  ];
  state.aiTinyModel.parameters = {
    template: 'dynamic_gemm',
    runtimeShape: 'single_row_identity_head',
  };

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.aiLab.innerHTML, /data-ai-param="template"/);
  assert.match(elements.aiLab.innerHTML, /dynamic_gemm/);
  assert.match(elements.aiLab.innerHTML, /dynamic_cnn/);
  assert.match(elements.aiLab.innerHTML, /tiny_attention_static/);
  assert.match(elements.aiLab.innerHTML, /data-ai-param="runtimeShape"/);
  assert.doesNotMatch(elements.aiLab.innerHTML, /data-ai-param="batch"/);
  assert.match(elements.aiLab.innerHTML, /single_row_identity_head/);
  assert.match(elements.aiLab.innerHTML, /Dynamic GEMM Profile/);
  assert.match(elements.aiLab.innerHTML, /gemm/);
  assert.match(elements.aiLab.innerHTML, /single_row_identity_head returns 1, 2, 3, 8/);
  assert.match(elements.aiLab.innerHTML, /runtime shape gating proves bounded dynamic GEMM path/);
  assert.match(elements.aiLab.innerHTML, /no arbitrary matrix sizes outside whitelist/);
});

test('renderApp shows AI tiny model validation errors without profile data', () => {
  const state = createAppState();
  state.aiTinyModel.templates = [
    {
      id: 'dynamic_tiny_model',
      title: 'Parameterized Tiny Model',
      parameters: {
        batch: { choices: [1, 2], default: 1 },
        inputPreset: { choices: ['balanced'], default: 'balanced' },
      },
      boundary: {
        allowsCustomGraph: false,
      },
    },
  ];
  state.aiTinyModel.runState = 'error';
  state.aiTinyModel.error = 'batch must be one of: 1, 2';

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.aiLab.innerHTML, /batch must be one of: 1, 2/);
  assert.doesNotMatch(elements.aiLab.innerHTML, /ai-tiny-model__result/);
});

test('renderApp marks AI tiny model evidence as mismatch when actual output diverges from expected', () => {
  const state = createAppState();
  state.aiTinyModel.templates = [
    {
      id: 'dynamic_cnn',
      title: 'Dynamic CNN Profile',
      summary: 'Bounded dynamic conv2d profile.',
      opChain: ['conv2d', 'eltwise_relu', 'layout_transpose', 'reduce_sum'],
      demo: {
        expectedMarker: 'compact_2x2 returns 15, 31',
        proves: ['bounded runtime shape path'],
        boundaries: ['no custom graph'],
      },
      parameters: {
        runtimeShape: {
          label: 'Runtime shape',
          choices: ['compact_2x2', 'full_3x3'],
          default: 'compact_2x2',
          choiceLabels: {
            compact_2x2: '3x3 -> 2x2 compact path',
            full_3x3: '4x4 -> 3x3 full path',
          },
        },
      },
      boundary: {
        allowsCustomGraph: false,
      },
    },
  ];
  state.aiTinyModel.parameters = {
    template: 'dynamic_cnn',
    runtimeShape: 'compact_2x2',
  };
  state.aiTinyModel.runState = 'completed';
  state.aiTinyModel.result = {
    output: {
      dtype: 'int32',
      shape: [2],
      values: [15, 30],
      expected: [15, 31],
    },
    profile: {
      progress: 'completed',
      shapeMode: 'dynamic_bounded',
      runtimeShapes: 't0:3x3,t2:2x2,t3:2x2,t4:2x2,t5:2',
      deviceCycles: 17,
    },
    ops: [],
  };

  const elements = createElements();

  renderApp(elements, state);

  assert.match(elements.aiLab.innerHTML, /Observed evidence/);
  assert.match(elements.aiLab.innerHTML, /data-ai-evidence="mismatch"/);
  assert.match(elements.aiLab.innerHTML, /Mismatch: actual output diverges from expected/);
  assert.match(elements.aiLab.innerHTML, /3x3 -&gt; 2x2 compact path/);
  assert.match(elements.aiLab.innerHTML, /15, 30/);
  assert.match(elements.aiLab.innerHTML, /expected 15, 31/);
});
