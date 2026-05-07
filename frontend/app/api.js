async function postJson(pathname, payload = {}) {
  const response = await fetch(pathname, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(payload),
    credentials: 'same-origin',
  });
  if (!response.ok) {
    const body = await response.json().catch(() => ({}));
    throw new Error(body.error ?? `${pathname} failed`);
  }
  return response.json();
}

export async function listTests() {
  const response = await fetch('/api/tests', { credentials: 'same-origin' });
  if (!response.ok) {
    throw new Error('failed to list tests');
  }
  return response.json();
}

export async function listAiTinyModelTemplates() {
  const response = await fetch('/api/ai/tiny-model/templates', { credentials: 'same-origin' });
  if (!response.ok) {
    throw new Error('failed to list AI tiny model templates');
  }
  return response.json();
}

export async function getAuthSession() {
  const response = await fetch('/api/auth/session', { credentials: 'same-origin' });
  if (!response.ok) {
    throw new Error('failed to fetch auth session');
  }
  return response.json();
}

export async function login(username, password) {
  return postJson('/api/auth/login', { username, password });
}

export async function logout() {
  return postJson('/api/auth/logout');
}

export async function releaseControl() {
  return postJson('/api/auth/release-control');
}

export async function runAiTinyModel(parameters) {
  return postJson('/api/ai/tiny-model/run', parameters);
}

export async function loadSession(test, backend) {
  return postJson('/api/session/load', { test, backend });
}

export async function stepCycle() {
  return postJson('/api/session/step-cycle');
}

export async function stepCommit() {
  return postJson('/api/session/step-commit');
}

export async function resetSession() {
  return postJson('/api/session/reset');
}

export async function terminateSession() {
  return postJson('/api/session/terminate');
}

export async function runSession(rateHz = 8) {
  return postJson('/api/session/run', { rateHz });
}

export async function pauseSession() {
  return postJson('/api/session/pause');
}

export async function jitDispatch() {
  return postJson('/api/session/jit-dispatch');
}

export async function terminalInput(text) {
  return postJson('/api/session/terminal-input', { text });
}

export async function terminalOutput(offset = 0) {
  return postJson('/api/session/terminal-output', { offset });
}

export function connectSnapshotSocket(onSnapshot, onError, onTerminal) {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const socket = new WebSocket(`${protocol}//${window.location.host}/ws`);
  socket.addEventListener('message', (event) => {
    const payload = JSON.parse(event.data);
    if (payload.type === 'snapshot' && payload.snapshot) {
      onSnapshot(payload.snapshot);
      return;
    }
    if (payload.type === 'terminal' && onTerminal) {
      onTerminal(payload);
      return;
    }
    if (payload.type === 'error' && onError) {
      onError(payload.message ?? 'socket error');
    }
  });
  return socket;
}
