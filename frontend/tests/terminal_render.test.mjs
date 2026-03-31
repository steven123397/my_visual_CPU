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
