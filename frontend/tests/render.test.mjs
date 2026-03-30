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

test('renderApp propagates failed MMIO bus details into the inspector bus slot', () => {
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
        redirected: false,
        trap_flush: true,
        committed: false,
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
  assert.match(elements.bus.innerHTML, /<span>device<\/span><strong>uart<\/strong>/);
  assert.match(elements.bus.innerHTML, /<span>status<\/span><strong>failed<\/strong>/);
  assert.match(elements.bus.innerHTML, /<span>detail<\/span><strong>invalid MMIO access<\/strong>/);
  assert.match(elements.events.innerHTML, /uart write 0x10000000 failed: invalid MMIO access/);
});
