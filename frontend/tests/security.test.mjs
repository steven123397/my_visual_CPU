import test from 'node:test';
import assert from 'node:assert/strict';

import {
  buildPasswordHashForTests,
  createSecurityManager,
  verifyPassword,
} from '../server/security.mjs';

function makeRequest({
  cookie = '',
  host = '127.0.0.1:4173',
  forwardedFor = '',
  remoteAddress = '127.0.0.1',
} = {}) {
  return {
    headers: {
      host,
      cookie,
      'x-forwarded-for': forwardedFor,
    },
    socket: { remoteAddress },
  };
}

test('verifyPassword accepts the matching scrypt hash and rejects mismatches', () => {
  const passwordHash = buildPasswordHashForTests('secret-1');
  assert.equal(verifyPassword('secret-1', passwordHash), true);
  assert.equal(verifyPassword('secret-2', passwordHash), false);
});

test('security manager enforces login before API access when enabled', async () => {
  const manager = createSecurityManager({
    enabled: true,
    users: [{ username: 'admin', passwordHash: buildPasswordHashForTests('pw') }],
  });

  assert.throws(
    () => manager.requireAuthentication(makeRequest(), 'GET:/api/tests'),
    /authentication required/,
  );

  const login = await manager.login(makeRequest(), { username: 'admin', password: 'pw' });
  assert.equal(login.auth.authenticated, true);
  const setCookie = login.headers['set-cookie'];
  assert.match(setCookie, /mycpu_session=/);

  const request = makeRequest({ cookie: setCookie });
  const session = manager.requireAuthentication(request, 'GET:/api/tests');
  assert.equal(session.username, 'admin');
  assert.equal(manager.authStateForRequest(request).controllerSession, false);
});

test('security manager limits concurrent authenticated sessions to three', async () => {
  const manager = createSecurityManager({
    enabled: true,
    users: [{ username: 'admin', passwordHash: buildPasswordHashForTests('pw') }],
    maxActiveSessions: 3,
  });

  await manager.login(makeRequest({ remoteAddress: '10.0.0.1' }), { username: 'admin', password: 'pw' });
  await manager.login(makeRequest({ remoteAddress: '10.0.0.2' }), { username: 'admin', password: 'pw' });
  await manager.login(makeRequest({ remoteAddress: '10.0.0.3' }), { username: 'admin', password: 'pw' });

  await assert.rejects(
    () => manager.login(makeRequest({ remoteAddress: '10.0.0.4' }), { username: 'admin', password: 'pw' }),
    /concurrent session limit reached \(3\)/,
  );
});

test('security manager grants a single controller and blocks competing writers', async () => {
  const manager = createSecurityManager({
    enabled: true,
    users: [{ username: 'admin', passwordHash: buildPasswordHashForTests('pw') }],
  });

  const first = await manager.login(makeRequest({ remoteAddress: '10.0.0.1' }), { username: 'admin', password: 'pw' });
  const second = await manager.login(makeRequest({ remoteAddress: '10.0.0.2' }), { username: 'admin', password: 'pw' });

  const firstRequest = makeRequest({ cookie: first.headers['set-cookie'], remoteAddress: '10.0.0.1' });
  const secondRequest = makeRequest({ cookie: second.headers['set-cookie'], remoteAddress: '10.0.0.2' });

  const firstSession = manager.requireController(firstRequest, 'load');
  assert.equal(firstSession.username, 'admin');
  assert.equal(manager.authStateForRequest(firstRequest).controllerSession, true);

  assert.throws(
    () => manager.requireController(secondRequest, 'load'),
    /controller locked by admin/,
  );

  await manager.releaseController(firstRequest);
  const secondSession = manager.requireController(secondRequest, 'run');
  assert.equal(secondSession.username, 'admin');
  assert.equal(manager.authStateForRequest(secondRequest).controllerSession, true);
});

test('security manager returns a clearing cookie on logout', async () => {
  const manager = createSecurityManager({
    enabled: true,
    users: [{ username: 'admin', passwordHash: buildPasswordHashForTests('pw') }],
  });

  const login = await manager.login(makeRequest(), { username: 'admin', password: 'pw' });
  const logout = await manager.logout(makeRequest({ cookie: login.headers['set-cookie'] }));
  assert.equal(logout.auth.authenticated, false);
  assert.match(logout.headers['set-cookie'], /Max-Age=0/);
});
