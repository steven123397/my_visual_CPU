import test from 'node:test';
import assert from 'node:assert/strict';

import {
  applyTerminalChunk,
  createTerminalProjectionState,
} from '../shared/terminal_projection.mjs';

test('applyTerminalChunk folds backspace erase sequences and carriage-return overwrites', () => {
  const state = createTerminalProjectionState();

  applyTerminalChunk(state, 'hej\b \blp');
  applyTerminalChunk(state, '\nabc\rXYZ');

  assert.equal(state.text, 'help\nXYZ');
});

test('applyTerminalChunk keeps a bounded projected tail', () => {
  const state = createTerminalProjectionState({ maxLength: 8 });

  applyTerminalChunk(state, 'abx\b \bcdefghij');

  assert.equal(state.text, 'cdefghij');
});
