import { diffVectorRegisters } from '../../state.js';
import { card, renderLaneStrip, renderMetricPill } from './shared.js';
import { activeVectorStages } from './workload.js';

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
