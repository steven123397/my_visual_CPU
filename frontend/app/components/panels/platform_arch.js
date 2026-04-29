import { classifyEventTone } from '../../state.js';
import { card, groupPanel, renderMetricPill } from './shared.js';

export function renderEvents(snapshot) {
  const events = snapshot?.events ?? [];
  return card(
    '事件流',
    `
      <div class="event-list">
        ${events.length === 0 ? '<div class="empty-state">还没有事件</div>' : events.map((event) => `
          <article class="event-item tone-${classifyEventTone(event.kind)}">
            <div class="event-kind">${event.kind}</div>
            <div class="event-cycle">cycle ${event.cycle}</div>
            <div class="event-detail">${event.detail}</div>
          </article>
        `).join('')}
      </div>
    `,
    'panel-events',
  );
}

export function renderDevices(snapshot) {
  const devices = snapshot?.devices ?? {};
  return card(
    '设备状态',
    `
      <div class="device-list">
        <article><span>UART</span><strong>IER ${devices.uart?.ier ?? 0} / out ${devices.uart?.output_size ?? 0}</strong></article>
        <article><span>CLINT</span><strong>mtime ${devices.clint?.mtime ?? 0} / cmp ${devices.clint?.mtimecmp ?? 0}</strong></article>
        <article><span>PLIC</span><strong>pending ${devices.plic?.pending ? 'yes' : 'no'} / level ${devices.plic?.level ? 'high' : 'low'}</strong></article>
        <article><span>Storage</span><strong>${devices.storage?.attached ? 'attached' : 'detached'} / lba ${devices.storage?.lba ?? 0}</strong></article>
      </div>
    `,
    'panel-devices',
  );
}

function renderCounterField(label, value) {
  return `<div class="kv-row"><span>${label}</span><strong>${value}</strong></div>`;
}

function renderAiField(label, value) {
  return renderCounterField(label, value);
}

function formatCounter(value, fallback = '-') {
  return Number.isFinite(value) ? value : fallback;
}

function formatAiCounter(value, fallback = '-') {
  return formatCounter(value, fallback);
}

export function renderAiAccelerator(snapshot) {
  const ai = snapshot?.devices?.ai_accelerator ?? null;
  if (!ai || ai.present === false) {
    return card(
      'AI accelerator',
      '<div class="empty-state">当前 snapshot 未暴露 AI accelerator counters。</div>',
      'panel-ai-accelerator',
    );
  }

  const busyLabel = ai.engine_busy ? 'busy' : 'idle';
  return card(
    'AI accelerator',
    `
      <div class="ai-accelerator-panel">
        <div class="ai-accelerator-panel__topline">
          <div class="ai-accelerator-status ${ai.engine_busy ? 'is-busy' : 'is-idle'}">
            <span>engine_busy</span>
            <strong>${busyLabel}</strong>
          </div>
          <div class="metric-pill-row">
            ${renderMetricPill('queue', formatAiCounter(ai.queue_depth))}
            ${renderMetricPill('scratchpad', formatAiCounter(ai.scratchpad_occupancy_bytes))}
            ${renderMetricPill('utilization', formatAiCounter(ai.utilization))}
          </div>
        </div>
        <div class="ai-accelerator-grid">
          <div class="kv-list">
            ${renderAiField('queue_depth', formatAiCounter(ai.queue_depth))}
            ${renderAiField('doorbell_count', formatAiCounter(ai.doorbell_count))}
            ${renderAiField('completion_count', formatAiCounter(ai.completion_count))}
            ${renderAiField('last_fault', formatAiCounter(ai.last_fault))}
            ${renderAiField('engine_busy', busyLabel)}
            ${renderAiField('scratchpad_occupancy_bytes', formatAiCounter(ai.scratchpad_occupancy_bytes))}
          </div>
          <div class="kv-list">
            ${renderAiField('dma_load_bytes', formatAiCounter(ai.dma_load_bytes))}
            ${renderAiField('dma_store_bytes', formatAiCounter(ai.dma_store_bytes))}
            ${renderAiField('device_cycles', formatAiCounter(ai.device_cycles))}
            ${renderAiField('dma_cycles', formatAiCounter(ai.dma_cycles))}
            ${renderAiField('compute_cycles', formatAiCounter(ai.compute_cycles))}
            ${renderAiField('stall_cycles', formatAiCounter(ai.stall_cycles))}
          </div>
          <div class="kv-list">
            ${renderAiField('busy_cycles', formatAiCounter(ai.busy_cycles))}
            ${renderAiField('queue_cycles', formatAiCounter(ai.queue_cycles))}
            ${renderAiField('completion_cycles', formatAiCounter(ai.completion_cycles))}
            ${renderAiField('effective_ops_per_cycle', formatAiCounter(ai.effective_ops_per_cycle))}
            ${renderAiField('utilization', formatAiCounter(ai.utilization))}
          </div>
        </div>
      </div>
    `,
    'panel-ai-accelerator',
  );
}

export function renderL1DataCache(snapshot) {
  const cache = snapshot?.l1_data_cache ?? null;
  if (!cache) {
    return card(
      'L1 data cache',
      '<div class="empty-state">当前 snapshot 未暴露 L1D counters。</div>',
      'panel-l1-data-cache',
    );
  }

  const enabledLabel = typeof cache.enabled === 'boolean'
    ? (cache.enabled ? 'enabled' : 'disabled')
    : '-';
  return card(
    'L1 data cache',
    `
      <div class="l1-data-cache-panel">
        <div class="metric-pill-row">
          ${renderMetricPill('enabled', enabledLabel)}
          ${renderMetricPill('line_size', formatCounter(cache.line_size_bytes))}
          ${renderMetricPill('capacity', formatCounter(cache.capacity_lines))}
        </div>
        <div class="l1-data-cache-grid">
          <div class="kv-list">
            ${renderCounterField('enabled', enabledLabel)}
            ${renderCounterField('line_size_bytes', formatCounter(cache.line_size_bytes))}
            ${renderCounterField('capacity_lines', formatCounter(cache.capacity_lines))}
          </div>
          <div class="kv-list">
            ${renderCounterField('loads', formatCounter(cache.loads))}
            ${renderCounterField('stores', formatCounter(cache.stores))}
            ${renderCounterField('hits', formatCounter(cache.hits))}
            ${renderCounterField('misses', formatCounter(cache.misses))}
          </div>
          <div class="kv-list">
            ${renderCounterField('evictions', formatCounter(cache.evictions))}
            ${renderCounterField('bypasses', formatCounter(cache.bypasses))}
            ${renderCounterField('write_through_stores', formatCounter(cache.write_through_stores))}
          </div>
        </div>
      </div>
    `,
    'panel-l1-data-cache',
  );
}

export function renderRegisters(registers) {
  return card(
    '通用寄存器',
    `
      <div class="register-grid">
        ${registers.map((item) => `
          <article class="register-cell ${item.changed ? 'is-changed' : ''} emphasis-${item.emphasis}">
            <span>x${item.index}</span>
            <strong>${item.value ?? '0x0'}</strong>
          </article>
        `).join('')}
      </div>
    `,
    'panel-registers',
  );
}

export function renderCsrs(snapshot) {
  const csrs = snapshot?.csrs ?? {};
  const fields = [
    ['mstatus', csrs.mstatus],
    ['sstatus', csrs.sstatus],
    ['mepc', csrs.mepc],
    ['sepc', csrs.sepc],
    ['mcause', csrs.mcause],
    ['scause', csrs.scause],
    ['mie', csrs.mie],
    ['mip', csrs.mip],
    ['satp', csrs.satp],
  ];
  return card(
    'CSR / Trap',
    `
      <div class="kv-list">
        ${fields.map(([label, value]) => `
          <div class="kv-row"><span>${label}</span><strong>${value ?? '0x0'}</strong></div>
        `).join('')}
      </div>
    `,
    'panel-csrs',
  );
}

export function renderBus(snapshot) {
  const bus = snapshot?.bus ?? {};
  return card(
    '总线访问',
    `
      <div class="kv-list">
        <div class="kv-row"><span>device</span><strong>${bus.device ?? '-'}</strong></div>
        <div class="kv-row"><span>addr</span><strong>${bus.addr ?? '0x0'}</strong></div>
        <div class="kv-row"><span>value</span><strong>${bus.value ?? '0x0'}</strong></div>
        <div class="kv-row"><span>size</span><strong>${bus.size ?? 0}</strong></div>
        <div class="kv-row"><span>kind</span><strong>${bus.write ? 'store' : 'load'}</strong></div>
        <div class="kv-row"><span>space</span><strong>${bus.mmio ? 'MMIO' : 'RAM'}</strong></div>
        <div class="kv-row"><span>status</span><strong>${bus.success === false ? 'failed' : 'ok'}</strong></div>
        <div class="kv-row"><span>detail</span><strong>${bus.detail || '-'}</strong></div>
      </div>
    `,
    'panel-bus',
  );
}

export function renderArchitectureGroup(snapshot, registers, isOpen = false) {
  return groupPanel(
    '架构状态',
    'CSR / Trap · 通用寄存器',
    [
      renderCsrs(snapshot),
      renderRegisters(registers),
    ],
    'architectureGroupOpen',
    isOpen,
    'panel-group-architecture',
  );
}

export function renderPlatformGroup(snapshot, isOpen = false) {
  return groupPanel(
    '平台与 I/O',
    '设备状态 · AI accelerator · L1D cache · 总线访问 · 事件流',
    [
      renderDevices(snapshot),
      renderAiAccelerator(snapshot),
      renderL1DataCache(snapshot),
      renderBus(snapshot),
      renderEvents(snapshot),
    ],
    'platformGroupOpen',
    isOpen,
    'panel-group-platform',
  );
}
