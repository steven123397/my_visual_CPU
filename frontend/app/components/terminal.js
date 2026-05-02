import { projectTerminalText } from '../../shared/terminal_projection.mjs';

function escapeHtml(text = '') {
  return text
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

export function projectTerminalBuffer(text = '') {
  return projectTerminalText(text);
}

function terminalPresentation(activeTest) {
  if (activeTest === 'linux_proto_console') {
    return {
      title: 'Linux serial terminal',
      target: 'Linux serial console',
    };
  }

  return {
    title: 'interactive_os terminal',
    target: 'guest monitor',
  };
}

function isLinuxLoadProgress(state) {
  return state.runState === 'loading' && state.loadProgress?.test === 'linux_proto_console';
}

function progressElapsedSeconds(progress) {
  const startedAt =
    typeof progress?.startedAt === 'number' && Number.isFinite(progress.startedAt)
      ? progress.startedAt
      : Date.now();
  const now =
    typeof progress?.now === 'number' && Number.isFinite(progress.now)
      ? progress.now
      : Date.now();
  return Math.max(0, Math.floor((now - startedAt) / 1000));
}

function renderLinuxBootProgress(progress) {
  if (!progress) {
    return '';
  }

  const backend = progress.backend ?? 'functional';
  const waitingFor = progress.waitingFor ?? 'mycpu-linux# ';
  const elapsed = progressElapsedSeconds(progress);
  return `
    <div class="linux-boot-progress" role="status">
      <span>Linux boot in progress</span>
      <strong>${elapsed}s</strong>
      <em>${escapeHtml(backend)}</em>
      <code>waiting for ${escapeHtml(waitingFor)}</code>
    </div>
  `;
}

export function renderTerminal(state) {
  const terminal = state.terminal;
  const summary = state.currentSnapshot?.summary ?? {};
  const collapsed = state.layout?.terminalCollapsed === true;
  const loadedSession = state.loadedSession ?? null;
  const loadProgress = isLinuxLoadProgress(state) ? state.loadProgress : null;
  const activeTest = typeof loadedSession?.test === 'string' && loadedSession.test.length > 0
    ? loadedSession.test
    : loadProgress?.test ?? null;
  const activeBackend =
    typeof summary.backend === 'string' && summary.backend.length > 0
      ? summary.backend
      : (typeof loadedSession?.backend === 'string' && loadedSession.backend.length > 0
        ? loadedSession.backend
        : (typeof loadProgress?.backend === 'string' && loadProgress.backend.length > 0 ? loadProgress.backend : '-'));
  const sessionLabel = activeTest ? `${activeTest} · ${activeBackend}` : `未加载会话 · ${activeBackend}`;
  const presentation = terminalPresentation(activeTest);
  let hint = '点击终端开始输入。';
  if (!terminal.connected) {
    if (loadProgress) {
      hint = collapsed
        ? 'terminal 已收起；Linux runtime 仍在启动。'
        : 'Linux runtime 仍在启动，等待串口 prompt 后即可输入。';
    } else {
      hint = collapsed
        ? 'terminal 已收起；先加载一个会话，然后再展开输入。'
        : '先加载一个会话，然后点击终端开始输入。';
    }
  } else if (terminal.pendingInput) {
    hint = collapsed
      ? `terminal 已收起，正在把按键送入 ${presentation.target}...`
      : `正在把按键送入 ${presentation.target}...`;
  } else if (collapsed) {
    hint = 'terminal 已收起，展开后继续交互。';
  } else if (terminal.focused) {
    hint = '输入已接管，只发送 ASCII / Enter / Backspace。';
  }
  const buffer = terminal.buffer.length > 0
    ? escapeHtml(projectTerminalBuffer(terminal.buffer))
    : '';
  const projectedLines = buffer.length > 0 ? buffer.split('\n') : [];
  const preview = projectedLines.slice(-3).join('\n');
  const actionLabel = collapsed ? '展开 terminal' : '收起 terminal';
  const windowClass = ['terminal-window', terminal.focused ? 'is-focused' : '', collapsed ? 'is-collapsed' : '']
    .filter(Boolean)
    .join(' ');

  return `
    <section class="${windowClass}">
      <header class="terminal-window__bar">
        <div class="window-dots" aria-hidden="true">
          <span></span><span></span><span></span>
        </div>
        <div class="terminal-window__title">
          <strong>${presentation.title}</strong>
          <span>${sessionLabel}</span>
        </div>
        <div class="terminal-window__actions">
          <div class="terminal-window__status">
            <span>${state.runState}</span>
            <strong>${summary.pc ?? '0x0'}</strong>
          </div>
          <button type="button" class="terminal-toggle" data-action="toggle-terminal-collapsed" aria-expanded="${collapsed ? 'false' : 'true'}">${actionLabel}</button>
        </div>
      </header>

      <div class="terminal-window__screen">
        ${renderLinuxBootProgress(loadProgress)}
        <div class="terminal-scrollport">
          <pre class="terminal-buffer">${buffer}${terminal.focused && !collapsed ? '<span class="terminal-caret" aria-hidden="true"></span>' : ''}</pre>
        </div>
        <div class="terminal-collapsed-preview">
          <span class="terminal-collapsed-preview__label">最近输出</span>
          <pre class="terminal-collapsed-preview__body">${preview || 'terminal 暂无输出'}</pre>
        </div>
        <footer class="terminal-hint ${terminal.focused ? 'is-live' : ''}">
          <span>${hint}</span>
          <span>priv ${summary.privilege ?? '-'} · cycle ${summary.cycle ?? 0}</span>
        </footer>
      </div>
    </section>
  `;
}
