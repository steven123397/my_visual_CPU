import { buildTimelineRows, diffRegisters, shouldAutoScrollToBottom } from './state.js';
import { renderTerminal } from './components/terminal.js';
import { renderPipelineBoard, renderTimeline } from './components/pipeline.js';
import {
  renderSummary,
  renderPredictor,
  renderOooPanel,
  renderArchitectureGroup,
  renderPlatformGroup,
} from './components/panels.js';

function queryEventList(...slots) {
  for (const slot of slots) {
    const list = slot?.querySelector?.('.event-list');
    if (list) {
      return list;
    }
  }
  return null;
}

export function renderApp(elements, state) {
  const snapshot = state.currentSnapshot;
  const previous = state.history.length > 1 ? state.history[state.history.length - 2] : null;
  const registers = diffRegisters(previous, snapshot);
  const timelineRows = buildTimelineRows(state.history).slice().reverse();
  const previousEventList = queryEventList(elements.devices, elements.events);
  const keepEventsPinned = shouldAutoScrollToBottom(previousEventList);
  const previousTerminal = elements.terminal.querySelector('.terminal-scrollport');
  const keepTerminalPinned = shouldAutoScrollToBottom(previousTerminal);
  const previousTerminalScrollTop = previousTerminal?.scrollTop ?? 0;

  elements.desktop.dataset.debugOpen = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.desktop.dataset.terminalCollapsed = state.layout.terminalCollapsed ? 'true' : 'false';
  elements.debugInspector.dataset.open = state.layout.debugPanelOpen ? 'true' : 'false';
  elements.terminal.innerHTML = renderTerminal(state);
  elements.summary.innerHTML = renderSummary(snapshot, state.runState);
  elements.predictor.innerHTML = renderPredictor(snapshot);
  elements.pipeline.innerHTML = `${renderPipelineBoard(snapshot)}${renderTimeline(timelineRows)}`;
  elements.events.innerHTML = renderOooPanel(snapshot);
  elements.devices.innerHTML = renderPlatformGroup(snapshot, state.layout.platformGroupOpen);
  elements.registers.innerHTML = renderArchitectureGroup(snapshot, registers, state.layout.architectureGroupOpen);
  elements.csrs.innerHTML = '';
  elements.bus.innerHTML = '';

  const nextEventList = queryEventList(elements.devices, elements.events);
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
}
