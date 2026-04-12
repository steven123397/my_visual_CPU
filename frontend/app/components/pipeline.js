import { classifyInstructionFlavor } from '../state.js';

function renderVectorBadge(text) {
  const flavor = classifyInstructionFlavor(text);
  if (!flavor) {
    return '';
  }
  return `<span class="vector-chip vector-chip-${flavor.kind}">${flavor.label}</span>`;
}

function stageToneClass(stage) {
  const flavor = classifyInstructionFlavor(stage?.text ?? '');
  if (!flavor) {
    return '';
  }
  return ` stage-card-vector stage-card-vector-${flavor.kind}`;
}

function renderStage(label, stage) {
  const valid = stage?.valid;
  return `
    <article class="stage-card ${valid ? 'is-valid' : 'is-empty'}${stageToneClass(stage)}">
      <div class="stage-label">${label}</div>
      <div class="stage-pc">${stage?.pc ?? '0x0'}</div>
      ${valid ? renderVectorBadge(stage?.text ?? '') : ''}
      <div class="stage-text">${valid ? stage?.text ?? 'unknown' : 'bubble'}</div>
      <div class="stage-raw">${stage?.raw ?? '0x0'}</div>
    </article>
  `;
}

function renderPipelineMeta(flags, ooo) {
  const chips = [
    `<span class="pipeline-meta-chip ${flags.stalled ? 'is-hot' : ''}">stall_reason: ${flags.stall_reason ?? 'none'}</span>`,
  ];

  if (ooo.lsq_load_state && ooo.lsq_load_state !== 'none') {
    chips.push(`<span class="pipeline-meta-chip is-warm">lsq: ${ooo.lsq_load_state}</span>`);
  }

  return `
    <div class="pipeline-meta">
      ${chips.join('')}
    </div>
  `;
}

export function renderPipelineBoard(snapshot) {
  const pipeline = snapshot?.pipeline ?? {};
  const flags = pipeline.flags ?? {};
  const ooo = pipeline.ooo ?? {};

  return `
    <section class="panel panel-pipeline">
      <div class="panel-header">
        <h2>五级流水线</h2>
        <div class="flag-strip">
          <span class="flag-chip ${flags.stalled ? 'is-hot' : ''}">stall</span>
          <span class="flag-chip ${flags.redirected ? 'is-hot' : ''}">redirect</span>
          <span class="flag-chip ${flags.trap_flush ? 'is-hot' : ''}">flush</span>
          <span class="flag-chip ${flags.committed ? 'is-hot' : ''}">commit</span>
        </div>
      </div>
      ${renderPipelineMeta(flags, ooo)}
      <div class="stages-grid">
        ${renderStage('IF', pipeline.if)}
        ${renderStage('ID', pipeline.id)}
        ${renderStage('EX', pipeline.ex)}
        ${renderStage('MEM', pipeline.mem)}
        ${renderStage('WB', pipeline.wb)}
      </div>
    </section>
  `;
}

function renderTimelineCell(text) {
  const flavor = classifyInstructionFlavor(text);
  const toneClass = flavor ? `timeline-stage-vector timeline-stage-vector-${flavor.kind}` : '';
  const vectorHint = flavor ? `<span class="timeline-stage-chip">${flavor.label}</span>` : '';
  return `
    <span class="timeline-stage ${toneClass}">
      <span class="timeline-stage-text">${text || '·'}</span>
      ${vectorHint}
    </span>
  `;
}

export function renderTimeline(rows) {
  return `
    <section class="panel panel-timeline">
      <div class="panel-header">
        <h2>最近周期</h2>
        <span class="muted">${rows.length} cycles</span>
      </div>
      <div class="timeline-table">
        <div class="timeline-head">
          <span>cycle</span><span>flag</span><span>IF</span><span>ID</span><span>EX</span><span>MEM</span><span>WB</span>
        </div>
        ${rows.map((row) => `
          <div class="timeline-row is-${row.flag}">
            <span>${row.cycle}</span>
            <span>${row.flagLabel ?? row.flag}</span>
            ${renderTimelineCell(row.stages.if)}
            ${renderTimelineCell(row.stages.id)}
            ${renderTimelineCell(row.stages.ex)}
            ${renderTimelineCell(row.stages.mem)}
            ${renderTimelineCell(row.stages.wb)}
          </div>
        `).join('')}
      </div>
    </section>
  `;
}
