#include "test_runner.h"

#include "api/peer_connection.h"
#include "executor/executor.h"
#include "rtc/config.h"
#include "rtc/counters.h"
#include "rtc/rtc.h"
#include "rtc/security.h"
#include "rtc/trace.h"

#include "observability/counters.h"
#include "security/security.h"
#include "srtp/srtp.h"

#include <string.h>

typedef struct security_test_state_t {
    int create_session_calls;
    int destroy_session_calls;
    int get_local_fingerprint_calls;
    int get_peer_fingerprint_calls;
    int export_keying_material_calls;
    int init_srtp_context_calls;
    int srtp_protect_rtp_calls;
    int srtp_unprotect_rtp_calls;
    int srtcp_protect_calls;
    int srtcp_unprotect_calls;
    int start_dtls_calls;
    int handle_dtls_datagram_calls;
    int datagram_count;
    int error_count;
    int state_count;
    const char *last_error_subsystem;
    const char *last_error_operation;
    rtc_status_t last_error_status;
    int last_error_detail;
    const char *last_state;
    const char *last_trace_reason;
    void *last_storage;
    size_t last_storage_len;
    uint8_t last_datagram[64];
    size_t last_datagram_len;
    rtc_security_dtls_role_t last_role;
    rtc_security_backend_event_cb event_cb;
    void *event_user_data;
    rtc_status_t create_status;
    rtc_status_t local_fingerprint_status;
    rtc_status_t peer_fingerprint_status;
    rtc_status_t start_dtls_status;
    rtc_status_t handle_dtls_datagram_status;
    rtc_status_t export_keying_material_status;
    rtc_status_t init_srtp_context_status;
    rtc_status_t srtp_protect_rtp_status;
    rtc_status_t srtp_unprotect_rtp_status;
    rtc_status_t srtcp_protect_status;
    rtc_status_t srtcp_unprotect_status;
    const char *local_fingerprint;
    const char *peer_fingerprint;
    const char *last_export_label;
    size_t last_export_label_len;
    size_t last_export_out_len;
    uint8_t last_keying_material[128];
    size_t last_keying_material_len;
    rtc_security_dtls_role_t last_srtp_role;
} security_test_state_t;

static rtc_status_t test_post(void *user_data, rtc_executor_task_fn task,
                              void *task_user_data)
{
    (void)user_data;
    if (task != 0) {
        task(task_user_data);
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_schedule_timer(void *user_data, uint64_t delay_ms,
                                        rtc_executor_task_fn task,
                                        void *task_user_data,
                                        uint64_t *out_timer_id)
{
    (void)user_data;
    (void)delay_ms;
    (void)task;
    (void)task_user_data;
    if (out_timer_id != 0) {
        *out_timer_id = 11;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_cancel_timer(void *user_data, uint64_t timer_id)
{
    (void)user_data;
    (void)timer_id;
    return RTC_STATUS_OK;
}

static rtc_executor_vtable_t test_executor(void *user_data)
{
    rtc_executor_vtable_t executor;
    executor.post = test_post;
    executor.schedule_timer = test_schedule_timer;
    executor.cancel_timer = test_cancel_timer;
    executor.user_data = user_data;
    return executor;
}

static void on_error(void *user_data, rtc_status_t status,
                     const char *subsystem, const char *operation,
                     int detail_code)
{
    security_test_state_t *state = (security_test_state_t *)user_data;
    state->error_count++;
    state->last_error_status = status;
    state->last_error_subsystem = subsystem;
    state->last_error_operation = operation;
    state->last_error_detail = detail_code;
}

static void on_state(void *user_data, const char *state_name)
{
    security_test_state_t *state = (security_test_state_t *)user_data;
    state->state_count++;
    state->last_state = state_name;
}

static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    security_test_state_t *state = (security_test_state_t *)user_data;
    if (data_len > sizeof(state->last_datagram)) {
        data_len = sizeof(state->last_datagram);
    }
    memcpy(state->last_datagram, data, data_len);
    state->last_datagram_len = data_len;
    state->datagram_count++;
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    security_test_state_t *state = (security_test_state_t *)user_data;
    size_t i;

    (void)event;
    for (i = 0; i < field_count; ++i) {
        if (strcmp(fields[i].key, RTC_TRACE_FIELD_REASON) == 0) {
            state->last_trace_reason = fields[i].value;
        }
    }
}

static rtc_status_t test_create_session(
    void *backend_user_data, void *storage, size_t storage_len,
    rtc_security_backend_event_cb event_cb, void *event_user_data,
    void **out_session)
{
    security_test_state_t *state = (security_test_state_t *)backend_user_data;

    state->create_session_calls++;
    state->last_storage = storage;
    state->last_storage_len = storage_len;
    state->event_cb = event_cb;
    state->event_user_data = event_user_data;
    if (state->create_status != RTC_STATUS_OK) {
        return state->create_status;
    }
    *out_session = state;
    return RTC_STATUS_OK;
}

static void test_destroy_session(void *session)
{
    security_test_state_t *state = (security_test_state_t *)session;
    state->destroy_session_calls++;
}

static rtc_status_t test_get_local_fingerprint(void *session, char *out,
                                               size_t *inout_len)
{
    security_test_state_t *state = (security_test_state_t *)session;
    const char *fingerprint = state->local_fingerprint;
    size_t len;

    state->get_local_fingerprint_calls++;
    if (state->local_fingerprint_status != RTC_STATUS_OK) {
        return state->local_fingerprint_status;
    }
    if (fingerprint == 0) {
        fingerprint =
            "sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
            "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";
    }
    len = strlen(fingerprint);
    if (inout_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (out == 0 || *inout_len <= len) {
        *inout_len = len + 1u;
        return RTC_STATUS_CAPACITY;
    }
    memcpy(out, fingerprint, len + 1u);
    *inout_len = len;
    return RTC_STATUS_OK;
}

static rtc_status_t test_get_peer_fingerprint(void *session, char *out,
                                              size_t *inout_len)
{
    security_test_state_t *state = (security_test_state_t *)session;
    const char *fingerprint = state->peer_fingerprint;
    size_t len;

    state->get_peer_fingerprint_calls++;
    if (state->peer_fingerprint_status != RTC_STATUS_OK) {
        return state->peer_fingerprint_status;
    }
    if (fingerprint == 0) {
        fingerprint =
            "sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
            "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99";
    }
    len = strlen(fingerprint);
    if (inout_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (out == 0 || *inout_len <= len) {
        *inout_len = len + 1u;
        return RTC_STATUS_CAPACITY;
    }
    memcpy(out, fingerprint, len + 1u);
    *inout_len = len;
    return RTC_STATUS_OK;
}

static rtc_status_t test_start_dtls(void *session,
                                    rtc_security_dtls_role_t role)
{
    security_test_state_t *state = (security_test_state_t *)session;
    state->start_dtls_calls++;
    state->last_role = role;
    if (state->start_dtls_status != RTC_STATUS_OK) {
        return state->start_dtls_status;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_handle_dtls_datagram(void *session,
                                              const uint8_t *packet,
                                              size_t packet_len)
{
    security_test_state_t *state = (security_test_state_t *)session;
    (void)packet;
    (void)packet_len;
    state->handle_dtls_datagram_calls++;
    if (state->handle_dtls_datagram_status != RTC_STATUS_OK) {
        return state->handle_dtls_datagram_status;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_export_keying_material(void *session,
                                                const char *label,
                                                size_t label_len,
                                                uint8_t *out, size_t out_len)
{
    security_test_state_t *state = (security_test_state_t *)session;
    size_t i;

    state->export_keying_material_calls++;
    state->last_export_label = label;
    state->last_export_label_len = label_len;
    state->last_export_out_len = out_len;
    if (state->export_keying_material_status != RTC_STATUS_OK) {
        return state->export_keying_material_status;
    }
    if (out == 0 || out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    for (i = 0; i < out_len; ++i) {
        out[i] = (uint8_t)(0xA0u + (uint8_t)i);
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_init_srtp_context(void *session,
                                           const uint8_t *keying_material,
                                           size_t keying_material_len,
                                           rtc_security_dtls_role_t local_role)
{
    security_test_state_t *state = (security_test_state_t *)session;

    state->init_srtp_context_calls++;
    state->last_srtp_role = local_role;
    state->last_keying_material_len = keying_material_len;
    if (keying_material_len > sizeof(state->last_keying_material)) {
        keying_material_len = sizeof(state->last_keying_material);
    }
    if (keying_material != 0 && keying_material_len > 0) {
        memcpy(state->last_keying_material, keying_material,
               keying_material_len);
    }
    if (state->init_srtp_context_status != RTC_STATUS_OK) {
        return state->init_srtp_context_status;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_srtp_protect_rtp(void *session, uint8_t *packet,
                                          size_t *inout_len, size_t capacity)
{
    security_test_state_t *state = (security_test_state_t *)session;

    state->srtp_protect_rtp_calls++;
    if (state->srtp_protect_rtp_status != RTC_STATUS_OK) {
        return state->srtp_protect_rtp_status;
    }
    if (packet == 0 || inout_len == 0 || capacity < *inout_len + 4u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    packet[*inout_len] = 0xAA;
    packet[*inout_len + 1u] = 0xBB;
    packet[*inout_len + 2u] = 0xCC;
    packet[*inout_len + 3u] = 0xDD;
    *inout_len += 4u;
    return RTC_STATUS_OK;
}

static rtc_status_t test_srtp_unprotect_rtp(void *session, uint8_t *packet,
                                            size_t *inout_len)
{
    security_test_state_t *state = (security_test_state_t *)session;

    (void)packet;
    state->srtp_unprotect_rtp_calls++;
    if (state->srtp_unprotect_rtp_status != RTC_STATUS_OK) {
        return state->srtp_unprotect_rtp_status;
    }
    if (inout_len == 0 || *inout_len < 4u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *inout_len -= 4u;
    return RTC_STATUS_OK;
}

static rtc_status_t test_srtcp_protect(void *session, uint8_t *packet,
                                       size_t *inout_len, size_t capacity)
{
    security_test_state_t *state = (security_test_state_t *)session;

    state->srtcp_protect_calls++;
    return test_srtp_protect_rtp(session, packet, inout_len, capacity);
}

static rtc_status_t test_srtcp_unprotect(void *session, uint8_t *packet,
                                         size_t *inout_len)
{
    security_test_state_t *state = (security_test_state_t *)session;

    state->srtcp_unprotect_calls++;
    return test_srtp_unprotect_rtp(session, packet, inout_len);
}

static rtc_security_backend_vtable_t test_backend_vtable(void)
{
    rtc_security_backend_vtable_t vtable;
    memset(&vtable, 0, sizeof(vtable));
    vtable.create_session = test_create_session;
    vtable.destroy_session = test_destroy_session;
    vtable.get_local_fingerprint = test_get_local_fingerprint;
    vtable.start_dtls = test_start_dtls;
    vtable.handle_dtls_datagram = test_handle_dtls_datagram;
    vtable.get_peer_fingerprint = test_get_peer_fingerprint;
    vtable.export_keying_material = test_export_keying_material;
    vtable.init_srtp_context = test_init_srtp_context;
    vtable.srtp_protect_rtp = test_srtp_protect_rtp;
    vtable.srtp_unprotect_rtp = test_srtp_unprotect_rtp;
    vtable.srtcp_protect = test_srtcp_protect;
    vtable.srtcp_unprotect = test_srtcp_unprotect;
    return vtable;
}

static rtc_peer_connection_config_t test_config(
    unsigned char *arena, size_t arena_size, security_test_state_t *state,
    rtc_security_backend_config_t *backend,
    rtc_security_backend_vtable_t *vtable)
{
    rtc_peer_connection_config_t config;
    memset(&config, 0, sizeof(config));
    memset(backend, 0, sizeof(*backend));
    *vtable = test_backend_vtable();
    backend->vtable = vtable;
    backend->user_data = state;
    backend->session_storage_bytes = 64;

    config.arena.data = arena;
    config.arena.size = arena_size;
    config.limits.sdp.max_description_bytes = 2048;
    config.limits.ice.max_candidates = 4;
    config.limits.ice.max_candidate_pairs = 4;
    config.limits.ice.max_transactions = 2;
    config.limits.ice.max_timer_slots = 4;
    config.limits.dtls.max_sessions = 1;
    config.limits.dtls.max_session_storage_bytes = 64;
    config.limits.rtp.max_packet_cache = 16;
    config.limits.rtcp.max_reports = 4;
    config.limits.trace.max_events = 16;
    config.sdp.ice_ufrag = "testufrag";
    config.sdp.ice_ufrag_len = strlen(config.sdp.ice_ufrag);
    config.sdp.ice_pwd = "testpassword1234567890";
    config.sdp.ice_pwd_len = strlen(config.sdp.ice_pwd);
    config.sdp.dtls_fingerprint =
        "sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    config.sdp.dtls_fingerprint_len = strlen(config.sdp.dtls_fingerprint);
    config.sdp.dtls_setup = "actpass";
    config.sdp.dtls_setup_len = strlen(config.sdp.dtls_setup);
    config.sdp.session_id = 1000;
    config.sdp.session_version = 2;
    config.local_host_ip = "192.0.2.10";
    config.local_host_ip_len = strlen(config.local_host_ip);
    config.local_host_port = 5000;
    config.executors.signaling = test_executor(state);
    config.executors.media = test_executor(state);
    config.executors.network = test_executor(state);
    config.observer.on_error = on_error;
    config.observer.on_state = on_state;
    config.observer.on_trace = on_trace;
    config.observer.on_datagram = on_datagram;
    config.observer.user_data = state;
    config.security_backend = backend;
    return config;
}

static int assert_last_security_error(security_test_state_t *state,
                                      rtc_status_t status,
                                      const char *subsystem,
                                      const char *operation,
                                      int detail_code,
                                      const char *reason)
{
    RTC_TEST_EQ_INT(status, state->last_error_status);
    RTC_TEST_ASSERT(strcmp(state->last_error_subsystem, subsystem) == 0);
    RTC_TEST_ASSERT(strcmp(state->last_error_operation, operation) == 0);
    RTC_TEST_EQ_INT(detail_code, state->last_error_detail);
    RTC_TEST_ASSERT(strcmp(state->last_trace_reason, reason) == 0);
    return 0;
}

static int test_security_offer_uses_backend_fingerprint(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    char offer[2048];
    size_t offer_len;

    memset(&state, 0, sizeof(state));
    state.local_fingerprint =
        "sha-256 10:20:30:40:50:60:70:80:90:A0:B0:C0:D0:E0:F0:00:"
        "10:20:30:40:50:60:70:80:90:A0:B0:C0:D0:E0:F0:00";
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    config.sdp.dtls_fingerprint =
        "sha-256 FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:"
        "FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF:FF";
    config.sdp.dtls_fingerprint_len = strlen(config.sdp.dtls_fingerprint);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    offer_len = sizeof(offer);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create_offer(pc, offer, &offer_len));
    RTC_TEST_EQ_INT(1, state.get_local_fingerprint_calls);
    RTC_TEST_ASSERT(strstr(offer, state.local_fingerprint) != 0);
    RTC_TEST_ASSERT(strstr(offer, config.sdp.dtls_fingerprint) == 0);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_answer_uses_backend_fingerprint(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    char offer[2048];
    char answer[2048];
    size_t offer_len;
    size_t answer_len;

    memset(&state, 0, sizeof(state));
    state.local_fingerprint =
        "sha-256 AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:"
        "AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89";
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    offer_len = sizeof(offer);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_sdp_write_offer(&config.sdp,
                                        RTC_SDP_DIRECTION_SENDRECV,
                                        RTC_SDP_DIRECTION_SENDRECV, offer,
                                        &offer_len));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_set_remote_description(pc, offer,
                                                               offer_len));
    answer_len = sizeof(answer);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create_answer(pc, answer,
                                                      &answer_len));
    RTC_TEST_EQ_INT(1, state.get_local_fingerprint_calls);
    RTC_TEST_ASSERT(strstr(answer, state.local_fingerprint) != 0);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_rejects_unsupported_backend_fingerprint_algorithm(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    char offer[2048];
    size_t offer_len;

    memset(&state, 0, sizeof(state));
    state.local_fingerprint =
        "sha-384 10:20:30:40:50:60:70:80:90:A0:B0:C0:D0:E0:F0:00";
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    offer_len = sizeof(offer);
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_create_offer(pc, offer, &offer_len));
    RTC_TEST_ASSERT(strcmp(state.last_error_subsystem, "dtls") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_error_operation, "local_fingerprint") ==
                    0);
    RTC_TEST_ASSERT(strcmp(state.last_trace_reason,
                           "unsupported_fingerprint_algorithm") == 0);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_backend_contract(void)
{
    rtc_security_backend_event_t event;
    rtc_security_backend_config_t backend;
    rtc_peer_connection_config_t config;

    event.type = RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM;
    event.datagram = 0;
    event.datagram_len = 0;
    event.status = RTC_STATUS_OK;
    event.detail_code = 0;

    backend.vtable = 0;
    backend.user_data = 0;
    backend.session_storage_bytes = 32;

    config.security_backend = &backend;

    RTC_TEST_EQ_INT(RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM, event.type);
    RTC_TEST_ASSERT(config.security_backend == &backend);

    return 0;
}

static int test_security_create_time_session_storage(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(1, state.create_session_calls);
    RTC_TEST_ASSERT(state.last_storage != 0);
    RTC_TEST_EQ_INT(64, state.last_storage_len);
    RTC_TEST_ASSERT(pc->security_session_storage == state.last_storage);
    RTC_TEST_EQ_INT(64, pc->security_session_storage_bytes);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    RTC_TEST_EQ_INT(1, state.destroy_session_calls);

    return 0;
}

static int test_security_session_storage_capacity(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    config.limits.dtls.max_session_storage_bytes = 20000;
    backend.session_storage_bytes = 20000;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_CAPACITY_RESOURCE_DTLS_SESSION, diag.resource);
    RTC_TEST_EQ_INT(0, state.create_session_calls);

    return 0;
}

static int test_security_backend_create_failure_is_diagnostic(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.create_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_BACKEND_ERROR,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(1, state.create_session_calls);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_BACKEND_ERROR, "security",
                        "create_session",
                        RTC_SECURITY_DETAIL_HANDSHAKE_FAILED,
                        "handshake_failed"));

    return 0;
}

static int test_security_rejects_early_dtls_datagram(void)
{
    unsigned char arena[16384];
    uint8_t dtls[3] = {22, 0xfe, 0xfd};
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_receive_datagram(pc, dtls,
                                                         sizeof(dtls)));
    RTC_TEST_EQ_INT(0, state.handle_dtls_datagram_calls);
    RTC_TEST_ASSERT(strcmp(state.last_trace_reason, "ice_not_connected") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.dtls.early_datagrams_rejected);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_starts_dtls_and_forwards_outgoing_datagram(void)
{
    static const uint8_t outgoing[] = {22, 0xfe, 0xfd, 0x01};
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_security_backend_event_t event;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_state = RTC_ICE_CONNECTED;
    pc->local_summary.type = RTC_SDP_TYPE_OFFER;
    pc->remote_summary.type = RTC_SDP_TYPE_ANSWER;
    memcpy(pc->remote_summary.dtls_setup, "active", 7);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_security_on_ice_connected(pc));
    RTC_TEST_EQ_INT(1, state.start_dtls_calls);
    RTC_TEST_EQ_INT(RTC_SECURITY_DTLS_ROLE_SERVER, state.last_role);

    memset(&event, 0, sizeof(event));
    event.type = RTC_SECURITY_BACKEND_EVENT_OUTGOING_DATAGRAM;
    event.datagram = outgoing;
    event.datagram_len = sizeof(outgoing);
    state.event_cb(state.event_user_data, &event);
    RTC_TEST_EQ_INT(1, state.datagram_count);
    RTC_TEST_EQ_INT((int)sizeof(outgoing), (int)state.last_datagram_len);
    RTC_TEST_ASSERT(memcmp(state.last_datagram, outgoing, sizeof(outgoing)) ==
                    0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.dtls.handshake_started);
    RTC_TEST_EQ_INT(1, (int)counters.dtls.outgoing_datagrams);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_handshake_backend_error_maps_stable_detail(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.start_dtls_status = RTC_STATUS_PROTOCOL_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_state = RTC_ICE_CONNECTED;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_BACKEND_ERROR,
                    rtc_security_on_ice_connected(pc));
    RTC_TEST_EQ_INT(1, state.start_dtls_calls);
    RTC_TEST_EQ_INT(RTC_SECURITY_DTLS_FAILED, pc->dtls_state);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_BACKEND_ERROR, "security",
                        "start_dtls",
                        RTC_SECURITY_DETAIL_HANDSHAKE_FAILED,
                        "handshake_failed"));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.dtls.handshake_started);
    RTC_TEST_EQ_INT(1, (int)counters.dtls.handshake_failed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_fingerprint_mismatch_blocks_key_export(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_security_backend_event_t event;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.peer_fingerprint =
        "sha-256 99:99:99:99:99:99:99:99:99:99:99:99:99:99:99:99:"
        "99:99:99:99:99:99:99:99:99:99:99:99:99:99:99:99";
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_state = RTC_ICE_CONNECTED;
    memcpy(pc->remote_summary.dtls_fingerprint,
           "sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
           "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99",
           sizeof("sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
                  "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99"));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_security_on_ice_connected(pc));
    memset(&event, 0, sizeof(event));
    event.type = RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE;
    state.event_cb(state.event_user_data, &event);

    RTC_TEST_EQ_INT(1, state.get_peer_fingerprint_calls);
    RTC_TEST_EQ_INT(0, state.export_keying_material_calls);
    RTC_TEST_ASSERT(strcmp(state.last_state, "dtls.failed") == 0);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_PROTOCOL_ERROR, "dtls",
                        "fingerprint_verify",
                        RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH,
                        "fingerprint_mismatch"));
    RTC_TEST_EQ_INT(RTC_SECURITY_DTLS_FAILED, pc->dtls_state);
    RTC_TEST_EQ_INT(0, pc->srtp_ready);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.dtls.fingerprint_mismatch);
    RTC_TEST_EQ_INT(1, (int)counters.dtls.handshake_failed);
    RTC_TEST_EQ_INT(0, (int)counters.dtls.handshake_completed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static void set_matching_remote_fingerprint(rtc_peer_connection_t *pc)
{
    memcpy(pc->remote_summary.dtls_fingerprint,
           "sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
           "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99",
           sizeof("sha-256 AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:"
                  "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99"));
}

static int test_security_handshake_complete_exports_key_and_sets_srtp_ready(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_security_backend_event_t event;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_state = RTC_ICE_CONNECTED;
    pc->dtls_role = RTC_SECURITY_DTLS_ROLE_SERVER;
    set_matching_remote_fingerprint(pc);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    memset(&event, 0, sizeof(event));
    event.type = RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE;
    state.event_cb(state.event_user_data, &event);

    RTC_TEST_EQ_INT(1, state.get_peer_fingerprint_calls);
    RTC_TEST_EQ_INT(1, state.export_keying_material_calls);
    RTC_TEST_EQ_INT(1, state.init_srtp_context_calls);
    RTC_TEST_ASSERT(strcmp(state.last_export_label, "EXTRACTOR-dtls_srtp") ==
                    0);
    RTC_TEST_EQ_INT(19, (int)state.last_export_label_len);
    RTC_TEST_EQ_INT((int)state.last_export_out_len,
                    (int)state.last_keying_material_len);
    RTC_TEST_ASSERT(memcmp(state.last_keying_material,
                           (const uint8_t *)"\xA0\xA1\xA2\xA3", 4) == 0);
    RTC_TEST_EQ_INT(RTC_SECURITY_DTLS_ROLE_SERVER, state.last_srtp_role);
    RTC_TEST_EQ_INT(RTC_SECURITY_DTLS_CONNECTED, pc->dtls_state);
    RTC_TEST_EQ_INT(1, pc->srtp_ready);
    RTC_TEST_ASSERT(strcmp(state.last_state, "srtp.ready") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.dtls.handshake_completed);
    RTC_TEST_EQ_INT(0, (int)counters.dtls.key_export_failed);
    RTC_TEST_EQ_INT(0, (int)counters.dtls.srtp_init_failed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_key_export_failure_blocks_srtp_ready(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_security_backend_event_t event;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.export_keying_material_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_state = RTC_ICE_CONNECTED;
    set_matching_remote_fingerprint(pc);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    memset(&event, 0, sizeof(event));
    event.type = RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE;
    state.event_cb(state.event_user_data, &event);

    RTC_TEST_EQ_INT(1, state.export_keying_material_calls);
    RTC_TEST_EQ_INT(0, state.init_srtp_context_calls);
    RTC_TEST_EQ_INT(RTC_SECURITY_DTLS_FAILED, pc->dtls_state);
    RTC_TEST_EQ_INT(0, pc->srtp_ready);
    RTC_TEST_ASSERT(strcmp(state.last_state, "dtls.failed") == 0);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_BACKEND_ERROR, "srtp",
                        "key_export",
                        RTC_SECURITY_DETAIL_KEY_EXPORT_FAILED,
                        "key_export_failed"));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.dtls.key_export_failed);
    RTC_TEST_EQ_INT(1, (int)counters.dtls.handshake_failed);
    RTC_TEST_EQ_INT(0, (int)counters.dtls.handshake_completed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_srtp_init_failure_blocks_srtp_ready(void)
{
    unsigned char arena[16384];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_security_backend_event_t event;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.init_srtp_context_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_state = RTC_ICE_CONNECTED;
    set_matching_remote_fingerprint(pc);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    memset(&event, 0, sizeof(event));
    event.type = RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE;
    state.event_cb(state.event_user_data, &event);

    RTC_TEST_EQ_INT(1, state.export_keying_material_calls);
    RTC_TEST_EQ_INT(1, state.init_srtp_context_calls);
    RTC_TEST_EQ_INT(RTC_SECURITY_DTLS_FAILED, pc->dtls_state);
    RTC_TEST_EQ_INT(0, pc->srtp_ready);
    RTC_TEST_ASSERT(strcmp(state.last_state, "dtls.failed") == 0);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_BACKEND_ERROR, "srtp",
                        "srtp_init",
                        RTC_SECURITY_DETAIL_SRTP_INIT_FAILED,
                        "srtp_init_failed"));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.dtls.srtp_init_failed);
    RTC_TEST_EQ_INT(1, (int)counters.dtls.handshake_failed);
    RTC_TEST_EQ_INT(0, (int)counters.dtls.handshake_completed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_srtp_protect_requires_ready(void)
{
    unsigned char arena[16384];
    uint8_t packet[32] = {0x80, 0x60, 0, 1};
    size_t packet_len = 4;
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_srtp_protect_rtp(pc, packet, &packet_len,
                                         sizeof(packet)));
    RTC_TEST_EQ_INT(0, state.srtp_protect_rtp_calls);
    RTC_TEST_ASSERT(strcmp(state.last_trace_reason, "srtp_not_ready") == 0);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_srtp_protect_failure_does_not_emit_datagram(void)
{
    unsigned char arena[16384];
    uint8_t packet[32] = {0x80, 0x60, 0, 1};
    size_t packet_len = 4;
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.srtp_protect_rtp_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;
    RTC_TEST_EQ_INT(RTC_STATUS_BACKEND_ERROR,
                    rtc_srtp_protect_rtp(pc, packet, &packet_len,
                                         sizeof(packet)));
    RTC_TEST_EQ_INT(1, state.srtp_protect_rtp_calls);
    RTC_TEST_EQ_INT(0, state.datagram_count);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_BACKEND_ERROR, "srtp",
                        "protect_rtp",
                        RTC_SECURITY_DETAIL_SRTP_PROTECT_FAILED,
                        "srtp_protect_failed"));

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.srtp.protect_failed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_srtp_unprotect_failure_blocks_media_output(void)
{
    unsigned char arena[16384];
    uint8_t packet[32] = {0x80, 0x60, 0, 1, 0xAA, 0xBB, 0xCC, 0xDD};
    size_t packet_len = 8;
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.srtp_unprotect_rtp_status = RTC_STATUS_PROTOCOL_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_srtp_unprotect_rtp(pc, packet, &packet_len));
    RTC_TEST_EQ_INT(1, state.srtp_unprotect_rtp_calls);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_PROTOCOL_ERROR, "srtp",
                        "unprotect_rtp",
                        RTC_SECURITY_DETAIL_SRTP_REPLAY_FAILED,
                        "srtp_replay_failed"));

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.srtp.unprotect_failed);
    RTC_TEST_EQ_INT(1, (int)counters.srtp.replay_failed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_srtcp_unprotect_backend_failure_uses_unprotect_detail(void)
{
    unsigned char arena[16384];
    uint8_t packet[32] = {0x81, 0xc9, 0, 1, 0xAA, 0xBB, 0xCC, 0xDD};
    size_t packet_len = 8;
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_counters_t counters;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.srtcp_unprotect_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;
    RTC_TEST_EQ_INT(RTC_STATUS_BACKEND_ERROR,
                    rtc_srtp_unprotect_rtcp(pc, packet, &packet_len));
    RTC_TEST_EQ_INT(1, state.srtcp_unprotect_calls);
    RTC_TEST_EQ_INT(0,
                    assert_last_security_error(
                        &state, RTC_STATUS_BACKEND_ERROR, "srtp",
                        "unprotect_rtcp",
                        RTC_SECURITY_DETAIL_SRTP_UNPROTECT_FAILED,
                        "srtp_unprotect_failed"));

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.srtp.unprotect_failed);
    RTC_TEST_EQ_INT(0, (int)counters.srtp.replay_failed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_srtcp_wrappers_call_backend(void)
{
    unsigned char arena[16384];
    uint8_t packet[32] = {0x81, 0xc9, 0, 1};
    size_t packet_len = 4;
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_srtp_protect_rtcp(pc, packet, &packet_len,
                                          sizeof(packet)));
    RTC_TEST_EQ_INT(1, state.srtcp_protect_calls);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_srtp_unprotect_rtcp(pc, packet, &packet_len));
    RTC_TEST_EQ_INT(1, state.srtcp_unprotect_calls);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_security_counters_and_trace_contract(void)
{
    rtc_peer_connection_counters_t counters;
    rtc_security_backend_event_t event;

    rtc_counters_init(&counters);

    RTC_TEST_EQ_INT(0, counters.dtls.handshake_started);
    RTC_TEST_EQ_INT(0, counters.dtls.handshake_completed);
    RTC_TEST_EQ_INT(0, counters.dtls.handshake_failed);
    RTC_TEST_EQ_INT(0, counters.dtls.early_datagrams_rejected);
    RTC_TEST_EQ_INT(0, counters.dtls.outgoing_datagrams);
    RTC_TEST_EQ_INT(0, counters.dtls.fingerprint_mismatch);
    RTC_TEST_EQ_INT(0, counters.dtls.key_export_failed);
    RTC_TEST_EQ_INT(0, counters.dtls.srtp_init_failed);
    RTC_TEST_EQ_INT(0, counters.srtp.protect_failed);
    RTC_TEST_EQ_INT(0, counters.srtp.unprotect_failed);
    RTC_TEST_EQ_INT(0, counters.srtp.replay_failed);

    event.type = RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE;
    event.datagram = 0;
    event.datagram_len = 0;
    event.status = RTC_STATUS_OK;
    event.detail_code = 0;

    RTC_TEST_ASSERT(RTC_TRACE_DTLS_STATE[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_DTLS_HANDSHAKE[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_SRTP_STATE[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_SRTP_PROTECT[0] != '\0');
    RTC_TEST_EQ_INT(RTC_SECURITY_BACKEND_EVENT_HANDSHAKE_COMPLETE, event.type);
    RTC_TEST_EQ_INT(4001, RTC_SECURITY_DETAIL_HANDSHAKE_FAILED);
    RTC_TEST_EQ_INT(4002, RTC_SECURITY_DETAIL_FINGERPRINT_MISMATCH);
    RTC_TEST_EQ_INT(4003, RTC_SECURITY_DETAIL_KEY_EXPORT_FAILED);
    RTC_TEST_EQ_INT(4004, RTC_SECURITY_DETAIL_SRTP_INIT_FAILED);
    RTC_TEST_EQ_INT(4005, RTC_SECURITY_DETAIL_SRTP_PROTECT_FAILED);
    RTC_TEST_EQ_INT(4006, RTC_SECURITY_DETAIL_SRTP_UNPROTECT_FAILED);
    RTC_TEST_EQ_INT(4007, RTC_SECURITY_DETAIL_SRTP_REPLAY_FAILED);

    return 0;
}

int rtc_test_security(void)
{
    int status = test_security_backend_contract();
    if (status != 0) {
        return status;
    }
    status = test_security_counters_and_trace_contract();
    if (status != 0) {
        return status;
    }
    status = test_security_create_time_session_storage();
    if (status != 0) {
        return status;
    }
    status = test_security_session_storage_capacity();
    if (status != 0) {
        return status;
    }
    status = test_security_backend_create_failure_is_diagnostic();
    if (status != 0) {
        return status;
    }
    status = test_security_offer_uses_backend_fingerprint();
    if (status != 0) {
        return status;
    }
    status = test_security_answer_uses_backend_fingerprint();
    if (status != 0) {
        return status;
    }
    status = test_security_rejects_unsupported_backend_fingerprint_algorithm();
    if (status != 0) {
        return status;
    }
    status = test_security_rejects_early_dtls_datagram();
    if (status != 0) {
        return status;
    }
    status = test_security_starts_dtls_and_forwards_outgoing_datagram();
    if (status != 0) {
        return status;
    }
    status = test_security_handshake_backend_error_maps_stable_detail();
    if (status != 0) {
        return status;
    }
    status = test_security_fingerprint_mismatch_blocks_key_export();
    if (status != 0) {
        return status;
    }
    status = test_security_handshake_complete_exports_key_and_sets_srtp_ready();
    if (status != 0) {
        return status;
    }
    status = test_security_key_export_failure_blocks_srtp_ready();
    if (status != 0) {
        return status;
    }
    status = test_security_srtp_init_failure_blocks_srtp_ready();
    if (status != 0) {
        return status;
    }
    status = test_srtp_protect_requires_ready();
    if (status != 0) {
        return status;
    }
    status = test_srtp_protect_failure_does_not_emit_datagram();
    if (status != 0) {
        return status;
    }
    status = test_srtp_unprotect_failure_blocks_media_output();
    if (status != 0) {
        return status;
    }
    status = test_srtcp_unprotect_backend_failure_uses_unprotect_detail();
    if (status != 0) {
        return status;
    }
    return test_srtcp_wrappers_call_backend();
}
