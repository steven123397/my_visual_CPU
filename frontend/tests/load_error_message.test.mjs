import test from 'node:test';
import assert from 'node:assert/strict';

import { formatLoadErrorMessage } from '../app/load_error_message.js';

test('formatLoadErrorMessage translates Linux boot timeout details into user-facing guidance', () => {
  const message = formatLoadErrorMessage(
    new Error('debug cli run_until_uart_contains timed out after 120000ms'),
    {
      test: 'linux_proto_console',
      backend: 'functional',
    },
  );

  assert.match(message, /Linux Serial Console 启动超时/);
  assert.match(message, /mycpu-linux# /);
  assert.match(message, /MYCPU_LINUX_PROTO_CONSOLE_IMAGE/);
  assert.match(message, /functional/);
  assert.doesNotMatch(message, /debug cli run_until_uart_contains/);
});

test('formatLoadErrorMessage keeps non-Linux load errors direct', () => {
  assert.equal(
    formatLoadErrorMessage(new Error('load failed'), {
      test: 'guest_interactive_os_demo',
      backend: 'pipeline',
    }),
    'load failed',
  );
});
