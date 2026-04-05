import test from 'node:test';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import net from 'node:net';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { startServer } from '../server/debug_server.mjs';
import { listTests } from '../server/tests_manifest.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..', '..');

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
      success: true,
      write: true,
      mmio: cycle > 0,
      detail: '',
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
  let currentTest = 'hello';
  let terminal = `boot:${sessionLabel}\r\n> `;
  let pendingTerminalOutput = '';
  let interactiveLine = '';
  let interactiveOutputCooldown = 0;
  let stepCommitCount = 0;
  const interactiveDripInterval = 24;
  return {
    async load(entry) {
      currentTest = entry?.name ?? 'hello';
      cycle = 0;
      pendingTerminalOutput = '';
      interactiveLine = '';
      interactiveOutputCooldown = 0;
      terminal =
        currentTest === 'guest_interactive_os_demo'
          ? ''
          : `boot:${sessionLabel}\r\n> `;
      return { ok: true };
    },
    async snapshot() {
      return makeSnapshot(cycle, sessionLabel);
    },
    async runUntilUartContains(text) {
      if (currentTest === 'guest_interactive_os_demo' && text === 'monitor> ') {
        cycle = 42;
        terminal = 'KMV\r\ninteractive monitor\r\nmonitor> ';
      }
      return this.snapshot();
    },
    async stepCycle() {
      cycle += 1;
      return this.snapshot();
    },
    async stepCommit() {
      stepCommitCount += 1;
      cycle += 2;
      if (pendingTerminalOutput) {
        if (currentTest === 'guest_interactive_os_demo') {
          if (interactiveOutputCooldown > 0) {
            interactiveOutputCooldown -= 1;
          } else {
            terminal += pendingTerminalOutput[0];
            pendingTerminalOutput = pendingTerminalOutput.slice(1);
            interactiveOutputCooldown = interactiveDripInterval;
          }
        } else {
          terminal += pendingTerminalOutput;
          pendingTerminalOutput = '';
        }
      }
      return this.snapshot();
    },
    async reset() {
      cycle = 0;
      terminal = '';
      pendingTerminalOutput = '';
      interactiveLine = '';
      interactiveOutputCooldown = 0;
      return this.snapshot();
    },
    async uartInput(text) {
      if (currentTest !== 'guest_interactive_os_demo') {
        pendingTerminalOutput += text;
        return { ok: true };
      }

      for (const char of text) {
        if (char === '\r') {
          pendingTerminalOutput += '\r\n';
          pendingTerminalOutput +=
            interactiveLine === 'help'
              ? 'help echo time uptime halt disk regs peek pagewalk pte\r\nmonitor> '
              : 'monitor> ';
          interactiveLine = '';
          continue;
        }

        if (char === '\b') {
          if (interactiveLine.length > 0) {
            interactiveLine = interactiveLine.slice(0, -1);
            pendingTerminalOutput += '\b \b';
          }
          continue;
        }

        interactiveLine += char;
        pendingTerminalOutput += char;
      }
      return { ok: true };
    },
    async uartOutput(offset = 0) {
      return {
        type: 'uart_output',
        offset,
        next_offset: terminal.length,
        text: terminal.slice(offset),
      };
    },
    async close() {},
    get stepCommitCount() {
      return stepCommitCount;
    },
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

function createEventTargetSocket(socket) {
  const listeners = new Map();

  function emit(type, event = {}) {
    const callbacks = listeners.get(type);
    if (!callbacks) {
      return;
    }
    for (const callback of [...callbacks]) {
      callback(event);
    }
  }

  return {
    addEventListener(type, callback) {
      if (!listeners.has(type)) {
        listeners.set(type, new Set());
      }
      listeners.get(type).add(callback);
    },
    removeEventListener(type, callback) {
      listeners.get(type)?.delete(callback);
    },
    close() {
      socket.end();
    },
    emit,
  };
}

function decodeFrames(buffer, emit) {
  let offset = 0;
  while (offset + 2 <= buffer.length) {
    const first = buffer[offset];
    const second = buffer[offset + 1];
    let payloadLength = second & 0x7f;
    let headerLength = 2;

    if (payloadLength === 126) {
      if (offset + 4 > buffer.length) {
        break;
      }
      payloadLength = buffer.readUInt16BE(offset + 2);
      headerLength = 4;
    } else if (payloadLength === 127) {
      if (offset + 10 > buffer.length) {
        break;
      }
      payloadLength = Number(buffer.readBigUInt64BE(offset + 2));
      headerLength = 10;
    }

    if (offset + headerLength + payloadLength > buffer.length) {
      break;
    }

    const opcode = first & 0x0f;
    const payload = buffer.slice(offset + headerLength, offset + headerLength + payloadLength);
    if (opcode === 0x1) {
      emit('message', { data: payload.toString('utf8') });
    } else if (opcode === 0x8) {
      emit('close', {});
    }
    offset += headerLength + payloadLength;
  }
  return buffer.slice(offset);
}

function openTestWebSocket(urlText) {
  return new Promise((resolve, reject) => {
    const url = new URL(urlText);
    const socket = net.createConnection({
      host: url.hostname,
      port: Number(url.port || 80),
    });
    const ws = createEventTargetSocket(socket);
    const key = crypto.randomBytes(16).toString('base64');
    let handshakeComplete = false;
    let buffer = Buffer.alloc(0);

    socket.on('connect', () => {
      socket.write([
        `GET ${url.pathname} HTTP/1.1`,
        `Host: ${url.host}`,
        'Upgrade: websocket',
        'Connection: Upgrade',
        `Sec-WebSocket-Key: ${key}`,
        'Sec-WebSocket-Version: 13',
        '',
        '',
      ].join('\r\n'));
    });

    socket.on('data', (chunk) => {
      buffer = Buffer.concat([buffer, chunk]);
      if (!handshakeComplete) {
        const boundary = buffer.indexOf('\r\n\r\n');
        if (boundary === -1) {
          return;
        }
        const header = buffer.slice(0, boundary).toString('utf8');
        if (!header.startsWith('HTTP/1.1 101')) {
          reject(new Error(`websocket upgrade failed: ${header}`));
          socket.destroy();
          return;
        }
        handshakeComplete = true;
        buffer = buffer.slice(boundary + 4);
        ws.emit('open', {});
        resolve(ws);
      }

      if (handshakeComplete && buffer.length > 0) {
        buffer = decodeFrames(buffer, ws.emit);
      }
    });

    socket.on('error', (error) => {
      ws.emit('error', { error });
      if (!handshakeComplete) {
        reject(error);
      }
    });
    socket.on('close', () => ws.emit('close', {}));
    socket.on('end', () => ws.emit('close', {}));
  });
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

function waitForWebSocketPayload(socket, predicate) {
  return new Promise((resolve, reject) => {
    const onMessage = (event) => {
      const payload = JSON.parse(event.data);
      if (!predicate(payload)) {
        return;
      }
      cleanup();
      resolve(payload);
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

function parseAsmTestsFromMakefile() {
  const makefilePath = path.join(repoRoot, 'myCPU', 'Makefile');
  const makefile = fs.readFileSync(makefilePath, 'utf8');
  const match = makefile.match(/^ASM_TESTS\s*=\s*(.+)$/m);
  assert.ok(match, 'ASM_TESTS should exist in myCPU/Makefile');
  return match[1].trim().split(/\s+/).filter(Boolean);
}

test('built-in asm manifest stays in sync with myCPU Makefile ASM_TESTS', () => {
  const expectedAsmTests = parseAsmTestsFromMakefile();
  const actualAsmTests = listTests(repoRoot)
    .filter((item) => item.kind === 'asm')
    .map((item) => item.name);
  assert.deepEqual(actualAsmTests, expectedAsmTests);
});

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
    assert.ok(body.tests.some((item) => item.name === 'guest_interactive_os_demo'));
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

test('session endpoints propagate CLI {type:error} responses instead of returning fake success', async () => {
  const server = await startServer({
    port: 0,
    createSession: async () => ({
      async load() {
        return { ok: true };
      },
      async snapshot() {
        return { type: 'error', message: 'snapshot failed' };
      },
      async stepCycle() {
        return { type: 'error', message: 'step-cycle failed' };
      },
      async stepCommit() {
        return { type: 'error', message: 'step-commit failed' };
      },
      async reset() {
        return { type: 'error', message: 'reset failed' };
      },
      async uartInput() {
        return { type: 'error', message: 'uart-input failed' };
      },
      async uartOutput() {
        return { type: 'error', message: 'uart-output failed' };
      },
      async close() {},
    }),
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 500);
    assert.equal(loadResponse.body.error, 'snapshot failed');

    const endpoints = [
      ['/api/session/snapshot', {}, 'snapshot failed'],
      ['/api/session/step-cycle', {}, 'step-cycle failed'],
      ['/api/session/step-commit', {}, 'step-commit failed'],
      ['/api/session/reset', {}, 'reset failed'],
      ['/api/session/terminal-input', { text: 'x' }, 'uart-input failed'],
      ['/api/session/terminal-output', { offset: 0 }, 'uart-output failed'],
    ];
    for (const [pathname, payload, expected] of endpoints) {
      const response = await postJson(server.baseUrl, pathname, payload);
      assert.equal(response.status, 500);
      assert.equal(response.body.error, expected);
    }
  } finally {
    await server.close();
  }
});

test('POST /api/session/load boots guest_interactive_os_demo to monitor prompt', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(response.status, 200);
    assert.equal(response.body.snapshot.summary.cycle, 42);
    assert.match(response.body.terminal.text, /interactive monitor/);
    assert.match(response.body.terminal.text, /monitor> /);
  } finally {
    await server.close();
  }
});

test('POST /api/session/terminal-input advances guest_interactive_os_demo until input is echoed', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'abc',
    });
    assert.equal(response.status, 200);
    assert.equal(response.body.text, 'abc');
    assert.equal(response.body.nextOffset, loadResponse.body.terminal.nextOffset + 3);
  } finally {
    await server.close();
  }
});

test('POST /api/session/terminal-input waits for interactive_os help output to settle', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'help\r',
    });
    assert.equal(response.status, 200);
    assert.match(response.body.text, /help echo time uptime halt disk regs peek pagewalk pte/);
    assert.match(response.body.text, /monitor> $/);
  } finally {
    await server.close();
  }
});

test('POST /api/session/terminal-input does not mutate a replacement session after concurrent load', async () => {
  const sessions = [];
  let releaseFirstCommit;
  const firstCommitGate = new Promise((resolve) => {
    releaseFirstCommit = resolve;
  });
  const server = await startServer({
    port: 0,
    createSession: async () => {
      const session = createFakeSession(`session-${sessions.length + 1}`);
      if (sessions.length === 0) {
        const baseStepCommit = session.stepCommit.bind(session);
        let firstCommitBlocked = false;
        session.stepCommit = async () => {
          if (!firstCommitBlocked) {
            firstCommitBlocked = true;
            await firstCommitGate;
          }
          return baseStepCommit();
        };
      }
      sessions.push(session);
      return session;
    },
  });

  try {
    const initialLoad = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(initialLoad.status, 200);

    const terminalInputPromise = postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'a',
    });
    await wait(10);

    const replacementLoadPromise = postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    await wait(10);
    releaseFirstCommit();

    const [terminalInputResponse, replacementLoad] = await Promise.all([
      terminalInputPromise,
      replacementLoadPromise,
    ]);
    assert.equal(terminalInputResponse.status, 409);
    assert.equal(replacementLoad.status, 200);
    assert.equal(sessions.length, 2);
    assert.equal(sessions[1].stepCommitCount, 0);

    const snapshotResponse = await postJson(server.baseUrl, '/api/session/snapshot', {});
    assert.equal(snapshotResponse.status, 200);
    assert.equal(snapshotResponse.body.snapshot.summary.cycle, 0);
    assert.equal(snapshotResponse.body.snapshot.events[0].detail, 'session-2:load');
  } finally {
    await server.close();
  }
});

test('POST /api/session/terminal-input keeps backspace erase output in the same response', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'abc\b',
    });
    assert.equal(response.status, 200);
    assert.equal(response.body.text, 'abc\b \b');
  } finally {
    await server.close();
  }
});

test('POST /api/session/terminal-input coalesces websocket updates while waiting for output to settle', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });
  const socket = await openTestWebSocket(`${server.baseUrl.replace('http', 'ws')}/ws`);
  const messages = [];
  const onMessage = (event) => {
    messages.push(JSON.parse(event.data));
  };
  socket.addEventListener('message', onMessage);

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    await wait(10);
    messages.length = 0;

    const response = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'help\r',
    });
    assert.equal(response.status, 200);
    await wait(20);

    const snapshotMessages = messages.filter((payload) => payload.type === 'snapshot');
    const terminalMessages = messages.filter((payload) => payload.type === 'terminal');

    assert.ok(snapshotMessages.length <= 1, `expected at most one snapshot update, got ${snapshotMessages.length}`);
    assert.ok(terminalMessages.length <= 1, `expected at most one terminal update, got ${terminalMessages.length}`);
    assert.match(terminalMessages.at(-1)?.text ?? '', /help echo time uptime halt/);
  } finally {
    socket.removeEventListener('message', onMessage);
    socket.close();
    await server.close();
  }
});

test('POST /api/session/terminal-input stops early when backspace has no effect', async () => {
  let currentSession = null;
  const server = await startServer({
    port: 0,
    createSession: async () => {
      currentSession = createFakeSession();
      return currentSession;
    },
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: '\b',
    });
    assert.equal(response.status, 200);
    assert.equal(response.body.text, '');
    assert.ok(currentSession.stepCommitCount < 128);
  } finally {
    await server.close();
  }
});

test('POST /api/session/terminal-input returns immediately for ignored characters', async () => {
  let currentSession = null;
  const server = await startServer({
    port: 0,
    createSession: async () => {
      currentSession = createFakeSession();
      currentSession.uartInput = async () => ({ ok: true });
      return currentSession;
    },
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: '你',
    });
    assert.equal(response.status, 200);
    assert.equal(response.body.text, '');
    assert.equal(currentSession.stepCommitCount, 0);
  } finally {
    await server.close();
  }
});

test('load and step-cycle preserve rich snapshots across HTTP and WebSocket', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });
  const socket = await openTestWebSocket(`${server.baseUrl.replace('http', 'ws')}/ws`);

  try {
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

test('POST /api/session/reset broadcasts a terminal reset and restarts output tracking for browser-style cadence', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });
  const socket = await openTestWebSocket(`${server.baseUrl.replace('http', 'ws')}/ws`);

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const initialOutput = await postJson(server.baseUrl, '/api/session/terminal-output', { offset: 0 });
    assert.equal(initialOutput.status, 200);

    const inputResponse = await postJson(server.baseUrl, '/api/session/terminal-input', { text: 'help\r' });
    assert.equal(inputResponse.status, 200);
    assert.equal(inputResponse.body.text, 'help\r');
    assert.ok(inputResponse.body.nextOffset > initialOutput.body.nextOffset);

    const resetTerminalMessage = waitForWebSocketPayload(
      socket,
      (payload) => payload.type === 'terminal' && payload.reset === true,
    );
    const resetResponse = await postJson(server.baseUrl, '/api/session/reset', {});
    assert.equal(resetResponse.status, 200);
    assert.equal(resetResponse.body.terminal.reset, true);
    assert.equal(resetResponse.body.terminal.text, '');
    assert.equal(resetResponse.body.terminal.nextOffset, 0);
    assert.deepEqual(await resetTerminalMessage, {
      type: 'terminal',
      text: '',
      nextOffset: 0,
      reset: true,
    });

    const resetOutput = await postJson(server.baseUrl, '/api/session/terminal-output', { offset: 0 });
    assert.equal(resetOutput.status, 200);
    assert.equal(resetOutput.body.text, '');
    assert.equal(resetOutput.body.nextOffset, 0);

    const staleOffsetOutput = await postJson(server.baseUrl, '/api/session/terminal-output', {
      offset: inputResponse.body.nextOffset,
    });
    assert.equal(staleOffsetOutput.status, 200);
    assert.equal(staleOffsetOutput.body.text, '');
    assert.equal(staleOffsetOutput.body.nextOffset, 0);
  } finally {
    socket.close();
    await server.close();
  }
});

test('terminal output API returns UART deltas and resets across session reloads', async () => {
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

    const initialOutput = await postJson(server.baseUrl, '/api/session/terminal-output', { offset: 0 });
    assert.equal(initialOutput.status, 200);
    assert.equal(initialOutput.body.text, 'boot:session-1\r\n> ');
    assert.equal(initialOutput.body.nextOffset, 'boot:session-1\r\n> '.length);

    const emptyDelta = await postJson(server.baseUrl, '/api/session/terminal-output', {
      offset: initialOutput.body.nextOffset,
    });
    assert.equal(emptyDelta.status, 200);
    assert.equal(emptyDelta.body.text, '');
    assert.equal(emptyDelta.body.nextOffset, initialOutput.body.nextOffset);

    const reloadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_supervisor_demo',
      backend: 'pipeline',
    });
    assert.equal(reloadResponse.status, 200);

    const resetOutput = await postJson(server.baseUrl, '/api/session/terminal-output', { offset: 0 });
    assert.equal(resetOutput.status, 200);
    assert.equal(resetOutput.body.text, 'boot:session-2\r\n> ');
  } finally {
    await server.close();
  }
});

test('terminal input API forwards text and broadcasts terminal deltas over websocket', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });
  const socket = await openTestWebSocket(`${server.baseUrl.replace('http', 'ws')}/ws`);

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const initialOutput = await postJson(server.baseUrl, '/api/session/terminal-output', { offset: 0 });
    assert.equal(initialOutput.status, 200);

    const terminalMessage = waitForWebSocketPayload(
      socket,
      (payload) => payload.type === 'terminal' && payload.text === 'help\r',
    );
    const inputResponse = await postJson(server.baseUrl, '/api/session/terminal-input', { text: 'help\r' });
    assert.equal(inputResponse.status, 200);
    assert.equal(inputResponse.body.text, 'help\r');
    assert.equal(inputResponse.body.nextOffset, initialOutput.body.nextOffset + 'help\r'.length);
    assert.deepEqual(await terminalMessage, {
      type: 'terminal',
      text: 'help\r',
      nextOffset: initialOutput.body.nextOffset + 'help\r'.length,
      reset: false,
    });
  } finally {
    socket.close();
    await server.close();
  }
});
