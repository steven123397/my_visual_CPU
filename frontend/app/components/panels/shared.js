export function card(title, body, extraClass = '') {
  return `
    <section class="panel ${extraClass}">
      <div class="panel-header"><h2>${title}</h2></div>
      ${body}
    </section>
  `;
}

export function renderWorkloadTag(text, tone = 'neutral') {
  return `<span class="workload-tag workload-tag-${tone}">${text}</span>`;
}

export function renderMetricPill(label, value) {
  return `
    <div class="metric-pill">
      <span>${label}</span>
      <strong>${value}</strong>
    </div>
  `;
}

export function renderLaneStrip(title, values, emphasis = 'neutral') {
  return `
    <div class="lane-strip lane-strip-${emphasis}">
      <span class="lane-strip__title">${title}</span>
      <div class="lane-strip__values">
        ${values.map((value) => `<strong>${value}</strong>`).join('')}
      </div>
    </div>
  `;
}

export function groupPanel(title, detail, panels, layoutKey, isOpen = false, extraClass = '') {
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
