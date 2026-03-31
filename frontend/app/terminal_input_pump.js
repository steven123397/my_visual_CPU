export function createTerminalInputPump({
  sendInput,
  onResponse = () => {},
  onError = () => {},
  onPendingChange = () => {},
} = {}) {
  let queuedText = '';
  let inFlight = false;
  let flushScheduled = false;
  let generation = 0;
  let pending = false;

  function setPending(nextPending) {
    if (pending === nextPending) {
      return;
    }
    pending = nextPending;
    onPendingChange(nextPending);
  }

  async function flushQueue() {
    if (inFlight) {
      return;
    }

    while (queuedText.length > 0) {
      const batch = queuedText;
      const requestGeneration = generation;

      queuedText = '';
      inFlight = true;
      setPending(true);
      try {
        const response = await sendInput(batch);
        if (requestGeneration === generation) {
          onResponse(response, batch);
        }
      } catch (error) {
        if (requestGeneration === generation) {
          onError(error, batch);
        }
      } finally {
        inFlight = false;
      }
    }

    setPending(false);
  }

  function scheduleFlush() {
    if (flushScheduled) {
      return;
    }
    flushScheduled = true;
    queueMicrotask(async () => {
      flushScheduled = false;
      await flushQueue();
    });
  }

  return {
    enqueue(text) {
      if (!text) {
        return;
      }
      queuedText += text;
      setPending(true);
      scheduleFlush();
    },
    reset() {
      generation += 1;
      queuedText = '';
      setPending(false);
    },
  };
}
