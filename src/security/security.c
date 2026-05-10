#include "security/security.h"

#include "api/peer_connection.h"
#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"

static void rtc_security_trace(rtc_peer_connection_t *pc, const char *event,
                               const char *operation, rtc_status_t status,
                               const char *reason)
{
    rtc_trace_field_t fields[5];
    size_t field_count = 4;

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "security";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_STATUS;
    fields[2].value = 0;
    fields[2].number = (uint64_t)status;
    fields[3].key = RTC_TRACE_FIELD_STATE;
    fields[3].value = "dtls.new";
    fields[3].number = 0;
    if (reason != 0) {
        fields[4].key = RTC_TRACE_FIELD_REASON;
        fields[4].value = reason;
        fields[4].number = 0;
        field_count = 5;
    }

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, event, fields, field_count);
}

static void rtc_security_backend_event_cb_dispatch(
    void *user_data,
    const rtc_security_backend_event_t *event)
{
    (void)user_data;
    (void)event;
}

static int rtc_security_backend_valid(
    const rtc_security_backend_config_t *backend)
{
    return backend != 0 && backend->vtable != 0 &&
           backend->vtable->create_session != 0 &&
           backend->vtable->destroy_session != 0 &&
           backend->vtable->start_dtls != 0 &&
           backend->vtable->handle_dtls_datagram != 0;
}

rtc_status_t rtc_security_init(
    rtc_peer_connection_t *pc,
    const rtc_security_backend_config_t *backend)
{
    rtc_status_t status;
    void *session = 0;

    pc->security_backend = backend;
    pc->security_session = 0;
    pc->dtls_role = RTC_SECURITY_DTLS_ROLE_CLIENT;
    pc->dtls_state = RTC_SECURITY_DTLS_NEW;
    pc->srtp_ready = 0;

    if (backend == 0) {
        return RTC_STATUS_OK;
    }
    if (!rtc_security_backend_valid(backend) ||
        pc->security_session_storage == 0 ||
        pc->security_session_storage_bytes == 0) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_INVALID_ARGUMENT,
                                "security", "create_session", 0);
        rtc_security_trace(pc, RTC_TRACE_DTLS_STATE, "create_session",
                           RTC_STATUS_INVALID_ARGUMENT,
                           "backend_create_failed");
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    status = backend->vtable->create_session(
        backend->user_data, pc->security_session_storage,
        pc->security_session_storage_bytes,
        rtc_security_backend_event_cb_dispatch, pc, &session);
    if (status != RTC_STATUS_OK || session == 0) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_BACKEND_ERROR,
                                "security", "create_session", 0);
        rtc_security_trace(pc, RTC_TRACE_DTLS_STATE, "create_session",
                           RTC_STATUS_BACKEND_ERROR,
                           "backend_create_failed");
        return RTC_STATUS_BACKEND_ERROR;
    }

    pc->security_session = session;
    rtc_security_trace(pc, RTC_TRACE_DTLS_STATE, "create_session",
                       RTC_STATUS_OK, 0);
    return RTC_STATUS_OK;
}

void rtc_security_shutdown(rtc_peer_connection_t *pc)
{
    if (pc == 0 || pc->security_backend == 0 ||
        pc->security_backend->vtable == 0 || pc->security_session == 0) {
        return;
    }

    if (pc->security_backend->vtable->destroy_session != 0) {
        pc->security_backend->vtable->destroy_session(pc->security_session);
    }
    pc->security_session = 0;
    pc->dtls_state = RTC_SECURITY_DTLS_FAILED;
}

rtc_status_t rtc_security_on_ice_connected(rtc_peer_connection_t *pc)
{
    (void)pc;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_security_handle_dtls_datagram(rtc_peer_connection_t *pc,
                                               const uint8_t *data,
                                               size_t data_len)
{
    (void)pc;
    (void)data;
    (void)data_len;
    return RTC_STATUS_OK;
}
