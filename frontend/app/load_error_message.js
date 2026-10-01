/**
 * 从 Error 或字符串中提取错误消息文本。
 * @param {Error|string|unknown} error
 * @returns {string} 提取后的错误消息
 */
function errorMessage(error) {
  if (error instanceof Error && typeof error.message === 'string') {
    return error.message;
  }
  if (typeof error === 'string') {
    return error;
  }
  return 'load failed';
}

/**
 * 判断错误消息是否为 Linux 引导超时。
 * @param {string} message
 * @returns {boolean}
 */
function isLinuxBootTimeout(message) {
  return /run_until_uart_contains|timed out|mycpu-linux#|Linux boot/i.test(message);
}

/**
 * 格式化工作负载加载失败错误消息。
 * @param {Error|string|unknown} error - 捕获的异常或错误字符串
 * @param {Object} [context] - 加载上下文（包含 test, backend 等）
 * @returns {string} 可读的错误提示文本
 */
export function formatLoadErrorMessage(error, context = {}) {
  const message = errorMessage(error);
  if (context.test !== 'linux_proto_console' || !isLinuxBootTimeout(message)) {
    return message;
  }

  const backend =
    typeof context.backend === 'string' && context.backend.length > 0
      ? context.backend
      : 'functional';
  return [
    'Linux Serial Console 启动超时。',
    `仍在等待 mycpu-linux# prompt（backend=${backend}）。`,
    '请确认 MYCPU_LINUX_PROTO_CONSOLE_IMAGE 指向可启动的 Linux Image，并重新点击 Load。',
  ].join(' ');
}
