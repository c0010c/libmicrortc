#include "srtp/srtp.h"

#include "api/peer_connection.h"
#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"

typedef rtc_status_t (*rtc_srtp_protect_fn)(void *session, uint8_t *packet,
                                            size_t *inout_len,
                                            size_t capacity);
typedef rtc_status_t (*rtc_srtp_unprotect_fn)(void *session, uint8_t *packet,
                                              size_t *inout_len);

static void rtc_srtp_trace(rtc_peer_connection_t *pc, const char *operation,
                           rtc_status_t status, const char *reason)
{
    rtc_trace_field_t fields[5];
    size_t field_count = 4;

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "srtp";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_STATUS;
    fields[2].value = 0;
    fields[2].number = (uint64_t)status;
    fields[3].key = RTC_TRACE_FIELD_STATE;
    fields[3].value = pc->srtp_ready ? "srtp.ready" : "srtp.not_ready";
    fields[3].number = 0;
    if (reason != 0) {
        fields[4].key = RTC_TRACE_FIELD_REASON;
        fields[4].value = reason;
        fields[4].number = 0;
        field_count = 5;
    }

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_SRTP_PROTECT, fields,
                   field_count);
}

static rtc_status_t rtc_srtp_fail(rtc_peer_connection_t *pc,
                                  const char *operation,
                                  rtc_status_t status,
                                  const char *reason,
                                  int detail_code)
{
    if (pc != 0) {
        rtc_observer_emit_error(&pc->observer, status, "srtp", operation,
                                detail_code);
        rtc_srtp_trace(pc, operation, status, reason);
    }
    return status;
}

static rtc_status_t rtc_srtp_require_ready(rtc_peer_connection_t *pc,
                                           const char *operation)
{
    if (pc == 0 || pc->is_closed) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (!pc->srtp_ready) {
        return rtc_srtp_fail(pc, operation, RTC_STATUS_INVALID_STATE,
                             "srtp_not_ready", 0);
    }
    if (pc->security_backend == 0 || pc->security_backend->vtable == 0 ||
        pc->security_session == 0) {
        return rtc_srtp_fail(pc, operation, RTC_STATUS_BACKEND_ERROR,
                             "srtp_backend_unavailable", 0);
    }
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_srtp_protect_common(rtc_peer_connection_t *pc,
                                            uint8_t *packet,
                                            size_t *inout_len,
                                            size_t capacity,
                                            rtc_srtp_protect_fn protect,
                                            const char *operation,
                                            const char *failure_reason,
                                            int detail_code)
{
    rtc_status_t status;

    if (packet == 0 || inout_len == 0 || *inout_len == 0 ||
        capacity < *inout_len) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    status = rtc_srtp_require_ready(pc, operation);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    if (protect == 0) {
        pc->counters.srtp.protect_failed++;
        return rtc_srtp_fail(pc, operation, RTC_STATUS_BACKEND_ERROR,
                             failure_reason, detail_code);
    }

    status = protect(pc->security_session, packet, inout_len, capacity);
    if (status != RTC_STATUS_OK) {
        pc->counters.srtp.protect_failed++;
        return rtc_srtp_fail(pc, operation, RTC_STATUS_BACKEND_ERROR,
                             failure_reason, detail_code);
    }

    rtc_srtp_trace(pc, operation, RTC_STATUS_OK, 0);
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_srtp_unprotect_common(
    rtc_peer_connection_t *pc, uint8_t *packet, size_t *inout_len,
    rtc_srtp_unprotect_fn unprotect, const char *operation,
    const char *failure_reason)
{
    rtc_status_t status;
    const char *reason = failure_reason;
    int detail_code = RTC_SECURITY_DETAIL_SRTP_UNPROTECT_FAILED;

    if (packet == 0 || inout_len == 0 || *inout_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    status = rtc_srtp_require_ready(pc, operation);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    if (unprotect == 0) {
        pc->counters.srtp.unprotect_failed++;
        return rtc_srtp_fail(pc, operation, RTC_STATUS_BACKEND_ERROR,
                             failure_reason, detail_code);
    }

    status = unprotect(pc->security_session, packet, inout_len);
    if (status != RTC_STATUS_OK) {
        pc->counters.srtp.unprotect_failed++;
        if (status == RTC_STATUS_PROTOCOL_ERROR) {
            pc->counters.srtp.replay_failed++;
            reason = "srtp_replay_failed";
            detail_code = RTC_SECURITY_DETAIL_SRTP_REPLAY_FAILED;
        } else {
            status = RTC_STATUS_BACKEND_ERROR;
        }
        return rtc_srtp_fail(pc, operation, status, reason, detail_code);
    }

    rtc_srtp_trace(pc, operation, RTC_STATUS_OK, 0);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_srtp_protect_rtp(rtc_peer_connection_t *pc,
                                  uint8_t *packet,
                                  size_t *inout_len,
                                  size_t capacity)
{
    rtc_srtp_protect_fn protect = 0;

    if (pc != 0 && pc->security_backend != 0 &&
        pc->security_backend->vtable != 0) {
        protect = pc->security_backend->vtable->srtp_protect_rtp;
    }
    return rtc_srtp_protect_common(pc, packet, inout_len, capacity, protect,
                                   "protect_rtp", "srtp_protect_failed",
                                   RTC_SECURITY_DETAIL_SRTP_PROTECT_FAILED);
}

rtc_status_t rtc_srtp_unprotect_rtp(rtc_peer_connection_t *pc,
                                    uint8_t *packet,
                                    size_t *inout_len)
{
    rtc_srtp_unprotect_fn unprotect = 0;

    if (pc != 0 && pc->security_backend != 0 &&
        pc->security_backend->vtable != 0) {
        unprotect = pc->security_backend->vtable->srtp_unprotect_rtp;
    }
    return rtc_srtp_unprotect_common(pc, packet, inout_len, unprotect,
                                     "unprotect_rtp",
                                     "srtp_unprotect_failed");
}

rtc_status_t rtc_srtp_protect_rtcp(rtc_peer_connection_t *pc,
                                   uint8_t *packet,
                                   size_t *inout_len,
                                   size_t capacity)
{
    rtc_srtp_protect_fn protect = 0;

    if (pc != 0 && pc->security_backend != 0 &&
        pc->security_backend->vtable != 0) {
        protect = pc->security_backend->vtable->srtcp_protect;
    }
    return rtc_srtp_protect_common(pc, packet, inout_len, capacity, protect,
                                   "protect_rtcp", "srtp_protect_failed",
                                   RTC_SECURITY_DETAIL_SRTP_PROTECT_FAILED);
}

rtc_status_t rtc_srtp_unprotect_rtcp(rtc_peer_connection_t *pc,
                                     uint8_t *packet,
                                     size_t *inout_len)
{
    rtc_srtp_unprotect_fn unprotect = 0;

    if (pc != 0 && pc->security_backend != 0 &&
        pc->security_backend->vtable != 0) {
        unprotect = pc->security_backend->vtable->srtcp_unprotect;
    }
    return rtc_srtp_unprotect_common(pc, packet, inout_len, unprotect,
                                     "unprotect_rtcp",
                                     "srtp_unprotect_failed");
}
