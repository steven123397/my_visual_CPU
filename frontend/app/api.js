async function postJson(pathname, payload = {}) {
  const response = await fetch(pathname, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(payload),
  });
  if (!response.ok) {
    const body = await response.json().catch(() => ({}));
    throw new Error(body.error ?? `${pathname} failed`);
  }
  return response.json();
}

export async function listTests() {
  const response = await fetch('/api/tests');
  if (!response.ok) {
    throw new Error('failed to list tests');
  }
  return response.json();
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

export async function runSession(rateHz = 8) {
  return postJson('/api/session/run', { rateHz });
}

export async function pauseSession() {
  return postJson('/api/session/pause');
}

export function connectSnapshotSocket(onSnapshot, onError) {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const socket = new WebSocket(`${protocol}//${window.location.host}/ws`);
  socket.addEventListener('message', (event) => {
    const payload = JSON.parse(event.data);
    if (payload.type === 'snapshot' && payload.snapshot) {
      onSnapshot(payload.snapshot);
      return;
    }
    if (payload.type === 'error' && onError) {
      onError(payload.message ?? 'socket error');
    }
  });
  return socket;
}
