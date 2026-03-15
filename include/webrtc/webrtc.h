#ifndef WEBRTC_WEBRTC_H
#define WEBRTC_WEBRTC_H

#include <stddef.h>
#include <stdint.h>

#include "webrtc/webrtc_config.h"
#include "webrtc/webrtc_pal.h"
#include "webrtc/webrtc_pump.h"
#include "webrtc/webrtc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct webrtc_peer_connection webrtc_peer_connection_t;

/*
 * Initializes a WebRTC instance.
 * Ownership: caller provides config/pal; library copies them internally.
 */
webrtc_status_t webrtc_init(const webrtc_config_t* config, const webrtc_pal_vtable_t* pal, webrtc_instance_t** out_instance);

/*
 * Deinitializes and releases a WebRTC instance created by webrtc_init.
 */
void webrtc_deinit(webrtc_instance_t* instance);

/*
 * Creates/frees a peer connection object bound to a webrtc_instance.
 */
webrtc_status_t webrtc_peer_connection_create(webrtc_instance_t* instance, webrtc_peer_connection_t** out_pc);
void webrtc_peer_connection_free(webrtc_peer_connection_t* pc);

webrtc_status_t webrtc_set_local_description(webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len);
webrtc_status_t webrtc_set_remote_description(webrtc_peer_connection_t* pc, const char* sdp, size_t sdp_len);
webrtc_status_t webrtc_add_ice_candidate(webrtc_peer_connection_t* pc, const char* candidate, size_t candidate_len);

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_WEBRTC_H */
