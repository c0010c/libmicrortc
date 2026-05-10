#ifndef RTC_API_PEER_CONNECTION_INTERNAL_H
#define RTC_API_PEER_CONNECTION_INTERNAL_H

#include <stddef.h>

#include "ice/ice.h"
#include "jsep/jsep.h"
#include "memory/arena.h"
#include "rtc/peer_connection.h"
#include "rtc/security.h"
#include "sdp/sdp.h"

#define RTC_PC_REMOTE_CANDIDATE_SLOT_BYTES 512u

typedef enum rtc_ice_candidate_type_t {
    RTC_ICE_CANDIDATE_TYPE_HOST = 0,
    RTC_ICE_CANDIDATE_TYPE_SRFLX
} rtc_ice_candidate_type_t;

typedef enum rtc_ice_pair_state_t {
    RTC_ICE_PAIR_FROZEN = 0,
    RTC_ICE_PAIR_WAITING,
    RTC_ICE_PAIR_IN_PROGRESS,
    RTC_ICE_PAIR_SUCCEEDED,
    RTC_ICE_PAIR_FAILED,
    RTC_ICE_PAIR_NOMINATED,
    RTC_ICE_PAIR_SELECTED
} rtc_ice_pair_state_t;

typedef enum rtc_ice_role_t {
    RTC_ICE_ROLE_UNKNOWN = 0,
    RTC_ICE_ROLE_CONTROLLING,
    RTC_ICE_ROLE_CONTROLLED
} rtc_ice_role_t;

typedef enum rtc_security_dtls_state_t {
    RTC_SECURITY_DTLS_NEW = 0,
    RTC_SECURITY_DTLS_CONNECTING,
    RTC_SECURITY_DTLS_CONNECTED,
    RTC_SECURITY_DTLS_FAILED
} rtc_security_dtls_state_t;

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
    rtc_ice_pair_state_t state;
    int selected;
} rtc_ice_candidate_pair_t;

typedef struct rtc_stun_transaction_t {
    uint8_t transaction_id[12];
    uint64_t timer_id;
    size_t pair_id;
    int purpose;
    int in_use;
} rtc_stun_transaction_t;

struct rtc_peer_connection_t {
    rtc_arena_view_t arena;
    rtc_peer_connection_limits_t limits;
    rtc_sdp_parameters_t sdp;
    const char *local_host_ip;
    size_t local_host_ip_len;
    uint16_t local_host_port;
    rtc_stun_server_t stun_server;
    size_t stun_server_count;
    rtc_executors_t executors;
    rtc_observer_vtable_t observer;
    rtc_peer_connection_counters_t counters;
    const rtc_security_backend_config_t *security_backend;
    void *security_session;
    void *security_session_storage;
    size_t security_session_storage_bytes;
    rtc_security_dtls_role_t dtls_role;
    rtc_security_dtls_state_t dtls_state;
    int srtp_ready;
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
    rtc_ice_state_t ice_state;
    rtc_ice_role_t ice_role;
    uint32_t stun_transaction_nonce;
    int srflx_candidate_gathered;
    int connectivity_checks_started;
    int remote_candidate_pending_pairs;
    int is_closed;
};

#endif
