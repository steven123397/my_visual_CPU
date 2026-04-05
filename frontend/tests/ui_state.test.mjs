import test from 'node:test';
import assert from 'node:assert/strict';

import { buildTimelineRows, classifyEventTone, diffRegisters, shouldAutoScrollToBottom } from '../app/state.js';

test('buildTimelineRows highlights stalls and redirects', () => {
  const rows = buildTimelineRows([
    {
      summary: { cycle: 1 },
      pipeline: {
        if: { text: 'lw' },
        id: { text: 'add' },
        ex: { text: '' },
        mem: { text: '' },
        wb: { text: '' },
        flags: {
          stalled: true,
          stall_reason: 'memory_path_busy',
          redirected: false,
        },
      },
    },
    {
      summary: { cycle: 2 },
      pipeline: { if: { text: 'jal' }, id: { text: 'nop' }, ex: { text: 'beq' }, mem: { text: '' }, wb: { text: '' }, flags: { stalled: false, redirected: true } },
    },
  ]);

  assert.equal(rows[0].flag, 'stall');
  assert.equal(rows[0].flagLabel, 'stall: memory_path_busy');
  assert.equal(rows[1].flag, 'redirect');
  assert.equal(rows[1].flagLabel, 'redirect');
  assert.equal(rows[0].cycle, 1);
  assert.equal(rows[1].stages.ex, 'beq');
});

test('diffRegisters assigns layered emphasis for value changes', () => {
  const registers = diffRegisters(
    { gpr: ['0x0', '0x0', '0x8'] },
    { gpr: ['0x0', '0x3', '0x0'] },
  );

  assert.equal(registers[0].emphasis, 'steady');
  assert.equal(registers[1].emphasis, 'rise');
  assert.equal(registers[2].emphasis, 'clear');
});

test('classifyEventTone groups event kinds for clearer highlighting', () => {
  assert.equal(classifyEventTone('stall'), 'control');
  assert.equal(classifyEventTone('redirect'), 'control');
  assert.equal(classifyEventTone('trap'), 'trap');
  assert.equal(classifyEventTone('store'), 'memory');
  assert.equal(classifyEventTone('halt'), 'lifecycle');
  assert.equal(classifyEventTone('unknown'), 'neutral');
});

test('shouldAutoScrollToBottom only sticks when user is near the latest event', () => {
  assert.equal(shouldAutoScrollToBottom(null), true);
  assert.equal(shouldAutoScrollToBottom({ scrollTop: 360, clientHeight: 240, scrollHeight: 600 }), true);
  assert.equal(shouldAutoScrollToBottom({ scrollTop: 100, clientHeight: 240, scrollHeight: 600 }), false);
});
