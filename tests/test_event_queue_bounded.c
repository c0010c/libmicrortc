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
    webrtc_event_t event;
    webrtc_status_t status;
    webrtc_pal_vtable_t pal;

    pal.mem_calloc = test_mem_calloc;
    pal.mem_free = test_mem_free;
    pal.user_data = NULL;

    status = webrtc_event_queue_init(&queue, &pal, (uint32_t) 2);
    if (status != WEBRTC_STATUS_OK) {
        return 1;
    }

    event.type = (uint32_t) 1;
    event.param_u32 = (uint32_t) 11;
    event.param_u64 = (uint64_t) 111;
    event.param_ptr = NULL;
    status = webrtc_event_queue_push(&queue, &event);
    if (status != WEBRTC_STATUS_OK) {
        webrtc_event_queue_deinit(&queue);
        return 2;
    }

    event.type = (uint32_t) 2;
    event.param_u32 = (uint32_t) 22;
    event.param_u64 = (uint64_t) 222;
    event.param_ptr = NULL;
    status = webrtc_event_queue_push(&queue, &event);
    if (status != WEBRTC_STATUS_OK) {
        webrtc_event_queue_deinit(&queue);
        return 3;
    }

    event.type = (uint32_t) 3;
    event.param_u32 = (uint32_t) 33;
    event.param_u64 = (uint64_t) 333;
    event.param_ptr = NULL;
    status = webrtc_event_queue_push(&queue, &event);
    if (status != WEBRTC_STATUS_QUEUE_FULL) {
        webrtc_event_queue_deinit(&queue);
        return 4;
    }

    if (webrtc_event_queue_size(&queue) != (uint32_t) 2) {
        webrtc_event_queue_deinit(&queue);
        return 5;
    }

    webrtc_event_queue_deinit(&queue);
    return 0;
}
