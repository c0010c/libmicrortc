#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "../src/core/webrtc_event_queue.h"

static void* test_mem_calloc(void* user_data, size_t count, size_t size)
{
    (void) user_data;
    return calloc(count, size);
}

static void test_mem_free(void* user_data, void* ptr)
{
    (void) user_data;
    free(ptr);
}

int main(void)
{
    webrtc_event_queue_t queue;
    webrtc_event_t pushed;
    webrtc_event_t popped;
    bool has_event = false;
    webrtc_status_t status;
    webrtc_pal_vtable_t pal;

    pal.mem_calloc = test_mem_calloc;
    pal.mem_free = test_mem_free;
    pal.user_data = NULL;

    status = webrtc_event_queue_init(&queue, &pal, (uint32_t) 1);
    if (status != WEBRTC_STATUS_OK) {
        return 1;
    }

    pushed.type = (uint32_t) 10;
    pushed.param_u32 = (uint32_t) 100;
    pushed.param_u64 = (uint64_t) 1000;
    pushed.param_ptr = NULL;
    status = webrtc_event_queue_push(&queue, &pushed);
    if (status != WEBRTC_STATUS_OK) {
        webrtc_event_queue_deinit(&queue);
        return 2;
    }

    pushed.type = (uint32_t) 11;
    status = webrtc_event_queue_push(&queue, &pushed);
    if (status != WEBRTC_STATUS_QUEUE_FULL) {
        webrtc_event_queue_deinit(&queue);
        return 3;
    }

    pushed.type = (uint32_t) 12;
    status = webrtc_event_queue_push(&queue, &pushed);
    if (status != WEBRTC_STATUS_QUEUE_FULL) {
        webrtc_event_queue_deinit(&queue);
        return 4;
    }

    status = webrtc_event_queue_pop(&queue, &popped, &has_event);
    if (status != WEBRTC_STATUS_OK || !has_event) {
        webrtc_event_queue_deinit(&queue);
        return 5;
    }

    if (popped.type != (uint32_t) 10) {
        webrtc_event_queue_deinit(&queue);
        return 6;
    }

    pushed.type = (uint32_t) 13;
    pushed.param_u32 = (uint32_t) 130;
    pushed.param_u64 = (uint64_t) 1300;
    pushed.param_ptr = NULL;
    status = webrtc_event_queue_push(&queue, &pushed);
    if (status != WEBRTC_STATUS_OK) {
        webrtc_event_queue_deinit(&queue);
        return 7;
    }

    status = webrtc_event_queue_pop(&queue, &popped, &has_event);
    if (status != WEBRTC_STATUS_OK || !has_event) {
        webrtc_event_queue_deinit(&queue);
        return 8;
    }

    if (popped.type != (uint32_t) 13) {
        webrtc_event_queue_deinit(&queue);
        return 9;
    }

    status = webrtc_event_queue_pop(&queue, &popped, &has_event);
    if (status != WEBRTC_STATUS_OK || has_event) {
        webrtc_event_queue_deinit(&queue);
        return 10;
    }

    webrtc_event_queue_deinit(&queue);
    return 0;
}
