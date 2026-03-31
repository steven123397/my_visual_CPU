import test from 'node:test';
import assert from 'node:assert/strict';

import { createTerminalInputPump } from '../app/terminal_input_pump.js';

function delay() {
  return new Promise((resolve) => setImmediate(resolve));
}

test('createTerminalInputPump batches synchronous input into one request', async () => {
  const sent = [];
  const responses = [];
  const pump = createTerminalInputPump({
    sendInput: async (text) => {
      sent.push(text);
      return { text };
    },
    onResponse: (payload) => {
      responses.push(payload.text);
    },
  });

  pump.enqueue('a');
  pump.enqueue('b');
  await delay();

  assert.deepEqual(sent, ['ab']);
  assert.deepEqual(responses, ['ab']);
});

test('createTerminalInputPump keeps queued input while a request is in flight', async () => {
  const sent = [];
  const responses = [];
  let releaseFirst;
  const firstRequest = new Promise((resolve) => {
    releaseFirst = resolve;
  });
  const pump = createTerminalInputPump({
    sendInput: async (text) => {
      sent.push(text);
      if (sent.length === 1) {
        await firstRequest;
      }
      return { text };
    },
    onResponse: (payload) => {
      responses.push(payload.text);
    },
  });

  pump.enqueue('a');
  await delay();
  pump.enqueue('b');
  releaseFirst();
  await delay();
  await delay();

  assert.deepEqual(sent, ['a', 'b']);
  assert.deepEqual(responses, ['a', 'b']);
});
