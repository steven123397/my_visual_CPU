import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

import { listTests } from './tests_manifest.mjs';
import { createWebSocketHub } from './ws.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..', '..');
const appRoot = path.join(repoRoot, 'frontend', 'app');

function json(response, statusCode, payload) {
  response.writeHead(statusCode, { 'content-type': 'application/json; charset=utf-8' });
  response.end(JSON.stringify(payload));
}

async function readBody(request) {
  let body = '';
  for await (const chunk of request) {
    body += chunk;
  }
  return body ? JSON.parse(body) : {};
}

function contentTypeFor(filePath) {
  if (filePath.endsWith('.html')) {
    return 'text/html; charset=utf-8';
  }
  if (filePath.endsWith('.js')) {
    return 'text/javascript; charset=utf-8';
  }
  if (filePath.endsWith('.css')) {
    return 'text/css; charset=utf-8';
  }
  if (filePath.endsWith('.json')) {
    return 'application/json; charset=utf-8';
  }
  return 'text/plain; charset=utf-8';
}

class DebugCliSession {
  constructor({ binaryPath }) {
    this.binaryPath = binaryPath;
    this.pending = [];
    this.child = spawn(binaryPath, ['--debug-cli'], {
      cwd: path.dirname(binaryPath),
      stdio: ['pipe', 'pipe', 'pipe'],
    });
    this.stdoutBuffer = '';
    this.stderrBuffer = '';

    this.child.stdout.setEncoding('utf8');
    this.child.stdout.on('data', (chunk) => {
      this.stdoutBuffer += chunk;
      this.flushLines();
    });
    this.child.stderr.setEncoding('utf8');
    this.child.stderr.on('data', (chunk) => {
      this.stderrBuffer += chunk;
    });
    this.child.on('exit', (code) => {
      const error = new Error(`debug cli exited with code ${code}`);
      while (this.pending.length > 0) {
        this.pending.shift().reject(error);
      }
    });
  }

  flushLines() {
    while (true) {
      const newline = this.stdoutBuffer.indexOf('\n');
      if (newline === -1) {
        return;
      }
      const line = this.stdoutBuffer.slice(0, newline).trim();
      this.stdoutBuffer = this.stdoutBuffer.slice(newline + 1);
      if (!line) {
        continue;
      }
      const pending = this.pending.shift();
      if (!pending) {
        continue;
      }
      pending.resolve(JSON.parse(line));
    }
  }

  send(command) {
    return new Promise((resolve, reject) => {
      this.pending.push({ resolve, reject });
      this.child.stdin.write(`${JSON.stringify(command)}\n`);
    });
  }

  async load(testEntry, backend) {
    const response = await this.send({
      cmd: 'load',
      image: testEntry.image,
      disk: testEntry.disk ?? undefined,
      disk_ready: testEntry.diskReady ?? true,
      disk_magic_valid: testEntry.diskMagicValid ?? true,
      backend,
    });
    if (response.type === 'error') {
      throw new Error(response.message);
    }
    return response;
  }

  async snapshot() {
    return this.send({ cmd: 'snapshot' });
  }

  async stepCycle() {
    return this.send({ cmd: 'step_cycle' });
  }

  async stepCommit() {
    return this.send({ cmd: 'step_commit' });
  }

  async reset() {
    return this.send({ cmd: 'reset' });
  }

  async uartInput(text) {
    const response = await this.send({ cmd: 'uart_input', text });
    if (response.type === 'error') {
      throw new Error(response.message);
    }
    return response;
  }

  async uartOutput(offset = 0) {
    const response = await this.send({ cmd: 'uart_output', offset });
    if (response.type === 'error') {
      throw new Error(response.message);
    }
    return {
      text: response.text ?? '',
      nextOffset: response.next_offset ?? offset,
    };
  }

  async close() {
    try {
      await this.send({ cmd: 'quit' });
    } catch {
      // Ignore shutdown races.
    }
    this.child.kill();
  }
}

async function serveStatic(response, pathname) {
  const relative = pathname === '/' ? '/index.html' : pathname;
  const filePath = path.join(appRoot, relative.replace(/^\/+/, ''));
  if (!filePath.startsWith(appRoot)) {
    json(response, 403, { error: 'forbidden' });
    return;
  }
  try {
    const content = await fs.readFile(filePath);
    response.writeHead(200, { 'content-type': contentTypeFor(filePath) });
    response.end(content);
  } catch {
    json(response, 404, { error: 'not found' });
  }
}

export async function startServer({
  host = '127.0.0.1',
  port = 4173,
  createSession = async () => new DebugCliSession({ binaryPath: path.join(repoRoot, 'myCPU', 'mycpu') }),
} = {}) {
  const tests = listTests(repoRoot);
  let currentSession = null;
  let currentSnapshot = null;
  let runTimer = null;
  let currentTerminalBuffer = '';
  let currentTerminalOffset = 0;

  function stopRunLoop() {
    if (runTimer) {
      clearInterval(runTimer);
      runTimer = null;
    }
  }

  function resetTerminalTracking() {
    currentTerminalBuffer = '';
    currentTerminalOffset = 0;
  }

  async function readTerminalOutput(offset = currentTerminalOffset) {
    if (!currentSession) {
      throw new Error('session not loaded');
    }
    const chunk = await currentSession.uartOutput(offset);
    return {
      text: chunk.text ?? '',
      nextOffset: chunk.nextOffset ?? chunk.next_offset ?? offset,
    };
  }

  function trackTerminalChunk(chunk, { reset = false } = {}) {
    const normalized = {
      type: 'terminal',
      text: chunk.text ?? '',
      nextOffset: chunk.nextOffset ?? currentTerminalOffset,
      reset,
    };

    if (reset) {
      currentTerminalBuffer = normalized.text;
    } else if (normalized.nextOffset > currentTerminalOffset) {
      currentTerminalBuffer += normalized.text;
    }
    currentTerminalOffset = normalized.nextOffset;
    return normalized;
  }

  async function syncTerminalDelta({ offset = currentTerminalOffset, reset = false, broadcast = false } = {}) {
    const message = trackTerminalChunk(await readTerminalOutput(offset), { reset });
    if (broadcast && (reset || message.text.length > 0)) {
      wsHub.broadcast(message);
    }
    return message;
  }

  const server = http.createServer(async (request, response) => {
    try {
      const url = new URL(request.url, `http://${request.headers.host}`);
      if (request.method === 'GET' && url.pathname === '/api/tests') {
        json(response, 200, {
          tests: tests.map(({ name, disk, kind }) => ({ name, hasDisk: Boolean(disk), kind })),
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/load') {
        const body = await readBody(request);
        const entry = tests.find((item) => item.name === body.test);
        if (!entry) {
          json(response, 404, { error: `unknown test: ${body.test}` });
          return;
        }
        stopRunLoop();
        if (currentSession) {
          await currentSession.close();
        }
        currentSession = await createSession();
        await currentSession.load(entry, body.backend ?? 'pipeline');
        currentSnapshot = await currentSession.snapshot();
        resetTerminalTracking();
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({ offset: 0, reset: true, broadcast: true });
        json(response, 200, { ok: true, snapshot: currentSnapshot, terminal });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/snapshot') {
        if (!currentSession) {
          json(response, 400, { error: 'session not loaded' });
          return;
        }
        currentSnapshot = await currentSession.snapshot();
        json(response, 200, { snapshot: currentSnapshot });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/step-cycle') {
        if (!currentSession) {
          json(response, 400, { error: 'session not loaded' });
          return;
        }
        currentSnapshot = await currentSession.stepCycle();
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({ broadcast: true });
        json(response, 200, { snapshot: currentSnapshot, terminal });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/step-commit') {
        if (!currentSession) {
          json(response, 400, { error: 'session not loaded' });
          return;
        }
        currentSnapshot = await currentSession.stepCommit();
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({ broadcast: true });
        json(response, 200, { snapshot: currentSnapshot, terminal });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/reset') {
        if (!currentSession) {
          json(response, 400, { error: 'session not loaded' });
          return;
        }
        currentSnapshot = await currentSession.reset();
        resetTerminalTracking();
        wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
        const terminal = await syncTerminalDelta({ offset: 0, reset: true, broadcast: true });
        json(response, 200, { snapshot: currentSnapshot, terminal });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/run') {
        if (!currentSession) {
          json(response, 400, { error: 'session not loaded' });
          return;
        }
        const body = await readBody(request);
        const intervalMs = Math.max(20, Math.floor(1000 / Math.max(1, body.rateHz ?? 8)));
        stopRunLoop();
        runTimer = setInterval(async () => {
          try {
            currentSnapshot = await currentSession.stepCycle();
            wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
            await syncTerminalDelta({ broadcast: true });
            if (currentSnapshot.summary?.halted) {
              stopRunLoop();
            }
          } catch (error) {
            wsHub.broadcast({ type: 'error', message: error.message });
            stopRunLoop();
          }
        }, intervalMs);
        json(response, 200, { ok: true });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/terminal-input') {
        if (!currentSession) {
          json(response, 400, { error: 'session not loaded' });
          return;
        }
        const body = await readBody(request);
        await currentSession.uartInput(body.text ?? '');
        const terminal = await syncTerminalDelta({ broadcast: true });
        json(response, 200, {
          ok: true,
          text: terminal.text,
          nextOffset: terminal.nextOffset,
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/terminal-output') {
        if (!currentSession) {
          json(response, 400, { error: 'session not loaded' });
          return;
        }
        const body = await readBody(request);
        const terminal = await readTerminalOutput(body.offset ?? 0);
        json(response, 200, terminal);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/pause') {
        stopRunLoop();
        json(response, 200, { ok: true, snapshot: currentSnapshot });
        return;
      }

      await serveStatic(response, url.pathname);
    } catch (error) {
      json(response, 500, { error: error.message });
    }
  });

  const wsHub = createWebSocketHub(server);

  await new Promise((resolve) => {
    server.listen(port, host, resolve);
  });

  const address = server.address();
  const boundPort = typeof address === 'object' && address ? address.port : port;

  return {
    host,
    port: boundPort,
    baseUrl: `http://${host}:${boundPort}`,
    async close() {
      stopRunLoop();
      if (currentSession) {
        await currentSession.close();
        currentSession = null;
      }
      wsHub.close();
      await new Promise((resolve, reject) => {
        server.close((error) => (error ? reject(error) : resolve()));
      });
    },
  };
}

if (process.argv[1] === __filename) {
  const portArg = process.argv.find((arg) => arg.startsWith('--port='));
  const port = portArg ? Number(portArg.slice('--port='.length)) : 4173;
  const server = await startServer({ port });
  process.stdout.write(`debug server listening at ${server.baseUrl}\n`);
}
