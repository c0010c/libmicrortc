#ifndef WEBRTC_WEBRTC_H
#define WEBRTC_WEBRTC_H

#include <stddef.h>
#include <stdint.h>

#include "webrtc/webrtc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct webrtc_instance webrtc_instance_t;
typedef struct webrtc_peer_connection webrtc_peer_connection_t;

webrtc_status_t webrtc_init(webrtc_instance_t** out_instance);
void webrtc_deinit(webrtc_instance_t* instance);

webrtc_status_t webrtc_peer_connection_create(webrtc_instance_t* instance, webrtc_peer_connection_t** out_pc);
void webrtc_peer_connection_free(webrtc_peer_connection_t* pc);

webrtc_status_t webrtc_set_local_description(webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len);
webrtc_status_t webrtc_set_remote_description(webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len);
webrtc_status_t webrtc_add_ice_candidate(webrtc_peer_connection_t* pc, const char* candidate, size_t candidate_len);

webrtc_status_t webrtc_pump_step(webrtc_instance_t* instance, uint64_t now_ms, uint32_t budget);

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_WEBRTC_H */
