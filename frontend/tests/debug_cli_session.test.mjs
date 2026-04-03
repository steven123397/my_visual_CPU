import test from 'node:test';
import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import { PassThrough } from 'node:stream';

import { DebugCliSession } from '../server/debug_server.mjs';

function createMockChild() {
  const child = new EventEmitter();
  child.stdout = new PassThrough();
  child.stderr = new PassThrough();
  child.stdin = {
    writes: [],
    write(chunk) {
      this.writes.push(chunk);
      return true;
    },
  };
  child.killed = false;
  child.kill = () => {
    child.killed = true;
  };
  return child;
}

test('DebugCliSession request times out and keeps pending queue consistent', { timeout: 1000 }, async () => {
  const child = createMockChild();
  const session = new DebugCliSession({
    binaryPath: '/tmp/fake-mycpu',
    spawnImpl: () => child,
    requestTimeoutMs: 20,
  });

  await assert.rejects(
    session.request({ cmd: 'snapshot' }),
    /timed out/i,
  );
  assert.equal(session.pending.length, 0);

  const next = session.request({ cmd: 'snapshot' });
  child.stdout.write(`${JSON.stringify({ type: 'snapshot', summary: { cycle: 7 } })}\n`);
  const response = await next;
  assert.equal(response.summary.cycle, 7);

  await session.close();
});

test('DebugCliSession rejects pending and future requests after child exit', { timeout: 1000 }, async () => {
  const child = createMockChild();
  const session = new DebugCliSession({
    binaryPath: '/tmp/fake-mycpu',
    spawnImpl: () => child,
    requestTimeoutMs: 100,
  });

  const pending = session.request({ cmd: 'snapshot' });
  child.emit('exit', 7, null);

  await assert.rejects(pending, /debug cli.*(exit|unavailable|closed)/i);
  assert.equal(session.pending.length, 0);
  await assert.rejects(
    session.request({ cmd: 'snapshot' }),
    /debug cli.*(exit|unavailable|closed)/i,
  );
});

test('DebugCliSession close tears down pending queue and rejects later requests', { timeout: 1000 }, async () => {
  const child = createMockChild();
  const session = new DebugCliSession({
    binaryPath: '/tmp/fake-mycpu',
    spawnImpl: () => child,
    requestTimeoutMs: 200,
  });

  const pending = session.request({ cmd: 'step_cycle' });
  const pendingRejected = assert.rejects(
    pending,
    /debug cli.*(closed|shutdown|unavailable|exit)/i,
  );
  await session.close();

  await pendingRejected;
  assert.equal(session.pending.length, 0);
  assert.equal(child.killed, true);
  await assert.rejects(
    session.request({ cmd: 'snapshot' }),
    /debug cli.*(closed|shutdown|unavailable|exit)/i,
  );
});
