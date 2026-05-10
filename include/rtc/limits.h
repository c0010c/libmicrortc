#ifndef RTC_LIMITS_H
#define RTC_LIMITS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_sdp_limits_t {
    size_t max_description_bytes;
} rtc_sdp_limits_t;

typedef struct rtc_ice_limits_t {
    size_t max_candidates;
    size_t max_candidate_pairs;
    size_t max_transactions;
    size_t max_timer_slots;
} rtc_ice_limits_t;

typedef struct rtc_dtls_limits_t {
    size_t max_sessions;
    size_t max_session_storage_bytes;
} rtc_dtls_limits_t;

typedef struct rtc_rtp_limits_t {
    size_t max_packet_cache;
} rtc_rtp_limits_t;

typedef struct rtc_rtcp_limits_t {
    size_t max_reports;
} rtc_rtcp_limits_t;

typedef struct rtc_trace_limits_t {
    size_t max_events;
} rtc_trace_limits_t;

typedef struct rtc_peer_connection_limits_t {
    rtc_sdp_limits_t sdp;
    rtc_ice_limits_t ice;
    rtc_dtls_limits_t dtls;
    rtc_rtp_limits_t rtp;
    rtc_rtcp_limits_t rtcp;
    rtc_trace_limits_t trace;
} rtc_peer_connection_limits_t;

#ifdef __cplusplus
}
#endif

#endif
