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

function createSlotWithQueries(queries = {}) {
  return {
    innerHTML: '',
    dataset: {},
    querySelector(selector) {
      return queries[selector] ?? null;
    },
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

  assert.match(elements.registers.innerHTML, /<details class="panel panel-group panel-group-architecture" data-layout-key="architectureGroupOpen" open>/);
  assert.match(elements.devices.innerHTML, /<details class="panel panel-group panel-group-platform" data-layout-key="platformGroupOpen" open>/);
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

  const elements = {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlotWithQueries({ '.panel-group': { open: false } }),
    devices: createSlotWithQueries({ '.panel-group': { open: false } }),
    registers: createSlotWithQueries({ '.panel-group': { open: false } }),
    csrs: createSlot(),
    bus: createSlot(),
  };

  renderApp(elements, state);

  assert.match(elements.registers.innerHTML, /data-layout-key="architectureGroupOpen" open/);
  assert.match(elements.devices.innerHTML, /data-layout-key="platformGroupOpen" open/);
  assert.equal(state.layout.architectureGroupOpen, true);
  assert.equal(state.layout.platformGroupOpen, true);
});

test('renderApp shows vector workload guide, CNN panel, and live vector registers', () => {
  const state = createAppState();
  state.runState = 'paused';
  state.backend = 'pipeline';
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

  const elements = {
    desktop: createSlot(),
    debugInspector: createSlot(),
    terminal: createSlot(),
    summary: createSlot(),
    workload: createSlot(),
    predictor: createSlot(),
    pipeline: createSlot(),
    events: createSlot(),
    vector: createSlot(),
    devices: createSlot(),
    registers: createSlot(),
    csrs: createSlot(),
    bus: createSlot(),
  };

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
