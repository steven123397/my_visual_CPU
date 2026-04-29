import test from 'node:test';
import assert from 'node:assert/strict';

import { renderAiAccelerator, renderBus } from '../app/components/panels.js';

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

test('renderAiAccelerator shows aggregate debug counters when present', () => {
  const html = renderAiAccelerator({
    devices: {
      ai_accelerator: {
        present: true,
        queue_depth: 0,
        doorbell_count: 1,
        last_fault: 0,
        completion_count: 1,
        engine_busy: false,
        scratchpad_occupancy_bytes: 0,
        dma_load_bytes: 12,
        dma_store_bytes: 4,
        device_cycles: 8,
        dma_cycles: 6,
        compute_cycles: 1,
        stall_cycles: 1,
        busy_cycles: 10,
        queue_cycles: 1,
        completion_cycles: 1,
        effective_ops_per_cycle: 3,
        utilization: 10,
      },
    },
  });

  assert.match(html, /AI accelerator/);
  assert.match(html, /<span>queue_depth<\/span><strong>0<\/strong>/);
  assert.match(html, /<span>engine_busy<\/span><strong>idle<\/strong>/);
  assert.match(html, /<span>scratchpad_occupancy_bytes<\/span><strong>0<\/strong>/);
  assert.match(html, /<span>dma_load_bytes<\/span><strong>12<\/strong>/);
  assert.match(html, /<span>dma_store_bytes<\/span><strong>4<\/strong>/);
  assert.match(html, /<span>device_cycles<\/span><strong>8<\/strong>/);
  assert.match(html, /<span>dma_cycles<\/span><strong>6<\/strong>/);
  assert.match(html, /<span>compute_cycles<\/span><strong>1<\/strong>/);
  assert.match(html, /<span>stall_cycles<\/span><strong>1<\/strong>/);
  assert.match(html, /<span>utilization<\/span><strong>10<\/strong>/);
});

test('renderAiAccelerator degrades gracefully without accelerator fields', () => {
  const html = renderAiAccelerator({
    devices: {
      uart: { ier: 0 },
    },
  });

  assert.match(html, /AI accelerator/);
  assert.match(html, /当前 snapshot 未暴露 AI accelerator counters/);
});
