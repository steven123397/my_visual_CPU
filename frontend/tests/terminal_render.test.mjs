import test from 'node:test';
import assert from 'node:assert/strict';

import { projectTerminalBuffer, renderTerminal } from '../app/components/terminal.js';

test('projectTerminalBuffer folds carriage returns and backspace erase sequences', () => {
  assert.equal(
    projectTerminalBuffer('KMV\r\nmonitor> hej\b \blp'),
    'KMV\nmonitor> help',
  );
});

test('renderTerminal shows the projected terminal text instead of raw control characters', () => {
  const html = renderTerminal({
    selectedTest: 'guest_interactive_os_demo',
    backend: 'pipeline',
    runState: 'paused',
    currentSnapshot: {
      summary: {
        pc: '0x80000000',
        privilege: 'S',
        cycle: 42,
      },
    },
    layout: {
      terminalCollapsed: false,
    },
    terminal: {
      connected: true,
      pendingInput: false,
      focused: false,
      buffer: 'monitor> hej\b \blp',
    },
  });

  assert.match(html, /monitor&gt; help/);
  assert.doesNotMatch(html, /\u0008/);
});

test('renderTerminal exposes a collapsed shell without dropping session context', () => {
  const html = renderTerminal({
    selectedTest: 'guest_interactive_os_demo',
    backend: 'pipeline',
    runState: 'paused',
    currentSnapshot: {
      summary: {
        pc: '0x80000000',
        privilege: 'S',
        cycle: 42,
      },
    },
    layout: {
      terminalCollapsed: true,
    },
    terminal: {
      connected: true,
      pendingInput: false,
      focused: false,
      buffer: 'boot\nmonitor> hej\b \blp',
    },
  });

  assert.match(html, /terminal-window is-collapsed/);
  assert.match(html, /展开 terminal/);
  assert.match(html, /monitor&gt; help/);
  assert.match(html, /terminal 已收起，展开后继续交互/);
  assert.doesNotMatch(html, /点击终端开始输入/);
  assert.doesNotMatch(html, /terminal-caret/);
});

test('renderTerminal keeps the load guidance visible when a collapsed terminal has no active session', () => {
  const html = renderTerminal({
    selectedTest: 'guest_interactive_os_demo',
    backend: 'pipeline',
    runState: 'idle',
    currentSnapshot: {
      summary: {
        pc: '0x0',
        privilege: 'M',
        cycle: 0,
      },
    },
    layout: {
      terminalCollapsed: true,
    },
    terminal: {
      connected: false,
      pendingInput: false,
      focused: false,
      buffer: '',
    },
  });

  assert.match(html, /先加载一个会话/);
  assert.doesNotMatch(html, /展开后继续交互/);
});

test('renderTerminal keeps the pending-input hint visible while collapsed', () => {
  const html = renderTerminal({
    selectedTest: 'guest_interactive_os_demo',
    backend: 'pipeline',
    runState: 'paused',
    currentSnapshot: {
      summary: {
        pc: '0x80000000',
        privilege: 'S',
        cycle: 42,
      },
    },
    layout: {
      terminalCollapsed: true,
    },
    terminal: {
      connected: true,
      pendingInput: true,
      focused: false,
      buffer: 'monitor> he',
    },
  });

  assert.match(html, /正在把按键送入 guest monitor/);
  assert.doesNotMatch(html, /展开后继续交互/);
});

test('renderTerminal keeps the active session identity separate from pending selector changes', () => {
  const html = renderTerminal({
    selectedTest: 'hello',
    backend: 'functional',
    loadedSession: {
      test: 'guest_vector_cnn_demo',
      backend: 'pipeline',
    },
    runState: 'paused',
    currentSnapshot: {
      summary: {
        pc: '0x80000000',
        privilege: 'S',
        cycle: 42,
        backend: 'pipeline',
      },
    },
    layout: {
      terminalCollapsed: false,
    },
    terminal: {
      connected: true,
      pendingInput: false,
      focused: false,
      buffer: 'monitor> ',
    },
  });

  assert.match(html, /guest_vector_cnn_demo · pipeline/);
  assert.doesNotMatch(html, /hello · functional/);
});
