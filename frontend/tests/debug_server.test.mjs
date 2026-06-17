import test from 'node:test';
import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import fs from 'node:fs';
import os from 'node:os';
import net from 'node:net';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { startServer } from '../server/debug_server.mjs';
import { createAiTinyModelService } from '../server/ai_tiny_model_service.mjs';
import { buildPasswordHashForTests } from '../server/security.mjs';
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
      currentPrompt =
        currentTest === 'linux_proto_console'
          ? 'mycpu-linux# '
          : (currentTest === 'guest_course_os_shell_demo' ? 'course-os> ' : '> ');
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
      } else if (currentTest === 'guest_course_os_shell_demo' && text === 'course-os> ') {
        cycle = 45;
        terminal = 'course-os shell ready\r\ncourse-os> ';
      }
      return this.snapshot();
    },
    async runUntilNewUartContains(offset, text) {
      if (currentTest === 'guest_course_os_shell_demo' && text === 'course-os> ') {
        terminal += pendingTerminalOutput;
        pendingTerminalOutput = '';
      }
      return {
        type: 'uart_output',
        offset,
        next_offset: terminal.length,
        text: terminal.slice(offset),
      };
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
        if (currentTest === 'guest_course_os_shell_demo') {
          pendingTerminalOutput += text;
          if (text === 'help\r') {
            pendingTerminalOutput += 'help ls cat echo ps kill cd pwd exit exec sh meminfo schedstat fsstat syscalls cow crashlog cpuinfo uptime status fd maps\r\ncourse-os> ';
          } else if (text === 'cpuinfo\r') {
            pendingTerminalOutput += 'isa=rv64im\r\nbackend=myCPU\r\ncourse-os> ';
          } else if (text.includes('\r')) {
            pendingTerminalOutput += 'course-os> ';
          }
        } else {
          pendingTerminalOutput += text;
        }
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
    async jitDispatch() {
      return {
        type: 'jit_dispatch',
        ok: currentTest === 'guest_vector_cnn_demo',
        source: 'hot-path-profile',
        action: currentTest === 'guest_vector_cnn_demo' ? 'lowered-ready' : 'reference-fallback',
        start_pc: '0x80000000',
        end_pc: '0x80000020',
        cache_state: currentTest === 'guest_vector_cnn_demo' ? 'hit' : 'miss',
        planned: true,
        translated: true,
        lowered: currentTest === 'guest_vector_cnn_demo',
        fallback_to_reference: currentTest !== 'guest_vector_cnn_demo',
        lowered_instruction_count: currentTest === 'guest_vector_cnn_demo' ? 5 : 0,
        candidate_executions: currentTest === 'guest_vector_cnn_demo' ? 18 : 3,
        candidate_retired_instructions: currentTest === 'guest_vector_cnn_demo' ? 72 : 6,
        reject_kind: currentTest === 'guest_vector_cnn_demo' ? 'none' : 'control-flow',
        reject_reason: currentTest === 'guest_vector_cnn_demo' ? 'none' : 'fallback-required',
        helper_replay_kind: 'none',
        host_code: false,
        executable_memory: false,
        guest_execution: false,
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

function createCapturingSessionFactory(loads) {
  let sessionId = 0;
  return async () => {
    sessionId += 1;
    const session = createFakeSession(`session-${sessionId}`);
    const originalLoad = session.load.bind(session);
    session.load = async (entry, backend) => {
      loads.push({ entry, backend });
      return originalLoad(entry, backend);
    };
    return session;
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

async function postJsonWithCookie(baseUrl, pathname, payload, cookie = '') {
  const headers = { 'content-type': 'application/json' };
  if (cookie) {
    headers.cookie = cookie;
  }
  const response = await fetch(`${baseUrl}${pathname}`, {
    method: 'POST',
    headers,
    body: JSON.stringify(payload),
  });
  return {
    status: response.status,
    cookie: response.headers.get('set-cookie'),
    body: await response.json(),
  };
}

async function getJsonWithCookie(baseUrl, pathname, cookie = '') {
  const headers = cookie ? { cookie } : {};
  const response = await fetch(`${baseUrl}${pathname}`, { headers });
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

test('Course OS shell manifest carries wider debug CLI runtime budgets', () => {
  const courseOsShell = listTests(repoRoot)
    .find((item) => item.name === 'guest_course_os_shell_demo');
  assert.ok(courseOsShell);
  assert.equal(courseOsShell.bootUntilUartText, 'course-os> ');
  assert.equal(courseOsShell.bootRequestTimeoutMs, 30000);
  assert.equal(courseOsShell.commandUntilUartText, 'course-os> ');
  assert.equal(courseOsShell.commandRequestTimeoutMs, 30000);
});

test('Course OS shell manifest exposes the Stage 11 host-only workflow contract', () => {
  const courseOsShell = listTests(repoRoot)
    .find((item) => item.name === 'guest_course_os_shell_demo');
  const stage11Entries = listTests(repoRoot)
    .filter((item) => item.name.includes('stage11') || item.name.includes('external_workflow'));

  assert.ok(courseOsShell);
  assert.deepEqual(
    stage11Entries.map((item) => item.name),
    [],
    'Stage 11 external workflow should not become a browser-loadable manifest entry',
  );
  assert.equal(courseOsShell.workload.hostOnlyWorkflow.enabled, true);
  assert.equal(courseOsShell.workload.hostOnlyWorkflow.route, 'host-only');
  assert.equal(
    courseOsShell.workload.hostOnlyWorkflow.externalRootfsEnv,
    'MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS',
  );
  assert.equal(
    courseOsShell.workload.hostOnlyWorkflow.target,
    'test-host-course_os_linux_compat_external_workflow_smoke',
  );
  assert.match(
    courseOsShell.workload.hostOnlyWorkflow.boundary,
    /browser console does not run external rootfs workflows/,
  );
  assert.deepEqual(
    courseOsShell.workload.hostOnlyWorkflow.commands.map((item) => item.command),
    [
      'git init stage11repo',
      'vim stage11repo/hello.c',
      'git -c safe.directory=/stage11repo -C stage11repo add hello.c',
      'git -C stage11repo -c safe.directory=/stage11repo -c user.name=stage11 -c user.email=stage11@example.invalid commit -m init',
      'git -C stage11repo -c safe.directory=/stage11repo --no-pager log --oneline',
      'cd stage11repo && gcc hello.c && ./a.out',
    ],
  );
  assert.deepEqual(
    courseOsShell.workload.hostOnlyWorkflow.commands.at(-1).markers,
    ['linux-compat: path=/usr/bin/gcc', 'stage11 hello', 'exec=real', 'course-os> '],
  );
});

test('interactive terminal manifest entries carry unified presentation metadata', () => {
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-linux-console-'));
  try {
    const imagePath = path.join(tempDir, 'Image');
    fs.writeFileSync(imagePath, 'fake linux image');

    const tests = withEnv({
      MYCPU_LINUX_PROTO_CONSOLE_IMAGE: imagePath,
    }, () => listTests(repoRoot));
    const byName = new Map(tests.map((item) => [item.name, item]));

    assert.deepEqual(
      [
        'guest_interactive_os_demo',
        'guest_course_os_shell_demo',
        'linux_proto_console',
      ].map((name) => {
        const entry = byName.get(name);
        assert.ok(entry, `${name} should be present`);
        return {
          name,
          title: entry.title,
          badge: entry.badge,
          summary: entry.summary,
          prompt: entry.terminalPrompt,
          bootUntil: entry.bootUntilUartText,
          commandUntil: entry.commandUntilUartText,
          commandMaxSteps: Number.isInteger(entry.commandMaxSteps),
          commandWait: entry.workload?.terminal?.commandWait,
          category: entry.workload?.category,
          target: entry.workload?.terminal?.target,
          kind: entry.workload?.terminal?.kind,
        };
      }),
      [
        {
          name: 'guest_interactive_os_demo',
          title: 'Interactive OS Monitor',
          badge: 'Monitor',
          summary: '进入 interactive_os guest monitor，通过 UART 操作 help、echo、time、uptime、disk、regs、peek、pagewalk 和 pte 命令。',
          prompt: 'monitor> ',
          bootUntil: 'monitor> ',
          commandUntil: undefined,
          commandMaxSteps: true,
          commandWait: 'activity',
          category: 'interactive-monitor',
          target: 'guest monitor',
          kind: 'monitor',
        },
        {
          name: 'guest_course_os_shell_demo',
          title: 'Course OS Shell',
          badge: 'Course OS',
          summary: '打开课程 OS Stage 4 交互 shell，通过 UART terminal 操作 procfs、FD / FS、pipe、ELF / libc、COW 与 crash evidence。',
          prompt: 'course-os> ',
          bootUntil: 'course-os> ',
          commandUntil: 'course-os> ',
          commandMaxSteps: true,
          commandWait: 'prompt',
          category: 'course-os-shell',
          target: 'Course OS shell',
          kind: 'course-os',
        },
        {
          name: 'linux_proto_console',
          title: 'Linux Serial Console',
          badge: 'Linux runtime',
          summary: '启动受控 linux_proto runtime，进入 UART 串口 console，观察 Linux userland smoke marker。',
          prompt: 'mycpu-linux# ',
          bootUntil: 'mycpu-linux# ',
          commandUntil: 'mycpu-linux# ',
          commandMaxSteps: true,
          commandWait: 'prompt',
          category: 'linux-serial-console',
          target: 'Linux serial console',
          kind: 'linux-serial',
        },
      ],
    );
    assert.equal(
      byName.get('guest_course_os_shell_demo').workload.hostOnlyWorkflow.route,
      'host-only',
    );
    assert.equal(
      byName.get('linux_proto_console').workload.assetEnvVar,
      'MYCPU_LINUX_PROTO_CONSOLE_IMAGE',
    );
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
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
    const courseOsShell = body.tests.find((item) => item.name === 'guest_course_os_shell_demo');
    const aiAccelDemo = body.tests.find((item) => item.name === 'guest_ai_accel_demo');
    const vectorDemo = body.tests.find((item) => item.name === 'guest_vector_demo');
    const vectorCnnDemo = body.tests.find((item) => item.name === 'guest_vector_cnn_demo');
    assert.ok(courseOsShell);
    assert.ok(aiAccelDemo);
    assert.ok(vectorDemo);
    assert.ok(vectorCnnDemo);
    assert.equal(courseOsShell.menuLabel, 'guest_course_os_shell_demo · Course OS shell');
    assert.equal(courseOsShell.bootUntilUartText, 'course-os> ');
    assert.equal(courseOsShell.terminalPrompt, 'course-os> ');
    assert.equal(courseOsShell.commandUntilUartText, 'course-os> ');
    assert.equal(courseOsShell.title, 'Course OS Shell');
    assert.equal(courseOsShell.workload.expectedMarker, 'course-os> ');
    assert.deepEqual(courseOsShell.workload.terminal, {
      kind: 'course-os',
      target: 'Course OS shell',
      commandWait: 'prompt',
      prompt: 'course-os> ',
      title: 'Course OS shell terminal',
    });
    assert.equal(courseOsShell.workload.hostOnlyWorkflow.route, 'host-only');
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

test('POST /api/session/load accepts a local ELF path from the configured custom root', async () => {
  const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-custom-elf-root-'));
  const elfPath = path.join(tempRoot, 'demo.elf');
  fs.writeFileSync(elfPath, Buffer.from([0x7f, 0x45, 0x4c, 0x46, 1, 2, 3, 4]));
  const previousRoots = process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS;
  process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS = tempRoot;
  const loads = [];
  const server = await startServer({
    port: 0,
    createSession: createCapturingSessionFactory(loads),
  });

  try {
    const response = await postJson(server.baseUrl, '/api/session/load', {
      elfPath,
      backend: 'functional',
    });

    assert.equal(response.status, 200);
    assert.equal(response.body.customElf.source, 'elfPath');
    assert.equal(response.body.customElf.name, 'demo.elf');
    assert.equal(loads.length, 1);
    assert.equal(loads[0].backend, 'functional');
    assert.equal(loads[0].entry.image, elfPath);
    assert.equal(loads[0].entry.kind, 'custom');
    assert.equal(loads[0].entry.disk, null);
  } finally {
    await server.close();
    if (previousRoots == null) {
      delete process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS;
    } else {
      process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS = previousRoots;
    }
    fs.rmSync(tempRoot, { recursive: true, force: true });
  }
});

test('POST /api/session/load rejects custom ELF paths outside configured roots without leaking the path', async () => {
  const allowedRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-custom-elf-allowed-'));
  const outsideRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-custom-elf-outside-'));
  const elfPath = path.join(outsideRoot, 'private.elf');
  fs.writeFileSync(elfPath, Buffer.from([0x7f, 0x45, 0x4c, 0x46, 1, 2, 3, 4]));
  const previousRoots = process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS;
  process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS = allowedRoot;
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await postJson(server.baseUrl, '/api/session/load', {
      elfPath,
      backend: 'pipeline',
    });

    assert.equal(response.status, 403);
    assert.equal(response.body.code, 'custom_elf_path_forbidden');
    assert.equal(response.body.error, 'custom ELF path is outside the allowed roots');
    assert.doesNotMatch(response.body.error, /private\.elf/);
    assert.doesNotMatch(JSON.stringify(response.body), new RegExp(outsideRoot.replaceAll('\\', '\\\\')));
  } finally {
    await server.close();
    if (previousRoots == null) {
      delete process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS;
    } else {
      process.env.MYCPU_FRONTEND_CUSTOM_ELF_ROOTS = previousRoots;
    }
    fs.rmSync(allowedRoot, { recursive: true, force: true });
    fs.rmSync(outsideRoot, { recursive: true, force: true });
  }
});

test('POST /api/session/load accepts base64 ELF input and removes the temporary image after load', async () => {
  const loads = [];
  const server = await startServer({
    port: 0,
    createSession: createCapturingSessionFactory(loads),
  });

  try {
    const response = await postJson(server.baseUrl, '/api/session/load', {
      elfBase64: Buffer.from([0x7f, 0x45, 0x4c, 0x46, 1, 2, 3, 4]).toString('base64'),
      elfName: 'browser-demo.elf',
      backend: 'pipeline',
    });

    assert.equal(response.status, 200);
    assert.equal(response.body.customElf.source, 'elfBase64');
    assert.equal(response.body.customElf.name, 'browser-demo.elf');
    assert.equal(loads.length, 1);
    assert.equal(loads[0].entry.kind, 'custom');
    assert.equal(path.basename(loads[0].entry.image), 'browser-demo.elf');
    assert.equal(fs.existsSync(loads[0].entry.image), false);
  } finally {
    await server.close();
  }
});

test('POST /api/session/load rejects malformed or oversized base64 ELF input', async () => {
  const previousMax = process.env.MYCPU_FRONTEND_CUSTOM_ELF_MAX_BYTES;
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const notElf = await postJson(server.baseUrl, '/api/session/load', {
      elfBase64: Buffer.from('not-elf').toString('base64'),
      elfName: 'not-elf.bin',
      backend: 'pipeline',
    });
    assert.equal(notElf.status, 400);
    assert.equal(notElf.body.code, 'custom_elf_invalid_magic');

    process.env.MYCPU_FRONTEND_CUSTOM_ELF_MAX_BYTES = '4';
    const tooLarge = await postJson(server.baseUrl, '/api/session/load', {
      elfBase64: Buffer.from([0x7f, 0x45, 0x4c, 0x46, 0x00]).toString('base64'),
      elfName: 'too-large.elf',
      backend: 'pipeline',
    });
    assert.equal(tooLarge.status, 413);
    assert.equal(tooLarge.body.code, 'custom_elf_too_large');
  } finally {
    await server.close();
    if (previousMax == null) {
      delete process.env.MYCPU_FRONTEND_CUSTOM_ELF_MAX_BYTES;
    } else {
      process.env.MYCPU_FRONTEND_CUSTOM_ELF_MAX_BYTES = previousMax;
    }
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
    assert.equal(body.linuxFacingContract.schema, 'ai_linux_contract_v1');
    assert.equal(body.linuxFacingContract.firstCut, 'host-facade');
    assert.equal(body.linuxFacingContract.deviceTree.compatible, 'mycpu,ai-accelerator');
    assert.equal(body.linuxFacingContract.mmio.base, '0x10002000');
    assert.equal(body.linuxFacingContract.irq.plicSource, 9);
    assert.equal(body.linuxFacingContract.queue.descriptorBytes, 48);
    assert.equal(body.linuxFacingContract.queue.completionBytes, 40);
    assert.equal(body.linuxFacingContract.profile.schemaVersion, 1);
    assert.equal(body.linuxFacingContract.profile.timingSchemaVersion, 1);
    assert.equal(body.linuxFacingContract.linuxDriver.status, 'not-implemented');
    const tinyModel = body.templates.find((item) => item.id === 'dynamic_tiny_model');
    const dynamicGemm = body.templates.find((item) => item.id === 'dynamic_gemm');
    const dynamicCnn = body.templates.find((item) => item.id === 'dynamic_cnn');
    const tinyAttention = body.templates.find((item) => item.id === 'tiny_attention_static');
    const customBoundedDynamicGemm =
      body.templates.find((item) => item.id === 'custom_bounded_dynamic_gemm');
    const customBoundedDynamicCnn =
      body.templates.find((item) => item.id === 'custom_bounded_dynamic_cnn');
    assert.ok(tinyModel);
    assert.ok(dynamicGemm);
    assert.ok(dynamicCnn);
    assert.ok(tinyAttention);
    assert.ok(customBoundedDynamicGemm);
    assert.ok(customBoundedDynamicCnn);
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
    assert.equal(customBoundedDynamicGemm.shapeMode, 'dynamic_bounded');
    assert.equal(customBoundedDynamicGemm.boundary.taskKind, 'bounded_dynamic_gemm_v1');
    assert.equal(customBoundedDynamicGemm.parameters.inputPreset.choiceLabels.balanced_rows, '2x8 mixed rows');
    assert.deepEqual(customBoundedDynamicGemm.parameters.inputPreset.choices, ['balanced_rows', 'identity_tail']);
    assert.match(customBoundedDynamicGemm.demo.expectedMarker, /balanced_rows returns 10, 5, 9, 5/i);
    assert.equal(customBoundedDynamicCnn.shapeMode, 'dynamic_bounded');
    assert.equal(customBoundedDynamicCnn.boundary.taskKind, 'bounded_dynamic_cnn_v1');
    assert.equal(customBoundedDynamicCnn.parameters.inputPreset.choiceLabels.compact_2x2, '3x3 -> 2x2 compact path');
    assert.deepEqual(customBoundedDynamicCnn.parameters.inputPreset.choices, ['compact_2x2', 'full_3x3']);
    assert.match(customBoundedDynamicCnn.demo.expectedMarker, /compact_2x2 returns 15, 31/i);
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

test('POST /api/ai/tiny-model/run forwards task-spec-backed parameters for custom bounded dynamic GEMM', async () => {
  const calls = [];
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
    aiTinyModelService: {
      templates() {
        return {
          templates: [
            {
              id: 'custom_bounded_dynamic_gemm',
              title: 'Custom Bounded Dynamic GEMM',
              shapeMode: 'dynamic_bounded',
            },
          ],
        };
      },
      async run(payload) {
        calls.push(payload);
        return {
          ok: true,
          template: 'custom_bounded_dynamic_gemm',
          parameters: {
            inputPreset: 'balanced_rows',
          },
          output: {
            dtype: 'int32',
            shape: [2, 4],
            values: [10, 5, 9, 5, 12, 10, 9, 10],
            expected: [10, 5, 9, 5, 12, 10, 9, 10],
          },
          profile: {
            progress: 'completed',
            shapeMode: 'dynamic_bounded',
            runtimeShapes: 't0:2x8,t2:2x4',
            bytesMoved: 80,
            retiredOps: 64,
            deviceCycles: 15,
            dmaCycles: 11,
            computeCycles: 2,
            stallCycles: 2,
            utilization: 11,
          },
          aggregate: {
            tileCount: 2,
            scratchpadPeakBytes: 80,
            opCount: 1,
          },
          ops: [
            { opIndex: 0, opcode: 'gemm', retiredOps: 64, computeCycles: 2, stallCycles: 2, tileCount: 2 },
          ],
        };
      },
    },
  });

  try {
    const response = await postJson(server.baseUrl, '/api/ai/tiny-model/run', {
      template: 'custom_bounded_dynamic_gemm',
      inputPreset: 'balanced_rows',
    });
    assert.equal(response.status, 200);
    assert.deepEqual(calls, [
      {
        template: 'custom_bounded_dynamic_gemm',
        inputPreset: 'balanced_rows',
      },
    ]);
    assert.equal(response.body.template, 'custom_bounded_dynamic_gemm');
    assert.equal(response.body.profile.runtimeShapes, 't0:2x8,t2:2x4');
    assert.deepEqual(response.body.output.values, [10, 5, 9, 5, 12, 10, 9, 10]);
  } finally {
    await server.close();
  }
});

test('POST /api/ai/tiny-model/run forwards task-spec-backed parameters for custom bounded dynamic CNN', async () => {
  const calls = [];
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
    aiTinyModelService: {
      templates() {
        return {
          templates: [
            {
              id: 'custom_bounded_dynamic_cnn',
              title: 'Custom Bounded Dynamic CNN',
              shapeMode: 'dynamic_bounded',
            },
          ],
        };
      },
      async run(payload) {
        calls.push(payload);
        return {
          ok: true,
          template: 'custom_bounded_dynamic_cnn',
          parameters: {
            inputPreset: 'compact_2x2',
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
      template: 'custom_bounded_dynamic_cnn',
      inputPreset: 'compact_2x2',
    });
    assert.equal(response.status, 200);
    assert.deepEqual(calls, [
      {
        template: 'custom_bounded_dynamic_cnn',
        inputPreset: 'compact_2x2',
      },
    ]);
    assert.equal(response.body.template, 'custom_bounded_dynamic_cnn');
    assert.equal(response.body.profile.runtimeShapes, 't0:3x3,t2:2x2,t3:2x2,t4:2x2,t5:2');
    assert.deepEqual(response.body.output.values, [15, 31]);
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

test('POST /api/ai/custom-graph returns a bounded profile result from the AI custom graph service', async () => {
  const calls = [];
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
    aiTinyModelService: {
      templates() {
        return { templates: [] };
      },
      async customGraph(payload) {
        calls.push(payload);
        return {
          ok: true,
          schema: 'ai_custom_graph_result_v1',
          customGraph: {
            schema: 'ai_custom_graph_v1',
            taskKind: 'bounded_dynamic_gemm_v1',
            opSequence: ['gemm'],
            dtype: 'int8/int32',
            shape: { batch: 2, inputColumns: 8, outputColumns: 4 },
            inputPreset: 'balanced_rows',
          },
          output: {
            dtype: 'int32',
            shape: [2, 4],
            values: [10, 5, 9, 5, 12, 10, 9, 10],
            expected: [10, 5, 9, 5, 12, 10, 9, 10],
          },
          profile: {
            progress: 'completed',
            shapeMode: 'dynamic_bounded',
            runtimeShapes: 't0:2x8,t2:2x4',
            bytesMoved: 80,
            retiredOps: 64,
            deviceCycles: 15,
            dmaCycles: 11,
            computeCycles: 2,
            stallCycles: 2,
            utilization: 11,
          },
          aggregate: {
            tileCount: 2,
            scratchpadPeakBytes: 80,
            opCount: 1,
          },
          ops: [
            { opIndex: 0, opcode: 'gemm', retiredOps: 64, computeCycles: 2, stallCycles: 2, tileCount: 2 },
          ],
          boundary: {
            allowsGraphPackageUpload: false,
            allowsModelUpload: false,
            serverGeneratedGraph: true,
          },
        };
      },
    },
  });

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
    const response = await postJson(server.baseUrl, '/api/ai/custom-graph', payload);
    assert.equal(response.status, 200);
    assert.deepEqual(calls, [payload]);
    assert.equal(response.body.schema, 'ai_custom_graph_result_v1');
    assert.equal(response.body.customGraph.taskKind, 'bounded_dynamic_gemm_v1');
    assert.deepEqual(response.body.customGraph.opSequence, ['gemm']);
    assert.deepEqual(response.body.output.values, [10, 5, 9, 5, 12, 10, 9, 10]);
    assert.equal(response.body.profile.runtimeShapes, 't0:2x8,t2:2x4');
    assert.equal(response.body.boundary.allowsGraphPackageUpload, false);
  } finally {
    await server.close();
  }
});

test('default AI custom graph service rejects unsafe or out-of-contract payloads', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const basePayload = {
      schema: 'ai_custom_graph_v1',
      opSequence: ['gemm'],
      dtype: 'int8/int32',
      shape: {
        kind: 'bounded_dynamic_gemm_v1',
        batch: 2,
      },
      inputPreset: 'balanced_rows',
    };

    const illegalOp = await postJson(server.baseUrl, '/api/ai/custom-graph', {
      ...basePayload,
      opSequence: ['matmul'],
    });
    assert.equal(illegalOp.status, 400);
    assert.equal(illegalOp.body.error, 'unsupported custom graph op: matmul');

    const shapeTooLarge = await postJson(server.baseUrl, '/api/ai/custom-graph', {
      ...basePayload,
      shape: {
        kind: 'bounded_dynamic_gemm_v1',
        batch: 3,
      },
    });
    assert.equal(shapeTooLarge.status, 400);
    assert.equal(shapeTooLarge.body.error, 'custom graph GEMM batch must be one of: 1, 2');

    const dtypeMismatch = await postJson(server.baseUrl, '/api/ai/custom-graph', {
      ...basePayload,
      dtype: 'fp16/fp32',
    });
    assert.equal(dtypeMismatch.status, 400);
    assert.equal(dtypeMismatch.body.error, 'custom graph dtype must be int8/int32 for gemm');

    const graphPackageUpload = await postJson(server.baseUrl, '/api/ai/custom-graph', {
      ...basePayload,
      graphPackage: 'browser-supplied.bin',
    });
    assert.equal(graphPackageUpload.status, 400);
    assert.equal(graphPackageUpload.body.error, 'custom graph package upload is not allowed');
  } finally {
    await server.close();
  }
});

test('default AI custom graph service lowers bounded GEMM through task-spec packaging', async () => {
  const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'mycpu-ai-custom-graph-test-'));
  const fakeBinary = path.join(tempRoot, 'fake-mycpu.mjs');
  fs.writeFileSync(fakeBinary, `#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';

const manifestIndex = process.argv.indexOf('--ai-profile-manifest');
const manifestPath = process.argv[manifestIndex + 1];
const manifestDir = path.dirname(manifestPath);
let outputPath = null;
let expectedPath = null;
for (const line of fs.readFileSync(manifestPath, 'utf8').trim().split(/\\n/)) {
  if (line.startsWith('output=')) {
    outputPath = path.join(manifestDir, line.slice('output='.length));
  }
  if (line.startsWith('expected_output=')) {
    expectedPath = path.join(manifestDir, line.slice('expected_output='.length));
  }
}
fs.copyFileSync(expectedPath, outputPath);
process.stdout.write('ai_profile progress=completed shape_mode=dynamic_bounded runtime_shapes=t0:2x8,t2:2x4 bytes_moved=80 retired_ops=64 device_cycles=15 dma_cycles=11 compute_cycles=2 stall_cycles=2 busy_cycles=15 queue_cycles=0 completion_cycles=0 utilization=11 effective_ops_per_cycle=4\\n');
process.stdout.write('ai_profile_aggregate tile_count=2 scratchpad_peak_bytes=80 op_count=1\\n');
process.stdout.write('ai_profile_op op_index=0 opcode=gemm retired_ops=64 compute_cycles=2 stall_cycles=2 tile_count=2\\n');
`);
  fs.chmodSync(fakeBinary, 0o755);

  try {
    const service = createAiTinyModelService({
      repoRoot,
      binaryPath: fakeBinary,
    });
    const response = await service.customGraph({
      schema: 'ai_custom_graph_v1',
      opSequence: ['gemm'],
      dtype: 'int8/int32',
      shape: {
        kind: 'bounded_dynamic_gemm_v1',
        batch: 2,
      },
      inputPreset: 'balanced_rows',
    });

    assert.equal(response.schema, 'ai_custom_graph_result_v1');
    assert.equal(response.customGraph.taskKind, 'bounded_dynamic_gemm_v1');
    assert.deepEqual(response.output.values, [10, 5, 9, 5, 12, 10, 9, 10]);
    assert.deepEqual(response.output.expected, [10, 5, 9, 5, 12, 10, 9, 10]);
    assert.equal(response.profile.runtimeShapes, 't0:2x8,t2:2x4');
    assert.equal(response.aggregate.opCount, 1);
    assert.equal(response.boundary.taskSpecImporter, true);
  } finally {
    fs.rmSync(tempRoot, { recursive: true, force: true });
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
    assert.match(body, /Lab workbench/);
    assert.match(body, /id="demo-workspace-slot"/);
    assert.match(body, /Lab workspace/);
    assert.match(body, /选择一个实验场景/);
    assert.match(body, /Linux serial console/);
    assert.match(body, /MYCPU_LINUX_PROTO_CONSOLE_IMAGE/);
    assert.match(body, /id="test-select"/);
    assert.match(body, /id="custom-elf-path"/);
    assert.match(body, /id="custom-elf-base64"/);
    assert.match(body, /id="load-custom-elf-button"/);
    assert.match(body, /id="terminate-button"/);
    assert.match(body, />Terminate</);
    assert.match(body, /src="\/app\.js"/);
  } finally {
    await server.close();
  }
});

test('POST /api/session/jit-dispatch returns the current runtime dry-run summary', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_vector_cnn_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/jit-dispatch', {});
    assert.equal(response.status, 200);
    assert.deepEqual(response.body.summary, {
      type: 'jit_dispatch',
      ok: true,
      source: 'hot-path-profile',
      action: 'lowered-ready',
      start_pc: '0x80000000',
      end_pc: '0x80000020',
      cache_state: 'hit',
      planned: true,
      translated: true,
      lowered: true,
      fallback_to_reference: false,
      lowered_instruction_count: 5,
      candidate_executions: 18,
      candidate_retired_instructions: 72,
      reject_kind: 'none',
      reject_reason: 'none',
      helper_replay_kind: 'none',
      host_code: false,
      executable_memory: false,
      guest_execution: false,
    });
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
    assert.match(body, /\/source\/showcase\/simulator\/ppt_screenshot_console_overview\.png/);
    assert.match(body, /\/source\/showcase\/simulator\/ppt_screenshot_pipeline\.png/);
    assert.match(body, /\/source\/showcase\/simulator\/ppt_screenshot_terminal\.png/);
    assert.match(body, /\/source\/showcase\/simulator\/ppt_screenshot_ai_or_vecto\.png/);
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
    const response = await fetch(`${server.baseUrl}/source/showcase/simulator/ppt_screenshot_console_overview.png`);
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

test('POST /api/session/debug-command forwards controlled debug write commands', async () => {
  const calls = [];
  const server = await startServer({
    port: 0,
    createSession: async () => {
      const session = createFakeSession('debug-control');
      session.setMemory = async (addr, value, size, virtualAddress) => {
        calls.push(['setMemory', addr, value, size, virtualAddress]);
        return { ok: true };
      };
      session.setCsr = async (csr, value) => {
        calls.push(['setCsr', csr, value]);
        return { ok: true };
      };
      session.breakAt = async (addr) => {
        calls.push(['breakAt', addr]);
        return { ok: true };
      };
      return session;
    },
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const setMemory = await postJson(server.baseUrl, '/api/session/debug-command', {
      cmd: 'set_memory',
      addr: '0x80000100',
      value: '0xaa',
      size: 1,
      virtual: true,
    });
    assert.equal(setMemory.status, 200);
    assert.equal(setMemory.body.ok, true);
    assert.equal(setMemory.body.snapshot.summary.cycle, 0);

    const setCsr = await postJson(server.baseUrl, '/api/session/debug-command', {
      cmd: 'set_csr',
      csr: 'mepc',
      value: '0x80000090',
    });
    assert.equal(setCsr.status, 200);
    assert.equal(setCsr.body.ok, true);

    const breakAt = await postJson(server.baseUrl, '/api/session/debug-command', {
      cmd: 'break_at',
      addr: '0x80000080',
    });
    assert.equal(breakAt.status, 200);
    assert.equal(breakAt.body.ok, true);

    assert.deepEqual(calls, [
      ['setMemory', '0x80000100', '0xaa', 1, true],
      ['setCsr', 'mepc', '0x80000090'],
      ['breakAt', '0x80000080'],
    ]);
  } finally {
    await server.close();
  }
});

test('POST /api/session/debug-command rejects unsupported debug CLI commands', async () => {
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

    const response = await postJson(server.baseUrl, '/api/session/debug-command', {
      cmd: 'quit',
    });
    assert.equal(response.status, 400);
    assert.match(response.body.error, /unsupported debug command/);
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

test('POST /api/session/load boots guest_course_os_shell_demo to course-os prompt', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const response = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_course_os_shell_demo',
      backend: 'pipeline',
    });
    assert.equal(response.status, 200);
    assert.match(response.body.terminal.text, /course-os shell ready/);
    assert.match(response.body.terminal.text, /course-os> /);
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

test('POST /api/session/terminal-input waits for Course OS shell prompt after command output', async () => {
  const server = await startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  });

  try {
    const loadResponse = await postJson(server.baseUrl, '/api/session/load', {
      test: 'guest_course_os_shell_demo',
      backend: 'pipeline',
    });
    assert.equal(loadResponse.status, 200);

    const response = await postJson(server.baseUrl, '/api/session/terminal-input', {
      text: 'help\r',
    });
    assert.equal(response.status, 200);
    assert.match(response.body.text, /meminfo schedstat fsstat/);
    assert.match(response.body.text, /course-os> $/);
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

test('auth-enabled server rejects unauthenticated API access and reports auth state', async () => {
  const passwordHash = buildPasswordHashForTests('pw');
  const server = await withEnv({
    MYCPU_AUTH_ENABLED: '1',
    MYCPU_AUTH_ADMIN_USERNAME: 'admin',
    MYCPU_AUTH_ADMIN_PASSWORD_HASH: passwordHash,
    MYCPU_AUTH_SESSION_LIMIT: '3',
    MYCPU_AUTH_SECURE_COOKIES: '0',
  }, () => startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  }));

  try {
    const authSession = await getJsonWithCookie(server.baseUrl, '/api/auth/session');
    assert.equal(authSession.status, 200);
    assert.equal(authSession.body.auth.required, true);
    assert.equal(authSession.body.auth.authenticated, false);

    const testsResponse = await getJsonWithCookie(server.baseUrl, '/api/tests');
    assert.equal(testsResponse.status, 401);
    assert.equal(testsResponse.body.error, 'authentication required');
  } finally {
    await server.close();
  }
});

test('production server rejects unauthenticated deployment template unless explicitly opted in', async () => {
  await assert.rejects(
    () => withEnv({
      NODE_ENV: 'production',
      MYCPU_AUTH_ENABLED: '0',
    }, () => startServer({
      port: 0,
      createSession: createFakeSessionFactory(),
    })),
    /MYCPU_AUTH_ENABLED=1 or MYCPU_PUBLIC_UNAUTH_OK=1/,
  );

  const server = await withEnv({
    NODE_ENV: 'production',
    MYCPU_AUTH_ENABLED: '0',
    MYCPU_PUBLIC_UNAUTH_OK: '1',
  }, () => startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  }));

  try {
    const testsResponse = await getJsonWithCookie(server.baseUrl, '/api/tests');
    assert.equal(testsResponse.status, 200);
    assert.equal(testsResponse.body.auth.required, false);
  } finally {
    await server.close();
  }
});

test('auth-enabled server grants login, enforces one controller, and limits concurrent sessions to three', async () => {
  const passwordHash = buildPasswordHashForTests('pw');
  const server = await withEnv({
    MYCPU_AUTH_ENABLED: '1',
    MYCPU_AUTH_ADMIN_USERNAME: 'admin',
    MYCPU_AUTH_ADMIN_PASSWORD_HASH: passwordHash,
    MYCPU_AUTH_SESSION_LIMIT: '3',
    MYCPU_AUTH_SECURE_COOKIES: '0',
  }, () => startServer({
    port: 0,
    createSession: createFakeSessionFactory(),
  }));

  try {
    const login1 = await postJsonWithCookie(server.baseUrl, '/api/auth/login', {
      username: 'admin',
      password: 'pw',
    });
    const login2 = await postJsonWithCookie(server.baseUrl, '/api/auth/login', {
      username: 'admin',
      password: 'pw',
    });
    const login3 = await postJsonWithCookie(server.baseUrl, '/api/auth/login', {
      username: 'admin',
      password: 'pw',
    });
    assert.equal(login1.status, 200);
    assert.equal(login2.status, 200);
    assert.equal(login3.status, 200);
    assert.match(login1.cookie, /mycpu_session=/);

    const login4 = await postJsonWithCookie(server.baseUrl, '/api/auth/login', {
      username: 'admin',
      password: 'pw',
    });
    assert.equal(login4.status, 429);
    assert.match(login4.body.error, /concurrent session limit reached \(3\)/);

    const testsResponse = await getJsonWithCookie(server.baseUrl, '/api/tests', login1.cookie);
    assert.equal(testsResponse.status, 200);
    assert.equal(testsResponse.body.auth.authenticated, true);
    assert.equal(testsResponse.body.auth.canControl, true);

    const firstLoad = await postJsonWithCookie(server.baseUrl, '/api/session/load', {
      test: 'hello',
      backend: 'pipeline',
    }, login1.cookie);
    assert.equal(firstLoad.status, 200);

    const secondLoad = await postJsonWithCookie(server.baseUrl, '/api/session/load', {
      test: 'sum',
      backend: 'pipeline',
    }, login2.cookie);
    assert.equal(secondLoad.status, 409);
    assert.match(secondLoad.body.error, /controller locked by admin/);

    const release = await postJsonWithCookie(server.baseUrl, '/api/auth/release-control', {}, login1.cookie);
    assert.equal(release.status, 200);
    assert.equal(release.body.auth.controllerSession, false);

    const secondLoadAfterRelease = await postJsonWithCookie(server.baseUrl, '/api/session/load', {
      test: 'sum',
      backend: 'pipeline',
    }, login2.cookie);
    assert.equal(secondLoadAfterRelease.status, 200);
  } finally {
    await server.close();
  }
});
