#include <stdlib.h>

#include "webrtc/webrtc.h"

struct webrtc_instance {
    uint32_t reserved;
};

struct webrtc_peer_connection {
    uint32_t reserved;
};

webrtc_status_t webrtc_init(webrtc_instance_t** out_instance)
{
    webrtc_instance_t* instance = NULL;

    if (out_instance == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    instance = (webrtc_instance_t*) malloc(sizeof(webrtc_instance_t));
    if (instance == NULL) {
        *out_instance = NULL;
        return WEBRTC_STATUS_NO_MEMORY;
    }

    instance->reserved = 0U;
    *out_instance = instance;
    return WEBRTC_STATUS_OK;
}

void webrtc_deinit(webrtc_instance_t* instance)
{
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
    if (instance == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    (void) now_ms;
    (void) budget;
    return WEBRTC_STATUS_NOT_IMPLEMENTED;
}
