import test from 'node:test';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
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
  let currentPrompt = '> ';
  const interactiveDripInterval = 24;
  return {
    async load(entry) {
      currentTest = entry?.name ?? 'hello';
      cycle = 0;
      pendingTerminalOutput = '';
      interactiveLine = '';
      interactiveOutputCooldown = 0;
      currentPrompt = currentTest === 'linux_proto_console' ? 'mycpu-linux# ' : '> ';
      terminal =
        currentTest === 'guest_interactive_os_demo'
          ? ''
          : `boot:${sessionLabel}\r\n${currentPrompt}`;
      return { ok: true };
    },
    async loadPayload() {
      return { ok: true };
    },
    async setGpr() {
      return { ok: true };
    },
    async snapshot() {
      return makeSnapshot(cycle, sessionLabel);
    },
    async runUntilUartContains(text) {
      if (currentTest === 'guest_interactive_os_demo' && text === 'monitor> ') {
        cycle = 42;
        terminal = 'KMV\r\ninteractive monitor\r\nmonitor> ';
      } else if (currentTest === 'linux_proto_console' && text === 'mycpu-linux# ') {
        cycle = 44;
        terminal = `boot:${sessionLabel}\r\nready\r\nmycpu-linux# `;
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

function withEnv(updates, callback) {
  const previous = new Map();
  for (const key of Object.keys(updates)) {
    previous.set(key, process.env[key]);
    if (updates[key] == null) {
      delete process.env[key];
    } else {
      process.env[key] = updates[key];
    }
  }

  try {
    return callback();
  } finally {
    for (const [key, value] of previous) {
      if (value == null) {
        delete process.env[key];
      } else {
        process.env[key] = value;
      }
    }
  }
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
    const aiAccelDemo = body.tests.find((item) => item.name === 'guest_ai_accel_demo');
    const vectorDemo = body.tests.find((item) => item.name === 'guest_vector_demo');
    const vectorCnnDemo = body.tests.find((item) => item.name === 'guest_vector_cnn_demo');
    assert.ok(aiAccelDemo);
    assert.ok(vectorDemo);
    assert.ok(vectorCnnDemo);
    assert.equal(aiAccelDemo.badge, 'AI Accelerator');
    assert.equal(aiAccelDemo.workload.expectedMarker, 'KMVAI');
    assert.deepEqual(aiAccelDemo.workload.ops, ['graph package', 'MMIO doorbell', 'DMA load/store', 'timed-simple profile']);
    assert.equal(vectorDemo.title, 'V-lite Operator Demo');
    assert.equal(vectorDemo.workload.expectedMarker, 'V2OK');
    assert.equal(vectorCnnDemo.badge, 'Vector + NN');
    assert.deepEqual(vectorCnnDemo.workload.cnn.relu, [7, 0, 7]);
    assert.ok(
      !body.tests.some((item) => item.name === 'linux_proto_console'),
      'Linux console should stay gated until a runtime Image is configured',
    );
    assert.deepEqual(body.diagnostics.linuxConsole, {
      status: 'missing-env',
      ready: false,
      envVar: 'MYCPU_LINUX_PROTO_CONSOLE_IMAGE',
      message: 'Set MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image before starting the frontend server.',
    });
  } finally {
    await server.close();
  }
});

test('GET /api/ai/tiny-model/templates returns the server-side whitelist', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/api/ai/tiny-model/templates`);
    const body = await response.json();
    assert.equal(response.status, 200);
    assert.ok(Array.isArray(body.templates));
    const tinyModel = body.templates.find((item) => item.id === 'dynamic_tiny_model');
    const dynamicGemm = body.templates.find((item) => item.id === 'dynamic_gemm');
    const dynamicCnn = body.templates.find((item) => item.id === 'dynamic_cnn');
    const tinyAttention = body.templates.find((item) => item.id === 'tiny_attention_static');
    assert.ok(tinyModel);
    assert.ok(dynamicGemm);
    assert.ok(dynamicCnn);
    assert.ok(tinyAttention);
    assert.equal(tinyModel.title, 'Parameterized Tiny Model');
    assert.equal(tinyModel.shapeMode, 'dynamic_bounded');
    assert.equal(tinyModel.parameters.batch.label, 'Batch');
    assert.equal(tinyModel.parameters.inputPreset.label, 'Input preset');
    assert.equal(tinyModel.parameters.inputPreset.choiceLabels.negative_clamp, 'ReLU clamp path');
    assert.deepEqual(tinyModel.parameters.batch.choices, [1, 2]);
    assert.deepEqual(tinyModel.parameters.inputPreset.choices, ['balanced', 'negative_clamp']);
    assert.equal(tinyModel.boundary.allowsCustomGraph, false);
    assert.match(tinyModel.demo.expectedMarker, /output matches expected fp32 values/i);
    assert.ok(Array.isArray(tinyModel.demo.proves));
    assert.ok(tinyModel.demo.proves.some((item) => /server regenerates/i.test(item)));
    assert.ok(Array.isArray(tinyModel.demo.boundaries));
    assert.ok(tinyModel.demo.boundaries.some((item) => /No custom graph upload/i.test(item)));
    assert.equal(dynamicGemm.shapeMode, 'dynamic_bounded');
    assert.equal(dynamicGemm.parameters.runtimeShape.label, 'Runtime shape');
    assert.equal(dynamicGemm.parameters.runtimeShape.choiceLabels.single_row_identity_head, '1x8 -> 1x4 single-row slice');
    assert.deepEqual(dynamicGemm.parameters.runtimeShape.choices, ['two_rows_identity_tail', 'single_row_identity_head']);
    assert.match(dynamicGemm.demo.expectedMarker, /single_row_identity_head returns 1, 2, 3, 8/i);
    assert.equal(dynamicCnn.shapeMode, 'dynamic_bounded');
    assert.equal(dynamicCnn.parameters.runtimeShape.choiceLabels.compact_2x2, '3x3 -> 2x2 compact path');
    assert.deepEqual(dynamicCnn.parameters.runtimeShape.choices, ['compact_2x2', 'full_3x3']);
    assert.match(dynamicCnn.demo.expectedMarker, /compact_2x2 returns 15, 31/i);
    assert.ok(dynamicCnn.demo.proves.some((item) => /conv2d -> relu -> transpose -> reduce/i.test(item)));
    assert.equal(tinyAttention.shapeMode, 'static');
    assert.equal(tinyAttention.parameters.inputPreset.choiceLabels.biased_query, 'Higher value mix');
    assert.deepEqual(tinyAttention.parameters.inputPreset.choices, ['uniform_query', 'biased_query']);
    assert.match(tinyAttention.demo.expectedMarker, /uniform_query returns 2/i);
  } finally {
    await server.close();
  }
});

test('POST /api/ai/tiny-model/run returns a bounded profile result from the AI tiny model service', async () => {
  const calls = [];
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
    aiTinyModelService: {
      templates() {
        return {
          templates: [
            {
              id: 'dynamic_tiny_model',
              title: 'Parameterized Tiny Model',
              shapeMode: 'dynamic_bounded',
            },
          ],
        };
      },
      async run(payload) {
        calls.push(payload);
        return {
          ok: true,
          template: 'dynamic_tiny_model',
          parameters: {
            batch: 2,
            inputPreset: 'balanced',
          },
          output: {
            dtype: 'fp32',
            shape: [2, 1],
            values: [2.5, 5.5],
            expected: [2.5, 5.5],
          },
          profile: {
            progress: 'completed',
            shapeMode: 'dynamic_bounded',
            runtimeShapes: 't0:2x3,t2:2x2,t3:2x2,t4:2x1',
            bytesMoved: 72,
            retiredOps: 12,
            deviceCycles: 33,
            dmaCycles: 12,
            computeCycles: 9,
            stallCycles: 6,
            utilization: 27,
          },
          aggregate: {
            tileCount: 3,
            scratchpadPeakBytes: 64,
            opCount: 3,
          },
          ops: [
            { opIndex: 0, opcode: 'gemm', retiredOps: 8, computeCycles: 4, stallCycles: 2, tileCount: 1 },
            { opIndex: 1, opcode: 'eltwise_relu', retiredOps: 2, computeCycles: 2, stallCycles: 2, tileCount: 1 },
            { opIndex: 2, opcode: 'pool_max', retiredOps: 2, computeCycles: 3, stallCycles: 2, tileCount: 1 },
          ],
        };
      },
    },
  });

  try {
    const response = await postJson(server.baseUrl, '/api/ai/tiny-model/run', {
      template: 'dynamic_tiny_model',
      batch: 2,
      inputPreset: 'balanced',
    });
    assert.equal(response.status, 200);
    assert.deepEqual(calls, [
      {
        template: 'dynamic_tiny_model',
        batch: 2,
        inputPreset: 'balanced',
      },
    ]);
    assert.equal(response.body.template, 'dynamic_tiny_model');
    assert.deepEqual(response.body.output.values, [2.5, 5.5]);
    assert.equal(response.body.profile.shapeMode, 'dynamic_bounded');
    assert.equal(response.body.profile.runtimeShapes, 't0:2x3,t2:2x2,t3:2x2,t4:2x1');
    assert.equal(response.body.aggregate.opCount, 3);
    assert.deepEqual(response.body.ops.map((item) => item.opcode), ['gemm', 'eltwise_relu', 'pool_max']);
  } finally {
    await server.close();
  }
});

test('POST /api/ai/tiny-model/run forwards template-specific runtime-shape parameters for dynamic GEMM', async () => {
  const calls = [];
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
    aiTinyModelService: {
      templates() {
        return {
          templates: [
            {
              id: 'dynamic_gemm',
              title: 'Dynamic GEMM Profile',
              shapeMode: 'dynamic_bounded',
            },
          ],
        };
      },
      async run(payload) {
        calls.push(payload);
        return {
          ok: true,
          template: 'dynamic_gemm',
          parameters: {
            runtimeShape: 'single_row_identity_head',
          },
          output: {
            dtype: 'int32',
            shape: [1, 4],
            values: [1, 2, 3, 0],
            expected: [1, 2, 3, 0],
          },
          profile: {
            progress: 'completed',
            shapeMode: 'dynamic_bounded',
            runtimeShapes: 't0:1x8,t2:1x4',
            bytesMoved: 52,
            retiredOps: 32,
            deviceCycles: 11,
            dmaCycles: 7,
            computeCycles: 2,
            stallCycles: 2,
            utilization: 14,
          },
          aggregate: {
            tileCount: 1,
            scratchpadPeakBytes: 64,
            opCount: 1,
          },
          ops: [
            { opIndex: 0, opcode: 'gemm', retiredOps: 32, computeCycles: 2, stallCycles: 2, tileCount: 1 },
          ],
        };
      },
    },
  });

  try {
    const response = await postJson(server.baseUrl, '/api/ai/tiny-model/run', {
      template: 'dynamic_gemm',
      runtimeShape: 'single_row_identity_head',
    });
    assert.equal(response.status, 200);
    assert.deepEqual(calls, [
      {
        template: 'dynamic_gemm',
        runtimeShape: 'single_row_identity_head',
      },
    ]);
    assert.equal(response.body.template, 'dynamic_gemm');
    assert.equal(response.body.profile.runtimeShapes, 't0:1x8,t2:1x4');
    assert.deepEqual(response.body.output.values, [1, 2, 3, 0]);
  } finally {
    await server.close();
  }
});

test('POST /api/ai/tiny-model/run forwards template-specific runtime-shape parameters for dynamic CNN', async () => {
  const calls = [];
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
    aiTinyModelService: {
      templates() {
        return {
          templates: [
            {
              id: 'dynamic_cnn',
              title: 'Dynamic CNN Profile',
              shapeMode: 'dynamic_bounded',
            },
          ],
        };
      },
      async run(payload) {
        calls.push(payload);
        return {
          ok: true,
          template: 'dynamic_cnn',
          parameters: {
            runtimeShape: 'compact_2x2',
          },
          output: {
            dtype: 'int32',
            shape: [2],
            values: [15, 31],
            expected: [15, 31],
          },
          profile: {
            progress: 'completed',
            shapeMode: 'dynamic_bounded',
            runtimeShapes: 't0:3x3,t2:2x2,t3:2x2,t4:2x2,t5:2',
            bytesMoved: 21,
            retiredOps: 28,
            deviceCycles: 17,
            dmaCycles: 9,
            computeCycles: 4,
            stallCycles: 4,
            utilization: 21,
          },
          aggregate: {
            tileCount: 4,
            scratchpadPeakBytes: 184,
            opCount: 4,
          },
          ops: [
            { opIndex: 0, opcode: 'conv2d', retiredOps: 16, computeCycles: 1, stallCycles: 1, tileCount: 1 },
            { opIndex: 1, opcode: 'eltwise_relu', retiredOps: 4, computeCycles: 1, stallCycles: 1, tileCount: 1 },
            { opIndex: 2, opcode: 'layout_transpose', retiredOps: 4, computeCycles: 1, stallCycles: 1, tileCount: 1 },
            { opIndex: 3, opcode: 'reduce_sum', retiredOps: 4, computeCycles: 1, stallCycles: 1, tileCount: 1 },
          ],
        };
      },
    },
  });

  try {
    const response = await postJson(server.baseUrl, '/api/ai/tiny-model/run', {
      template: 'dynamic_cnn',
      runtimeShape: 'compact_2x2',
    });
    assert.equal(response.status, 200);
    assert.deepEqual(calls, [
      {
        template: 'dynamic_cnn',
        runtimeShape: 'compact_2x2',
      },
    ]);
    assert.equal(response.body.template, 'dynamic_cnn');
    assert.equal(response.body.profile.runtimeShapes, 't0:3x3,t2:2x2,t3:2x2,t4:2x2,t5:2');
    assert.deepEqual(response.body.output.values, [15, 31]);
  } finally {
    await server.close();
  }
});

test('POST /api/ai/tiny-model/run maps service validation failures to 4xx JSON', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
    aiTinyModelService: {
      templates() {
        return { templates: [] };
      },
      async run() {
        const error = new Error('batch must be one of: 1, 2');
        error.statusCode = 400;
        throw error;
      },
    },
  });

  try {
    const response = await postJson(server.baseUrl, '/api/ai/tiny-model/run', {
      template: 'dynamic_tiny_model',
      batch: 8,
      inputPreset: 'balanced',
    });
    assert.equal(response.status, 400);
    assert.equal(response.body.error, 'batch must be one of: 1, 2');
  } finally {
    await server.close();
  }
});

test('default AI tiny model service rejects custom graph payloads before running a profile', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await postJson(server.baseUrl, '/api/ai/tiny-model/run', {
      template: 'dynamic_tiny_model',
      batch: 1,
      inputPreset: 'balanced',
      graphPackage: 'browser-supplied.bin',
    });
    assert.equal(response.status, 400);
    assert.equal(response.body.error, 'unsupported AI tiny model parameter: graphPackage');
  } finally {
    await server.close();
  }
});

test('default AI tiny model service rejects parameters that do not belong to the selected template', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await postJson(server.baseUrl, '/api/ai/tiny-model/run', {
      template: 'tiny_attention_static',
      batch: 2,
      inputPreset: 'uniform_query',
    });
    assert.equal(response.status, 400);
    assert.equal(response.body.error, 'unsupported AI tiny model parameter for tiny_attention_static: batch');
  } finally {
    await server.close();
  }
});

test('GET /api/tests reports a Linux console diagnostic when the configured Image is missing', async () => {
  const missingImage = path.join(os.tmpdir(), `mycpu-missing-linux-image-${Date.now()}`, 'Image');
  const server = await withEnv({
    MYCPU_LINUX_PROTO_CONSOLE_IMAGE: missingImage,
  }, () => startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  }));

  try {
    const response = await fetch(`${server.baseUrl}/api/tests`);
    const body = await response.json();
    assert.equal(response.status, 200);
    assert.ok(!body.tests.some((item) => item.name === 'linux_proto_console'));
    assert.equal(body.diagnostics.linuxConsole.status, 'not-found');
    assert.equal(body.diagnostics.linuxConsole.ready, false);
    assert.equal(body.diagnostics.linuxConsole.path, missingImage);
    assert.match(body.diagnostics.linuxConsole.message, /Image path does not exist/);
  } finally {
    await server.close();
  }
});

test('GET /api/tests reports a Linux console diagnostic when the configured Image path is not a file', async () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-linux-console-dir-'));
  try {
    const server = await withEnv({
      MYCPU_LINUX_PROTO_CONSOLE_IMAGE: tempDir,
    }, () => startServer({
      port: 0,
      createSession: createFakeSessionFactory(),
    }));

    try {
      const response = await fetch(`${server.baseUrl}/api/tests`);
      const body = await response.json();
      assert.equal(response.status, 200);
      assert.ok(!body.tests.some((item) => item.name === 'linux_proto_console'));
      assert.equal(body.diagnostics.linuxConsole.status, 'not-file');
      assert.equal(body.diagnostics.linuxConsole.ready, false);
      assert.equal(body.diagnostics.linuxConsole.path, tempDir);
      assert.match(body.diagnostics.linuxConsole.message, /Image path is not a file/);
    } finally {
      await server.close();
    }
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
});

test('GET /api/tests reports a Linux console diagnostic when the configured Image path is not readable', async () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-linux-console-unreadable-'));
  const imagePath = path.join(tempDir, 'Image');
  try {
    fs.writeFileSync(imagePath, 'fake linux image');
    fs.chmodSync(imagePath, 0o000);

    const server = await withEnv({
      MYCPU_LINUX_PROTO_CONSOLE_IMAGE: imagePath,
    }, () => startServer({
      port: 0,
      createSession: createFakeSessionFactory(),
    }));

    try {
      const response = await fetch(`${server.baseUrl}/api/tests`);
      const body = await response.json();
      assert.equal(response.status, 200);
      assert.ok(!body.tests.some((item) => item.name === 'linux_proto_console'));
      assert.equal(body.diagnostics.linuxConsole.status, 'not-readable');
      assert.equal(body.diagnostics.linuxConsole.ready, false);
      assert.equal(body.diagnostics.linuxConsole.path, imagePath);
      assert.match(body.diagnostics.linuxConsole.message, /Image path is not readable/);
    } finally {
      await server.close();
    }
  } finally {
    fs.chmodSync(imagePath, 0o600);
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
});

test('listTests adds linux_proto_console when a local Linux runtime Image is configured', () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-linux-console-'));
  try {
    const imagePath = path.join(tempDir, 'Image');
    fs.writeFileSync(imagePath, 'fake linux image');

    const tests = withEnv({
      MYCPU_LINUX_PROTO_CONSOLE_IMAGE: imagePath,
    }, () => listTests(repoRoot));

    const linuxConsole = tests.find((item) => item.name === 'linux_proto_console');
    assert.ok(linuxConsole);
    assert.equal(linuxConsole.menuLabel, 'linux_proto_console · Linux serial');
    assert.equal(linuxConsole.kind, 'linux');
    assert.equal(linuxConsole.backend, 'functional');
    assert.equal(linuxConsole.image, path.join(repoRoot, 'myCPU', 'workloads', 'linux_proto', 'linux_sbi_shim.bin'));
    assert.equal(linuxConsole.imageFormat, 'flat');
    assert.equal(linuxConsole.loadAddr, '0x80000000');
    assert.equal(linuxConsole.blockTransport, 'virtio-blk');
    assert.equal(linuxConsole.bootUntilUartText, 'mycpu-linux# ');
    assert.equal(linuxConsole.terminalPrompt, 'mycpu-linux# ');
    assert.equal(linuxConsole.bootRequestTimeoutMs, 120000);
    assert.equal(linuxConsole.commandUntilUartText, 'mycpu-linux# ');
    assert.equal(linuxConsole.commandMaxSteps, 50000000);
    assert.equal(linuxConsole.commandRequestTimeoutMs, 30000);
    assert.deepEqual(linuxConsole.payloads, [
      { image: imagePath, addr: '0x80200000' },
      { image: path.join(repoRoot, 'myCPU', 'workloads', 'linux_proto', 'mycpu_virt.dtb'), addr: '0x87f00000' },
    ]);
    assert.deepEqual(linuxConsole.gprSeeds, [
      { reg: 'a0', value: '0x0' },
      { reg: 'a1', value: '0x87f00000' },
      { reg: 'a2', value: '0x80200000' },
    ]);
    assert.equal(linuxConsole.disk, path.join(repoRoot, 'myCPU', 'workloads', 'linux_proto', 'rootfs.ext4'));
    assert.equal(linuxConsole.workload.category, 'linux-serial-console');
    assert.equal(linuxConsole.workload.expectedMarker, 'mycpu-linux# ');
    assert.ok(linuxConsole.workload.assetNote.includes('MYCPU_LINUX_PROTO_CONSOLE_IMAGE'));
    assert.deepEqual(tests.diagnostics.linuxConsole, {
      status: 'ready',
      ready: true,
      envVar: 'MYCPU_LINUX_PROTO_CONSOLE_IMAGE',
      path: imagePath,
      message: 'Linux serial console Image is configured.',
    });
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
});

test('GET /api/tests exposes the Linux console workload only when configured', async () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-linux-console-'));
  try {
    const imagePath = path.join(tempDir, 'Image');
    fs.writeFileSync(imagePath, 'fake linux image');

    const server = await withEnv({
      MYCPU_LINUX_PROTO_CONSOLE_IMAGE: imagePath,
    }, () => startServer({
      port: 0,
      createSession: createFakeSessionFactory(),
    }));

    try {
      const response = await fetch(`${server.baseUrl}/api/tests`);
      const body = await response.json();
      assert.equal(response.status, 200);
      const linuxConsole = body.tests.find((item) => item.name === 'linux_proto_console');
      assert.ok(linuxConsole);
      assert.equal(linuxConsole.kind, 'linux');
      assert.equal(linuxConsole.backend, 'functional');
      assert.equal(linuxConsole.title, 'Linux Serial Console');
      assert.equal(linuxConsole.badge, 'Linux runtime');
      assert.equal(linuxConsole.workload.expectedMarker, 'mycpu-linux# ');
      assert.ok(linuxConsole.workload.assetNote.includes('MYCPU_LINUX_PROTO_CONSOLE_IMAGE'));
      assert.equal(body.diagnostics.linuxConsole.status, 'ready');
      assert.equal(body.diagnostics.linuxConsole.ready, true);
      assert.equal(body.diagnostics.linuxConsole.path, imagePath);
    } finally {
      await server.close();
    }
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
});

test('GET / returns the Wave 7 product homepage instead of the console app', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/`);
    const body = await response.text();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /text\/html/);
    assert.match(body, /myCPU/);
    assert.match(body, /在浏览器里启动一颗 CPU/);
    assert.match(body, /打开控制台/);
    assert.match(body, /阅读产品文档/);
    assert.doesNotMatch(body, /id="test-select"/);
  } finally {
    await server.close();
  }
});

test('GET /console keeps serving the existing browser console app', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/console`);
    const body = await response.text();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /text\/html/);
    assert.match(body, /交互式终端调试台/);
    assert.match(body, /id="demo-workspace-slot"/);
    assert.match(body, /Demo workspace/);
    assert.match(body, /Linux serial console/);
    assert.match(body, /MYCPU_LINUX_PROTO_CONSOLE_IMAGE/);
    assert.match(body, /id="test-select"/);
    assert.match(body, /id="terminate-button"/);
    assert.match(body, />Terminate</);
    assert.match(body, /src="\/app\.js"/);
  } finally {
    await server.close();
  }
});

test('GET /docs returns the readable Wave 7 product documentation v1 entry', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/docs`);
    const body = await response.text();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /text\/html/);
    for (const label of [
      'Overview',
      'Try the Console',
      'Demo Routes',
      'Architecture',
      'OS Bring-up',
      'AI Accelerator',
      'Runtime Labs',
      'Verification',
      'Roadmap',
      'Design References',
    ]) {
      assert.match(body, new RegExp(label.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
    }
    for (const label of [
      '/',
      '/console',
      '/docs',
      'OS Bring-up',
      'Machine Inspector',
      'AI Accelerator',
      'Runtime Labs',
      'Load',
      'Run',
      'Pause',
      'Reset',
      'Terminate',
      'Coming soon',
      'xv6 shell',
      'Linux Serial Console',
      'MYCPU_LINUX_PROTO_CONSOLE_IMAGE',
      'MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E',
      'mycpu-linux# ',
      'interactive_os',
      'guest_ai_accel_demo',
      'Vector CNN',
      'L1D / shadow cache',
      'JIT / DBT opt-in',
      'cd frontend && node --test',
      'git diff --check',
    ]) {
      assert.match(body, new RegExp(label.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
    }
    assert.doesNotMatch(body, /面向技术评审 \/ 招聘面试官/);
    assert.doesNotMatch(body, /<a href="#boundaries">Boundaries<\/a>/);
    assert.doesNotMatch(body, /<h2>Boundaries<\/h2>/);
    assert.match(body, /wave7_productization_and_showcase_design\.md/);
    assert.match(body, /debug_frontend_integration\.md/);
    assert.match(body, /future_expansion_roadmap_design\.md/);
    assert.match(body, /npu_tpu_accelerator_status\.md/);
    assert.match(body, /mainline_status\.md/);
  } finally {
    await server.close();
  }
});

test('GET / exposes scroll storytelling sections with progressive reveal hooks', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/`);
    const body = await response.text();
    assert.equal(response.status, 200);
    for (const label of [
      'Run real systems',
      'Inspect the machine',
      'Accelerate workloads',
      'Core architecture',
      'Memory & OS',
      'Differential verification',
      'Runtime labs',
      'Verification',
      '验证',
      'Start',
    ]) {
      assert.match(body, new RegExp(label.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
    }
    assert.match(body, /class="product-nav"/);
    assert.match(body, /data-reveal="rise"/);
    assert.match(body, /data-reveal="from-right"/);
    assert.match(body, /data-motion-track/);
    assert.match(body, /data-count="75"/);
    assert.match(body, /data-count="1"/);
  } finally {
    await server.close();
  }
});

test('GET / presents project-specific evidence instead of a generic landing page', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/`);
    const body = await response.text();
    assert.equal(response.status, 200);
    for (const label of [
      'functional / pipeline',
      'xv6 shell',
      'timerfd-one-shot-readback-ok',
      'AI accelerator',
      'Vector CNN',
      'L1D / shadow cache',
      'JIT / DBT opt-in',
      'Spike differential',
      'Linux serial console',
      '参数化小模型',
      'InstructionSemantics',
      'AddressSpace',
      'Sv39',
      'CLINT',
      'PLIC',
      'virtio-blk',
      'Spike oracle',
      'functional replay',
      'commit trace',
    ]) {
      assert.match(body, new RegExp(label.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));
    }
    assert.doesNotMatch(body, /<a href="#boundaries">Boundaries<\/a>/);
    assert.doesNotMatch(body, /Know the boundaries/);
    assert.doesNotMatch(body, /不开放任意 Linux 镜像上传/);
    assert.doesNotMatch(body, /不开放任意 AI 模型上传/);
    assert.doesNotMatch(body, /技术评审/);
    assert.doesNotMatch(body, /招聘面试官/);
    assert.match(body, /\/source\/showcase\/frontend_overview\.png/);
    assert.match(body, /\/source\/showcase\/pipeline_timeline\.png/);
    assert.match(body, /\/source\/showcase\/interactive_terminal\.png/);
    assert.match(body, /\/source\/showcase\/auto-code-image-905\.png/);
    assert.match(body, /\/source\/showcase\/vector_panel\.png/);
    assert.match(body, /src="\/home\.js"/);
  } finally {
    await server.close();
  }
});

test('GET /home.css provides product-page motion with a reduced-motion fallback', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/home.css`);
    const body = await response.text();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /text\/css/);
    assert.match(body, /@keyframes reveal-up/);
    assert.match(body, /\[data-reveal="from-right"\]/);
    assert.match(body, /position: sticky/);
    assert.match(body, /scroll-snap-type: x mandatory/);
    assert.match(body, /font-family: var\(--display\)/);
    assert.match(body, /prefers-reduced-motion: reduce/);
  } finally {
    await server.close();
  }
});

test('GET /home.js wires scroll reveal, parallax, and count-up interactions', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/home.js`);
    const body = await response.text();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /text\/javascript/);
    assert.match(body, /IntersectionObserver/);
    assert.match(body, /requestAnimationFrame/);
    assert.match(body, /data-motion-track/);
    assert.match(body, /prefers-reduced-motion/);
  } finally {
    await server.close();
  }
});

test('GET /source/docs serves curated evidence documents referenced by product docs', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/source/docs/design/wave7_productization_and_showcase_design.md`);
    const body = await response.text();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /text\/markdown|text\/plain/);
    assert.match(body, /Wave 7 产品化展示与在线控制台设计/);
  } finally {
    await server.close();
  }
});

test('GET /source/showcase serves homepage screenshot assets', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/source/showcase/frontend_overview.png`);
    const body = await response.arrayBuffer();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /image\/png/);
    assert.ok(body.byteLength > 1024);
  } finally {
    await server.close();
  }
});

test('GET /shared/terminal_projection.mjs serves the browser-shared terminal projection module', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await fetch(`${server.baseUrl}/shared/terminal_projection.mjs`);
    const body = await response.text();
    assert.equal(response.status, 200);
    assert.match(response.headers.get('content-type') ?? '', /text\/javascript/);
    assert.match(body, /projectTerminalText/);
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
  const loadFailureServer = await startServer({
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
    const loadResponse = await postJson(loadFailureServer.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 500);
    assert.equal(loadResponse.body.error, 'snapshot failed');
    const snapshotAfterFailedLoad = await postJson(loadFailureServer.baseUrl, '/api/session/snapshot', {});
    assert.equal(snapshotAfterFailedLoad.status, 400);
    assert.equal(snapshotAfterFailedLoad.body.error, 'session not loaded');
  } finally {
    await loadFailureServer.close();
  }

  const server = await startServer({
    port: 0,
    createSession: async () => {
      let snapshotCalls = 0;
      let uartOutputCalls = 0;

      return {
      async load() {
        return { ok: true };
      },
      async snapshot() {
        snapshotCalls += 1;
        if (snapshotCalls > 1) {
          return { type: 'error', message: 'snapshot failed' };
        }
        return {
          type: 'snapshot',
          snapshot: {
            summary: {
              cycle: 0,
            },
          },
        };
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
        uartOutputCalls += 1;
        if (uartOutputCalls > 1) {
          return { type: 'error', message: 'uart-output failed' };
        }
        return { type: 'terminal', text: '', nextOffset: 0 };
      },
      async close() {},
    };
    },
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

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

test('POST /api/session/reset re-arms linux_proto_console payloads and returns to the Linux prompt', async () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-linux-console-'));
  try {
    const imagePath = path.join(tempDir, 'Image');
    fs.writeFileSync(imagePath, 'fake linux image');

    const server = await withEnv({
      MYCPU_LINUX_PROTO_CONSOLE_IMAGE: imagePath,
    }, () => startServer({
      port: 0,
      createSession: createFakeSessionFactory(),
    }));

    try {
      const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
        test: 'linux_proto_console',
        backend: 'pipeline',
      });
      assert.equal(loadResponse.status, 200);
      assert.match(loadResponse.body.terminal.text, /mycpu-linux# /);

      const resetResponse = await postJson(server.baseUrl, '/api/session/reset', {});
      assert.equal(resetResponse.status, 200);
      assert.equal(resetResponse.body.terminal.reset, true);
      assert.match(resetResponse.body.terminal.text, /ready/);
      assert.match(resetResponse.body.terminal.text, /mycpu-linux# $/);
      assert.equal(resetResponse.body.snapshot.summary.cycle, 44);

      const staleOutput = await postJson(server.baseUrl, '/api/session/terminal-output', {
        offset: loadResponse.body.terminal.nextOffset,
      });
      assert.equal(staleOutput.status, 200);
      assert.equal(staleOutput.body.text, '');
      assert.equal(staleOutput.body.nextOffset, resetResponse.body.terminal.nextOffset);
    } finally {
      await server.close();
    }
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
});

test('POST /api/session/terminate closes the current session and broadcasts a terminal reset', async () => {
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

    const resetTerminalMessage = waitForWebSocketPayload(
      socket,
      (payload) => payload.type === 'terminal' && payload.reset === true && payload.nextOffset === 0,
    );
    const terminateResponse = await postJson(server.baseUrl, '/api/session/terminate', {});
    assert.equal(terminateResponse.status, 200);
    assert.equal(terminateResponse.body.ok, true);
    assert.equal(terminateResponse.body.snapshot, null);
    assert.deepEqual(terminateResponse.body.terminal, {
      type: 'terminal',
      text: '',
      nextOffset: 0,
      reset: true,
    });
    assert.deepEqual(await resetTerminalMessage, terminateResponse.body.terminal);

    const stepResponse = await postJson(server.baseUrl, '/api/session/step-cycle', {});
    assert.equal(stepResponse.status, 400);
    assert.equal(stepResponse.body.error, 'session not loaded');
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
