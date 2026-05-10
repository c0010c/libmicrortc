#include "api/peer_connection.h"

#include <stddef.h>

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

    return RTC_STATUS_OK;
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
    pc->executors = config->executors;
    pc->observer = config->observer;
    rtc_counters_init(&pc->counters);
    pc->counters.api.create_calls = 1;
    pc->is_closed = 0;
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
    (void)out_sdp;
    (void)inout_sdp_len;
    return rtc_unsupported_signaling(pc, "create_offer");
}

rtc_status_t rtc_peer_connection_create_answer(rtc_peer_connection_t *pc,
                                               char *out_sdp,
                                               size_t *inout_sdp_len)
{
    (void)out_sdp;
    (void)inout_sdp_len;
    return rtc_unsupported_signaling(pc, "create_answer");
}

rtc_status_t rtc_peer_connection_set_local_description(rtc_peer_connection_t *pc,
                                                       const char *sdp,
                                                       size_t sdp_len)
{
    (void)sdp;
    (void)sdp_len;
    return rtc_unsupported_signaling(pc, "set_local_description");
}

rtc_status_t rtc_peer_connection_set_remote_description(rtc_peer_connection_t *pc,
                                                        const char *sdp,
                                                        size_t sdp_len)
{
    (void)sdp;
    (void)sdp_len;
    return rtc_unsupported_signaling(pc, "set_remote_description");
}

rtc_status_t rtc_peer_connection_add_ice_candidate(rtc_peer_connection_t *pc,
                                                   const char *candidate,
                                                   size_t candidate_len)
{
    (void)candidate;
    (void)candidate_len;
    return rtc_unsupported_signaling(pc, "add_ice_candidate");
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
