#include "security/security.h"

#include "api/peer_connection.h"
#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"

#include <string.h>

#define RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH 4001

static const char *rtc_security_dtls_state_name(rtc_security_dtls_state_t state)
{
    switch (state) {
    case RTC_SECURITY_DTLS_CONNECTING:
        return "dtls.connecting";
    case RTC_SECURITY_DTLS_CONNECTED:
        return "dtls.connected";
    case RTC_SECURITY_DTLS_FAILED:
        return "dtls.failed";
    case RTC_SECURITY_DTLS_NEW:
    default:
        return "dtls.new";
    }
}

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
    fields[3].value = rtc_security_dtls_state_name(pc->dtls_state);
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

static void rtc_security_emit_state(rtc_peer_connection_t *pc,
                                    rtc_security_dtls_state_t state,
                                    const char *reason)
{
    pc->dtls_state = state;
    rtc_observer_emit_state(&pc->observer, rtc_security_dtls_state_name(state));
    rtc_security_trace(pc, RTC_TRACE_DTLS_STATE, "state", RTC_STATUS_OK,
                       reason);
}

static int rtc_security_is_sha256_fingerprint(const char *fingerprint,
                                              size_t fingerprint_len)
{
    static const char prefix[] = "sha-256 ";
    size_t prefix_len = sizeof(prefix) - 1u;

    return fingerprint != 0 && fingerprint_len > prefix_len &&
           strncmp(fingerprint, prefix, prefix_len) == 0;
}

static rtc_status_t rtc_security_fail_fingerprint(
    rtc_peer_connection_t *pc, const char *operation, rtc_status_t status,
    const char *reason, int detail_code)
{
    if (pc != 0) {
        rtc_observer_emit_error(&pc->observer, status, "dtls", operation,
                                detail_code);
        rtc_security_trace(pc, RTC_TRACE_DTLS_STATE, operation, status,
                           reason);
    }
    return status;
}

static void rtc_security_handle_handshake_complete(rtc_peer_connection_t *pc)
{
    rtc_status_t status;

    status = rtc_security_verify_peer_fingerprint(pc);
    if (status != RTC_STATUS_OK) {
        /* fingerprint_mismatch must stop before export_keying_material. */
        return;
    }

    pc->counters.dtls.handshake_completed++;
    rtc_security_emit_state(pc, RTC_SECURITY_DTLS_CONNECTED, 0);
}

static void rtc_security_backend_event_cb_dispatch(
    void *user_data,
    const rtc_security_backend_event_t *event)
{
    rtc_peer_connection_t *pc = (rtc_peer_connection_t *)user_data;

    if (pc == 0 || event == 0) {
        return;
    }

    switch (event->type) {
    case RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM:
        if (event->datagram != 0 && event->datagram_len > 0 &&
            pc->observer.on_datagram != 0) {
            pc->observer.on_datagram(pc->observer.user_data, event->datagram,
                                     event->datagram_len);
        }
        pc->counters.dtls.outgoing_datagrams++;
        rtc_security_trace(pc, RTC_TRACE_DTLS_HANDSHAKE,
                           "outgoing_datagram", RTC_STATUS_OK, 0);
        break;
    case RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE:
        rtc_security_handle_handshake_complete(pc);
        break;
    case RTC_SECURITY_BACKEND_EVENT_ERROR:
    default:
        pc->counters.dtls.handshake_failed++;
        rtc_security_emit_state(pc, RTC_SECURITY_DTLS_FAILED,
                                "backend_event_error");
        rtc_observer_emit_error(&pc->observer,
                                event->status == RTC_STATUS_OK
                                    ? RTC_STATUS_BACKEND_ERROR
                                    : event->status,
                                "security", "backend_event",
                                event->detail_code);
        break;
    }
}

static int rtc_security_backend_valid(
    const rtc_security_backend_config_t *backend)
{
    return backend != 0 && backend->vtable != 0 &&
           backend->vtable->create_session != 0 &&
           backend->vtable->destroy_session != 0 &&
           backend->vtable->get_local_fingerprint != 0 &&
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
    pc->local_dtls_fingerprint[0] = '\0';
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

rtc_status_t rtc_security_prepare_local_fingerprint(rtc_peer_connection_t *pc)
{
    rtc_status_t status;
    size_t fingerprint_len;

    if (pc == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (pc->security_backend == 0 || pc->security_backend->vtable == 0 ||
        pc->security_backend->vtable->get_local_fingerprint == 0 ||
        pc->security_session == 0) {
        return rtc_security_fail_fingerprint(
            pc, "local_fingerprint", RTC_STATUS_BACKEND_ERROR,
            "local_fingerprint_failed", 0);
    }

    fingerprint_len = sizeof(pc->local_dtls_fingerprint);
    status = pc->security_backend->vtable->get_local_fingerprint(
        pc->security_session, pc->local_dtls_fingerprint, &fingerprint_len);
    if (status != RTC_STATUS_OK || fingerprint_len == 0 ||
        fingerprint_len >= sizeof(pc->local_dtls_fingerprint)) {
        pc->local_dtls_fingerprint[0] = '\0';
        return rtc_security_fail_fingerprint(
            pc, "local_fingerprint", RTC_STATUS_BACKEND_ERROR,
            "local_fingerprint_failed", 0);
    }

    pc->local_dtls_fingerprint[fingerprint_len] = '\0';
    if (!rtc_security_is_sha256_fingerprint(pc->local_dtls_fingerprint,
                                            fingerprint_len)) {
        pc->local_dtls_fingerprint[0] = '\0';
        return rtc_security_fail_fingerprint(
            pc, "local_fingerprint", RTC_STATUS_PROTOCOL_ERROR,
            "unsupported_fingerprint_algorithm", 0);
    }

    pc->sdp.dtls_fingerprint = pc->local_dtls_fingerprint;
    pc->sdp.dtls_fingerprint_len = fingerprint_len;
    rtc_security_trace(pc, RTC_TRACE_DTLS_STATE, "local_fingerprint",
                       RTC_STATUS_OK, 0);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_security_verify_peer_fingerprint(rtc_peer_connection_t *pc)
{
    rtc_status_t status;
    char peer_fingerprint[128];
    size_t peer_fingerprint_len;
    size_t remote_fingerprint_len;

    if (pc == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (pc->security_backend == 0 || pc->security_backend->vtable == 0 ||
        pc->security_backend->vtable->get_peer_fingerprint == 0 ||
        pc->security_session == 0) {
        pc->dtls_state = RTC_SECURITY_DTLS_FAILED;
        pc->counters.dtls.handshake_failed++;
        rtc_observer_emit_state(&pc->observer, "dtls.failed");
        return rtc_security_fail_fingerprint(
            pc, "fingerprint_verify", RTC_STATUS_BACKEND_ERROR,
            "fingerprint_verify_failed", 0);
    }

    peer_fingerprint_len = sizeof(peer_fingerprint);
    status = pc->security_backend->vtable->get_peer_fingerprint(
        pc->security_session, peer_fingerprint, &peer_fingerprint_len);
    if (status != RTC_STATUS_OK || peer_fingerprint_len == 0 ||
        peer_fingerprint_len >= sizeof(peer_fingerprint)) {
        pc->dtls_state = RTC_SECURITY_DTLS_FAILED;
        pc->counters.dtls.handshake_failed++;
        rtc_observer_emit_state(&pc->observer, "dtls.failed");
        return rtc_security_fail_fingerprint(
            pc, "fingerprint_verify", RTC_STATUS_BACKEND_ERROR,
            "fingerprint_verify_failed", 0);
    }
    peer_fingerprint[peer_fingerprint_len] = '\0';

    if (!rtc_security_is_sha256_fingerprint(peer_fingerprint,
                                            peer_fingerprint_len)) {
        pc->dtls_state = RTC_SECURITY_DTLS_FAILED;
        pc->counters.dtls.handshake_failed++;
        rtc_observer_emit_state(&pc->observer, "dtls.failed");
        return rtc_security_fail_fingerprint(
            pc, "fingerprint_verify", RTC_STATUS_PROTOCOL_ERROR,
            "unsupported_fingerprint_algorithm", 0);
    }

    remote_fingerprint_len = strlen(pc->remote_summary.dtls_fingerprint);
    if (remote_fingerprint_len != peer_fingerprint_len ||
        memcmp(pc->remote_summary.dtls_fingerprint, peer_fingerprint,
               peer_fingerprint_len) != 0) {
        pc->dtls_state = RTC_SECURITY_DTLS_FAILED;
        pc->counters.dtls.fingerprint_mismatch++;
        pc->counters.dtls.handshake_failed++;
        rtc_observer_emit_state(&pc->observer, "dtls.failed");
        return rtc_security_fail_fingerprint(
            pc, "fingerprint_verify", RTC_STATUS_PROTOCOL_ERROR,
            "fingerprint_mismatch",
            RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH);
    }

    rtc_security_trace(pc, RTC_TRACE_DTLS_STATE, "fingerprint_verify",
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
    rtc_status_t status;
    rtc_security_dtls_role_t role = RTC_SECURITY_DTLS_ROLE_SERVER;

    if (pc == 0 || pc->security_backend == 0) {
        return RTC_STATUS_OK;
    }
    if (pc->ice_state != RTC_ICE_CONNECTED) {
        return RTC_STATUS_INVALID_STATE;
    }
    if (pc->dtls_state == RTC_SECURITY_DTLS_CONNECTING ||
        pc->dtls_state == RTC_SECURITY_DTLS_CONNECTED) {
        return RTC_STATUS_OK;
    }

    if (pc->remote_summary.dtls_setup[0] != '\0') {
        if (pc->remote_summary.dtls_setup[0] == 'p') {
            role = RTC_SECURITY_DTLS_ROLE_CLIENT;
        } else if (pc->remote_summary.dtls_setup[0] == 'a' &&
                   pc->remote_summary.dtls_setup[1] == 'c' &&
                   pc->remote_summary.dtls_setup[2] == 't' &&
                   pc->remote_summary.dtls_setup[3] == 'p') {
            role = pc->local_summary.dtls_setup[0] == 'a'
                       ? RTC_SECURITY_DTLS_ROLE_CLIENT
                       : RTC_SECURITY_DTLS_ROLE_SERVER;
        } else {
            role = RTC_SECURITY_DTLS_ROLE_SERVER;
        }
    }

    pc->dtls_role = role;
    pc->dtls_state = RTC_SECURITY_DTLS_CONNECTING;
    pc->counters.dtls.handshake_started++;
    rtc_observer_emit_state(&pc->observer, "dtls.connecting");
    rtc_security_trace(pc, RTC_TRACE_DTLS_HANDSHAKE, "start_dtls",
                       RTC_STATUS_OK, 0);

    status =
        pc->security_backend->vtable->start_dtls(pc->security_session, role);
    if (status != RTC_STATUS_OK) {
        pc->dtls_state = RTC_SECURITY_DTLS_FAILED;
        pc->counters.dtls.handshake_failed++;
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_BACKEND_ERROR,
                                "security", "start_dtls", 0);
        rtc_security_trace(pc, RTC_TRACE_DTLS_HANDSHAKE, "start_dtls",
                           RTC_STATUS_BACKEND_ERROR, "start_dtls_failed");
        return RTC_STATUS_BACKEND_ERROR;
    }

    return RTC_STATUS_OK;
}

rtc_status_t rtc_security_handle_dtls_datagram(rtc_peer_connection_t *pc,
                                               const uint8_t *data,
                                               size_t data_len)
{
    rtc_status_t status;

    if (pc == 0 || data == 0 || data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (pc->ice_state != RTC_ICE_CONNECTED) {
        pc->counters.dtls.early_datagrams_rejected++;
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_INVALID_STATE,
                                "security", "handle_dtls_datagram", 0);
        rtc_security_trace(pc, RTC_TRACE_DTLS_HANDSHAKE,
                           "handle_dtls_datagram", RTC_STATUS_INVALID_STATE,
                           "ice_not_connected");
        return RTC_STATUS_INVALID_STATE;
    }
    if (pc->security_backend == 0) {
        return RTC_STATUS_OK;
    }

    status = pc->security_backend->vtable->handle_dtls_datagram(
        pc->security_session, data, data_len);
    if (status != RTC_STATUS_OK) {
        pc->dtls_state = RTC_SECURITY_DTLS_FAILED;
        pc->counters.dtls.handshake_failed++;
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_BACKEND_ERROR,
                                "security", "handle_dtls_datagram", 0);
        rtc_security_trace(pc, RTC_TRACE_DTLS_HANDSHAKE,
                           "handle_dtls_datagram", RTC_STATUS_BACKEND_ERROR,
                           "backend_input_failed");
        return RTC_STATUS_BACKEND_ERROR;
    }

    return RTC_STATUS_OK;
}
