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

#include <string.h>

typedef struct security_test_state_t {
    int create_session_calls;
    int destroy_session_calls;
    int start_dtls_calls;
    int handle_dtls_datagram_calls;
    int datagram_count;
    int error_count;
    const char *last_error_subsystem;
    const char *last_error_operation;
    const char *last_trace_reason;
    void *last_storage;
    size_t last_storage_len;
    uint8_t last_datagram[64];
    size_t last_datagram_len;
    rtc_security_dtls_role_t last_role;
    rtc_security_backend_event_cb event_cb;
    void *event_user_data;
    rtc_status_t create_status;
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
    (void)status;
    (void)detail_code;
    state->error_count++;
    state->last_error_subsystem = subsystem;
    state->last_error_operation = operation;
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

static rtc_status_t test_start_dtls(void *session,
                                    rtc_security_dtls_role_t role)
{
    security_test_state_t *state = (security_test_state_t *)session;
    state->start_dtls_calls++;
    state->last_role = role;
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
    return RTC_STATUS_OK;
}

static rtc_security_backend_vtable_t test_backend_vtable(void)
{
    rtc_security_backend_vtable_t vtable;
    memset(&vtable, 0, sizeof(vtable));
    vtable.create_session = test_create_session;
    vtable.destroy_session = test_destroy_session;
    vtable.start_dtls = test_start_dtls;
    vtable.handle_dtls_datagram = test_handle_dtls_datagram;
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
    config.observer.on_trace = on_trace;
    config.observer.on_datagram = on_datagram;
    config.observer.user_data = state;
    config.security_backend = backend;
    return config;
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
    unsigned char arena[9000];
    security_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    config.limits.dtls.max_session_storage_bytes = 8192;
    backend.session_storage_bytes = 8192;

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
    RTC_TEST_ASSERT(strcmp(state.last_error_subsystem, "security") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_error_operation, "create_session") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_trace_reason, "backend_create_failed") ==
                    0);

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
    status = test_security_rejects_early_dtls_datagram();
    if (status != 0) {
        return status;
    }
    return test_security_starts_dtls_and_forwards_outgoing_datagram();
}
