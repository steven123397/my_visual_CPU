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

test('renderTerminal labels a loaded Linux serial console session without interactive_os wording', () => {
  const html = renderTerminal({
    selectedTest: 'guest_interactive_os_demo',
    backend: 'functional',
    loadedSession: {
      test: 'linux_proto_console',
      backend: 'pipeline',
    },
    runState: 'paused',
    currentSnapshot: {
      summary: {
        pc: '0x80200000',
        privilege: 'S',
        cycle: 300000004,
        backend: 'pipeline',
      },
    },
    layout: {
      terminalCollapsed: false,
    },
    terminal: {
      connected: true,
      pendingInput: true,
      focused: false,
      buffer: 'mycpu-linux# help\r\ncommands: help uptime exit\r\nmycpu-linux# ',
    },
  });

  assert.match(html, /Linux serial terminal/);
  assert.match(html, /linux_proto_console · pipeline/);
  assert.match(html, /正在把按键送入 Linux serial console/);
  assert.doesNotMatch(html, /interactive_os terminal/);
  assert.doesNotMatch(html, /guest monitor/);
});

test('renderTerminal labels a loaded Course OS shell session without interactive_os wording', () => {
  const html = renderTerminal({
    selectedTest: 'guest_interactive_os_demo',
    backend: 'functional',
    loadedSession: {
      test: 'guest_course_os_shell_demo',
      backend: 'pipeline',
    },
    runState: 'paused',
    currentSnapshot: {
      summary: {
        pc: '0x80000000',
        privilege: 'S',
        cycle: 4500000,
        backend: 'pipeline',
      },
    },
    layout: {
      terminalCollapsed: false,
    },
    terminal: {
      connected: true,
      pendingInput: true,
      focused: false,
      buffer: 'course-os> help\r\nmeminfo schedstat fsstat\r\ncourse-os> ',
    },
  });

  assert.match(html, /Course OS shell terminal/);
  assert.match(html, /guest_course_os_shell_demo · pipeline/);
  assert.match(html, /正在把按键送入 Course OS shell/);
  assert.doesNotMatch(html, /interactive_os terminal/);
  assert.doesNotMatch(html, /guest monitor/);
});

test('renderTerminal prefers manifest terminal presentation for active sessions', () => {
  const html = renderTerminal({
    tests: [
      {
        name: 'guest_course_os_shell_demo',
        workload: {
          terminal: {
            title: 'Manifest Course Console',
            target: 'manifest shell target',
          },
        },
      },
    ],
    selectedTest: 'guest_interactive_os_demo',
    backend: 'functional',
    loadedSession: {
      test: 'guest_course_os_shell_demo',
      backend: 'pipeline',
    },
    runState: 'paused',
    currentSnapshot: {
      summary: {
        pc: '0x80000000',
        privilege: 'S',
        cycle: 4500000,
        backend: 'pipeline',
      },
    },
    layout: {
      terminalCollapsed: false,
    },
    terminal: {
      connected: true,
      pendingInput: true,
      focused: false,
      buffer: 'course-os> ',
    },
  });

  assert.match(html, /Manifest Course Console/);
  assert.match(html, /正在把按键送入 manifest shell target/);
  assert.doesNotMatch(html, /Course OS shell terminal/);
});

test('renderTerminal shows Linux boot progress while the load request is pending', () => {
  const html = renderTerminal({
    selectedTest: 'linux_proto_console',
    backend: 'functional',
    loadedSession: null,
    runState: 'loading',
    loadProgress: {
      test: 'linux_proto_console',
      backend: 'functional',
      startedAt: 1700000000000,
      now: 1700000042000,
      waitingFor: 'mycpu-linux# ',
      label: 'Booting Linux',
    },
    currentSnapshot: null,
    layout: {
      terminalCollapsed: false,
    },
    terminal: {
      connected: false,
      pendingInput: false,
      focused: false,
      buffer: '',
    },
  });

  assert.match(html, /Linux boot in progress/);
  assert.match(html, /waiting for mycpu-linux# /);
  assert.match(html, /functional/);
  assert.match(html, /42s/);
  assert.match(html, /Linux serial terminal/);
});
