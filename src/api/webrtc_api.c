#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "webrtc/webrtc.h"
#include "src/core/webrtc_event_queue.h"

struct webrtc_instance {
    webrtc_config_t config;
    webrtc_pal_vtable_t pal;
    webrtc_event_queue_t event_queue;
};

struct webrtc_peer_connection {
    uint32_t reserved;
};

static webrtc_status_t webrtc_validate_pal(const webrtc_pal_vtable_t* pal)
{
    if (pal == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    if (pal->mem_malloc == NULL || pal->mem_calloc == NULL || pal->mem_free == NULL ||
        pal->time_monotonic_ms == NULL || pal->lock_create == NULL || pal->lock_destroy == NULL ||
        pal->lock_acquire == NULL || pal->lock_release == NULL || pal->socket_open_udp == NULL ||
        pal->socket_bind == NULL || pal->socket_sendto == NULL || pal->socket_recvfrom == NULL ||
        pal->socket_set_nonblocking == NULL || pal->socket_close == NULL || pal->random_fill == NULL) {
        return WEBRTC_STATUS_GENERAL_PAL_INCOMPLETE;
    }

    return WEBRTC_STATUS_OK;
}

webrtc_status_t webrtc_init(const webrtc_config_t* config, const webrtc_pal_vtable_t* pal, webrtc_instance_t** out_instance)
{
    webrtc_instance_t* instance = NULL;
    webrtc_config_t effective_config;
    webrtc_status_t status = WEBRTC_STATUS_OK;

    if (out_instance == NULL || config == NULL || pal == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    *out_instance = NULL;

    status = webrtc_validate_pal(pal);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    effective_config = *config;
    status = webrtc_config_validate(&effective_config);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    instance = (webrtc_instance_t*) pal->mem_calloc(pal->user_data, 1U, sizeof(webrtc_instance_t));
    if (instance == NULL) {
        return WEBRTC_STATUS_NO_MEMORY;
    }

    instance->config = effective_config;
    instance->pal = *pal;

    status = webrtc_event_queue_init(&instance->event_queue, pal, effective_config.event_queue_capacity);
    if (status != WEBRTC_STATUS_OK) {
        pal->mem_free(pal->user_data, instance);
        return status;
    }

    *out_instance = instance;

    return WEBRTC_STATUS_OK;
}

void webrtc_deinit(webrtc_instance_t* instance)
{
    if (instance == NULL) {
        return;
    }

    webrtc_event_queue_deinit(&instance->event_queue);

    if (instance->pal.mem_free != NULL) {
        instance->pal.mem_free(instance->pal.user_data, instance);
        return;
    }

    free(instance);
}

webrtc_status_t webrtc_peer_connection_create(webrtc_instance_t* instance, webrtc_peer_connection_t** out_pc)
{
    if (instance == NULL || out_pc == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    *out_pc = NULL;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

void webrtc_peer_connection_free(webrtc_peer_connection_t* pc)
{
    (void) pc;
}

webrtc_status_t webrtc_set_local_description(webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len)
{
    if (pc == NULL || sdp == NULL || sdp_len == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

webrtc_status_t webrtc_set_remote_description(webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len)
{
    if (pc == NULL || sdp == NULL || sdp_len == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

webrtc_status_t webrtc_add_ice_candidate(webrtc_peer_connection_t* pc, const char* candidate, size_t candidate_len)
{
    if (pc == NULL || candidate == NULL || candidate_len == 0U) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}

webrtc_status_t webrtc_pump_step(webrtc_instance_t* instance, uint64_t now_ms, uint32_t budget)
{
    uint32_t i = 0U;
    bool has_event = false;
    webrtc_event_t event;
    webrtc_status_t status = WEBRTC_STATUS_OK;

    if (instance == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    (void) now_ms;
    if (budget == 0U) {
        return WEBRTC_STATUS_OK;
    }

    for (i = 0U; i < budget; ++i) {
        status = webrtc_event_queue_pop(&instance->event_queue, &event, &has_event);
        if (status != WEBRTC_STATUS_OK) {
            return status;
        }

        if (!has_event) {
            break;
        }

        /*
         * Event dispatch hooks will be added by later protocol chunks.
         * For id=21~23 we only need bounded draining behavior.
         */
        (void) event.type;
        (void) event.param_u32;
        (void) event.param_u64;
        (void) event.param_ptr;
    }

    return WEBRTC_STATUS_OK;
}
