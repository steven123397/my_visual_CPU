import test from 'node:test';
import assert from 'node:assert/strict';

import {
  appendTerminalOutput,
  createAppState,
  normalizeTerminalInput,
  setDebugPanelOpen,
  setTerminalFocus,
  setTerminalPendingInput,
} from '../app/state.js';

test('createAppState starts with a collapsed inspector and inactive terminal', () => {
  const state = createAppState();

  assert.deepEqual(state.terminal, {
    buffer: '',
    nextOffset: 0,
    focused: false,
    connected: false,
    pendingInput: false,
  });
  assert.deepEqual(state.layout, {
    debugPanelOpen: false,
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
  assert.equal(state.terminal.buffer, 'boot\r\n');
  assert.equal(state.terminal.nextOffset, 6);
  assert.equal(state.terminal.connected, true);

  appendTerminalOutput(state, {
    text: '> ',
    nextOffset: 8,
  });
  assert.equal(state.terminal.buffer, 'boot\r\n> ');
  assert.equal(state.terminal.nextOffset, 8);
});

test('terminal focus, pending input and inspector visibility update independently', () => {
  const state = createAppState();

  setTerminalFocus(state, true);
  setTerminalPendingInput(state, true);
  setDebugPanelOpen(state, true);
  assert.equal(state.terminal.focused, true);
  assert.equal(state.terminal.pendingInput, true);
  assert.equal(state.layout.debugPanelOpen, true);

  setTerminalFocus(state, false);
  setTerminalPendingInput(state, false);
  setDebugPanelOpen(state, false);
  assert.equal(state.terminal.focused, false);
  assert.equal(state.terminal.pendingInput, false);
  assert.equal(state.layout.debugPanelOpen, false);
});
