import fs from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { DebugCliSession } from './debug_cli_session.mjs';
import { createDebugServerRuntime } from './debug_server_runtime.mjs';
import { createAiTinyModelService } from './ai_tiny_model_service.mjs';
import { listTests } from './tests_manifest.mjs';
import { createSecurityManagerFromEnv } from './security.mjs';
import { createWebSocketHub } from './ws.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..', '..');
const frontendRoot = path.join(repoRoot, 'frontend');
const appRoot = path.join(repoRoot, 'frontend', 'app');
const sharedRoot = path.join(repoRoot, 'frontend', 'shared');
const docsRoot = path.join(repoRoot, 'docs');
const showcaseRoot = path.join(repoRoot, 'docs', 'showcase');
const customElfRootsEnv = 'MYCPU_FRONTEND_CUSTOM_ELF_ROOTS';
const customElfMaxBytesEnv = 'MYCPU_FRONTEND_CUSTOM_ELF_MAX_BYTES';
const defaultCustomElfMaxBytes = 16 * 1024 * 1024;

class HttpError extends Error {
  constructor(statusCode, code, message) {
    super(message);
    this.name = 'HttpError';
    this.statusCode = statusCode;
    this.code = code;
  }
}

function httpError(statusCode, code, message) {
  return new HttpError(statusCode, code, message);
}

function json(response, statusCode, payload, headers = {}) {
  response.writeHead(statusCode, {
    'content-type': 'application/json; charset=utf-8',
    ...headers,
  });
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
  if (filePath.endsWith('.js') || filePath.endsWith('.mjs')) {
    return 'text/javascript; charset=utf-8';
  }
  if (filePath.endsWith('.css')) {
    return 'text/css; charset=utf-8';
  }
  if (filePath.endsWith('.json')) {
    return 'application/json; charset=utf-8';
  }
  if (filePath.endsWith('.png')) {
    return 'image/png';
  }
  if (filePath.endsWith('.md')) {
    return 'text/markdown; charset=utf-8';
  }
  return 'text/plain; charset=utf-8';
}

function staticRouteFor(pathname) {
  if (pathname === '/') {
    return '/home.html';
  }
  if (pathname === '/console') {
    return '/index.html';
  }
  if (pathname === '/docs') {
    return '/docs.html';
  }
  return pathname;
}

async function serveStatic(response, pathname, headers = {}) {
  const relative = staticRouteFor(pathname);
  if (relative.startsWith('/source/docs/')) {
    await serveSourceDoc(response, relative);
    return;
  }
  if (relative.startsWith('/source/showcase/')) {
    await serveShowcaseAsset(response, relative);
    return;
  }
  const root = relative.startsWith('/shared/') ? sharedRoot : appRoot;
  const trimmed = relative.startsWith('/shared/')
    ? relative.replace(/^\/shared\/+/, '')
    : relative.replace(/^\/+/, '');
  const filePath = path.join(root, trimmed);
  if (!filePath.startsWith(root) || !filePath.startsWith(frontendRoot)) {
    json(response, 403, { error: 'forbidden' });
    return;
  }
  try {
    const content = await fs.readFile(filePath);
    response.writeHead(200, { 'content-type': contentTypeFor(filePath), ...headers });
    response.end(content);
  } catch {
    json(response, 404, { error: 'not found' }, headers);
  }
}

async function serveSourceDoc(response, pathname, headers = {}) {
  const trimmed = pathname.replace(/^\/source\/docs\/+/, '');
  const filePath = path.join(docsRoot, trimmed);
  if (!filePath.startsWith(docsRoot) || !filePath.endsWith('.md')) {
    json(response, 403, { error: 'forbidden' });
    return;
  }
  try {
    const content = await fs.readFile(filePath);
    response.writeHead(200, { 'content-type': contentTypeFor(filePath), ...headers });
    response.end(content);
  } catch {
    json(response, 404, { error: 'not found' }, headers);
  }
}

async function serveShowcaseAsset(response, pathname, headers = {}) {
  const trimmed = pathname.replace(/^\/source\/showcase\/+/, '');
  const filePath = path.join(showcaseRoot, trimmed);
  if (!filePath.startsWith(showcaseRoot) || !filePath.endsWith('.png')) {
    json(response, 403, { error: 'forbidden' });
    return;
  }
  try {
    const content = await fs.readFile(filePath);
    response.writeHead(200, { 'content-type': contentTypeFor(filePath), ...headers });
    response.end(content);
  } catch {
    json(response, 404, { error: 'not found' }, headers);
  }
}

async function respondWithAction(response, action, headers = {}) {
  try {
    json(response, 200, await action(), headers);
  } catch (error) {
    const statusCode = Number.isInteger(error?.statusCode) ? error.statusCode : 500;
    const payload = { error: error.message };
    if (typeof error?.code === 'string' && error.code.length > 0) {
      payload.code = error.code;
    }
    json(response, statusCode, payload, headers);
  }
}

function parseCustomElfMaxBytes() {
  const raw = process.env[customElfMaxBytesEnv];
  if (!raw) {
    return defaultCustomElfMaxBytes;
  }
  const parsed = Number(raw);
  return Number.isFinite(parsed) && parsed > 0
    ? Math.floor(parsed)
    : defaultCustomElfMaxBytes;
}

function customElfAllowedRoots() {
  const configured = (process.env[customElfRootsEnv] ?? '')
    .split(path.delimiter)
    .map((item) => item.trim())
    .filter(Boolean)
    .map((item) => path.resolve(item));
  return configured.length > 0 ? configured : [repoRoot];
}

function isInsideRoot(filePath, root) {
  const relative = path.relative(root, filePath);
  return relative === '' || (!relative.startsWith('..') && !path.isAbsolute(relative));
}

function sanitizeElfName(rawName, fallback = 'custom.elf') {
  const base = path.basename(String(rawName || fallback));
  const sanitized = base.replace(/[^A-Za-z0-9._-]/g, '_');
  return sanitized.length > 0 ? sanitized : fallback;
}

function customElfMetadata(source, name) {
  return {
    source,
    name,
    maxBytes: parseCustomElfMaxBytes(),
  };
}

function customElfEntry({ image, name, source }) {
  return {
    name: `custom_${source}_${name}`,
    menuLabel: `custom ELF · ${name}`,
    title: 'Custom ELF',
    badge: 'Local ELF',
    summary: 'Loads a user-provided local RISC-V ELF through the existing debug session path.',
    image,
    disk: null,
    diskReady: true,
    diskMagicValid: true,
    kind: 'custom',
    workload: {
      stage: 'PROJECT_EVOLUTION P0',
      category: 'custom-elf',
      expectedMarker: 'user ELF',
      ops: ['local server file', 'ELF loader', 'debug session'],
      pipelineNote: 'Custom ELF loading is local-server only and keeps the existing manifest-backed session contract intact.',
      assetNote: source === 'elfPath'
        ? `${customElfRootsEnv}=<allowed-local-root>`
        : 'base64 payload is written to a short-lived server temp file before load',
    },
    customElf: customElfMetadata(source, name),
  };
}

async function assertElfMagic(filePath) {
  let handle = null;
  try {
    handle = await fs.open(filePath, 'r');
    const buffer = Buffer.alloc(4);
    const { bytesRead } = await handle.read(buffer, 0, 4, 0);
    if (
      bytesRead < 4 ||
      buffer[0] !== 0x7f ||
      buffer[1] !== 0x45 ||
      buffer[2] !== 0x4c ||
      buffer[3] !== 0x46
    ) {
      throw httpError(400, 'custom_elf_invalid_magic', 'custom ELF payload is not an ELF file');
    }
  } finally {
    await handle?.close();
  }
}

function assertElfBufferMagic(buffer) {
  if (
    buffer.length < 4 ||
    buffer[0] !== 0x7f ||
    buffer[1] !== 0x45 ||
    buffer[2] !== 0x4c ||
    buffer[3] !== 0x46
  ) {
    throw httpError(400, 'custom_elf_invalid_magic', 'custom ELF payload is not an ELF file');
  }
}

async function resolveCustomElfPath(rawPath) {
  if (typeof rawPath !== 'string' || rawPath.length === 0) {
    throw httpError(400, 'custom_elf_path_required', 'custom ELF path is required');
  }
  const resolved = path.resolve(rawPath);
  if (!customElfAllowedRoots().some((root) => isInsideRoot(resolved, root))) {
    throw httpError(403, 'custom_elf_path_forbidden', 'custom ELF path is outside the allowed roots');
  }

  let stat = null;
  try {
    stat = await fs.stat(resolved);
  } catch {
    throw httpError(404, 'custom_elf_not_found', 'custom ELF file was not found');
  }
  if (!stat.isFile()) {
    throw httpError(400, 'custom_elf_not_file', 'custom ELF path is not a file');
  }
  if (stat.size > parseCustomElfMaxBytes()) {
    throw httpError(413, 'custom_elf_too_large', 'custom ELF payload exceeds the configured size limit');
  }
  try {
    await fs.access(resolved);
  } catch {
    throw httpError(403, 'custom_elf_not_readable', 'custom ELF file is not readable');
  }
  await assertElfMagic(resolved);
  const name = sanitizeElfName(path.basename(resolved));
  return {
    entry: customElfEntry({ image: resolved, name, source: 'elfPath' }),
    cleanup: null,
  };
}

async function resolveCustomElfBase64(body) {
  const raw = body?.elfBase64;
  if (typeof raw !== 'string' || raw.length === 0) {
    throw httpError(400, 'custom_elf_base64_required', 'custom ELF base64 payload is required');
  }
  const buffer = Buffer.from(raw, 'base64');
  assertElfBufferMagic(buffer);
  if (buffer.length > parseCustomElfMaxBytes()) {
    throw httpError(413, 'custom_elf_too_large', 'custom ELF payload exceeds the configured size limit');
  }

  const tempRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'mycpu-custom-elf-'));
  const name = sanitizeElfName(body.elfName, 'custom.elf');
  const image = path.join(tempRoot, name);
  await fs.writeFile(image, buffer, { mode: 0o600 });
  return {
    entry: customElfEntry({ image, name, source: 'elfBase64' }),
    cleanup: async () => fs.rm(tempRoot, { recursive: true, force: true }),
  };
}

async function resolveSessionLoadEntry(body, tests) {
  if (typeof body?.elfPath === 'string' && body.elfPath.length > 0) {
    return resolveCustomElfPath(body.elfPath);
  }
  if (typeof body?.elfBase64 === 'string' && body.elfBase64.length > 0) {
    return resolveCustomElfBase64(body);
  }

  const testName = body?.test ?? body?.name;
  const entry = tests.find((item) => item.name === testName);
  if (!entry) {
    throw httpError(404, 'unknown_test', `unknown test: ${testName}`);
  }
  return {
    entry,
    cleanup: null,
  };
}

export async function startServer({
  host = '127.0.0.1',
  port = 4173,
  createSession = async () => new DebugCliSession({ binaryPath: path.join(repoRoot, 'myCPU', 'mycpu') }),
  aiTinyModelService = createAiTinyModelService({
    repoRoot,
    binaryPath: path.join(repoRoot, 'myCPU', 'mycpu'),
  }),
} = {}) {
  const tests = listTests(repoRoot);
  const diagnostics = tests.diagnostics ?? {};
  const security = createSecurityManagerFromEnv(process.env);

  const server = http.createServer(async (request, response) => {
    try {
      const url = new URL(request.url, `http://${request.headers.host}`);
      const securityHeaders = security.commonSecurityHeaders();
      const routeKey = `${request.method}:${url.pathname}`;
      if (request.method === 'GET' && url.pathname === '/api/auth/session') {
        json(response, 200, { auth: security.authStateForRequest(request) }, securityHeaders);
        return;
      }
      if (request.method === 'POST' && url.pathname === '/api/auth/login') {
        const body = await readBody(request);
        const result = await security.login(request, body);
        json(response, 200, { auth: result.auth }, {
          ...securityHeaders,
          ...result.headers,
        });
        return;
      }
      if (request.method === 'POST' && url.pathname === '/api/auth/logout') {
        const result = await security.logout(request);
        json(response, 200, { auth: result.auth }, {
          ...securityHeaders,
          ...result.headers,
        });
        return;
      }
      if (request.method === 'POST' && url.pathname === '/api/auth/release-control') {
        await security.requireAuthentication(request, routeKey);
        await respondWithAction(response, () => security.releaseController(request), securityHeaders);
        return;
      }

      if (request.method === 'GET' && url.pathname === '/api/tests') {
        security.requireAuthentication(request, routeKey);
        json(response, 200, {
          tests: tests.map(({
            name,
            disk,
            kind,
            menuLabel,
            backend,
            bootUntilUartText,
            terminalPrompt,
            commandUntilUartText,
            title,
            badge,
            summary,
            workload,
          }) => ({
            name,
            hasDisk: Boolean(disk),
            kind,
            menuLabel,
            backend,
            bootUntilUartText,
            terminalPrompt,
            commandUntilUartText,
            title,
            badge,
            summary,
            workload,
          })),
          diagnostics,
          auth: security.authStateForRequest(request),
        }, securityHeaders);
        return;
      }

      if (request.method === 'GET' && url.pathname === '/api/ai/tiny-model/templates') {
        security.requireAuthentication(request, routeKey);
        await respondWithAction(response, () => aiTinyModelService.templates(), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/ai/tiny-model/run') {
        security.requireController(request, 'ai-tiny-model-run');
        const body = await readBody(request);
        await respondWithAction(response, () => aiTinyModelService.run(body), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/load') {
        security.requireController(request, 'load');
        const body = await readBody(request);
        await respondWithAction(response, async () => {
          const { entry, cleanup } = await resolveSessionLoadEntry(body, tests);
          try {
            const result = await runtime.load(entry, body.backend ?? entry.backend ?? 'pipeline');
            return entry.customElf
              ? { ...result, customElf: entry.customElf }
              : result;
          } finally {
            await cleanup?.();
          }
        }, securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/snapshot') {
        security.requireAuthentication(request, routeKey);
        await respondWithAction(response, () => runtime.snapshot(), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/step-cycle') {
        security.requireController(request, 'step-cycle');
        await respondWithAction(response, () => runtime.stepCycle(), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/step-commit') {
        security.requireController(request, 'step-commit');
        await respondWithAction(response, () => runtime.stepCommit(), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/debug-command') {
        security.requireController(request, 'debug-command');
        const body = await readBody(request);
        await respondWithAction(response, () => runtime.debugCommand(body), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/reset') {
        security.requireController(request, 'reset');
        await respondWithAction(response, () => runtime.reset(), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/terminate') {
        security.requireController(request, 'terminate');
        await respondWithAction(response, () => runtime.terminate(), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/run') {
        security.requireController(request, 'run');
        const body = await readBody(request);
        await respondWithAction(response, () => runtime.run(body.rateHz ?? 8), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/terminal-input') {
        security.requireController(request, 'terminal-input');
        const body = await readBody(request);
        await respondWithAction(response, () => runtime.terminalInput(body.text ?? ''), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/terminal-output') {
        security.requireAuthentication(request, routeKey);
        const body = await readBody(request);
        await respondWithAction(response, () => runtime.terminalOutput(body.offset ?? 0), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/jit-dispatch') {
        security.requireAuthentication(request, routeKey);
        await respondWithAction(response, () => runtime.jitDispatch(), securityHeaders);
        return;
      }

      if (request.method === 'POST' && url.pathname === '/api/session/pause') {
        security.requireController(request, 'pause');
        await respondWithAction(response, () => runtime.pause(), securityHeaders);
        return;
      }

      await serveStatic(response, url.pathname, securityHeaders);
    } catch (error) {
      const statusCode = Number.isInteger(error?.statusCode) ? error.statusCode : 500;
      json(response, statusCode, { error: error.message }, security.commonSecurityHeaders());
    }
  });

  const wsHub = createWebSocketHub(server);
  server.__mycpuWebSocketGuard = (request) => security.assertWebSocketAllowed(request);
  const runtime = createDebugServerRuntime({ createSession, wsHub });

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
      await runtime.close();
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
