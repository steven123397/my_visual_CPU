function renderStage(label, stage) {
  const valid = stage?.valid;
  return `
    <article class="stage-card ${valid ? 'is-valid' : 'is-empty'}">
      <div class="stage-label">${label}</div>
      <div class="stage-pc">${stage?.pc ?? '0x0'}</div>
      <div class="stage-text">${valid ? stage?.text ?? 'unknown' : 'bubble'}</div>
      <div class="stage-raw">${stage?.raw ?? '0x0'}</div>
    </article>
  `;
}

export function renderPipelineBoard(snapshot) {
  const pipeline = snapshot?.pipeline ?? {};
  const flags = pipeline.flags ?? {};

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
            <span>${row.flag}</span>
            <span>${row.stages.if || '·'}</span>
            <span>${row.stages.id || '·'}</span>
            <span>${row.stages.ex || '·'}</span>
            <span>${row.stages.mem || '·'}</span>
            <span>${row.stages.wb || '·'}</span>
          </div>
        `).join('')}
      </div>
    </section>
  `;
}
