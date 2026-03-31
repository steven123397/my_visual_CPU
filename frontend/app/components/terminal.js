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
  const hint = !terminal.connected
    ? '先加载一个会话，然后点击终端开始输入。'
    : terminal.pendingInput
      ? '正在把按键送入 guest monitor...'
      : terminal.focused
        ? '输入已接管，只发送 ASCII / Enter / Backspace。'
        : '点击终端开始输入。';
  const buffer = terminal.buffer.length > 0
    ? escapeHtml(projectTerminalBuffer(terminal.buffer))
    : '';

  return `
    <section class="terminal-window ${terminal.focused ? 'is-focused' : ''}">
      <header class="terminal-window__bar">
        <div class="window-dots" aria-hidden="true">
          <span></span><span></span><span></span>
        </div>
        <div class="terminal-window__title">
          <strong>interactive_os terminal</strong>
          <span>${state.selectedTest} · ${state.backend}</span>
        </div>
        <div class="terminal-window__status">
          <span>${state.runState}</span>
          <strong>${summary.pc ?? '0x0'}</strong>
        </div>
      </header>

      <div class="terminal-window__screen">
        <div class="terminal-scrollport">
          <pre class="terminal-buffer">${buffer}${terminal.focused ? '<span class="terminal-caret" aria-hidden="true"></span>' : ''}</pre>
        </div>
        <footer class="terminal-hint ${terminal.focused ? 'is-live' : ''}">
          <span>${hint}</span>
          <span>priv ${summary.privilege ?? '-'} · cycle ${summary.cycle ?? 0}</span>
        </footer>
      </div>
    </section>
  `;
}
