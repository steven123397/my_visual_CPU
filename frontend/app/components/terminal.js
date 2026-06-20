// 终端窗口渲染：把 UART 文本投影、会话状态和 Linux boot 进度组装成终端 HTML。
import { projectTerminalText } from '../../shared/terminal_projection.mjs';

// 转义 HTML 特殊字符，防止终端输出注入 HTML。
function escapeHtml(text = '') {
  return text
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#39;');
}

// 把终端缓冲文本投影成可见文本（转发到 terminal_projection）。
export function projectTerminalBuffer(text = '') {
  return projectTerminalText(text);
}

// 从 manifest 的 workload.terminal 取终端展示元数据（title/target）。
function manifestTerminalPresentation(state, activeTest) {
  const entry = Array.isArray(state.tests)
    ? state.tests.find((item) => item.name === activeTest)
    : null;
  const terminal = entry?.workload?.terminal;
  if (!terminal || typeof terminal !== 'object') {
    return null;
  }

  const title =
    typeof terminal.title === 'string' && terminal.title.length > 0
      ? terminal.title
      : null;
  const target =
    typeof terminal.target === 'string' && terminal.target.length > 0
      ? terminal.target
      : null;
  if (!title && !target) {
    return null;
  }
  return {
    title: title ?? entry.title ?? entry.menuLabel ?? `${activeTest} terminal`,
    target: target ?? entry.title ?? activeTest,
  };
}

// 决定终端展示元数据：优先 manifest，否则按 activeTest 给默认标题。
function terminalPresentation(state, activeTest) {
  const manifestPresentation = manifestTerminalPresentation(state, activeTest);
  if (manifestPresentation) {
    return manifestPresentation;
  }

  if (activeTest === 'linux_proto_console') {
    return {
      title: 'Linux serial terminal',
      target: 'Linux serial console',
    };
  }
  if (activeTest === 'guest_course_os_shell_demo') {
    return {
      title: 'Course OS shell terminal',
      target: 'Course OS shell',
    };
  }

  return {
    title: 'interactive_os terminal',
    target: 'guest monitor',
  };
}

// 是否处于 Linux proto console 的加载进度态。
function isLinuxLoadProgress(state) {
  return state.runState === 'loading' && state.loadProgress?.test === 'linux_proto_console';
}

// 计算 Linux boot 已等待秒数（兼容缺失时间戳的回退）。
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

// 渲染 Linux boot 进度条 HTML（已等待秒数、backend、等待中的 prompt）。
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

// 渲染整个终端窗口：标题栏、屏幕、缓冲投影、收起预览与底部 hint。
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
  const presentation = terminalPresentation(state, activeTest);
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
