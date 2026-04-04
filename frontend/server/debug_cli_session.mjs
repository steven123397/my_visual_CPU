import path from 'node:path';
import { spawn } from 'node:child_process';

import { DEBUG_CLI_SESSION_BUDGET } from './debug_budget.mjs';

export function normalizeCliResponse(response, context = 'debug cli request') {
  if (response && typeof response === 'object' && response.type === 'error') {
    const message = typeof response.message === 'string' && response.message.length > 0
      ? response.message
      : `${context} failed`;
    throw new Error(message);
  }
  return response;
}

export class DebugCliSession {
  constructor({
    binaryPath,
    spawnImpl = spawn,
    requestTimeoutMs = DEBUG_CLI_SESSION_BUDGET.requestTimeoutMs,
    closeWaitTimeoutMs = DEBUG_CLI_SESSION_BUDGET.closeWaitTimeoutMs,
  }) {
    this.binaryPath = binaryPath;
    this.requestTimeoutMs = requestTimeoutMs;
    this.closeWaitTimeoutMs = closeWaitTimeoutMs;
    this.pending = [];
    this.unavailableError = null;
    this.closePromise = null;
    this.child = spawnImpl(binaryPath, ['--debug-cli'], {
      cwd: path.dirname(binaryPath),
      stdio: ['pipe', 'pipe', 'pipe'],
    });
    if (typeof this.child.stdin.on === 'function') {
      this.child.stdin.on('error', () => {});
    }
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
    this.child.on('exit', (code, signal) => {
      this.teardown(this.buildExitError('exited', code, signal));
    });
    this.child.on('close', (code, signal) => {
      this.teardown(this.buildExitError('closed', code, signal));
    });
  }

  buildExitError(reason, code, signal) {
    if (signal) {
      return new Error(`debug cli ${reason} with signal ${signal}`);
    }
    if (code == null) {
      return new Error(`debug cli ${reason}`);
    }
    return new Error(`debug cli ${reason} with code ${code}`);
  }

  teardown(error) {
    if (this.unavailableError) {
      return;
    }
    this.unavailableError = error;
    while (this.pending.length > 0) {
      const pending = this.pending.shift();
      if (pending.timeout) {
        clearTimeout(pending.timeout);
      }
      pending.reject(error);
    }
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
      if (pending.timeout) {
        clearTimeout(pending.timeout);
      }
      try {
        pending.resolve(JSON.parse(line));
      } catch (error) {
        pending.reject(new Error(`debug cli returned invalid JSON: ${error.message}`));
      }
    }
  }

  send(command, { timeoutMs = this.requestTimeoutMs } = {}) {
    if (this.unavailableError) {
      return Promise.reject(this.unavailableError);
    }

    return new Promise((resolve, reject) => {
      const commandName = command?.cmd ?? 'request';
      const pending = {
        resolve,
        reject,
        timeout: null,
      };
      if (Number.isFinite(timeoutMs) && timeoutMs > 0) {
        pending.timeout = setTimeout(() => {
          const index = this.pending.indexOf(pending);
          if (index >= 0) {
            this.pending.splice(index, 1);
          }
          pending.reject(new Error(`debug cli ${commandName} timed out after ${timeoutMs}ms`));
        }, timeoutMs);
      }

      this.pending.push(pending);
      try {
        this.child.stdin.write(`${JSON.stringify(command)}\n`);
      } catch (error) {
        if (pending.timeout) {
          clearTimeout(pending.timeout);
        }
        const index = this.pending.indexOf(pending);
        if (index >= 0) {
          this.pending.splice(index, 1);
        }
        reject(new Error(`debug cli write failed: ${error.message}`));
      }
    });
  }

  async request(command, options) {
    return normalizeCliResponse(await this.send(command, options), `debug cli ${command.cmd}`);
  }

  async load(testEntry, backend) {
    return this.request({
      cmd: 'load',
      image: testEntry.image,
      disk: testEntry.disk ?? undefined,
      disk_ready: testEntry.diskReady ?? true,
      disk_magic_valid: testEntry.diskMagicValid ?? true,
      backend,
    });
  }

  async snapshot() {
    return this.request({ cmd: 'snapshot' });
  }

  async runUntilUartContains(text, maxSteps) {
    return this.request({
      cmd: 'run_until_uart_contains',
      text,
      max_steps: maxSteps,
    });
  }

  async stepCycle() {
    return this.request({ cmd: 'step_cycle' });
  }

  async stepCommit() {
    return this.request({ cmd: 'step_commit' });
  }

  async reset() {
    return this.request({ cmd: 'reset' });
  }

  async uartInput(text) {
    return this.request({ cmd: 'uart_input', text });
  }

  async uartOutput(offset = 0) {
    const response = await this.request({ cmd: 'uart_output', offset });
    return {
      text: response.text ?? '',
      nextOffset: response.next_offset ?? offset,
    };
  }

  async close() {
    if (this.closePromise) {
      return this.closePromise;
    }

    this.closePromise = (async () => {
      this.teardown(new Error('debug cli session closed'));
      if (!this.child.stdin.destroyed && this.child.stdin.writable) {
        try {
          this.child.stdin.write(`${JSON.stringify({ cmd: 'quit' })}\n`);
        } catch {
          // Ignore shutdown races.
        }
      }
      this.child.kill();

      await new Promise((resolve) => {
        let settled = false;
        const finish = () => {
          if (settled) {
            return;
          }
          settled = true;
          resolve();
        };
        const timer = setTimeout(finish, this.closeWaitTimeoutMs);
        const onCloseOrExit = () => {
          clearTimeout(timer);
          finish();
        };
        this.child.once('close', onCloseOrExit);
        this.child.once('exit', onCloseOrExit);
      });
    })();

    await this.closePromise;
  }
}
