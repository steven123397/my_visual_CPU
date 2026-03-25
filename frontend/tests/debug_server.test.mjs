import test from 'node:test';
import assert from 'node:assert/strict';

import { startServer } from '../server/debug_server.mjs';

function createFakeSession() {
  let cycle = 0;
  return {
    async load() {
      cycle = 0;
      return { ok: true };
    },
    async snapshot() {
      return {
        type: 'snapshot',
        summary: {
          cycle,
          instret: cycle,
          pc: '0x80000000',
          halted: false,
          privilege: 'M',
          backend: 'pipeline',
        },
        pipeline: { if: { valid: cycle > 0, text: 'addi 0x00000013' }, flags: {} },
        devices: {},
        bus: {},
        events: [],
      };
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

test('GET /api/tests returns built-in test manifest', async () => {
  const server = await startServer({
    port: 0,
    createSession: async () => createFakeSession(),
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
    createSession: async () => createFakeSession(),
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
