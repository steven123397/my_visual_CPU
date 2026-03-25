import { classifyEventTone } from '../state.js';

function card(title, body, extraClass = '') {
  return `
    <section class="panel ${extraClass}">
      <div class="panel-header"><h2>${title}</h2></div>
      ${body}
    </section>
  `;
}

export function renderSummary(snapshot, runState) {
  const summary = snapshot?.summary ?? {};
  return card(
    '运行摘要',
    `
      <div class="summary-grid">
        <div><span>state</span><strong>${runState}</strong></div>
        <div><span>backend</span><strong>${summary.backend ?? '-'}</strong></div>
        <div><span>cycle</span><strong>${summary.cycle ?? 0}</strong></div>
        <div><span>instret</span><strong>${summary.instret ?? 0}</strong></div>
        <div><span>pc</span><strong>${summary.pc ?? '0x0'}</strong></div>
        <div><span>privilege</span><strong>${summary.privilege ?? '-'}</strong></div>
      </div>
    `,
    'panel-summary',
  );
}

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
      </div>
    `,
    'panel-bus',
  );
}
