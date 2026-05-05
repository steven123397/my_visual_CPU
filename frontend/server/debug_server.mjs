import fs from 'node:fs/promises';
import http from 'node:http';
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
    json(response, statusCode, { error: error.message }, headers);
  }
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
        const entry = tests.find((item) => item.name === body.test);
        if (!entry) {
          json(response, 404, { error: `unknown test: ${body.test}` }, securityHeaders);
          return;
        }
        await respondWithAction(response, () => runtime.load(entry, body.backend ?? 'pipeline'), securityHeaders);
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
