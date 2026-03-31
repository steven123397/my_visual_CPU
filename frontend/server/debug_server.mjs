import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';

import { listTests } from './tests_manifest.mjs';
import { createWebSocketHub } from './ws.mjs';
import {
  applyTerminalChunk,
  createTerminalProjectionState,
  DEFAULT_TERMINAL_MAX_LENGTH,
  resetTerminalProjectionState,
} from '../shared/terminal_projection.mjs';

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

  async runUntilUartContains(text, maxSteps) {
    const response = await this.send({
      cmd: 'run_until_uart_contains',
      text,
      max_steps: maxSteps,
    });
    if (response.type === 'error') {
      throw new Error(response.message);
    }
    return response;
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

class StaleSessionError extends Error {
  constructor(message = 'stale session action') {
    super(message);
    this.name = 'StaleSessionError';
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
  let currentTerminalPrompt = null;
  let runTimer = null;
  let currentTerminalProjection = createTerminalProjectionState({
    maxLength: DEFAULT_TERMINAL_MAX_LENGTH,
  });
  let currentTerminalOffset = 0;
  let currentGeneration = 0;
  let sessionActionQueue = Promise.resolve();
  let runLoopToken = 0;

  function enqueueSessionAction(action) {
    const queued = sessionActionQueue.then(action, action);
    sessionActionQueue = queued.catch(() => {});
    return queued;
  }

  function beginSessionGeneration() {
    currentGeneration += 1;
    return currentGeneration;
  }

  function assertGeneration(generation) {
    if (generation !== currentGeneration) {
      throw new StaleSessionError();
    }
  }

  function stopRunLoop() {
    if (runTimer) {
      clearTimeout(runTimer);
      runTimer = null;
    }
    runLoopToken += 1;
  }

  function resetTerminalTracking() {
    resetTerminalProjectionState(currentTerminalProjection);
    currentTerminalOffset = 0;
  }

  async function readTerminalOutput(offset = currentTerminalOffset, generation = currentGeneration) {
    if (!currentSession) {
      throw new Error('session not loaded');
    }
    const chunk = await currentSession.uartOutput(offset);
    assertGeneration(generation);
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
      resetTerminalProjectionState(currentTerminalProjection);
      applyTerminalChunk(currentTerminalProjection, normalized.text);
    } else if (normalized.nextOffset > currentTerminalOffset) {
      applyTerminalChunk(currentTerminalProjection, normalized.text);
    }
    currentTerminalOffset = normalized.nextOffset;
    return normalized;
  }

  async function syncTerminalDelta({
    offset = currentTerminalOffset,
    reset = false,
    broadcast = false,
    generation = currentGeneration,
  } = {}) {
    const message = trackTerminalChunk(await readTerminalOutput(offset, generation), { reset });
    if (broadcast && (reset || message.text.length > 0)) {
      wsHub.broadcast(message);
    }
    return message;
  }

  async function advanceUntilTerminalActivity({
    maxCommits = 4096,
    settleCommits = 256,
    idleCommitsWithoutOutput = null,
    shouldStop = null,
    generation = currentGeneration,
  } = {}) {
    const aggregate = {
      type: 'terminal',
      text: '',
      nextOffset: currentTerminalOffset,
      reset: false,
    };
    let quietCommits = 0;
    let sawOutput = false;
    let commits = 0;

    for (let i = 0; i < maxCommits; ++i) {
      assertGeneration(generation);
      currentSnapshot = await currentSession.stepCommit();
      assertGeneration(generation);
      commits += 1;

      const terminal = await syncTerminalDelta({ broadcast: false, generation });
      if (terminal.text.length > 0) {
        aggregate.text += terminal.text;
        aggregate.nextOffset = terminal.nextOffset;
        sawOutput = true;
        quietCommits = 0;
        if (shouldStop?.({ aggregate, currentSnapshot, currentTerminalBuffer: currentTerminalProjection.text })) {
          break;
        }
      } else {
        quietCommits += 1;
        if (sawOutput) {
          if (quietCommits >= settleCommits) {
            break;
          }
        } else if (idleCommitsWithoutOutput != null &&
                   quietCommits >= idleCommitsWithoutOutput) {
          break;
        }
      }

      if (currentSnapshot.summary?.halted) {
        break;
      }
    }

    return {
      commits,
      terminal: aggregate,
    };
  }

  function buildTerminalAdvancePlan(text) {
    if (!text) {
      return null;
    }

    let visibleAsciiCount = 0;
    let controlCount = 0;

    for (const char of text) {
      if (char === '\r' || char === '\n') {
        controlCount += 1;
        continue;
      }
      if (char === '\b' || char === '\x7f') {
        controlCount += 1;
        continue;
      }
      if (/^[\x20-\x7e]$/.test(char)) {
        visibleAsciiCount += 1;
      }
    }

    if (visibleAsciiCount === 0 && controlCount === 0) {
      return null;
    }

    if (text.includes('\r') || text.includes('\n')) {
      return {
        maxCommits: 16384,
        settleCommits: 1024,
        idleCommitsWithoutOutput: 128,
        shouldStop: ({ aggregate, currentSnapshot, currentTerminalBuffer: terminalBuffer }) =>
          aggregate.text.length > 0
          && (
            currentSnapshot.summary?.halted
            || (currentTerminalPrompt && terminalBuffer.endsWith(currentTerminalPrompt))
          ),
      };
    }

    if (controlCount > 0) {
      return {
        maxCommits: Math.max(1024, (visibleAsciiCount + controlCount) * 1024),
        settleCommits: 64,
        idleCommitsWithoutOutput: 64,
      };
    }

    if (visibleAsciiCount > 0) {
      return {
        maxCommits: Math.max(2048, (visibleAsciiCount + controlCount) * 1024),
        settleCommits: 256,
        idleCommitsWithoutOutput: 64,
        shouldStop: ({ aggregate }) => aggregate.text.length >= visibleAsciiCount,
      };
    }
  }

  function startRunLoop(intervalMs, generation) {
    stopRunLoop();
    const token = runLoopToken;

    const scheduleNextTick = () => {
      if (token !== runLoopToken || generation !== currentGeneration || !currentSession) {
        return;
      }

      runTimer = setTimeout(async () => {
        runTimer = null;
        try {
          await enqueueSessionAction(async () => {
            assertGeneration(generation);
            if (!currentSession || token !== runLoopToken) {
              return;
            }

            currentSnapshot = await currentSession.stepCycle();
            assertGeneration(generation);
            wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
            await syncTerminalDelta({ broadcast: true, generation });
            if (currentSnapshot.summary?.halted) {
              stopRunLoop();
            }
          });
        } catch (error) {
          if (error instanceof StaleSessionError) {
            return;
          }
          wsHub.broadcast({ type: 'error', message: error.message });
          stopRunLoop();
          return;
        }

        if (token === runLoopToken && generation === currentGeneration && !currentSnapshot?.summary?.halted) {
          scheduleNextTick();
        }
      }, intervalMs);
    };

    scheduleNextTick();
  }

  async function runQueued(response, action) {
    try {
      await enqueueSessionAction(action);
      return true;
    } catch (error) {
      if (error instanceof StaleSessionError) {
        json(response, 409, { error: error.message });
        return true;
      }
      throw error;
    }
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
        const generation = beginSessionGeneration();
        stopRunLoop();
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (currentSession) {
            await currentSession.close();
            assertGeneration(generation);
          }
          currentSession = await createSession();
          currentTerminalPrompt = entry.terminalPrompt ?? null;
          await currentSession.load(entry, body.backend ?? 'pipeline');
          assertGeneration(generation);
          currentSnapshot = entry.bootUntilUartText
            ? await currentSession.runUntilUartContains(
                entry.bootUntilUartText,
                entry.bootMaxSteps ?? 0,
              )
            : await currentSession.snapshot();
          assertGeneration(generation);
          resetTerminalTracking();
          wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
          const terminal = await syncTerminalDelta({
            offset: 0,
            reset: true,
            broadcast: true,
            generation,
          });
          json(response, 200, { ok: true, snapshot: currentSnapshot, terminal });
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/snapshot') {
        const generation = currentGeneration;
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (!currentSession) {
            json(response, 400, { error: 'session not loaded' });
            return;
          }
          currentSnapshot = await currentSession.snapshot();
          assertGeneration(generation);
          json(response, 200, { snapshot: currentSnapshot });
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/step-cycle') {
        const generation = currentGeneration;
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (!currentSession) {
            json(response, 400, { error: 'session not loaded' });
            return;
          }
          currentSnapshot = await currentSession.stepCycle();
          assertGeneration(generation);
          wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
          const terminal = await syncTerminalDelta({ broadcast: true, generation });
          json(response, 200, { snapshot: currentSnapshot, terminal });
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/step-commit') {
        const generation = currentGeneration;
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (!currentSession) {
            json(response, 400, { error: 'session not loaded' });
            return;
          }
          currentSnapshot = await currentSession.stepCommit();
          assertGeneration(generation);
          wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
          const terminal = await syncTerminalDelta({ broadcast: true, generation });
          json(response, 200, { snapshot: currentSnapshot, terminal });
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/reset') {
        const generation = beginSessionGeneration();
        stopRunLoop();
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (!currentSession) {
            json(response, 400, { error: 'session not loaded' });
            return;
          }
          currentSnapshot = await currentSession.reset();
          assertGeneration(generation);
          resetTerminalTracking();
          wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
          const terminal = await syncTerminalDelta({
            offset: 0,
            reset: true,
            broadcast: true,
            generation,
          });
          json(response, 200, { snapshot: currentSnapshot, terminal });
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/run') {
        const body = await readBody(request);
        const intervalMs = Math.max(20, Math.floor(1000 / Math.max(1, body.rateHz ?? 8)));
        const generation = currentGeneration;
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (!currentSession) {
            json(response, 400, { error: 'session not loaded' });
            return;
          }
          startRunLoop(intervalMs, generation);
          json(response, 200, { ok: true });
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/terminal-input') {
        const body = await readBody(request);
        const text = body.text ?? '';
        const generation = currentGeneration;
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (!currentSession) {
            json(response, 400, { error: 'session not loaded' });
            return;
          }
          await currentSession.uartInput(text);
          assertGeneration(generation);
          const advancePlan = buildTerminalAdvancePlan(text);
          let terminal;
          let shouldBroadcastSnapshot = false;
          if (runTimer) {
            terminal = await syncTerminalDelta({ broadcast: true, generation });
          } else if (advancePlan) {
            const result = await advanceUntilTerminalActivity({
              ...advancePlan,
              generation,
            });
            terminal = result.terminal;
            shouldBroadcastSnapshot = result.commits > 0;
            if (shouldBroadcastSnapshot) {
              wsHub.broadcast({ type: 'snapshot', snapshot: currentSnapshot });
            }
            if (terminal.text.length > 0) {
              wsHub.broadcast(terminal);
            }
          } else {
            terminal = await syncTerminalDelta({ broadcast: true, generation });
          }
          json(response, 200, {
            ok: true,
            text: terminal.text,
            nextOffset: terminal.nextOffset,
          });
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/terminal-output') {
        const body = await readBody(request);
        const generation = currentGeneration;
        await runQueued(response, async () => {
          assertGeneration(generation);
          if (!currentSession) {
            json(response, 400, { error: 'session not loaded' });
            return;
          }
          const terminal = await readTerminalOutput(body.offset ?? 0, generation);
          json(response, 200, terminal);
        });
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/pause') {
        stopRunLoop();
        await runQueued(response, async () => {
          json(response, 200, { ok: true, snapshot: currentSnapshot });
        });
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
