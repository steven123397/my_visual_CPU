import { classifyEventTone, classifyInstructionFlavor, diffVectorRegisters } from '../state.js';

function card(title, body, extraClass = '') {
  return `
    <section class="panel ${extraClass}">
      <div class="panel-header"><h2>${title}</h2></div>
      ${body}
    </section>
  `;
}

function normalizeVectorHex(value) {
  if (typeof value !== 'string' || !value.startsWith('0x')) {
    return '0x00000000000000000000000000000000';
  }
  return value;
}

function vectorHexToBytes(value) {
  const normalized = normalizeVectorHex(value).slice(2);
  const bytes = [];
  for (let i = 0; i + 1 < normalized.length; i += 2) {
    bytes.push(Number.parseInt(normalized.slice(i, i + 2), 16));
  }
  return bytes;
}

function decodeSignedLanes(value, sewBytes = 1, vl = 0) {
  if (!Number.isInteger(sewBytes) || sewBytes <= 0 || !Number.isInteger(vl) || vl <= 0) {
    return [];
  }

  const bytes = vectorHexToBytes(value);
  const lanes = [];
  for (let lane = 0; lane < vl; lane += 1) {
    const base = lane * sewBytes;
    if (base + sewBytes > bytes.length) {
      break;
    }
    let laneValue = 0n;
    for (let offset = 0; offset < sewBytes; offset += 1) {
      laneValue |= BigInt(bytes[base + offset] ?? 0) << BigInt(offset * 8);
    }

    const bitWidth = BigInt(sewBytes * 8);
    const signBit = 1n << (bitWidth - 1n);
    const fullRange = 1n << bitWidth;
    if ((laneValue & signBit) !== 0n) {
      laneValue -= fullRange;
    }
    lanes.push(laneValue.toString());
  }
  return lanes;
}

function summarizeVectorRegister(value, sewBytes, vl) {
  const lanes = decodeSignedLanes(value, sewBytes, vl);
  if (lanes.length > 0) {
    return lanes.join(' · ');
  }
  return normalizeVectorHex(value).slice(0, 18);
}

function activeVectorStages(snapshot) {
  const pipeline = snapshot?.pipeline ?? {};
  const stages = [
    ['IF', pipeline.if],
    ['ID', pipeline.id],
    ['EX', pipeline.ex],
    ['MEM', pipeline.mem],
    ['WB', pipeline.wb],
  ];
  return stages
    .map(([label, stage]) => {
      const flavor = classifyInstructionFlavor(stage?.text ?? '');
      if (!stage?.valid || !flavor) {
        return null;
      }
      return {
        label,
        text: stage.text ?? '',
        flavor,
      };
    })
    .filter(Boolean);
}

function renderWorkloadTag(text, tone = 'neutral') {
  return `<span class="workload-tag workload-tag-${tone}">${text}</span>`;
}

function renderMetricPill(label, value) {
  return `
    <div class="metric-pill">
      <span>${label}</span>
      <strong>${value}</strong>
    </div>
  `;
}

function renderLaneStrip(title, values, emphasis = 'neutral') {
  return `
    <div class="lane-strip lane-strip-${emphasis}">
      <span class="lane-strip__title">${title}</span>
      <div class="lane-strip__values">
        ${values.map((value) => `<strong>${value}</strong>`).join('')}
      </div>
    </div>
  `;
}

function groupPanel(title, detail, panels, layoutKey, isOpen = false, extraClass = '') {
  return `
    <details class="panel panel-group ${extraClass}" data-layout-key="${layoutKey}" ${isOpen ? 'open' : ''}>
      <summary class="panel-group__summary" data-layout-key="${layoutKey}">
        <div>
          <h2>${title}</h2>
          <span>${detail}</span>
        </div>
        <span class="panel-group__toggle">展开</span>
      </summary>
      <div class="panel-group__body">
        ${panels.join('')}
      </div>
    </details>
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

export function renderWorkloadPanel(testEntry, snapshot) {
  if (!testEntry) {
    return '';
  }

  const workload = testEntry.workload ?? null;
  const activeStages = activeVectorStages(snapshot);
  const body = workload
    ? `
      <div class="workload-panel">
        <div class="workload-panel__hero">
          <div>
            <div class="panel-kicker">${testEntry.badge ?? (testEntry.kind === 'asm' ? 'ASM Demo' : 'Guest Demo')}</div>
            <h3>${testEntry.title ?? testEntry.name}</h3>
          </div>
          <div class="workload-tag-row">
            ${renderWorkloadTag(workload.stage ?? 'P0-P3', 'accent')}
            ${workload.expectedMarker ? renderWorkloadTag(workload.expectedMarker, 'teal') : ''}
          </div>
        </div>
        <p class="panel-copy">${testEntry.summary ?? workload.summary ?? '当前 demo 没有额外说明。'}</p>
        <div class="workload-tag-row">
          ${(workload.ops ?? []).map((op) => renderWorkloadTag(op, 'neutral')).join('')}
        </div>
        <div class="workload-progress">
          ${[
            ['P0', 'demo 入口 + workload 卡 + 指令高亮'],
            ['P1', 'vector snapshot + 寄存器 diff'],
            ['P2', '固定 conv -> relu 专题视图'],
            ['P3', '当前向量执行边界提示'],
          ].map(([label, detail]) => `
            <div class="workload-progress__item">
              <span>${label}</span>
              <strong>${detail}</strong>
            </div>
          `).join('')}
        </div>
        <div class="workload-callout">
          <strong>当前边界</strong>
          <p>${workload.pipelineNote ?? '当前 demo 只展示仓库里已经稳定落地的 workload 与执行边界。'}</p>
        </div>
        ${activeStages.length > 0 ? `
          <div class="workload-active">
            <span>当前可见的向量 stage</span>
            <div class="workload-tag-row">
              ${activeStages.map((item) => renderWorkloadTag(`${item.label} · ${item.flavor.label}`, item.flavor.kind === 'memory' ? 'warn' : 'teal')).join('')}
            </div>
          </div>
        ` : ''}
      </div>
    `
    : `
      <div class="empty-state">当前 demo 暂无专门的 workload 说明，仍可用这套前端观察流水线、寄存器与设备状态。</div>
    `;

  return card('工作负载导览', body, 'panel-workload');
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
        <div class="kv-row"><span>status</span><strong>${bus.success === false ? 'failed' : 'ok'}</strong></div>
        <div class="kv-row"><span>detail</span><strong>${bus.detail || '-'}</strong></div>
      </div>
    `,
    'panel-bus',
  );
}

export function renderPredictor(snapshot) {
  const predictor = snapshot?.pipeline?.predictor ?? {};
  const resolvedCount = predictor.total_predictions ?? 0;
  const hasResolvedData = resolvedCount > 0;

  const accuracy = hasResolvedData
    ? ((predictor.correct_predictions / resolvedCount) * 100).toFixed(1)
    : '0.0';

  const lastPredClass = predictor.last_prediction_valid
    ? (predictor.last_prediction_correct ? 'pred-correct' : 'pred-mispredict')
    : '';
  
  return card(
    '分支预测器',
    `
      <div class="predictor-grid">
        <div class="predictor-stats">
          <div class="stat-box">
            <span class="stat-label">模式</span>
            <strong class="stat-value">${predictor.mode ?? '-'}</strong>
          </div>
          <div class="stat-box">
            <span class="stat-label">已解析分支</span>
            <strong class="stat-value">${resolvedCount}</strong>
          </div>
          <div class="stat-box">
            <span class="stat-label">命中</span>
            <strong class="stat-value stat-correct">${predictor.correct_predictions ?? 0}</strong>
          </div>
          <div class="stat-box">
            <span class="stat-label">失误</span>
            <strong class="stat-value stat-mispredict">${predictor.mispredictions ?? 0}</strong>
          </div>
          <div class="stat-box">
            <span class="stat-label">命中率</span>
            <strong class="stat-value stat-accuracy">${accuracy}%</strong>
          </div>
        </div>
        ${predictor.last_prediction_valid ? `
        <div class="predictor-last ${lastPredClass}">
          <div class="predictor-section-title">最近预测</div>
          <div class="kv-list">
            <div class="kv-row">
              <span>PC</span>
              <strong>${predictor.last_prediction_pc ?? '0x0'}</strong>
            </div>
            <div class="kv-row">
              <span>目标</span>
              <strong>${predictor.last_prediction_target ?? '0x0'}</strong>
            </div>
            <div class="kv-row">
              <span>方向</span>
              <strong>${predictor.last_prediction_taken ? 'taken' : 'not taken'}</strong>
            </div>
            <div class="kv-row">
              <span>结果</span>
              <strong class="pred-result">${predictor.last_prediction_correct ? '✓ 命中' : '✗ 失误'}</strong>
            </div>
          </div>
        </div>
        ` : '<div class="predictor-empty">暂无预测记录</div>'}
        ${predictor.last_mispredict_valid ? `
        <div class="predictor-mispredict">
          <div class="predictor-section-title">最近失误</div>
          <div class="kv-list">
            <div class="kv-row">
              <span>PC</span>
              <strong>${predictor.last_mispredict_pc ?? '0x0'}</strong>
            </div>
            <div class="kv-row">
              <span>目标</span>
              <strong>${predictor.last_mispredict_target ?? '0x0'}</strong>
            </div>
          </div>
        </div>
        ` : ''}
      </div>
    `,
    'panel-predictor',
  );
}

export function renderVectorPanel(snapshot, previousSnapshot, testEntry, backend) {
  const vector = snapshot?.vector;
  const workload = testEntry?.workload;
  if (!vector || !Array.isArray(vector.registers)) {
    return '';
  }

  const sewBytes = Number(vector.sew_bytes ?? 0);
  const vl = Number(vector.vl ?? 0);
  const activeStages = activeVectorStages(snapshot);
  const registers = diffVectorRegisters(previousSnapshot, snapshot)
    .filter((item) => item.changed || !item.empty || (workload?.registerFocus ?? []).includes(item.index))
    .sort((left, right) => {
      const leftFocus = (workload?.registerFocus ?? []).includes(left.index) ? 1 : 0;
      const rightFocus = (workload?.registerFocus ?? []).includes(right.index) ? 1 : 0;
      if (leftFocus !== rightFocus) {
        return rightFocus - leftFocus;
      }
      if (left.changed !== right.changed) {
        return left.changed ? -1 : 1;
      }
      return left.index - right.index;
    });
  const visibleRegisters = registers.length > 0
    ? registers
    : vector.registers.slice(0, 8).map((value, index) => ({
      index,
      value,
      changed: false,
      empty: true,
      emphasis: 'idle',
    }));

  const cnn = workload?.cnn ?? null;
  const liveConv = cnn ? decodeSignedLanes(vector.registers[cnn.liveConvReg ?? 4] ?? '', 4, 3) : [];
  const liveRelu = cnn ? decodeSignedLanes(vector.registers[cnn.liveReluReg ?? 5] ?? '', 4, 3) : [];

  return card(
    'Vector State',
    `
      <div class="vector-panel">
        <div class="vector-panel__topline">
          <div>
            <div class="panel-kicker">向量寄存器与标量寄存器分离</div>
            <p class="panel-copy"><code>GPR</code> 继续承载标量 / 地址 / 控制状态；这里展示的是独立的 <code>v0..v31</code>，并由 <code>SEW / VL</code> 决定当前 lane 解释。</p>
          </div>
          <div class="metric-pill-row">
            ${renderMetricPill('SEW', sewBytes > 0 ? `${sewBytes}B` : '-')}
            ${renderMetricPill('VL', vl)}
            ${renderMetricPill('backend', snapshot?.summary?.backend ?? backend ?? '-')}
          </div>
        </div>

        <div class="vector-boundary">
          <div>
            <span class="panel-kicker">执行边界</span>
            <p class="panel-copy">${workload?.pipelineNote ?? '当前前端只展示已经落地的向量边界，不推导额外微架构。'}</p>
          </div>
          ${snapshot?.pipeline?.flags?.stall_reason === 'vector_state_busy'
            ? '<div class="vector-alert">当前 stall_reason = vector_state_busy，说明年轻向量链路正在等待更老的向量状态提交或 materialize。</div>'
            : ''}
          ${activeStages.length > 0 ? `
            <div class="vector-stage-strip">
              ${activeStages.map((item) => `
                <div class="vector-stage-pill vector-stage-pill-${item.flavor.kind}">
                  <span>${item.label}</span>
                  <strong>${item.text}</strong>
                </div>
              `).join('')}
            </div>
          ` : '<div class="empty-state">当前快照里没有正在占据流水线的向量指令。</div>'}
        </div>

        ${cnn ? `
          <div class="vector-cnn-card">
            <div class="vector-cnn-card__copy">
              <span class="panel-kicker">Fixed conv -> relu</span>
              <p class="panel-copy">这条 guest demo 不追求通用模型执行，而是把当前仓库已经稳定落地的最小 CNN-style workload 用固定样本讲清楚。</p>
            </div>
            <div class="vector-cnn-card__grid">
              ${renderLaneStrip('input', cnn.input ?? [], 'neutral')}
              ${renderLaneStrip('kernel', cnn.kernel ?? [], 'neutral')}
              ${renderLaneStrip('expected conv', cnn.conv ?? [], 'warn')}
              ${renderLaneStrip('expected relu', cnn.relu ?? [], 'teal')}
              ${renderLaneStrip('live v4 / conv', liveConv.length > 0 ? liveConv : ['待产生'], liveConv.length > 0 ? 'warn' : 'neutral')}
              ${renderLaneStrip('live v5 / relu', liveRelu.length > 0 ? liveRelu : ['待产生'], liveRelu.length > 0 ? 'teal' : 'neutral')}
            </div>
          </div>
        ` : ''}

        <div class="vector-register-grid">
          ${visibleRegisters.map((item) => `
            <article class="vector-register-card emphasis-${item.emphasis} ${(workload?.registerFocus ?? []).includes(item.index) ? 'is-focus' : ''}">
              <div class="vector-register-card__head">
                <span>v${item.index}</span>
                <strong>${item.changed ? 'changed' : item.empty ? 'idle' : 'live'}</strong>
              </div>
              <div class="vector-register-card__lanes">${summarizeVectorRegister(item.value, sewBytes, vl)}</div>
              <div class="vector-register-card__raw">${normalizeVectorHex(item.value)}</div>
            </article>
          `).join('')}
        </div>
      </div>
    `,
    'panel-vector',
  );
}

export function renderOooPanel(snapshot) {
  const ooo = snapshot?.pipeline?.ooo ?? {};
  const flags = snapshot?.pipeline?.flags ?? {};
  const fields = [
    ['rob_depth', ooo.rob_depth ?? 0],
    ['rob_head_sequence_id', ooo.rob_head_sequence_id ?? 0],
    ['lsq_depth', ooo.lsq_depth ?? 0],
    ['lsq_head_sequence_id', ooo.lsq_head_sequence_id ?? 0],
    ['lsq_load_state', ooo.lsq_load_state ?? 'none'],
    ['lsq_load_sequence_id', ooo.lsq_load_sequence_id ?? 0],
    ['lsq_store_sequence_id', ooo.lsq_store_sequence_id ?? 0],
    ['stall_reason', flags.stall_reason ?? 'none'],
  ];

  return card(
    'OoO / 微架构',
    `
      <div class="ooo-grid">
        ${fields.map(([label, value]) => `
          <div class="ooo-metric">
            <span>${label}</span>
            <strong>${value}</strong>
          </div>
        `).join('')}
      </div>
      <div class="ooo-flag-strip">
        <span class="ooo-flag ${flags.replay_flush ? 'is-hot' : ''}">replay_flush: ${flags.replay_flush ? 'true' : 'false'}</span>
        <span class="ooo-flag ${flags.trap_flush ? 'is-hot' : ''}">trap_flush: ${flags.trap_flush ? 'true' : 'false'}</span>
        <span class="ooo-flag ${flags.committed ? 'is-hot' : ''}">committed: ${flags.committed ? 'true' : 'false'}</span>
      </div>
    `,
    'panel-ooo',
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
    '设备状态 · 总线访问 · 事件流',
    [
      renderDevices(snapshot),
      renderBus(snapshot),
      renderEvents(snapshot),
    ],
    'platformGroupOpen',
    isOpen,
    'panel-group-platform',
  );
}
