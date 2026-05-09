import test from 'node:test';
import assert from 'node:assert/strict';

import {
  MAX_TERMINAL_BUFFER,
  appendTerminalOutput,
  createAppState,
  normalizeTerminalInput,
  setDebugPanelOpen,
  setInspectorGroupOpen,
  setTerminalCollapsed,
  setTerminalFocus,
  setTerminalPendingInput,
} from '../app/state.js';

test('createAppState starts with an open inspector and inactive terminal', () => {
  const state = createAppState();

  assert.deepEqual(state.terminal, {
    buffer: '',
    nextOffset: 0,
    focused: false,
    connected: false,
    pendingInput: false,
  });
  assert.deepEqual(state.layout, {
    debugPanelOpen: true,
    architectureGroupOpen: false,
    platformGroupOpen: false,
    terminalCollapsed: false,
    collapsedPanels: [],
  });
});

test('normalizeTerminalInput only accepts the minimal terminal key set', () => {
  assert.equal(normalizeTerminalInput({ key: 'a' }), 'a');
  assert.equal(normalizeTerminalInput({ key: 'Enter' }), '\r');
  assert.equal(normalizeTerminalInput({ key: 'Backspace' }), '\b');
  assert.equal(normalizeTerminalInput({ key: 'ArrowUp' }), null);
  assert.equal(normalizeTerminalInput({ key: 'c', ctrlKey: true }), null);
  assert.equal(normalizeTerminalInput({ key: 'v', metaKey: true }), null);
});

test('appendTerminalOutput can reset or extend the terminal buffer while advancing offset', () => {
  const state = createAppState();

  appendTerminalOutput(state, {
    text: 'boot\r\n',
    nextOffset: 6,
    reset: true,
  });
  assert.equal(state.terminal.buffer, 'boot\n');
  assert.equal(state.terminal.nextOffset, 6);
  assert.equal(state.terminal.connected, true);

  appendTerminalOutput(state, {
    text: '> ',
    nextOffset: 8,
  });
  assert.equal(state.terminal.buffer, 'boot\n> ');
  assert.equal(state.terminal.nextOffset, 8);
});

test('appendTerminalOutput keeps only the latest terminal tail once the buffer grows too large', () => {
  const state = createAppState();
  const oversized = 'a'.repeat(MAX_TERMINAL_BUFFER + 32);

  appendTerminalOutput(state, {
    text: oversized,
    nextOffset: oversized.length,
    reset: true,
  });

  assert.equal(state.terminal.buffer.length, MAX_TERMINAL_BUFFER);
  assert.equal(state.terminal.buffer, oversized.slice(-MAX_TERMINAL_BUFFER));
});

test('terminal focus, pending input and inspector visibility update independently', () => {
  const state = createAppState();

  setTerminalFocus(state, true);
  setTerminalPendingInput(state, true);
  setDebugPanelOpen(state, true);
  setTerminalCollapsed(state, true);
  setInspectorGroupOpen(state, 'architectureGroupOpen', true);
  setInspectorGroupOpen(state, 'platformGroupOpen', true);
  assert.equal(state.terminal.focused, true);
  assert.equal(state.terminal.pendingInput, true);
  assert.equal(state.layout.debugPanelOpen, true);
  assert.equal(state.layout.terminalCollapsed, true);
  assert.equal(state.layout.architectureGroupOpen, true);
  assert.equal(state.layout.platformGroupOpen, true);

  setTerminalFocus(state, false);
  setTerminalPendingInput(state, false);
  setDebugPanelOpen(state, false);
  setTerminalCollapsed(state, false);
  setInspectorGroupOpen(state, 'architectureGroupOpen', false);
  setInspectorGroupOpen(state, 'platformGroupOpen', false);
  assert.equal(state.terminal.focused, false);
  assert.equal(state.terminal.pendingInput, false);
  assert.equal(state.layout.debugPanelOpen, false);
  assert.equal(state.layout.terminalCollapsed, false);
  assert.equal(state.layout.architectureGroupOpen, false);
  assert.equal(state.layout.platformGroupOpen, false);
});
