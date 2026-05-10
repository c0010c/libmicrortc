#ifndef RTC_PEER_CONNECTION_H
#define RTC_PEER_CONNECTION_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/config.h"
#include "rtc/counters.h"
#include "rtc/media.h"
#include "rtc/status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_peer_connection_t rtc_peer_connection_t;

rtc_status_t rtc_peer_connection_create(const rtc_peer_connection_config_t *config,
                                        rtc_capacity_diagnostics_t *diag,
                                        rtc_peer_connection_t **out_pc);
rtc_status_t rtc_peer_connection_destroy(rtc_peer_connection_t *pc);

rtc_status_t rtc_peer_connection_create_offer(rtc_peer_connection_t *pc,
                                              char *out_sdp,
                                              size_t *inout_sdp_len);
rtc_status_t rtc_peer_connection_create_answer(rtc_peer_connection_t *pc,
                                               char *out_sdp,
                                               size_t *inout_sdp_len);
rtc_status_t rtc_peer_connection_set_local_description(rtc_peer_connection_t *pc,
                                                       const char *sdp,
                                                       size_t sdp_len);
rtc_status_t rtc_peer_connection_set_remote_description(rtc_peer_connection_t *pc,
                                                        const char *sdp,
                                                        size_t sdp_len);
rtc_status_t rtc_peer_connection_add_ice_candidate(rtc_peer_connection_t *pc,
                                                   const char *candidate,
                                                   size_t candidate_len);
rtc_status_t rtc_peer_connection_gather_candidates(rtc_peer_connection_t *pc);
rtc_status_t rtc_peer_connection_start_connectivity_checks(
    rtc_peer_connection_t *pc);
rtc_status_t rtc_peer_connection_receive_datagram(rtc_peer_connection_t *pc,
                                                  const uint8_t *data,
                                                  size_t data_len);
rtc_status_t rtc_peer_connection_send_media_frame(
    rtc_peer_connection_t *pc,
    const rtc_media_frame_t *frame);
rtc_status_t rtc_peer_connection_request_keyframe(
    rtc_peer_connection_t *pc,
    rtc_media_kind_t kind);
rtc_status_t rtc_peer_connection_get_counters(
    rtc_peer_connection_t *pc,
    rtc_peer_connection_counters_t *out_counters);

#ifdef __cplusplus
}
#endif

#endif
