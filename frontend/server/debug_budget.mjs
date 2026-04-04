export const DEBUG_CLI_SESSION_BUDGET = Object.freeze({
  requestTimeoutMs: 1500,
  closeWaitTimeoutMs: 200,
});

export const DEFAULT_DEBUG_RUN_RATE_HZ = 8;

export const INTERACTIVE_OS_BOOT_BUDGET = Object.freeze({
  maxSteps: 5000000,
  prompt: 'monitor> ',
});

export const TERMINAL_ACTIVITY_BUDGET = Object.freeze({
  defaultMaxCommits: 4096,
  defaultSettleCommits: 256,
});

export const TERMINAL_ADVANCE_NEWLINE_BUDGET = Object.freeze({
  maxCommits: 16384,
  settleCommits: 1024,
  idleCommitsWithoutOutput: 128,
});

export const TERMINAL_ADVANCE_CONTROL_BUDGET = Object.freeze({
  minMaxCommits: 1024,
  commitsPerChar: 1024,
  settleCommits: 64,
  idleCommitsWithoutOutput: 64,
});

export const TERMINAL_ADVANCE_VISIBLE_BUDGET = Object.freeze({
  minMaxCommits: 2048,
  commitsPerChar: 1024,
  settleCommits: 256,
  idleCommitsWithoutOutput: 64,
});

export const INTERACTIVE_OS_COMMAND_BUDGET = Object.freeze({
  maxSteps: 500000,
});

export const interactiveOsBudgets = Object.freeze({
  bootMaxSteps: INTERACTIVE_OS_BOOT_BUDGET.maxSteps,
  commandMaxSteps: INTERACTIVE_OS_COMMAND_BUDGET.maxSteps,
  prompt: INTERACTIVE_OS_BOOT_BUDGET.prompt,
});

export const terminalLimits = Object.freeze({
  stepCommitBudget: TERMINAL_ACTIVITY_BUDGET.defaultMaxCommits,
  textCommitScale: TERMINAL_ADVANCE_CONTROL_BUDGET.commitsPerChar,
  newline: {
    maxCommits: TERMINAL_ADVANCE_NEWLINE_BUDGET.maxCommits,
    settleCommits: TERMINAL_ADVANCE_NEWLINE_BUDGET.settleCommits,
    idleCommitsWithoutOutput: TERMINAL_ADVANCE_NEWLINE_BUDGET.idleCommitsWithoutOutput,
  },
  control: {
    minMaxCommits: TERMINAL_ADVANCE_CONTROL_BUDGET.minMaxCommits,
    settleCommits: TERMINAL_ADVANCE_CONTROL_BUDGET.settleCommits,
    idleCommitsWithoutOutput: TERMINAL_ADVANCE_CONTROL_BUDGET.idleCommitsWithoutOutput,
  },
  ascii: {
    minMaxCommits: TERMINAL_ADVANCE_VISIBLE_BUDGET.minMaxCommits,
    settleCommits: TERMINAL_ADVANCE_VISIBLE_BUDGET.settleCommits,
    idleCommitsWithoutOutput: TERMINAL_ADVANCE_VISIBLE_BUDGET.idleCommitsWithoutOutput,
  },
});
