import test from 'node:test';
import assert from 'node:assert/strict';

import { loadSession, runAiCustomGraph } from '../app/api.js';

test('loadSession posts custom ELF path payloads without a manifest test name', async () => {
  const previousFetch = globalThis.fetch;
  const calls = [];
  globalThis.fetch = async (pathname, options) => {
    calls.push({ pathname, options });
    return new Response(JSON.stringify({ ok: true }), {
      status: 200,
      headers: { 'content-type': 'application/json' },
    });
  };

  try {
    await loadSession(null, 'functional', {
      elfPath: '/allowed/demo.elf',
    });

    assert.equal(calls.length, 1);
    assert.equal(calls[0].pathname, '/api/session/load');
    assert.deepEqual(JSON.parse(calls[0].options.body), {
      backend: 'functional',
      elfPath: '/allowed/demo.elf',
    });
  } finally {
    globalThis.fetch = previousFetch;
  }
});

test('loadSession posts custom ELF base64 payloads with an optional display name', async () => {
  const previousFetch = globalThis.fetch;
  const calls = [];
  globalThis.fetch = async (pathname, options) => {
    calls.push({ pathname, options });
    return new Response(JSON.stringify({ ok: true }), {
      status: 200,
      headers: { 'content-type': 'application/json' },
    });
  };

  try {
    await loadSession(null, 'pipeline', {
      elfBase64: 'f0VMRg==',
      elfName: 'browser-demo.elf',
    });

    assert.deepEqual(JSON.parse(calls[0].options.body), {
      backend: 'pipeline',
      elfBase64: 'f0VMRg==',
      elfName: 'browser-demo.elf',
    });
  } finally {
    globalThis.fetch = previousFetch;
  }
});

test('runAiCustomGraph posts bounded custom graph JSON to the facade endpoint', async () => {
  const previousFetch = globalThis.fetch;
  const calls = [];
  globalThis.fetch = async (pathname, options) => {
    calls.push({ pathname, options });
    return new Response(JSON.stringify({ ok: true, schema: 'ai_custom_graph_result_v1' }), {
      status: 200,
      headers: { 'content-type': 'application/json' },
    });
  };

  try {
    const payload = {
      schema: 'ai_custom_graph_v1',
      opSequence: ['gemm'],
      dtype: 'int8/int32',
      shape: {
        kind: 'bounded_dynamic_gemm_v1',
        batch: 2,
      },
      inputPreset: 'balanced_rows',
    };
    const response = await runAiCustomGraph(payload);

    assert.equal(response.schema, 'ai_custom_graph_result_v1');
    assert.equal(calls.length, 1);
    assert.equal(calls[0].pathname, '/api/ai/custom-graph');
    assert.deepEqual(JSON.parse(calls[0].options.body), payload);
  } finally {
    globalThis.fetch = previousFetch;
  }
});
