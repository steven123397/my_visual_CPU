import { card } from './shared.js';

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
