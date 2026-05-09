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
