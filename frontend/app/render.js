import { buildTimelineRows, diffRegisters, shouldAutoScrollToBottom } from './state.js';
import { renderPipelineBoard, renderTimeline } from './components/pipeline.js';
import { renderSummary, renderEvents, renderDevices, renderRegisters, renderCsrs, renderBus } from './components/panels.js';

export function renderApp(elements, state) {
  const snapshot = state.currentSnapshot;
  const previous = state.history.length > 1 ? state.history[state.history.length - 2] : null;
  const registers = diffRegisters(previous, snapshot);
  const timelineRows = buildTimelineRows(state.history).slice().reverse();
  const previousEventList = elements.events.querySelector('.event-list');
  const keepEventsPinned = shouldAutoScrollToBottom(previousEventList);

  elements.summary.innerHTML = renderSummary(snapshot, state.runState);
  elements.pipeline.innerHTML = `${renderPipelineBoard(snapshot)}${renderTimeline(timelineRows)}`;
  elements.events.innerHTML = renderEvents(snapshot);
  elements.devices.innerHTML = renderDevices(snapshot);
  elements.registers.innerHTML = renderRegisters(registers);
  elements.csrs.innerHTML = renderCsrs(snapshot);
  elements.bus.innerHTML = renderBus(snapshot);

  const nextEventList = elements.events.querySelector('.event-list');
  if (nextEventList && keepEventsPinned) {
    nextEventList.scrollTop = nextEventList.scrollHeight;
  }
}

export function updateControls(elements, state) {
  elements.testSelect.innerHTML = state.tests.map((item) => `
    <option value="${item.name}" ${item.name === state.selectedTest ? 'selected' : ''}>
      ${item.name}${item.hasDisk ? ' [disk]' : ''}
    </option>
  `).join('');
  elements.backendSelect.value = state.backend;
  elements.statusBadge.textContent = state.runState;
}
