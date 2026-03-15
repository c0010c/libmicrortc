#ifndef WEBRTC_EVENT_QUEUE_H
#define WEBRTC_EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "webrtc/webrtc_pal.h"
#include "webrtc/webrtc_status.h"

typedef struct webrtc_event {
    uint32_t type;
    uint32_t param_u32;
    uint64_t param_u64;
    void* param_ptr;
} webrtc_event_t;

typedef struct webrtc_event_queue {
    webrtc_event_t* slots;
    webrtc_pal_free_fn mem_free;
    void* mem_user_data;
    uint32_t capacity;
    uint32_t head;
    uint32_t size;
} webrtc_event_queue_t;

webrtc_status_t webrtc_event_queue_init(webrtc_event_queue_t* queue, const webrtc_pal_vtable_t* pal, uint32_t capacity);
void webrtc_event_queue_deinit(webrtc_event_queue_t* queue);
webrtc_status_t webrtc_event_queue_push(webrtc_event_queue_t* queue, const webrtc_event_t* event);
webrtc_status_t webrtc_event_queue_pop(webrtc_event_queue_t* queue, webrtc_event_t* out_event, bool* out_has_event);
uint32_t webrtc_event_queue_size(const webrtc_event_queue_t* queue);

#endif /* WEBRTC_EVENT_QUEUE_H */
