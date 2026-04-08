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

export function renderTerminal(state) {
  const terminal = state.terminal;
  const summary = state.currentSnapshot?.summary ?? {};
  const collapsed = state.layout?.terminalCollapsed === true;
  let hint = '点击终端开始输入。';
  if (!terminal.connected) {
    hint = collapsed
      ? 'terminal 已收起；先加载一个会话，然后再展开输入。'
      : '先加载一个会话，然后点击终端开始输入。';
  } else if (terminal.pendingInput) {
    hint = collapsed
      ? 'terminal 已收起，正在把按键送入 guest monitor...'
      : '正在把按键送入 guest monitor...';
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
          <strong>interactive_os terminal</strong>
          <span>${state.selectedTest} · ${state.backend}</span>
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
