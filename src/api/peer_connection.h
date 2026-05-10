#ifndef RTC_API_PEER_CONNECTION_INTERNAL_H
#define RTC_API_PEER_CONNECTION_INTERNAL_H

#include <stddef.h>

#include "jsep/jsep.h"
#include "memory/arena.h"
#include "rtc/peer_connection.h"
#include "sdp/sdp.h"

#define RTC_PC_REMOTE_CANDIDATE_SLOT_BYTES 512u

struct rtc_peer_connection_t {
    rtc_arena_view_t arena;
    rtc_peer_connection_limits_t limits;
    rtc_sdp_parameters_t sdp;
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
    size_t remote_candidate_count;
    int is_closed;
};

#endif
