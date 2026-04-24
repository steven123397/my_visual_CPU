#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/ai_accel.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_queue_init_validation(void) {
    ai_accel_queue_state_t state;
    ai_accel_submission_descriptor_t submit_queue[2];
    ai_accel_completion_entry_t completion_queue[2];

    if (ai_accel_queue_state_init(NULL,
                                  submit_queue,
                                  2U,
                                  completion_queue,
                                  2U)) {
        return fail("expected queue init to reject null state");
    }
    if (ai_accel_queue_state_init(&state,
                                  NULL,
                                  2U,
                                  completion_queue,
                                  2U)) {
        return fail("expected queue init to reject null submit queue");
    }
    if (ai_accel_queue_state_init(&state,
                                  submit_queue,
                                  2U,
                                  NULL,
                                  2U)) {
        return fail("expected queue init to reject null completion queue");
    }
    if (ai_accel_queue_state_init(&state,
                                  submit_queue,
                                  0U,
                                  completion_queue,
                                  2U)) {
        return fail("expected queue init to reject zero submit entries");
    }
    if (ai_accel_queue_state_init(&state,
                                  submit_queue,
                                  2U,
                                  completion_queue,
                                  0U)) {
        return fail("expected queue init to reject zero completion entries");
    }
    if (ai_accel_queue_state_init(&state,
                                  submit_queue,
                                  AI_ACCEL_QUEUE_MAX_ENTRIES + 1U,
                                  completion_queue,
                                  2U)) {
        return fail("expected queue init to reject oversized submit queue");
    }
    if (ai_accel_queue_state_init(&state,
                                  submit_queue,
                                  2U,
                                  completion_queue,
                                  AI_ACCEL_QUEUE_MAX_ENTRIES + 1U)) {
        return fail("expected queue init to reject oversized completion queue");
    }
    if (!ai_accel_queue_state_init(&state,
                                   submit_queue,
                                   2U,
                                   completion_queue,
                                   2U)) {
        return fail("expected queue init to accept valid queues");
    }

    return state.submit_head == 0U && state.submit_tail == 0U &&
                   state.completion_head == 0U && state.completion_tail == 0U
               ? 0
               : fail("expected queue init to zero queue cursors");
}

static int test_submission_enqueue_contract(void) {
    ai_accel_queue_state_t state;
    ai_accel_submission_descriptor_t submit_queue[2];
    ai_accel_completion_entry_t completion_queue[2];
    uint32_t submit_tail = 0;
    const ai_accel_submission_descriptor_t descriptor1 = {.token = 1U};
    const ai_accel_submission_descriptor_t descriptor2 = {.token = 2U};
    const ai_accel_submission_descriptor_t descriptor3 = {.token = 3U};

    memset(submit_queue, 0, sizeof(submit_queue));
    memset(completion_queue, 0, sizeof(completion_queue));
    if (!ai_accel_queue_state_init(&state,
                                   submit_queue,
                                   2U,
                                   completion_queue,
                                   2U)) {
        return fail("expected queue init before submission enqueue test");
    }

    if (ai_accel_queue_enqueue_submission(NULL, 0U, &descriptor1, &submit_tail)) {
        return fail("expected enqueue to reject null state");
    }
    if (ai_accel_queue_enqueue_submission(&state, 0U, NULL, &submit_tail)) {
        return fail("expected enqueue to reject null descriptor");
    }

    if (!ai_accel_queue_enqueue_submission(&state, 0U, &descriptor1, &submit_tail) ||
        submit_tail != 1U ||
        submit_queue[0].token != descriptor1.token) {
        return fail("expected first enqueue to write slot 0 and advance tail");
    }
    if (!ai_accel_queue_enqueue_submission(&state, 0U, &descriptor2, &submit_tail) ||
        submit_tail != 2U ||
        submit_queue[1].token != descriptor2.token) {
        return fail("expected second enqueue to write slot 1 and advance tail");
    }
    if (ai_accel_queue_enqueue_submission(&state, 0U, &descriptor3, &submit_tail)) {
        return fail("expected enqueue to reject a full submit queue");
    }
    if (ai_accel_queue_enqueue_submission(&state, 3U, &descriptor3, &submit_tail)) {
        return fail("expected enqueue to reject submit head ahead of tail");
    }
    if (!ai_accel_queue_enqueue_submission(&state, 1U, &descriptor3, &submit_tail) ||
        submit_tail != 3U ||
        submit_queue[0].token != descriptor3.token) {
        return fail("expected enqueue to wrap and reuse freed slot");
    }

    return 0;
}

static int test_completion_tail_and_dequeue_contract(void) {
    ai_accel_queue_state_t state;
    ai_accel_submission_descriptor_t submit_queue[2];
    ai_accel_completion_entry_t completion_queue[2];
    ai_accel_completion_entry_t completion;
    uint32_t completion_head = 0;

    memset(submit_queue, 0, sizeof(submit_queue));
    memset(completion_queue, 0, sizeof(completion_queue));
    completion_queue[0].token = 11U;
    completion_queue[1].token = 12U;
    if (!ai_accel_queue_state_init(&state,
                                   submit_queue,
                                   2U,
                                   completion_queue,
                                   2U)) {
        return fail("expected queue init before completion dequeue test");
    }

    if (ai_accel_queue_sync_completion_tail(NULL, 1U)) {
        return fail("expected completion tail sync to reject null state");
    }
    if (!ai_accel_queue_sync_completion_tail(&state, 1U)) {
        return fail("expected completion tail sync to accept first completion");
    }
    if (ai_accel_queue_sync_completion_tail(&state, 0U)) {
        return fail("expected completion tail sync to reject tail regression");
    }
    if (ai_accel_queue_dequeue_completion(NULL, &completion, &completion_head)) {
        return fail("expected completion dequeue to reject null state");
    }
    if (ai_accel_queue_dequeue_completion(&state, NULL, &completion_head)) {
        return fail("expected completion dequeue to reject null completion output");
    }
    if (!ai_accel_queue_dequeue_completion(&state, &completion, &completion_head) ||
        completion.token != 11U ||
        completion_head != 1U) {
        return fail("expected completion dequeue to return slot 0 and advance head");
    }
    if (ai_accel_queue_dequeue_completion(&state, &completion, &completion_head)) {
        return fail("expected completion dequeue to reject empty queue");
    }

    if (!ai_accel_queue_state_init(&state,
                                   submit_queue,
                                   2U,
                                   completion_queue,
                                   2U)) {
        return fail("expected queue re-init before wrap test");
    }
    state.completion_head = 1U;
    completion_queue[1].token = 21U;
    completion_queue[0].token = 22U;
    if (!ai_accel_queue_sync_completion_tail(&state, 3U)) {
        return fail("expected completion tail sync to allow wrapped completions");
    }
    if (!ai_accel_queue_dequeue_completion(&state, &completion, &completion_head) ||
        completion.token != 21U ||
        completion_head != 2U) {
        return fail("expected wrapped completion dequeue to return slot 1 first");
    }
    if (!ai_accel_queue_dequeue_completion(&state, &completion, &completion_head) ||
        completion.token != 22U ||
        completion_head != 3U) {
        return fail("expected wrapped completion dequeue to return slot 0 second");
    }

    if (!ai_accel_queue_state_init(&state,
                                   submit_queue,
                                   1U,
                                   completion_queue,
                                   1U)) {
        return fail("expected queue re-init before overflow test");
    }
    return !ai_accel_queue_sync_completion_tail(&state, 2U)
               ? 0
               : fail("expected completion tail sync to reject queue overflow");
}

int main(void) {
    if (test_queue_init_validation() != 0 ||
        test_submission_enqueue_contract() != 0 ||
        test_completion_tail_and_dequeue_contract() != 0) {
        return 1;
    }

    return 0;
}
