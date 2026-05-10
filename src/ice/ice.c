#include "ice/ice.h"

#include <stdio.h>
#include <string.h>

#include "api/peer_connection.h"
#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"
#include "security/security.h"
#include "stun/stun.h"

#define RTC_ICE_CANDIDATE_STRING_BYTES 256u
#define RTC_ICE_STUN_REQUEST_BYTES 48u
#define RTC_ICE_STUN_TIMEOUT_MS 500u
#define RTC_ICE_TIE_BREAKER 0x6f70656e72746331ull

enum {
    RTC_STUN_PURPOSE_SRFLX = 0,
    RTC_STUN_PURPOSE_PAIR_CHECK = 1,
    RTC_STUN_PURPOSE_NOMINATION = 2
};

static const char *RTC_ICE_FAILURE_NO_LOCAL_CANDIDATES =
    "no_local_candidates";
static const char *RTC_ICE_FAILURE_NO_REMOTE_CANDIDATES =
    "no_remote_candidates";
static const char *RTC_ICE_FAILURE_STUN_TIMEOUT = "stun_timeout";
static const char *RTC_ICE_FAILURE_ROLE_CONFLICT = "role_conflict";
static const char *RTC_ICE_FAILURE_PAIR_CHECK_EXHAUSTED =
    "pair_check_exhausted";
static const char *RTC_ICE_FAILURE_CAPACITY_EXHAUSTED =
    "capacity_exhausted";
static const char *RTC_ICE_FAILURE_MALFORMED_STUN = "malformed_stun";
static const char *RTC_ICE_FAILURE_UNKNOWN_DATAGRAM = "unknown_datagram";

static const char *rtc_ice_state_name(rtc_ice_state_t state)
{
    switch (state) {
    case RTC_ICE_GATHERING:
        return "ice.gathering";
    case RTC_ICE_GATHERING_COMPLETE:
        return "ice.gathering_complete";
    case RTC_ICE_CHECKING:
        return "ice.checking";
    case RTC_ICE_CONNECTED:
        return "ice.connected";
    case RTC_ICE_FAILED:
        return "ice.failed";
    case RTC_ICE_NEW:
    default:
        return "ice.new";
    }
}

static const char *rtc_ice_candidate_type_name(rtc_ice_candidate_type_t type)
{
    return type == RTC_ICE_CANDIDATE_TYPE_SRFLX ? "srflx" : "host";
}

static const char *rtc_ice_role_name(rtc_ice_role_t role)
{
    return role == RTC_ICE_ROLE_CONTROLLED ? "controlled" : "controlling";
}

static void rtc_ice_emit_state_reason(rtc_peer_connection_t *pc,
                                      rtc_ice_state_t state,
                                      const char *reason)
{
    rtc_trace_field_t fields[3];
    size_t field_count = 2;

    pc->ice_state = state;
    rtc_observer_emit_state(&pc->observer, rtc_ice_state_name(state));

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "ice";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_STATE;
    fields[1].value = rtc_ice_state_name(state);
    fields[1].number = 0;
    if (reason != 0) {
        fields[2].key = RTC_TRACE_FIELD_REASON;
        fields[2].value = reason;
        fields[2].number = 0;
        field_count = 3;
    }
    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_ICE_STATE, fields, field_count);
}

static void rtc_ice_emit_state(rtc_peer_connection_t *pc, rtc_ice_state_t state)
{
    rtc_ice_emit_state_reason(pc, state, 0);
}

static void rtc_ice_fail(rtc_peer_connection_t *pc, const char *operation,
                         const char *reason)
{
    pc->counters.ice.checks_failed++;
    rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR, "ice",
                            operation, 0);
    rtc_ice_emit_state_reason(pc, RTC_ICE_FAILED, reason);
}

static void rtc_ice_trace_candidate(rtc_peer_connection_t *pc,
                                    const rtc_ice_candidate_summary_t *candidate,
                                    size_t candidate_id)
{
    rtc_trace_field_t fields[6];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "ice";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = "gather_candidates";
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_CANDIDATE_TYPE;
    fields[2].value = rtc_ice_candidate_type_name(candidate->type);
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_LOCAL_ADDRESS;
    fields[3].value = candidate->address;
    fields[3].number = 0;
    fields[4].key = RTC_TRACE_FIELD_PORT;
    fields[4].value = 0;
    fields[4].number = candidate->port;
    fields[5].key = RTC_TRACE_FIELD_CANDIDATE_ID;
    fields[5].value = 0;
    fields[5].number = candidate_id;
    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_ICE_CANDIDATE_LOCAL, fields, 6);
}

static void rtc_ice_trace_stun(rtc_peer_connection_t *pc, const char *operation,
                               size_t transaction_id, rtc_status_t status)
{
    rtc_trace_field_t fields[5];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "stun";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_TRANSACTION_ID;
    fields[2].value = 0;
    fields[2].number = transaction_id;
    fields[3].key = RTC_TRACE_FIELD_STATUS;
    fields[3].value = 0;
    fields[3].number = (uint64_t)status;
    fields[4].key = RTC_TRACE_FIELD_REMOTE_ADDRESS;
    fields[4].value = pc->stun_server.ip;
    fields[4].number = 0;
    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_STUN_TRANSACTION, fields, 5);
}

static void rtc_ice_trace_pair_created(rtc_peer_connection_t *pc,
                                       size_t pair_id)
{
    rtc_ice_candidate_pair_t *pair = &pc->candidate_pairs[pair_id];
    rtc_ice_candidate_summary_t *local =
        &pc->local_candidate_summaries[pair->local_candidate_id];
    rtc_ice_candidate_summary_t *remote =
        &pc->remote_candidate_summaries[pair->remote_candidate_id];
    rtc_trace_field_t fields[7];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "ice";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_PAIR_ID;
    fields[1].value = 0;
    fields[1].number = pair_id;
    fields[2].key = RTC_TRACE_FIELD_LOCAL_ADDRESS;
    fields[2].value = local->address;
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_REMOTE_ADDRESS;
    fields[3].value = remote->address;
    fields[3].number = 0;
    fields[4].key = RTC_TRACE_FIELD_CANDIDATE_TYPE;
    fields[4].value = rtc_ice_candidate_type_name(local->type);
    fields[4].number = 0;
    fields[5].key = RTC_TRACE_FIELD_PORT;
    fields[5].value = 0;
    fields[5].number = remote->port;
    fields[6].key = RTC_TRACE_FIELD_STATUS;
    fields[6].value = 0;
    fields[6].number = pair->priority;
    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_ICE_PAIR_CREATED, fields, 7);
}

static void rtc_ice_trace_selected_pair(rtc_peer_connection_t *pc,
                                        size_t pair_id)
{
    rtc_ice_candidate_pair_t *pair = &pc->candidate_pairs[pair_id];
    rtc_ice_candidate_summary_t *local =
        &pc->local_candidate_summaries[pair->local_candidate_id];
    rtc_ice_candidate_summary_t *remote =
        &pc->remote_candidate_summaries[pair->remote_candidate_id];
    rtc_trace_field_t fields[7];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "ice";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_PAIR_ID;
    fields[1].value = 0;
    fields[1].number = pair_id;
    fields[2].key = RTC_TRACE_FIELD_ROLE;
    fields[2].value = rtc_ice_role_name(pc->ice_role);
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_LOCAL_ADDRESS;
    fields[3].value = local->address;
    fields[3].number = 0;
    fields[4].key = RTC_TRACE_FIELD_REMOTE_ADDRESS;
    fields[4].value = remote->address;
    fields[4].number = 0;
    fields[5].key = RTC_TRACE_FIELD_CANDIDATE_TYPE;
    fields[5].value = rtc_ice_candidate_type_name(local->type);
    fields[5].number = 0;
    fields[6].key = RTC_TRACE_FIELD_PORT;
    fields[6].value = 0;
    fields[6].number = remote->port;
    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_ICE_SELECTED_PAIR, fields, 7);
}

static rtc_status_t rtc_ice_format_candidate(
    const rtc_ice_candidate_summary_t *candidate, const char *base_ip,
    uint16_t base_port, char *out, size_t out_capacity, size_t *out_len)
{
    int written;

    if (candidate->type == RTC_ICE_CANDIDATE_TYPE_SRFLX) {
        written = snprintf(out, out_capacity,
                           "candidate:%s 1 udp %u %s %u typ srflx raddr %s "
                           "rport %u",
                           candidate->foundation, candidate->priority,
                           candidate->address, candidate->port, base_ip,
                           base_port);
    } else {
        written = snprintf(out, out_capacity, "candidate:%s 1 udp %u %s %u "
                                             "typ host",
                           candidate->foundation, candidate->priority,
                           candidate->address, candidate->port);
    }

    if (written < 0 || (size_t)written >= out_capacity) {
        return RTC_STATUS_CAPACITY_ICE_CANDIDATES;
    }
    *out_len = (size_t)written;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_ice_emit_local_candidate(
    rtc_peer_connection_t *pc, rtc_ice_candidate_summary_t *candidate,
    const char *base_ip, uint16_t base_port)
{
    char text[RTC_ICE_CANDIDATE_STRING_BYTES];
    size_t text_len;
    size_t candidate_id;
    rtc_status_t status;

    if (pc->local_candidate_count >= pc->limits.ice.max_candidates) {
        return RTC_STATUS_CAPACITY_ICE_CANDIDATES;
    }

    status = rtc_ice_format_candidate(candidate, base_ip, base_port, text,
                                      sizeof(text), &text_len);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    candidate_id = pc->local_candidate_count;
    pc->local_candidate_summaries[candidate_id] = *candidate;
    pc->local_candidate_count++;
    pc->counters.ice.local_candidates++;

    if (pc->observer.on_local_candidate != 0) {
        pc->observer.on_local_candidate(pc->observer.user_data, text, text_len);
    }
    rtc_ice_trace_candidate(pc, candidate, candidate_id);
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_ice_emit_host_candidate(rtc_peer_connection_t *pc)
{
    rtc_ice_candidate_summary_t candidate;

    memset(&candidate, 0, sizeof(candidate));
    candidate.type = RTC_ICE_CANDIDATE_TYPE_HOST;
    memcpy(candidate.foundation, "1", 2);
    memcpy(candidate.transport, "udp", 4);
    if (pc->local_host_ip_len >= sizeof(candidate.address)) {
        return RTC_STATUS_CAPACITY_ICE_CANDIDATES;
    }
    memcpy(candidate.address, pc->local_host_ip, pc->local_host_ip_len);
    candidate.address[pc->local_host_ip_len] = '\0';
    candidate.priority = 2130706431u;
    candidate.component = 1;
    candidate.port = pc->local_host_port;
    return rtc_ice_emit_local_candidate(pc, &candidate, candidate.address,
                                        candidate.port);
}

static rtc_stun_transaction_t *rtc_ice_alloc_transaction(
    rtc_peer_connection_t *pc, size_t *out_index)
{
    size_t i;

    for (i = 0; i < pc->limits.ice.max_transactions; ++i) {
        if (!pc->stun_transactions[i].in_use) {
            memset(&pc->stun_transactions[i], 0, sizeof(pc->stun_transactions[i]));
            pc->stun_transactions[i].in_use = 1;
            pc->stun_transaction_count++;
            *out_index = i;
            return &pc->stun_transactions[i];
        }
    }

    return 0;
}

static void rtc_ice_generate_transaction_id(rtc_peer_connection_t *pc,
                                            uint8_t *transaction_id)
{
    uint32_t nonce = pc->stun_transaction_nonce++;
    size_t i;

    for (i = 0; i < RTC_STUN_TRANSACTION_ID_BYTES; ++i) {
        transaction_id[i] = (uint8_t)(0xa5u + (uint8_t)i);
    }
    transaction_id[8] = (uint8_t)(nonce >> 24);
    transaction_id[9] = (uint8_t)(nonce >> 16);
    transaction_id[10] = (uint8_t)(nonce >> 8);
    transaction_id[11] = (uint8_t)nonce;
}

static rtc_status_t rtc_ice_send_srflx_request(rtc_peer_connection_t *pc)
{
    rtc_stun_transaction_t *transaction;
    uint8_t request[RTC_ICE_STUN_REQUEST_BYTES];
    size_t request_len = 0;
    size_t transaction_index = 0;
    rtc_status_t status;

    transaction = rtc_ice_alloc_transaction(pc, &transaction_index);
    if (transaction == 0) {
        return RTC_STATUS_CAPACITY_STUN_TRANSACTIONS;
    }

    rtc_ice_generate_transaction_id(pc, transaction->transaction_id);
    transaction->purpose = RTC_STUN_PURPOSE_SRFLX;
    status = rtc_stun_write_binding_request(request, sizeof(request),
                                            transaction->transaction_id,
                                            &request_len);
    if (status != RTC_STATUS_OK) {
        transaction->in_use = 0;
        pc->stun_transaction_count--;
        return status;
    }

    status = pc->executors.network.schedule_timer(
        pc->executors.network.user_data, RTC_ICE_STUN_TIMEOUT_MS,
        rtc_ice_stun_transaction_timeout, pc, &transaction->timer_id);
    if (status != RTC_STATUS_OK) {
        transaction->in_use = 0;
        pc->stun_transaction_count--;
        return status;
    }

    if (pc->observer.on_datagram != 0) {
        pc->observer.on_datagram(pc->observer.user_data, request, request_len);
    }
    pc->counters.stun.transactions_sent++;
    rtc_ice_trace_stun(pc, "srflx_request", transaction_index, RTC_STATUS_OK);
    return RTC_STATUS_OK;
}

static uint64_t rtc_ice_pair_priority(
    const rtc_ice_candidate_summary_t *local,
    const rtc_ice_candidate_summary_t *remote)
{
    uint32_t g = local->priority < remote->priority ? local->priority
                                                    : remote->priority;
    uint32_t d = local->priority > remote->priority ? local->priority
                                                    : remote->priority;
    return ((uint64_t)g << 32) + (uint64_t)(2u * d) +
           (local->priority > remote->priority ? 1u : 0u);
}

static void rtc_ice_sort_pairs(rtc_peer_connection_t *pc)
{
    size_t i;
    size_t j;

    for (i = 0; i < pc->candidate_pair_count; ++i) {
        for (j = i + 1u; j < pc->candidate_pair_count; ++j) {
            if (pc->candidate_pairs[j].priority >
                pc->candidate_pairs[i].priority) {
                rtc_ice_candidate_pair_t temp = pc->candidate_pairs[i];
                pc->candidate_pairs[i] = pc->candidate_pairs[j];
                pc->candidate_pairs[j] = temp;
            }
        }
    }
}

static rtc_status_t rtc_ice_create_pair(rtc_peer_connection_t *pc,
                                        size_t local_id, size_t remote_id)
{
    rtc_ice_candidate_pair_t *pair;
    size_t pair_id;
    size_t i;

    for (i = 0; i < pc->candidate_pair_count; ++i) {
        if (pc->candidate_pairs[i].local_candidate_id == local_id &&
            pc->candidate_pairs[i].remote_candidate_id == remote_id) {
            return RTC_STATUS_OK;
        }
    }
    if (pc->candidate_pair_count >= pc->limits.ice.max_candidate_pairs) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_CAPACITY_ICE_PAIRS,
                                "ice", "start_connectivity_checks", 0);
        return RTC_STATUS_CAPACITY_ICE_PAIRS;
    }

    pair_id = pc->candidate_pair_count;
    pair = &pc->candidate_pairs[pair_id];
    memset(pair, 0, sizeof(*pair));
    pair->local_candidate_id = local_id;
    pair->remote_candidate_id = remote_id;
    pair->priority = rtc_ice_pair_priority(&pc->local_candidate_summaries[local_id],
                                           &pc->remote_candidate_summaries[remote_id]);
    pair->state = RTC_ICE_PAIR_FROZEN;
    pc->candidate_pair_count++;
    pc->counters.ice.candidate_pairs++;
    rtc_ice_trace_pair_created(pc, pair_id);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_ice_add_remote_candidate_pairs(rtc_peer_connection_t *pc,
                                                size_t remote_candidate_id)
{
    size_t local_id;
    rtc_status_t status;

    if (remote_candidate_id >= pc->remote_candidate_count) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    for (local_id = 0; local_id < pc->local_candidate_count; ++local_id) {
        status = rtc_ice_create_pair(pc, local_id, remote_candidate_id);
        if (status != RTC_STATUS_OK) {
            return status;
        }
    }
    rtc_ice_sort_pairs(pc);
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_ice_create_all_pairs(rtc_peer_connection_t *pc)
{
    size_t remote_id;
    rtc_status_t status;

    for (remote_id = 0; remote_id < pc->remote_candidate_count; ++remote_id) {
        status = rtc_ice_add_remote_candidate_pairs(pc, remote_id);
        if (status != RTC_STATUS_OK) {
            rtc_observer_emit_error(&pc->observer, status, "ice",
                                    "start_connectivity_checks", 0);
            return status;
        }
    }
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_ice_send_pair_request(rtc_peer_connection_t *pc,
                                              size_t pair_id,
                                              int use_candidate)
{
    rtc_stun_transaction_t *transaction;
    uint8_t request[RTC_ICE_STUN_REQUEST_BYTES];
    size_t request_len = 0;
    size_t transaction_index = 0;
    rtc_status_t status;

    transaction = rtc_ice_alloc_transaction(pc, &transaction_index);
    if (transaction == 0) {
        rtc_observer_emit_error(&pc->observer,
                                RTC_STATUS_CAPACITY_STUN_TRANSACTIONS, "stun",
                                "pair_check", 0);
        return RTC_STATUS_CAPACITY_STUN_TRANSACTIONS;
    }

    rtc_ice_generate_transaction_id(pc, transaction->transaction_id);
    transaction->purpose = use_candidate ? RTC_STUN_PURPOSE_NOMINATION
                                         : RTC_STUN_PURPOSE_PAIR_CHECK;
    transaction->pair_id = pair_id;
    status = rtc_stun_write_ice_binding_request(
        request, sizeof(request), transaction->transaction_id, use_candidate,
        pc->ice_role != RTC_ICE_ROLE_CONTROLLED, RTC_ICE_TIE_BREAKER,
        &request_len);
    if (status != RTC_STATUS_OK) {
        transaction->in_use = 0;
        pc->stun_transaction_count--;
        return status;
    }

    status = pc->executors.network.schedule_timer(
        pc->executors.network.user_data, RTC_ICE_STUN_TIMEOUT_MS,
        rtc_ice_stun_transaction_timeout, pc, &transaction->timer_id);
    if (status != RTC_STATUS_OK) {
        transaction->in_use = 0;
        pc->stun_transaction_count--;
        return status;
    }

    pc->candidate_pairs[pair_id].state = RTC_ICE_PAIR_IN_PROGRESS;
    if (pc->observer.on_datagram != 0) {
        pc->observer.on_datagram(pc->observer.user_data, request, request_len);
    }
    pc->counters.stun.transactions_sent++;
    rtc_ice_trace_stun(pc, use_candidate ? "nomination_request" : "pair_check",
                       transaction_index, RTC_STATUS_OK);
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_ice_start_next_pair(rtc_peer_connection_t *pc)
{
    size_t i;

    for (i = 0; i < pc->candidate_pair_count; ++i) {
        if (pc->candidate_pairs[i].state == RTC_ICE_PAIR_FROZEN ||
            pc->candidate_pairs[i].state == RTC_ICE_PAIR_WAITING) {
            pc->candidate_pairs[i].state = RTC_ICE_PAIR_WAITING;
            return rtc_ice_send_pair_request(pc, i, 0);
        }
    }

    rtc_ice_fail(pc, "pair_check", RTC_ICE_FAILURE_PAIR_CHECK_EXHAUSTED);
    return RTC_STATUS_PROTOCOL_ERROR;
}

static void rtc_ice_select_pair(rtc_peer_connection_t *pc, size_t pair_id)
{
    rtc_status_t security_status;

    pc->candidate_pairs[pair_id].selected = 1;
    pc->candidate_pairs[pair_id].state = RTC_ICE_PAIR_SELECTED;
    pc->counters.ice.selected_pairs++;
    rtc_ice_trace_selected_pair(pc, pair_id);
    rtc_ice_emit_state(pc, RTC_ICE_CONNECTED);
    security_status = rtc_security_on_ice_connected(pc);
    if (security_status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, security_status, "security",
                                "on_ice_connected", 0);
    }
}

rtc_status_t rtc_ice_start_connectivity_checks(rtc_peer_connection_t *pc)
{
    rtc_status_t status;

    if (pc->local_candidate_count == 0) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_INVALID_STATE, "ice",
                                "start_connectivity_checks", 0);
        rtc_ice_emit_state_reason(pc, RTC_ICE_FAILED,
                                  RTC_ICE_FAILURE_NO_LOCAL_CANDIDATES);
        return RTC_STATUS_INVALID_STATE;
    }
    if (pc->remote_candidate_count == 0) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_INVALID_STATE, "ice",
                                "start_connectivity_checks", 0);
        rtc_ice_emit_state_reason(pc, RTC_ICE_FAILED,
                                  RTC_ICE_FAILURE_NO_REMOTE_CANDIDATES);
        return RTC_STATUS_INVALID_STATE;
    }
    if (pc->ice_role == RTC_ICE_ROLE_UNKNOWN) {
        pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
    }

    status = rtc_ice_create_all_pairs(pc);
    if (status != RTC_STATUS_OK) {
        if (status == RTC_STATUS_CAPACITY_ICE_PAIRS) {
            rtc_ice_emit_state_reason(pc, RTC_ICE_FAILED,
                                      RTC_ICE_FAILURE_CAPACITY_EXHAUSTED);
        }
        return status;
    }
    pc->connectivity_checks_started = 1;
    rtc_ice_emit_state(pc, RTC_ICE_CHECKING);
    return rtc_ice_start_next_pair(pc);
}

rtc_status_t rtc_ice_gather_candidates(rtc_peer_connection_t *pc)
{
    rtc_status_t status;

    if (pc->ice_state == RTC_ICE_GATHERING ||
        pc->ice_state == RTC_ICE_GATHERING_COMPLETE) {
        return RTC_STATUS_INVALID_STATE;
    }

    rtc_ice_emit_state(pc, RTC_ICE_GATHERING);
    status = rtc_ice_emit_host_candidate(pc);
    if (status != RTC_STATUS_OK) {
        pc->counters.ice.gathering_failures++;
        rtc_ice_emit_state(pc, RTC_ICE_FAILED);
        return status;
    }

    if (pc->stun_server_count == 0) {
        rtc_ice_emit_state(pc, RTC_ICE_GATHERING_COMPLETE);
        return RTC_STATUS_OK;
    }

    status = rtc_ice_send_srflx_request(pc);
    if (status != RTC_STATUS_OK) {
        pc->counters.ice.gathering_failures++;
        rtc_ice_emit_state(pc, RTC_ICE_FAILED);
    }
    return status;
}

void rtc_ice_stun_transaction_timeout(void *user_data)
{
    rtc_peer_connection_t *pc = (rtc_peer_connection_t *)user_data;
    size_t i;

    if (pc == 0) {
        return;
    }

    for (i = 0; i < pc->limits.ice.max_transactions; ++i) {
        if (pc->stun_transactions[i].in_use) {
            int purpose = pc->stun_transactions[i].purpose;
            size_t pair_id = pc->stun_transactions[i].pair_id;
            pc->stun_transactions[i].in_use = 0;
            pc->executors.network.cancel_timer(pc->executors.network.user_data,
                                               pc->stun_transactions[i].timer_id);
            if (pc->stun_transaction_count > 0) {
                pc->stun_transaction_count--;
            }
            pc->counters.stun.transactions_timed_out++;
            rtc_ice_trace_stun(pc, "timeout", i, RTC_STATUS_PROTOCOL_ERROR);
            if ((purpose == RTC_STUN_PURPOSE_PAIR_CHECK ||
                 purpose == RTC_STUN_PURPOSE_NOMINATION) &&
                pair_id < pc->candidate_pair_count) {
                pc->candidate_pairs[pair_id].state = RTC_ICE_PAIR_FAILED;
                rtc_observer_emit_error(&pc->observer,
                                        RTC_STATUS_PROTOCOL_ERROR, "stun",
                                        "timeout", 0);
                rtc_ice_emit_state_reason(pc, RTC_ICE_CHECKING,
                                          RTC_ICE_FAILURE_STUN_TIMEOUT);
            }
        }
    }

    if (pc->connectivity_checks_started) {
        rtc_ice_start_next_pair(pc);
    } else if (pc->local_candidate_count > 0) {
        rtc_ice_emit_state(pc, RTC_ICE_GATHERING_COMPLETE);
    } else {
        pc->counters.ice.gathering_failures++;
        rtc_ice_emit_state(pc, RTC_ICE_FAILED);
    }
}

rtc_status_t rtc_ice_handle_stun_response(rtc_peer_connection_t *pc,
                                          const uint8_t *data,
                                          size_t data_len)
{
    rtc_stun_header_t header;
    rtc_stun_binding_request_attrs_t request_attrs;
    rtc_stun_xor_mapped_address_t mapped;
    rtc_ice_candidate_summary_t candidate;
    size_t i;
    int matched = 0;
    int purpose = RTC_STUN_PURPOSE_SRFLX;
    size_t pair_id = 0;
    rtc_status_t status;

    status = rtc_stun_parse_header(data, data_len, &header);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR,
                                "stun", "handle_stun_response", 0);
        if (pc->connectivity_checks_started) {
            rtc_ice_emit_state_reason(pc, RTC_ICE_FAILED,
                                      RTC_ICE_FAILURE_MALFORMED_STUN);
            pc->counters.ice.checks_failed++;
        }
        return status;
    }

    if (header.type == RTC_STUN_BINDING_REQUEST) {
        status = rtc_stun_parse_binding_request_attrs(data, data_len,
                                                      &request_attrs);
        if (status != RTC_STATUS_OK) {
            return status;
        }
        if (request_attrs.use_candidate &&
            pc->ice_role == RTC_ICE_ROLE_CONTROLLED &&
            pc->candidate_pair_count > 0) {
            pc->candidate_pairs[0].state = RTC_ICE_PAIR_NOMINATED;
            rtc_ice_select_pair(pc, 0);
            return RTC_STATUS_OK;
        }
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    for (i = 0; i < pc->limits.ice.max_transactions; ++i) {
        if (pc->stun_transactions[i].in_use &&
            memcmp(pc->stun_transactions[i].transaction_id,
                   header.transaction_id, RTC_STUN_TRANSACTION_ID_BYTES) == 0) {
            purpose = pc->stun_transactions[i].purpose;
            pair_id = pc->stun_transactions[i].pair_id;
            pc->stun_transactions[i].in_use = 0;
            if (pc->stun_transaction_count > 0) {
                pc->stun_transaction_count--;
            }
            matched = 1;
            break;
        }
    }
    if (!matched) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR,
                                "stun", "handle_stun_response", 0);
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    if (header.type == RTC_STUN_BINDING_ERROR_RESPONSE) {
        uint16_t code = 0;
        if (rtc_stun_parse_error_code(data, data_len, &code) == RTC_STATUS_OK &&
            code == RTC_STUN_ERROR_ROLE_CONFLICT) {
            rtc_ice_fail(pc, "pair_check", RTC_ICE_FAILURE_ROLE_CONFLICT);
        }
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    if (purpose == RTC_STUN_PURPOSE_PAIR_CHECK ||
        purpose == RTC_STUN_PURPOSE_NOMINATION) {
        if (header.type != RTC_STUN_BINDING_SUCCESS_RESPONSE ||
            pair_id >= pc->candidate_pair_count) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        pc->counters.stun.transactions_received++;
        rtc_ice_trace_stun(pc,
                           purpose == RTC_STUN_PURPOSE_NOMINATION
                               ? "nomination_response"
                               : "pair_check_response",
                           i, RTC_STATUS_OK);
        if (purpose == RTC_STUN_PURPOSE_PAIR_CHECK) {
            pc->candidate_pairs[pair_id].state = RTC_ICE_PAIR_SUCCEEDED;
            if (pc->ice_role == RTC_ICE_ROLE_CONTROLLING) {
                return rtc_ice_send_pair_request(pc, pair_id, 1);
            }
            return RTC_STATUS_OK;
        }
        pc->candidate_pairs[pair_id].state = RTC_ICE_PAIR_NOMINATED;
        rtc_ice_select_pair(pc, pair_id);
        return RTC_STATUS_OK;
    }

    status = rtc_stun_parse_xor_mapped_address(data, data_len, &mapped);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    memset(&candidate, 0, sizeof(candidate));
    candidate.type = RTC_ICE_CANDIDATE_TYPE_SRFLX;
    memcpy(candidate.foundation, "2", 2);
    memcpy(candidate.transport, "udp", 4);
    memcpy(candidate.address, mapped.ip, strlen(mapped.ip) + 1u);
    candidate.priority = 1694498815u;
    candidate.component = 1;
    candidate.port = mapped.port;

    status = rtc_ice_emit_local_candidate(pc, &candidate, pc->local_host_ip,
                                          pc->local_host_port);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    pc->srflx_candidate_gathered = 1;
    pc->counters.stun.transactions_received++;
    rtc_ice_trace_stun(pc, "srflx_response", i, RTC_STATUS_OK);
    rtc_ice_emit_state(pc, RTC_ICE_GATHERING_COMPLETE);
    return RTC_STATUS_OK;
}
