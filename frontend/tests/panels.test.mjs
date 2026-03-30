import test from 'node:test';
import assert from 'node:assert/strict';

import { renderBus } from '../app/components/panels.js';

test('renderBus highlights failed MMIO accesses with status and detail', () => {
  const html = renderBus({
    bus: {
      device: 'uart',
      addr: '0x10000000',
      value: '0x00000005',
      size: 4,
      write: true,
      mmio: true,
      success: false,
      detail: 'invalid MMIO access',
    },
  });

  assert.match(html, /<span>device<\/span><strong>uart<\/strong>/);
  assert.match(html, /<span>kind<\/span><strong>store<\/strong>/);
  assert.match(html, /<span>space<\/span><strong>MMIO<\/strong>/);
  assert.match(html, /<span>status<\/span><strong>failed<\/strong>/);
  assert.match(html, /<span>detail<\/span><strong>invalid MMIO access<\/strong>/);
});
