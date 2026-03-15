#include "src/core/webrtc_event_queue.h"

#include <stddef.h>
#include <string.h>

webrtc_status_t webrtc_event_queue_init(webrtc_event_queue_t* queue, const webrtc_pal_vtable_t* pal, uint32_t capacity)
{
    if (queue == NULL || pal == NULL || pal->mem_calloc == NULL || pal->mem_free == NULL || capacity == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    memset(queue, 0, sizeof(*queue));

    queue->slots = (webrtc_event_t*) pal->mem_calloc(pal->user_data, capacity, sizeof(webrtc_event_t));
    if (queue->slots == NULL) {
        return WEBRTC_STATUS_NO_MEMORY;
    }

    queue->mem_free = pal->mem_free;
    queue->mem_user_data = pal->user_data;
    queue->capacity = capacity;
    queue->head = 0U;
    queue->size = 0U;

    return WEBRTC_STATUS_OK;
}

void webrtc_event_queue_deinit(webrtc_event_queue_t* queue)
{
    if (queue == NULL) {
        return;
    }

    if (queue->slots != NULL && queue->mem_free != NULL) {
        queue->mem_free(queue->mem_user_data, queue->slots);
    }

    memset(queue, 0, sizeof(*queue));
}

webrtc_status_t webrtc_event_queue_push(webrtc_event_queue_t* queue, const webrtc_event_t* event)
{
    uint32_t tail = 0U;

    if (queue == NULL || event == NULL || queue->slots == NULL || queue->capacity == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    if (queue->size >= queue->capacity) {
        return WEBRTC_STATUS_QUEUE_FULL;
    }

    tail = queue->head + queue->size;
    if (tail >= queue->capacity) {
        tail -= queue->capacity;
    }

    queue->slots[tail] = *event;
    queue->size++;

    return WEBRTC_STATUS_OK;
}

webrtc_status_t webrtc_event_queue_pop(webrtc_event_queue_t* queue, webrtc_event_t* out_event, bool* out_has_event)
{
    if (queue == NULL || out_event == NULL || out_has_event == NULL || queue->slots == NULL || queue->capacity == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    if (queue->size == 0U) {
        *out_has_event = false;
        return WEBRTC_STATUS_OK;
    }

    *out_event = queue->slots[queue->head];
    queue->head++;
    if (queue->head >= queue->capacity) {
        queue->head = 0U;
    }
    queue->size--;
    *out_has_event = true;

    return WEBRTC_STATUS_OK;
}

uint32_t webrtc_event_queue_size(const webrtc_event_queue_t* queue)
{
    if (queue == NULL) {
        return 0U;
    }

    return queue->size;
}
