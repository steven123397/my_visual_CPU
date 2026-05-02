function errorMessage(error) {
  if (error instanceof Error && typeof error.message === 'string') {
    return error.message;
  }
  if (typeof error === 'string') {
    return error;
  }
  return 'load failed';
}

function isLinuxBootTimeout(message) {
  return /run_until_uart_contains|timed out|mycpu-linux#|Linux boot/i.test(message);
}

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
