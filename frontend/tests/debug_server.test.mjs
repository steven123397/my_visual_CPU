import test from 'node:test';
import assert from 'node:assert/strict';

import { startServer } from '../server/debug_server.mjs';

function makeSnapshot(cycle, sessionLabel) {
  return {
    type: 'snapshot',
    summary: {
      cycle,
      instret: cycle,
      pc: '0x80000000',
      halted: false,
      privilege: cycle === 0 ? 'M' : 'S',
      backend: 'pipeline',
    },
    pipeline: {
      if: { valid: cycle > 0, text: 'addi 0x00000013' },
      flags: {
        committed: cycle > 0,
        trap_flush: cycle === 0,
      },
    },
    devices: {
      uart: {
        ier: 1,
        recent_output: sessionLabel,
        output_size: sessionLabel.length,
      },
      clint: {
        mtime: cycle,
        mtimecmp: 4,
        timer_interrupt_pending: cycle >= 4,
      },
      plic: {
        pending: cycle > 0,
        claimed: cycle === 2,
        supervisor_has_pending: cycle > 0,
      },
    },
    bus: {
      device: cycle > 0 ? 'uart' : '-',
      addr: '0x10000000',
      value: '0x00000041',
      size: 1,
      write: true,
      mmio: cycle > 0,
    },
    events: [
      {
        kind: cycle === 0 ? 'trap' : 'commit',
        cycle,
        detail: `${sessionLabel}:${cycle === 0 ? 'load' : `cycle-${cycle}`}`,
      },
    ],
  };
}

function createFakeSession(sessionLabel = 'session-1') {
  let cycle = 0;
  return {
    async load() {
      cycle = 0;
      return { ok: true };
    },
    async snapshot() {
      return makeSnapshot(cycle, sessionLabel);
    },
    async stepCycle() {
      cycle += 1;
      return this.snapshot();
    },
    async stepCommit() {
      cycle += 2;
      return this.snapshot();
    },
    async reset() {
      cycle = 0;
      return this.snapshot();
    },
    async close() {},
  };
}

function createFakeSessionFactory() {
  let sessionId = 0;
  return async () => {
    sessionId += 1;
    return createFakeSession(`session-${sessionId}`);
  };
}

async function postJson(baseUrl, pathname, payload) {
  const response = await fetch(`${baseUrl}${pathname}`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(payload),
  });
  return {
    status: response.status,
    body: await response.json(),
  };
}

function waitForWebSocketOpen(socket) {
  return new Promise((resolve, reject) => {
    const onOpen = () => {
      cleanup();
      resolve();
    };
    const onError = (event) => {
      cleanup();
      reject(event.error ?? new Error('websocket connection failed'));
    };
    const cleanup = () => {
      socket.removeEventListener('open', onOpen);
      socket.removeEventListener('error', onError);
    };
    socket.addEventListener('open', onOpen);
    socket.addEventListener('error', onError);
  });
}

function waitForWebSocketMessage(socket) {
  return new Promise((resolve, reject) => {
    const onMessage = (event) => {
      cleanup();
      resolve(JSON.parse(event.data));
    };
    const onError = (event) => {
      cleanup();
      reject(event.error ?? new Error('websocket message failed'));
    };
    const cleanup = () => {
      socket.removeEventListener('message', onMessage);
      socket.removeEventListener('error', onError);
    };
    socket.addEventListener('message', onMessage);
    socket.addEventListener('error', onError);
  });
}

function wait(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

test('GET /api/tests returns built-in test manifest', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/api/tests`);
    const body = await response.json();
    assert.equal(response.status, 200);
    assert.ok(body.tests.some((item) => item.name === 'hello'));
    assert.ok(body.tests.some((item) => item.name === 'guest_supervisor_demo'));
    assert.ok(body.tests.some((item) => item.name === 'guest_kernel_alpha_demo'));
    assert.ok(body.tests.some((item) => item.name === 'guest_kernel_alpha_storage_not_ready_demo'));
  } finally {
    await server.close();
  }
});

test('POST /api/session/step-cycle returns updated snapshot', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/step-cycle', {});
    assert.equal(response.status, 200);
    assert.equal(response.body.snapshot.summary.cycle, 1);
  } finally {
    await server.close();
  }
});

test('load and step-cycle preserve rich snapshots across HTTP and WebSocket', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });
  const socket = new WebSocket(`${server.baseUrl.replace('http', 'ws')}/ws`);

  try {
    await waitForWebSocketOpen(socket);

    const loadMessage = waitForWebSocketMessage(socket);
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);
    assert.equal(loadResponse.body.snapshot.devices.clint.timer_interrupt_pending, false);
    assert.equal(loadResponse.body.snapshot.devices.plic.supervisor_has_pending, false);
    assert.equal(loadResponse.body.snapshot.devices.uart.recent_output, 'session-1');
    assert.equal(loadResponse.body.snapshot.pipeline.flags.trap_flush, true);
    assert.equal(loadResponse.body.snapshot.events[0].detail, 'session-1:load');
    assert.deepEqual(await loadMessage, { type: 'snapshot', snapshot: loadResponse.body.snapshot });

    const stepMessage = waitForWebSocketMessage(socket);
    const stepResponse = await postJson(server.baseUrl, '/api/session/step-cycle', {});
    assert.equal(stepResponse.status, 200);
    assert.equal(stepResponse.body.snapshot.summary.cycle, 1);
    assert.equal(stepResponse.body.snapshot.devices.clint.mtime, 1);
    assert.equal(stepResponse.body.snapshot.devices.plic.pending, true);
    assert.equal(stepResponse.body.snapshot.pipeline.flags.committed, true);
    assert.equal(stepResponse.body.snapshot.bus.mmio, true);
    assert.equal(stepResponse.body.snapshot.events[0].detail, 'session-1:cycle-1');
    assert.deepEqual(await stepMessage, { type: 'snapshot', snapshot: stepResponse.body.snapshot });
  } finally {
    socket.close();
    await server.close();
  }
});

test('POST /api/session/load stops a previous run before replacing the session', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const initialLoad = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(initialLoad.status, 200);

    const runResponse = await postJson(server.baseUrl, '/api/session/run', { rateHz: 1000 });
    assert.equal(runResponse.status, 200);
    await wait(35);

    const replacementLoad = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_supervisor_demo',
      backend: 'pipeline',
    });
    assert.equal(replacementLoad.status, 200);
    assert.equal(replacementLoad.body.snapshot.events[0].detail, 'session-2:load');

    await wait(70);

    const snapshotResponse = await postJson(server.baseUrl, '/api/session/snapshot', {});
    assert.equal(snapshotResponse.status, 200);
    assert.equal(snapshotResponse.body.snapshot.summary.cycle, 0);
    assert.equal(snapshotResponse.body.snapshot.events[0].detail, 'session-2:load');
  } finally {
    await server.close();
  }
});
