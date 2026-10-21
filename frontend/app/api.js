/**
 * Helper to post JSON requests to backend API endpoints.
 * @param {string} pathname
 * @param {Object} [payload]
 * @returns {Promise<any>}
 */
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

/**
 * Fetches the list of available workload tests.
 * @returns {Promise<Object>}
 */
export async function listTests() {
  const response = await fetch('/api/tests', { credentials: 'same-origin' });
  if (!response.ok) {
    throw new Error('failed to list tests');
  }
  return response.json();
}

/**
 * Fetches the whitelist templates for AI tiny model.
 * @returns {Promise<Object>}
 */
export async function listAiTinyModelTemplates() {
  const response = await fetch('/api/ai/tiny-model/templates', { credentials: 'same-origin' });
  if (!response.ok) {
    throw new Error('failed to list AI tiny model templates');
  }
  return response.json();
}

/**
 * Fetches current authentication and controller session state.
 * @returns {Promise<Object>}
 */
export async function getAuthSession() {
  const response = await fetch('/api/auth/session', { credentials: 'same-origin' });
  if (!response.ok) {
    throw new Error('failed to fetch auth session');
  }
  return response.json();
}

/**
 * Log in to obtain authenticated controller session.
 * @param {string} username
 * @param {string} password
 * @returns {Promise<Object>}
 */
export async function login(username, password) {
  return postJson('/api/auth/login', { username, password });
}

/**
 * Log out and clear session cookie.
 * @returns {Promise<Object>}
 */
export async function logout() {
  return postJson('/api/auth/logout');
}

/**
 * Releases active controller lock for the session.
 * @returns {Promise<Object>}
 */
export async function releaseControl() {
  return postJson('/api/auth/release-control');
}

/**
 * Executes an AI tiny model workload run.
 * @param {Object} parameters
 * @returns {Promise<Object>}
 */
export async function runAiTinyModel(parameters) {
  return postJson('/api/ai/tiny-model/run', parameters);
}

/**
 * Executes a custom AI graph workload.
 * @param {Object} parameters
 * @returns {Promise<Object>}
 */
export async function runAiCustomGraph(parameters) {
  return postJson('/api/ai/custom-graph', parameters);
}

/**
 * Loads a CPU simulation session with specified test/backend or custom ELF.
 * @param {string} test
 * @param {string} backend
 * @param {Object|null} [customElf]
 * @returns {Promise<Object>}
 */
export async function loadSession(test, backend, customElf = null) {
  const payload = { backend };
  if (customElf?.elfPath) {
    payload.elfPath = customElf.elfPath;
  } else if (customElf?.elfBase64) {
    payload.elfBase64 = customElf.elfBase64;
    if (customElf.elfName) {
      payload.elfName = customElf.elfName;
    }
  } else {
    payload.test = test;
  }
  return postJson('/api/session/load', payload);
}

/**
 * Steps CPU execution by one cycle.
 * @returns {Promise<Object>}
 */
export async function stepCycle() {
  return postJson('/api/session/step-cycle');
}

/**
 * Steps CPU execution until instruction commit.
 * @returns {Promise<Object>}
 */
export async function stepCommit() {
  return postJson('/api/session/step-commit');
}

/**
 * Resets CPU simulation session state.
 * @returns {Promise<Object>}
 */
export async function resetSession() {
  return postJson('/api/session/reset');
}

/**
 * Terminates the active simulation session.
 * @returns {Promise<Object>}
 */
export async function terminateSession() {
  return postJson('/api/session/terminate');
}

/**
 * Continuous run simulation session at a specific frequency rate.
 * @param {number} [rateHz=8]
 * @returns {Promise<Object>}
 */
export async function runSession(rateHz = 8) {
  return postJson('/api/session/run', { rateHz });
}

/**
 * Pauses continuous simulation session run.
 * @returns {Promise<Object>}
 */
export async function pauseSession() {
  return postJson('/api/session/pause');
}

/**
 * Dispatches JIT translation / evaluation step.
 * @returns {Promise<Object>}
 */
export async function jitDispatch() {
  return postJson('/api/session/jit-dispatch');
}

/**
 * Sends terminal input characters to active interactive session.
 * @param {string} text
 * @returns {Promise<Object>}
 */
export async function terminalInput(text) {
  return postJson('/api/session/terminal-input', { text });
}

/**
 * Reads terminal output stream starting from specified byte offset.
 * @param {number} [offset=0]
 * @returns {Promise<Object>}
 */
export async function terminalOutput(offset = 0) {
  return postJson('/api/session/terminal-output', { offset });
}

/**
 * Connects WebSocket for real-time CPU snapshot and terminal streaming.
 * @param {function(Object):void} onSnapshot
 * @param {function(string):void} [onError]
 * @param {function(Object):void} [onTerminal]
 * @returns {WebSocket}
 */
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
