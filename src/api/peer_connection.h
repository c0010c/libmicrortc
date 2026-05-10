#ifndef RTC_API_PEER_CONNECTION_INTERNAL_H
#define RTC_API_PEER_CONNECTION_INTERNAL_H

#include <stddef.h>

#include "jsep/jsep.h"
#include "memory/arena.h"
#include "rtc/peer_connection.h"
#include "sdp/sdp.h"

#define RTC_PC_REMOTE_CANDIDATE_SLOT_BYTES 512u

typedef enum rtc_ice_candidate_type_t {
    RTC_ICE_CANDIDATE_TYPE_HOST = 0,
    RTC_ICE_CANDIDATE_TYPE_SRFLX
} rtc_ice_candidate_type_t;

typedef struct rtc_ice_candidate_summary_t {
    rtc_ice_candidate_type_t type;
    char foundation[32];
    char transport[8];
    char address[64];
    uint32_t priority;
    uint16_t component;
    uint16_t port;
} rtc_ice_candidate_summary_t;

typedef struct rtc_ice_candidate_pair_t {
    size_t local_candidate_id;
    size_t remote_candidate_id;
    uint64_t priority;
    int selected;
} rtc_ice_candidate_pair_t;

typedef struct rtc_stun_transaction_t {
    uint8_t transaction_id[12];
    uint64_t timer_id;
    int in_use;
} rtc_stun_transaction_t;

struct rtc_peer_connection_t {
    rtc_arena_view_t arena;
    rtc_peer_connection_limits_t limits;
    rtc_sdp_parameters_t sdp;
    rtc_stun_server_t stun_server;
    size_t stun_server_count;
    rtc_executors_t executors;
    rtc_observer_vtable_t observer;
    rtc_peer_connection_counters_t counters;
    rtc_jsep_state_t signaling_state;
    rtc_sdp_description_t local_summary;
    rtc_sdp_description_t remote_summary;
    char *local_description;
    char *remote_description;
    size_t local_description_len;
    size_t remote_description_len;
    char *remote_candidates;
    size_t remote_candidate_slot_bytes;
    rtc_ice_candidate_summary_t *local_candidate_summaries;
    rtc_ice_candidate_summary_t *remote_candidate_summaries;
    rtc_ice_candidate_pair_t *candidate_pairs;
    rtc_stun_transaction_t *stun_transactions;
    size_t local_candidate_count;
    size_t remote_candidate_count;
    size_t candidate_pair_count;
    size_t stun_transaction_count;
    int connectivity_checks_started;
    int remote_candidate_pending_pairs;
    int is_closed;
};

#endif
