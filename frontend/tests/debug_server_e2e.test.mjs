import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { startServer } from '../server/debug_server.mjs';
import { listTests } from '../server/tests_manifest.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..', '..');

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

test('real debug server + debug CLI can load hello and surface UART output', async () => {
  const tests = listTests(repoRoot);
  const helloTest = tests.find((item) => item.name === 'hello');
  assert.ok(helloTest, 'hello should be part of the manifest');

  const server = await startServer({ port: 0 });
  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: helloTest.name,
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);
    assert.ok(loadResponse.body.snapshot.summary.instret >= 0);

    const commitResponse = await postJson(server.baseUrl, '/api/session/step-commit', {});
    assert.equal(commitResponse.status, 200);
    assert.ok(commitResponse.body.snapshot.summary.instret > 0);
    assert.equal(commitResponse.body.snapshot.summary.backend, 'pipeline');

    let terminalText = '';
    let terminalOffset = 0;
    for (let attempt = 0; attempt < 256; ++attempt) {
      const terminalResponse = await postJson(server.baseUrl, '/api/session/terminal-output', { offset: terminalOffset });
      assert.equal(terminalResponse.status, 200);
      terminalText += terminalResponse.body.text;
      terminalOffset = terminalResponse.body.nextOffset;
      if (/Hello, RISC-V!/.test(terminalText)) {
        break;
      }
      await postJson(server.baseUrl, '/api/session/step-commit', {});
    }
    assert.match(terminalText, /Hello, RISC-V!/);

    const snapshotResponse = await postJson(server.baseUrl, '/api/session/snapshot', {});
    assert.equal(snapshotResponse.status, 200);
    assert.ok(snapshotResponse.body.snapshot.pipeline);
  } finally {
    await server.close();
  }
});
