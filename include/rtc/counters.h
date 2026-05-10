#ifndef RTC_COUNTERS_H
#define RTC_COUNTERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_memory_counters_t {
    uint64_t capacity_errors;
} rtc_memory_counters_t;

typedef struct rtc_executor_counters_t {
    uint64_t affinity_errors;
} rtc_executor_counters_t;

typedef struct rtc_api_counters_t {
    uint64_t create_calls;
    uint64_t destroy_calls;
    uint64_t unsupported_api_calls;
} rtc_api_counters_t;

typedef struct rtc_ice_counters_t {
    uint64_t local_candidates;
    uint64_t remote_candidates;
    uint64_t candidate_pairs;
    uint64_t selected_pairs;
    uint64_t gathering_failures;
    uint64_t checks_failed;
} rtc_ice_counters_t;

typedef struct rtc_stun_counters_t {
    uint64_t transactions_sent;
    uint64_t transactions_received;
    uint64_t transactions_timed_out;
} rtc_stun_counters_t;

typedef struct rtc_net_counters_t {
    uint64_t demux_stun;
    uint64_t demux_dtls;
    uint64_t demux_rtp;
    uint64_t demux_rtcp;
    uint64_t demux_unknown;
} rtc_net_counters_t;

typedef struct rtc_dtls_counters_t {
    uint64_t handshake_started;
    uint64_t handshake_completed;
    uint64_t handshake_failed;
    uint64_t early_datagrams_rejected;
    uint64_t outgoing_datagrams;
    uint64_t fingerprint_mismatch;
    uint64_t key_export_failed;
    uint64_t srtp_init_failed;
} rtc_dtls_counters_t;

typedef struct rtc_srtp_counters_t {
    uint64_t protect_failed;
    uint64_t unprotect_failed;
    uint64_t replay_failed;
} rtc_srtp_counters_t;

typedef struct rtc_rtp_counters_t {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_dropped;
} rtc_rtp_counters_t;

typedef struct rtc_rtcp_counters_t {
    uint64_t rtcp_sr_sent;
    uint64_t rtcp_rr_received;
    uint64_t pli_sent;
    uint64_t pli_received;
    uint64_t nack_received;
    uint64_t nack_no_retransmit;
} rtc_rtcp_counters_t;

typedef struct rtc_media_counters_t {
    uint64_t frames_sent;
    uint64_t frames_received;
    uint64_t h264_reassembly_drops;
    uint64_t media_queue_full;
} rtc_media_counters_t;

typedef struct rtc_trace_counters_t {
    uint64_t trace_events;
} rtc_trace_counters_t;

typedef struct rtc_peer_connection_counters_t {
    rtc_memory_counters_t memory;
    rtc_executor_counters_t executor;
    rtc_api_counters_t api;
    rtc_ice_counters_t ice;
    rtc_stun_counters_t stun;
    rtc_net_counters_t net;
    rtc_dtls_counters_t dtls;
    rtc_srtp_counters_t srtp;
    rtc_rtp_counters_t rtp;
    rtc_rtcp_counters_t rtcp;
    rtc_media_counters_t media;
    rtc_trace_counters_t trace;
} rtc_peer_connection_counters_t;

#ifdef __cplusplus
}
#endif

#endif
