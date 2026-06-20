// 终端文本投影：把带控制字符（\r \n \b）的 UART 字节流增量投影成终端可见文本。
export const DEFAULT_TERMINAL_MAX_LENGTH = 32768;

// 创建一个终端投影状态（text/cursor/lineStart/carriageReturn + 长度上限）。
export function createTerminalProjectionState({ maxLength = DEFAULT_TERMINAL_MAX_LENGTH } = {}) {
  return {
    text: '',
    cursor: 0,
    lineStart: 0,
    carriageReturn: false,
    maxLength,
  };
}

// 复位投影状态到空文本（保留 maxLength 配置）。
export function resetTerminalProjectionState(state) {
  state.text = '';
  state.cursor = 0;
  state.lineStart = 0;
  state.carriageReturn = false;
  return state;
}

// 当文本超过 maxLength 时从头裁剪，并同步调整 cursor/lineStart。
function clampProjectionState(state, chars) {
  const maxLength = Number.isFinite(state.maxLength) ? state.maxLength : null;
  if (maxLength == null || chars.length <= maxLength) {
    return;
  }

  const overflow = chars.length - maxLength;
  chars.splice(0, overflow);
  state.cursor = Math.max(0, state.cursor - overflow);
  state.lineStart = Math.max(0, state.lineStart - overflow);
  if (state.lineStart > state.cursor) {
    state.lineStart = state.cursor;
  }
}

// 把一段 chunk 增量应用到投影状态：处理 \r 回行首、\n 换行、\b 退格与可见字符覆写。
export function applyTerminalChunk(state, chunk = '') {
  const chars = Array.from(state.text);

  for (const char of chunk) {
    if (char === '\r') {
      state.cursor = state.lineStart;
      state.carriageReturn = true;
      continue;
    }

    if (char === '\n') {
      if (state.carriageReturn) {
        while (state.cursor < chars.length && chars[state.cursor] !== '\n') {
          state.cursor += 1;
        }
      }
      if (state.cursor === chars.length) {
        chars.push('\n');
      } else {
        chars.splice(state.cursor, 0, '\n');
      }
      state.cursor += 1;
      state.lineStart = state.cursor;
      state.carriageReturn = false;
      continue;
    }

    if (char === '\b' || char === '\x7f') {
      if (state.cursor > state.lineStart) {
        state.cursor -= 1;
        if (state.cursor < chars.length && chars[state.cursor] !== '\n') {
          chars.splice(state.cursor, 1);
        }
      }
      state.carriageReturn = false;
      continue;
    }

    if (state.cursor < chars.length && chars[state.cursor] !== '\n') {
      chars[state.cursor] = char;
    } else {
      chars.splice(state.cursor, 0, char);
    }
    state.cursor += 1;
    state.carriageReturn = false;
  }

  clampProjectionState(state, chars);
  state.text = chars.join('');
  return state;
}

// 一次性把 text 投影成终端文本（无状态便利函数）。
export function projectTerminalText(text = '', { maxLength = Number.POSITIVE_INFINITY } = {}) {
  const state = createTerminalProjectionState({ maxLength });
  applyTerminalChunk(state, text);
  return state.text;
}
