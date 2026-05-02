import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { startServer } from '../server/debug_server.mjs';
import { listTests } from '../server/tests_manifest.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..', '..');
const linuxConsoleE2eEnabled = process.env.MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E === '1';

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

test('real debug server + debug CLI can keep guest_interactive_os_demo responsive across run/pause and terminal input', async () => {
  const tests = listTests(repoRoot);
  const interactiveTest = tests.find((item) => item.name === 'guest_interactive_os_demo');
  assert.ok(interactiveTest, 'guest_interactive_os_demo should be part of the manifest');

  const server = await startServer({ port: 0 });
  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: interactiveTest.name,
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);
    assert.match(loadResponse.body.terminal.text, /monitor> /);

    const initialCycle = loadResponse.body.snapshot.summary.cycle;
    const runResponse = await postJson(server.baseUrl, '/api/session/run', { rateHz: 1000 });
    assert.equal(runResponse.status, 200);

    await new Promise((resolve) => setTimeout(resolve, 80));

    const pauseResponse = await postJson(server.baseUrl, '/api/session/pause', {});
    assert.equal(pauseResponse.status, 200);
    assert.ok(pauseResponse.body.snapshot.summary.cycle > initialCycle);

    const helpResponse = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'help\r',
    });
    assert.equal(helpResponse.status, 200);
    assert.match(helpResponse.body.text, /help echo time uptime halt/);
    assert.match(helpResponse.body.text, /monitor> $/);

    const snapshotResponse = await postJson(server.baseUrl, '/api/session/snapshot', {});
    assert.equal(snapshotResponse.status, 200);
    assert.equal(snapshotResponse.body.snapshot.summary.backend, 'pipeline');
    assert.ok(snapshotResponse.body.snapshot.summary.cycle >= pauseResponse.body.snapshot.summary.cycle);
  } finally {
    await server.close();
  }
});

test('real debug server + debug CLI can drive linux_proto_console help when explicitly enabled', {
  skip: linuxConsoleE2eEnabled ? false : 'set MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E=1 to run the real Linux console e2e guardrail',
}, async () => {
  const imagePath = process.env.MYCPU_LINUX_PROTO_CONSOLE_IMAGE
    ?? process.env.MYCPU_LINUX_PROTO_RUNTIME_IMAGE
    ?? null;
  assert.ok(imagePath, 'set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image');
  assert.ok(fs.existsSync(imagePath), `missing Linux console Image: ${imagePath}`);

  const tests = listTests(repoRoot);
  const linuxConsole = tests.find((item) => item.name === 'linux_proto_console');
  assert.ok(linuxConsole, 'linux_proto_console should be part of the manifest when a real Image is configured');
  assert.equal(linuxConsole.backend, 'functional');

  const server = await startServer({ port: 0 });
  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: linuxConsole.name,
      backend: linuxConsole.backend,
    });
    assert.equal(loadResponse.status, 200);
    assert.match(loadResponse.body.terminal.text, /mycpu-linux# /);

    const helpResponse = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'help\r',
    });
    assert.equal(helpResponse.status, 200);
    assert.match(helpResponse.body.text, /commands: help uptime exit/);
    assert.match(helpResponse.body.text, /mycpu-linux# $/);

    const terminateResponse = await postJson(server.baseUrl, '/api/session/terminate', {});
    assert.equal(terminateResponse.status, 200);
    assert.equal(terminateResponse.body.ok, true);
    assert.deepEqual(terminateResponse.body.terminal, {
      type: 'terminal',
      text: '',
      nextOffset: 0,
      reset: true,
    });
  } finally {
    await server.close();
  }
});
