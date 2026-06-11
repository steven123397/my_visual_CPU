import test from 'node:test';
import assert from 'node:assert/strict';

import { loadSession } from '../app/api.js';

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
