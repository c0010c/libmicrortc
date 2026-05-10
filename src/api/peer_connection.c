#include "api/peer_connection.h"

#include <stddef.h>
#include <string.h>

#include "executor/executor.h"
#include "memory/allocator.h"
#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"

static void rtc_pc_trace(rtc_peer_connection_t *pc, const char *event,
                         const char *operation, rtc_status_t status)
{
    rtc_trace_field_t fields[3];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "peer_connection";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_STATUS;
    fields[2].value = 0;
    fields[2].number = (uint64_t)status;

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, event, fields, 3);
}

static void rtc_pc_trace_jsep(rtc_peer_connection_t *pc, const char *event,
                              const char *operation, rtc_status_t status,
                              rtc_jsep_state_t target_state,
                              rtc_sdp_type_t type)
{
    rtc_trace_field_t fields[5];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "jsep";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_STATUS;
    fields[2].value = 0;
    fields[2].number = (uint64_t)status;
    fields[3].key = RTC_TRACE_FIELD_TARGET_STATE;
    fields[3].value = rtc_jsep_state_name(target_state);
    fields[3].number = 0;
    fields[4].key = RTC_TRACE_FIELD_DESCRIPTION_TYPE;
    fields[4].value = type == RTC_SDP_TYPE_OFFER ? "offer" : "answer";
    fields[4].number = 0;

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, event, fields, 5);
}

static rtc_status_t rtc_pc_affinity_violation(rtc_peer_connection_t *pc,
                                              const char *operation)
{
    rtc_counters_note_affinity_error(&pc->counters);
    rtc_observer_emit_error(&pc->observer, RTC_STATUS_AFFINITY_VIOLATION,
                            "executor", operation, 0);
    rtc_pc_trace(pc, RTC_TRACE_AFFINITY_VIOLATION, operation,
                 RTC_STATUS_AFFINITY_VIOLATION);
    return RTC_STATUS_AFFINITY_VIOLATION;
}

static rtc_status_t rtc_pc_unsupported(rtc_peer_connection_t *pc,
                                       const char *operation)
{
    rtc_counters_note_unsupported_api(&pc->counters);
    rtc_observer_emit_error(&pc->observer, RTC_STATUS_UNSUPPORTED, "api",
                            operation, 0);
    rtc_pc_trace(pc, RTC_TRACE_UNSUPPORTED_API, operation,
                 RTC_STATUS_UNSUPPORTED);
    return RTC_STATUS_UNSUPPORTED;
}

static int rtc_executor_vtable_valid(const rtc_executor_vtable_t *executor)
{
    return executor != 0 && executor->post != 0 &&
           executor->schedule_timer != 0 && executor->cancel_timer != 0;
}

static rtc_status_t rtc_validate_config(const rtc_peer_connection_config_t *config)
{
    if (config == 0 || config->arena.data == 0 || config->arena.size == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    if (!rtc_executor_vtable_valid(&config->executors.signaling) ||
        !rtc_executor_vtable_valid(&config->executors.media) ||
        !rtc_executor_vtable_valid(&config->executors.network)) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    if (config->limits.sdp.max_description_bytes == 0 ||
        config->sdp.ice_ufrag == 0 || config->sdp.ice_ufrag_len == 0 ||
        config->sdp.ice_pwd == 0 || config->sdp.ice_pwd_len == 0 ||
        config->sdp.dtls_fingerprint == 0 ||
        config->sdp.dtls_fingerprint_len == 0 ||
        config->sdp.dtls_setup == 0 || config->sdp.dtls_setup_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    return RTC_STATUS_OK;
}

static rtc_status_t rtc_pc_alloc_sdp_buffer(rtc_arena_view_t *arena,
                                            size_t size, char **out_buffer,
                                            rtc_capacity_diagnostics_t *diag)
{
    *out_buffer = (char *)rtc_core_alloc(arena, size, sizeof(char),
                                         RTC_CAPACITY_RESOURCE_SDP_BUFFER,
                                         diag);
    return *out_buffer != 0 ? RTC_STATUS_OK : RTC_STATUS_CAPACITY_SDP_BUFFER;
}

static rtc_status_t rtc_pc_alloc_candidates(rtc_arena_view_t *arena,
                                            size_t max_candidates,
                                            char **out_candidates,
                                            rtc_capacity_diagnostics_t *diag)
{
    size_t bytes;

    bytes = max_candidates * RTC_PC_REMOTE_CANDIDATE_SLOT_BYTES;
    *out_candidates = (char *)rtc_core_alloc(
        arena, bytes, sizeof(char), RTC_CAPACITY_RESOURCE_ICE_CANDIDATES, diag);
    return *out_candidates != 0 ? RTC_STATUS_OK
                                : RTC_STATUS_CAPACITY_ICE_CANDIDATES;
}

static rtc_status_t rtc_require_pc(rtc_peer_connection_t *pc)
{
    if (pc == 0 || pc->is_closed) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    return RTC_STATUS_OK;
}

static rtc_status_t rtc_unsupported_signaling(rtc_peer_connection_t *pc,
                                              const char *operation)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, operation);
    }

    return rtc_pc_unsupported(pc, operation);
}

rtc_status_t rtc_peer_connection_create(const rtc_peer_connection_config_t *config,
                                        rtc_capacity_diagnostics_t *diag,
                                        rtc_peer_connection_t **out_pc)
{
    rtc_arena_view_t arena;
    rtc_peer_connection_t *pc;
    rtc_status_t status;

    if (out_pc == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *out_pc = 0;

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_validate_config(config);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    rtc_arena_init(&arena, config->arena);
    pc = (rtc_peer_connection_t *)rtc_core_alloc(
        &arena, sizeof(*pc), sizeof(void *), RTC_CAPACITY_RESOURCE_ARENA, diag);
    if (pc == 0) {
        return RTC_STATUS_CAPACITY_ARENA;
    }

    pc->arena = arena;
    pc->limits = config->limits;
    pc->sdp = config->sdp;
    pc->executors = config->executors;
    pc->observer = config->observer;
    rtc_counters_init(&pc->counters);
    pc->counters.api.create_calls = 1;
    pc->signaling_state = RTC_JSEP_STABLE;
    pc->local_description_len = 0;
    pc->remote_description_len = 0;
    pc->remote_candidate_slot_bytes = RTC_PC_REMOTE_CANDIDATE_SLOT_BYTES;
    pc->remote_candidate_count = 0;
    pc->is_closed = 0;

    status = rtc_pc_alloc_sdp_buffer(&pc->arena,
                                     config->limits.sdp.max_description_bytes,
                                     &pc->local_description, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_pc_alloc_sdp_buffer(&pc->arena,
                                     config->limits.sdp.max_description_bytes,
                                     &pc->remote_description, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_pc_alloc_candidates(&pc->arena,
                                     config->limits.ice.max_candidates,
                                     &pc->remote_candidates, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    *out_pc = pc;
    rtc_pc_trace(pc, RTC_TRACE_PC_CREATE, "create", RTC_STATUS_OK);

    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_destroy(rtc_peer_connection_t *pc)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "destroy");
    }

    pc->counters.api.destroy_calls++;
    rtc_pc_trace(pc, RTC_TRACE_PC_DESTROY, "destroy", RTC_STATUS_OK);
    pc->is_closed = 1;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_create_offer(rtc_peer_connection_t *pc,
                                              char *out_sdp,
                                              size_t *inout_sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "create_offer");
    }
    status = rtc_jsep_can_create_offer(pc->signaling_state);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "jsep", "create_offer",
                                0);
        rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_REJECT, "create_offer", status,
                          pc->signaling_state, RTC_SDP_TYPE_OFFER);
        return status;
    }
    status = rtc_sdp_write_offer(&pc->sdp, RTC_SDP_DIRECTION_SENDRECV,
                                 RTC_SDP_DIRECTION_SENDRECV, out_sdp,
                                 inout_sdp_len);
    rtc_pc_trace(pc, RTC_TRACE_SDP_WRITE, "create_offer", status);
    return status;
}

rtc_status_t rtc_peer_connection_create_answer(rtc_peer_connection_t *pc,
                                               char *out_sdp,
                                               size_t *inout_sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "create_answer");
    }
    status = rtc_jsep_can_create_answer(pc->signaling_state);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "jsep", "create_answer",
                                0);
        rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_REJECT, "create_answer", status,
                          pc->signaling_state, RTC_SDP_TYPE_ANSWER);
        return status;
    }
    status = rtc_sdp_write_answer(&pc->sdp, RTC_SDP_DIRECTION_SENDRECV,
                                  RTC_SDP_DIRECTION_SENDRECV, out_sdp,
                                  inout_sdp_len);
    rtc_pc_trace(pc, RTC_TRACE_SDP_WRITE, "create_answer", status);
    return status;
}

static rtc_status_t rtc_pc_store_description(rtc_peer_connection_t *pc,
                                             const char *operation,
                                             const char *sdp, size_t sdp_len,
                                             int is_local)
{
    rtc_sdp_description_t parsed;
    rtc_jsep_state_t next_state;
    rtc_status_t status;

    if (sdp == 0 || sdp_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (sdp_len >= pc->limits.sdp.max_description_bytes) {
        return RTC_STATUS_CAPACITY_SDP_BUFFER;
    }

    status = rtc_sdp_parse(sdp, sdp_len, &parsed);
    rtc_pc_trace(pc, RTC_TRACE_SDP_PARSE, operation, status);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "sdp", operation, 0);
        return status;
    }

    status = is_local ? rtc_jsep_apply_local(pc->signaling_state, parsed.type,
                                             &next_state)
                      : rtc_jsep_apply_remote(pc->signaling_state, parsed.type,
                                              &next_state);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "jsep", operation, 0);
        rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_REJECT, operation, status,
                          pc->signaling_state, parsed.type);
        return status;
    }

    if (is_local) {
        memcpy(pc->local_description, sdp, sdp_len);
        pc->local_description[sdp_len] = '\0';
        pc->local_description_len = sdp_len;
        pc->local_summary = parsed;
    } else {
        memcpy(pc->remote_description, sdp, sdp_len);
        pc->remote_description[sdp_len] = '\0';
        pc->remote_description_len = sdp_len;
        pc->remote_summary = parsed;
    }
    pc->signaling_state = next_state;
    rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_TRANSITION, operation, RTC_STATUS_OK,
                      next_state, parsed.type);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_set_local_description(rtc_peer_connection_t *pc,
                                                       const char *sdp,
                                                       size_t sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "set_local_description");
    }
    return rtc_pc_store_description(pc, "set_local_description", sdp, sdp_len,
                                    1);
}

rtc_status_t rtc_peer_connection_set_remote_description(rtc_peer_connection_t *pc,
                                                        const char *sdp,
                                                        size_t sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "set_remote_description");
    }
    return rtc_pc_store_description(pc, "set_remote_description", sdp, sdp_len,
                                    0);
}

rtc_status_t rtc_peer_connection_add_ice_candidate(rtc_peer_connection_t *pc,
                                                   const char *candidate,
                                                   size_t candidate_len)
{
    rtc_status_t status;
    char *slot;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "add_ice_candidate");
    }
    if (candidate == 0 || candidate_len == 0 ||
        candidate_len >= pc->remote_candidate_slot_bytes) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (!(candidate_len >= 10u && memcmp(candidate, "candidate:", 10u) == 0) &&
        !(candidate_len >= 12u && memcmp(candidate, "a=candidate:", 12u) == 0)) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR, "ice",
                                "add_ice_candidate", 0);
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (pc->remote_candidate_count >= pc->limits.ice.max_candidates) {
        return RTC_STATUS_CAPACITY_ICE_CANDIDATES;
    }

    slot = pc->remote_candidates +
           pc->remote_candidate_count * pc->remote_candidate_slot_bytes;
    memcpy(slot, candidate, candidate_len);
    slot[candidate_len] = '\0';
    pc->remote_candidate_count++;
    rtc_pc_trace(pc, RTC_TRACE_ICE_CANDIDATE_STORED, "add_ice_candidate",
                 RTC_STATUS_OK);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_receive_datagram(rtc_peer_connection_t *pc,
                                                  const uint8_t *data,
                                                  size_t data_len)
{
    rtc_status_t status;

    (void)data;
    (void)data_len;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_NETWORK);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "receive_datagram");
    }

    return rtc_pc_unsupported(pc, "receive_datagram");
}

rtc_status_t rtc_peer_connection_get_counters(
    rtc_peer_connection_t *pc,
    rtc_peer_connection_counters_t *out_counters)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK || out_counters == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "get_counters");
    }

    *out_counters = pc->counters;
    return RTC_STATUS_OK;
}
