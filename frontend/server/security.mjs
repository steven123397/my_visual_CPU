import crypto from 'node:crypto';
import fs from 'node:fs/promises';

class SecurityError extends Error {
  constructor(message, statusCode = 400) {
    super(message);
    this.name = 'SecurityError';
    this.statusCode = statusCode;
  }
}

const DEFAULT_SESSION_TTL_MS = 12 * 60 * 60 * 1000;
const DEFAULT_SESSION_LIMIT = 3;
const DEFAULT_COOKIE_NAME = 'mycpu_session';
const DEFAULT_AUDIT_LOG_PATH = process.env.MYCPU_AUDIT_LOG_PATH ?? null;

function nowMs() {
  return Date.now();
}

function parseCookies(header = '') {
  const cookies = {};
  for (const part of String(header).split(';')) {
    const index = part.indexOf('=');
    if (index === -1) {
      continue;
    }
    const key = part.slice(0, index).trim();
    const value = part.slice(index + 1).trim();
    if (!key) {
      continue;
    }
    cookies[key] = decodeURIComponent(value);
  }
  return cookies;
}

function formatCookie({
  name,
  value,
  maxAge = null,
  expires = null,
  httpOnly = true,
  secure = true,
  sameSite = 'Lax',
  path = '/',
}) {
  const parts = [`${name}=${encodeURIComponent(value)}`];
  if (path) {
    parts.push(`Path=${path}`);
  }
  if (maxAge != null) {
    parts.push(`Max-Age=${maxAge}`);
  }
  if (expires instanceof Date) {
    parts.push(`Expires=${expires.toUTCString()}`);
  }
  if (httpOnly) {
    parts.push('HttpOnly');
  }
  if (secure) {
    parts.push('Secure');
  }
  if (sameSite) {
    parts.push(`SameSite=${sameSite}`);
  }
  return parts.join('; ');
}

function firstForwardedIp(value = '') {
  return String(value)
    .split(',')[0]
    .trim();
}

export function clientIpForRequest(request) {
  const forwarded = firstForwardedIp(request.headers['x-forwarded-for']);
  if (forwarded) {
    return forwarded;
  }
  return request.socket?.remoteAddress ?? 'unknown';
}

async function appendAuditLine(path, payload) {
  if (!path) {
    return;
  }
  await fs.appendFile(path, `${JSON.stringify(payload)}\n`, 'utf8');
}

export function buildPasswordHashForTests(password, {
  salt = crypto.randomBytes(16),
  N = 16384,
  r = 8,
  p = 1,
} = {}) {
  const hash = crypto.scryptSync(password, salt, 64, { N, r, p });
  return `scrypt$${N}$${r}$${p}$${salt.toString('base64url')}$${hash.toString('base64url')}`;
}

export function verifyPassword(password, encodedHash) {
  if (typeof encodedHash !== 'string' || !encodedHash.startsWith('scrypt$')) {
    return false;
  }
  const parts = encodedHash.split('$');
  if (parts.length !== 6) {
    return false;
  }
  const [, NText, rText, pText, saltText, digestText] = parts;
  const N = Number(NText);
  const r = Number(rText);
  const p = Number(pText);
  if (!Number.isInteger(N) || !Number.isInteger(r) || !Number.isInteger(p)) {
    return false;
  }
  const salt = Buffer.from(saltText, 'base64url');
  const expected = Buffer.from(digestText, 'base64url');
  const actual = crypto.scryptSync(password, salt, expected.length, { N, r, p });
  if (actual.length !== expected.length) {
    return false;
  }
  return crypto.timingSafeEqual(actual, expected);
}

function parseBooleanEnv(value, fallback = false) {
  if (value == null) {
    return fallback;
  }
  return value === '1' || value === 'true' || value === 'yes';
}

function parseIntegerEnv(value, fallback) {
  const parsed = Number(value);
  if (!Number.isInteger(parsed) || parsed <= 0) {
    return fallback;
  }
  return parsed;
}

function cleanupSlidingWindow(bucket, cutoff) {
  while (bucket.length > 0 && bucket[0] <= cutoff) {
    bucket.shift();
  }
}

class SlidingWindowRateLimiter {
  constructor({ now = nowMs } = {}) {
    this.now = now;
    this.windows = new Map();
  }

  consume(key, { limit, windowMs }) {
    const now = this.now();
    const bucket = this.windows.get(key) ?? [];
    cleanupSlidingWindow(bucket, now - windowMs);
    if (bucket.length >= limit) {
      throw new SecurityError('rate limit exceeded', 429);
    }
    bucket.push(now);
    this.windows.set(key, bucket);
    return {
      limit,
      remaining: Math.max(0, limit - bucket.length),
    };
  }
}

export function createSecurityManager({
  enabled = false,
  users = [],
  maxActiveSessions = DEFAULT_SESSION_LIMIT,
  sessionTtlMs = DEFAULT_SESSION_TTL_MS,
  cookieName = DEFAULT_COOKIE_NAME,
  secureCookies = true,
  auditLogPath = DEFAULT_AUDIT_LOG_PATH,
  now = nowMs,
} = {}) {
  const sessionStore = new Map();
  const usersByName = new Map();
  const rateLimiter = new SlidingWindowRateLimiter({ now });
  let controllerSessionId = null;

  for (const user of users) {
    if (!user?.username || !user?.passwordHash) {
      continue;
    }
    usersByName.set(user.username, {
      username: user.username,
      passwordHash: user.passwordHash,
      role: user.role ?? 'operator',
    });
  }

  function audit(event, fields = {}) {
    return appendAuditLine(auditLogPath, {
      ts: new Date(now()).toISOString(),
      event,
      ...fields,
    }).catch(() => {});
  }

  function cleanupExpiredSessions() {
    if (!enabled) {
      return;
    }
    const expirationCutoff = now() - sessionTtlMs;
    for (const [sessionId, session] of sessionStore) {
      if (session.lastSeenAt < expirationCutoff) {
        sessionStore.delete(sessionId);
        if (controllerSessionId === sessionId) {
          controllerSessionId = null;
        }
      }
    }
  }

  function currentController() {
    cleanupExpiredSessions();
    return controllerSessionId ? sessionStore.get(controllerSessionId) ?? null : null;
  }

  function authStateFor(session = null) {
    const controller = currentController();
    return {
      required: enabled,
      authenticated: Boolean(session),
      username: session?.username ?? null,
      role: session?.role ?? null,
      activeSessions: enabled ? sessionStore.size : 0,
      sessionLimit: enabled ? maxActiveSessions : 0,
      controllerUsername: controller?.username ?? null,
      controllerSession: Boolean(session && controller?.id === session.id),
      canControl: !enabled
        || Boolean(session && (!controller || controller.id === session.id)),
    };
  }

  function cookieHeadersForSession(sessionId) {
    const expires = new Date(now() + sessionTtlMs);
    return {
      'set-cookie': formatCookie({
        name: cookieName,
        value: sessionId,
        maxAge: Math.floor(sessionTtlMs / 1000),
        expires,
        secure: secureCookies,
      }),
    };
  }

  function clearCookieHeaders() {
    return {
      'set-cookie': formatCookie({
        name: cookieName,
        value: '',
        maxAge: 0,
        expires: new Date(0),
        secure: secureCookies,
      }),
    };
  }

  function requestSession(request) {
    if (!enabled) {
      return null;
    }
    cleanupExpiredSessions();
    const cookies = parseCookies(request.headers.cookie);
    const sessionId = cookies[cookieName];
    if (!sessionId) {
      return null;
    }
    const session = sessionStore.get(sessionId) ?? null;
    if (!session) {
      return null;
    }
    session.lastSeenAt = now();
    return session;
  }

  function requireAuthentication(request) {
    const session = requestSession(request);
    if (!enabled) {
      return null;
    }
    if (!session) {
      throw new SecurityError('authentication required', 401);
    }
    return session;
  }

  function enforceReadRateLimit(request, routeKey, session) {
    if (!enabled) {
      return;
    }
    const ip = clientIpForRequest(request);
    const actor = session?.id ?? ip;
    rateLimiter.consume(`read:${routeKey}:${actor}`, { limit: 240, windowMs: 60_000 });
  }

  function enforceWriteRateLimit(request, routeKey, session) {
    if (!enabled) {
      return;
    }
    const ip = clientIpForRequest(request);
    const actor = session?.id ?? ip;
    const routeLimit = routeKey === 'terminal-input' ? 180 : routeKey === 'load' ? 20 : 60;
    rateLimiter.consume(`write:${routeKey}:${actor}`, { limit: routeLimit, windowMs: 60_000 });
  }

  function requireController(request, routeKey) {
    if (!enabled) {
      return null;
    }
    const session = requireAuthentication(request);
    enforceWriteRateLimit(request, routeKey, session);
    const controller = currentController();
    if (!controller) {
      controllerSessionId = session.id;
      audit('controller_claim', {
        username: session.username,
        sessionId: session.id,
        ip: clientIpForRequest(request),
        routeKey,
      });
      return session;
    }
    if (controller.id !== session.id) {
      throw new SecurityError(`controller locked by ${controller.username}`, 409);
    }
    return session;
  }

  return {
    enabled,

    authStateForRequest(request) {
      return authStateFor(requestSession(request));
    },

    requireAuthentication(request, routeKey = 'api') {
      const session = requireAuthentication(request);
      enforceReadRateLimit(request, routeKey, session);
      return session;
    },

    requireController(request, routeKey = 'control') {
      return requireController(request, routeKey);
    },

    assertWebSocketAllowed(request) {
      if (!enabled) {
        return;
      }
      const session = requireAuthentication(request);
      const ip = clientIpForRequest(request);
      rateLimiter.consume(`ws:${session.id}:${ip}`, { limit: 20, windowMs: 10 * 60_000 });
    },

    async login(request, { username, password }) {
      if (!enabled) {
        return {
          headers: {},
          auth: authStateFor(null),
        };
      }
      const ip = clientIpForRequest(request);
      rateLimiter.consume(`login:${ip}`, { limit: 8, windowMs: 10 * 60_000 });
      cleanupExpiredSessions();
      const user = usersByName.get(String(username ?? '').trim());
      if (!user || !verifyPassword(password ?? '', user.passwordHash)) {
        audit('login_failed', {
          username: String(username ?? '').trim(),
          ip,
        });
        throw new SecurityError('invalid username or password', 401);
      }
      if (sessionStore.size >= maxActiveSessions) {
        throw new SecurityError(`concurrent session limit reached (${maxActiveSessions})`, 429);
      }
      const sessionId = crypto.randomBytes(24).toString('base64url');
      const session = {
        id: sessionId,
        username: user.username,
        role: user.role,
        createdAt: now(),
        lastSeenAt: now(),
      };
      sessionStore.set(sessionId, session);
      audit('login_success', {
        username: user.username,
        sessionId,
        ip,
      });
      return {
        headers: cookieHeadersForSession(sessionId),
        auth: authStateFor(session),
      };
    },

    async logout(request) {
      if (!enabled) {
        return {
          headers: {},
          auth: authStateFor(null),
        };
      }
      const session = requestSession(request);
      if (!session) {
        return {
          headers: clearCookieHeaders(),
          auth: authStateFor(null),
        };
      }
      sessionStore.delete(session.id);
      if (controllerSessionId === session.id) {
        controllerSessionId = null;
      }
      audit('logout', {
        username: session.username,
        sessionId: session.id,
        ip: clientIpForRequest(request),
      });
      return {
        headers: clearCookieHeaders(),
        auth: authStateFor(null),
      };
    },

    async releaseController(request) {
      if (!enabled) {
        return { auth: authStateFor(null) };
      }
      const session = requireAuthentication(request);
      const controller = currentController();
      if (!controller) {
        return { auth: authStateFor(session) };
      }
      if (controller.id !== session.id) {
        throw new SecurityError(`controller locked by ${controller.username}`, 409);
      }
      controllerSessionId = null;
      audit('controller_release', {
        username: session.username,
        sessionId: session.id,
        ip: clientIpForRequest(request),
      });
      return { auth: authStateFor(session) };
    },

    commonSecurityHeaders() {
      return {
        'x-frame-options': 'DENY',
        'x-content-type-options': 'nosniff',
        'referrer-policy': 'same-origin',
        'cache-control': 'no-store',
      };
    },
  };
}

export function createSecurityManagerFromEnv(env = process.env) {
  const enabled = parseBooleanEnv(env.MYCPU_AUTH_ENABLED, false);
  const publicUnauthOk = parseBooleanEnv(env.MYCPU_PUBLIC_UNAUTH_OK, false);
  const productionLike = env.NODE_ENV === 'production';
  if (!enabled) {
    if (productionLike && !publicUnauthOk) {
      throw new Error('public frontend deployment requires MYCPU_AUTH_ENABLED=1 or MYCPU_PUBLIC_UNAUTH_OK=1');
    }
    return createSecurityManager({ enabled: false });
  }
  const username = String(env.MYCPU_AUTH_ADMIN_USERNAME ?? '').trim();
  const passwordHash = String(env.MYCPU_AUTH_ADMIN_PASSWORD_HASH ?? '').trim();
  if (!username || !passwordHash) {
    throw new Error('MYCPU_AUTH_ENABLED=1 requires MYCPU_AUTH_ADMIN_USERNAME and MYCPU_AUTH_ADMIN_PASSWORD_HASH');
  }
  return createSecurityManager({
    enabled: true,
    users: [{ username, passwordHash, role: 'admin' }],
    maxActiveSessions: parseIntegerEnv(env.MYCPU_AUTH_SESSION_LIMIT, DEFAULT_SESSION_LIMIT),
    sessionTtlMs: parseIntegerEnv(env.MYCPU_AUTH_SESSION_TTL_MS, DEFAULT_SESSION_TTL_MS),
    secureCookies: parseBooleanEnv(env.MYCPU_AUTH_SECURE_COOKIES, true),
    auditLogPath: env.MYCPU_AUDIT_LOG_PATH ?? DEFAULT_AUDIT_LOG_PATH,
  });
}
