import test from 'node:test';
import assert from 'node:assert/strict';

import { createDebugServerRuntime } from '../server/debug_server_runtime.mjs';

function makeSnapshot(cycle, label = 'session-1') {
  return {
    type: 'snapshot',
    summary: {
      cycle,
      instret: cycle,
      pc: '0x80000000',
      halted: false,
      privilege: 'S',
      backend: 'pipeline',
    },
    pipeline: {
      flags: {
        committed: cycle > 0,
        trap_flush: false,
      },
    },
    devices: {
      uart: {
        recent_output: label,
        output_size: label.length,
      },
    },
    events: [],
  };
}

function createFakeSession() {
  let cycle = 0;
  let terminal = 'boot:session-1\r\n> ';
  let closed = false;
  return {
    async load() {
      cycle = 0;
      terminal = 'boot:session-1\r\n> ';
      return { ok: true };
    },
    async snapshot() {
      return makeSnapshot(cycle);
    },
    async stepCycle() {
      cycle += 1;
      return makeSnapshot(cycle);
    },
    async stepCommit() {
      cycle += 1;
      return makeSnapshot(cycle);
    },
    async reset() {
      cycle = 0;
      terminal = 'reset\r\n> ';
      return makeSnapshot(cycle);
    },
    async runUntilUartContains() {
      cycle = 4;
      terminal = 'boot:session-1\r\nready\r\n> ';
      return makeSnapshot(cycle);
    },
    async uartInput(text) {
      terminal += text;
      return { ok: true };
    },
    async uartOutput(offset = 0) {
      return {
        text: terminal.slice(offset),
        nextOffset: terminal.length,
      };
    },
    async close() {
      closed = true;
    },
    get closed() {
      return closed;
    },
  };
}

function createWsHub() {
  return {
    messages: [],
    broadcast(message) {
      this.messages.push(message);
    },
  };
}

test('createDebugServerRuntime load resets terminal tracking and broadcasts initial snapshot', async () => {
  const session = createFakeSession();
  const wsHub = createWsHub();
  const runtime = createDebugServerRuntime({
    createSession: async () => session,
    wsHub,
  });

  const result = await runtime.load({
    name: 'hello',
    image: 'tests/asm/hello.elf',
    terminalPrompt: '> ',
    bootUntilUartText: 'ready',
    bootMaxSteps: 64,
  }, 'pipeline');

  assert.equal(result.ok, true);
  assert.equal(result.snapshot.summary.cycle, 4);
  assert.equal(result.terminal.reset, true);
  assert.equal(result.terminal.text, 'boot:session-1\r\nready\r\n> ');
  assert.deepEqual(
    wsHub.messages.map((message) => message.type),
    ['snapshot', 'terminal'],
  );

  await runtime.close();
  assert.equal(session.closed, true);
});
