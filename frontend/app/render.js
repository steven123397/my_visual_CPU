import { buildTimelineRows, diffRegisters, shouldAutoScrollToBottom } from './state.js';
import { renderTerminal } from './components/terminal.js';
import { renderPipelineBoard, renderTimeline } from './components/pipeline.js';
import { renderSummary, renderEvents, renderDevices, renderRegisters, renderCsrs, renderBus } from './components/panels.js';

export function renderApp(elements, state) {
  const snapshot = state.currentSnapshot;
  const previous = state.history.length > 1 ? state.history[state.history.length - 2] : null;
  const registers = diffRegisters(previous, snapshot);
  const timelineRows = buildTimelineRows(state.history).slice().reverse();
  const previousEventList = elements.events.querySelector('.event-list');
  const keepEventsPinned = shouldAutoScrollToBottom(previousEventList);
  const previousTerminal = elements.terminal.querySelector('.terminal-scrollport');
  const keepTerminalPinned = shouldAutoScrollToBottom(previousTerminal);
  const previousTerminalScrollTop = previousTerminal?.scrollTop ?? 0;

  elements.desktop.dataset.debugOpen = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.debugInspector.dataset.open = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.terminal.innerHTML = renderTerminal(state);
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

  const nextTerminal = elements.terminal.querySelector('.terminal-scrollport');
  if (nextTerminal) {
    if (keepTerminalPinned) {
      nextTerminal.scrollTop = nextTerminal.scrollHeight;
    } else {
      nextTerminal.scrollTop = previousTerminalScrollTop;
    }
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
  elements.toggleDebugButton.textContent = state.layout.debugPanelOpen ? '收起 Debug' : '展开 Debug';
  elements.toggleDebugButton.setAttribute('aria-expanded', state.layout.debugPanelOpen ? 'true' : 'false');
}
