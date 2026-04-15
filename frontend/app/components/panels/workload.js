import { classifyInstructionFlavor } from '../../state.js';
import { card, renderWorkloadTag } from './shared.js';

export function activeVectorStages(snapshot) {
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
