/**
 * 终端输入泵：把浏览器侧的输入文本排队后异步批量发给 debug server，
 * 跟踪 pending 状态并在响应/错误时回调，避免并发请求互相覆盖。
 * @param {Object} options
 * @param {Function} options.sendInput - 发送终端文本的异步 API 函数
 * @param {Function} [options.onResponse] - 成功响应回调
 * @param {Function} [options.onError] - 异常回调
 * @param {Function} [options.onPendingChange] - Pending 状态变化回调
 * @returns {Object} 包含 enqueue 和 reset 方法的输入泵实例
 */
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

  /**
   * 更新 pending 标记并通知 onPendingChange。
   * @param {boolean} nextPending
   */
  function setPending(nextPending) {
    if (pending === nextPending) {
      return;
    }
    pending = nextPending;
    onPendingChange(nextPending);
  }

  /**
   * 串行排空队列：每次取全部排队文本批量发送，期间置 pending，结束清掉。
   */
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

  /**
   * 用 microtask 调度一次 flush，避免同 tick 多次 enqueue 触发并发请求。
   */
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
