#include "executor/executor.h"
#include "ice/ice.h"
#include "rtc/rtc.h"
#include "rtc/security.h"
#include "stun/stun.h"
#include "test_runner.h"

#include <string.h>

typedef struct observer_state_t {
    int error_count;
    int trace_count;
    int local_candidate_count;
    int datagram_count;
    int saw_checking;
    int saw_connected;
    int saw_failed;
    uint8_t last_datagram[64];
    size_t last_datagram_len;
    rtc_status_t last_error;
    const char *last_trace;
    const char *last_state;
} observer_state_t;

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
        *out_timer_id = 1;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_cancel_timer(void *user_data, uint64_t timer_id)
{
    (void)user_data;
    (void)timer_id;
    return RTC_STATUS_OK;
}

static rtc_executor_vtable_t test_executor(void)
{
    rtc_executor_vtable_t executor;
    executor.post = test_post;
    executor.schedule_timer = test_schedule_timer;
    executor.cancel_timer = test_cancel_timer;
    executor.user_data = 0;
    return executor;
}

static rtc_status_t test_security_backend_create_session(
    void *backend_user_data, void *storage, size_t storage_len,
    rtc_security_backend_event_cb event_cb, void *event_user_data,
    void **out_session)
{
    (void)storage;
    (void)storage_len;
    (void)event_cb;
    (void)event_user_data;
    *out_session = backend_user_data;
    return RTC_STATUS_OK;
}

static void test_security_backend_destroy_session(void *session)
{
    (void)session;
}

static rtc_status_t test_security_backend_get_local_fingerprint(
    void *session, char *out, size_t *inout_len)
{
    static const char backend_fingerprint[] =
        "sha-256 24:68:AC:E0:24:68:AC:E0:24:68:AC:E0:24:68:AC:E0:"
        "24:68:AC:E0:24:68:AC:E0:24:68:AC:E0:24:68:AC:E0";
    size_t len = strlen(backend_fingerprint);
    (void)session;
    if (out == 0 || inout_len == 0 || *inout_len <= len) {
        if (inout_len != 0) {
            *inout_len = len + 1u;
        }
        return RTC_STATUS_CAPACITY;
    }
    memcpy(out, backend_fingerprint, len + 1u);
    *inout_len = len;
    return RTC_STATUS_OK;
}

static rtc_status_t test_security_backend_start_dtls(
    void *session, rtc_security_dtls_role_t role)
{
    (void)session;
    (void)role;
    return RTC_STATUS_OK;
}

static rtc_status_t test_security_backend_handle_dtls_datagram(
    void *session, const uint8_t *packet, size_t packet_len)
{
    (void)session;
    (void)packet;
    (void)packet_len;
    return RTC_STATUS_OK;
}

static const rtc_security_backend_config_t *test_security_backend(void)
{
    static int state;
    static rtc_security_backend_vtable_t vtable;
    static rtc_security_backend_config_t backend;

    if (backend.vtable == 0) {
        memset(&vtable, 0, sizeof(vtable));
        vtable.create_session = test_security_backend_create_session;
        vtable.destroy_session = test_security_backend_destroy_session;
        vtable.get_local_fingerprint =
            test_security_backend_get_local_fingerprint;
        vtable.start_dtls = test_security_backend_start_dtls;
        vtable.handle_dtls_datagram =
            test_security_backend_handle_dtls_datagram;
        backend.vtable = &vtable;
        backend.user_data = &state;
        backend.session_storage_bytes = 64;
    }
    return &backend;
}

static void on_error(void *user_data, rtc_status_t status,
                     const char *subsystem, const char *operation,
                     int detail_code)
{
    observer_state_t *state = (observer_state_t *)user_data;
    (void)subsystem;
    (void)operation;
    (void)detail_code;
    state->error_count++;
    state->last_error = status;
}

static void on_state(void *user_data, const char *state)
{
    observer_state_t *observer = (observer_state_t *)user_data;
    observer->last_state = state;
    if (strcmp(state, "ice.checking") == 0) {
        observer->saw_checking = 1;
    } else if (strcmp(state, "ice.connected") == 0) {
        observer->saw_connected = 1;
    } else if (strcmp(state, "ice.failed") == 0) {
        observer->saw_failed = 1;
    }
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    observer_state_t *state = (observer_state_t *)user_data;
    (void)fields;
    (void)field_count;
    state->trace_count++;
    state->last_trace = event;
}

static void on_local_candidate(void *user_data, const char *candidate,
                               size_t candidate_len)
{
    observer_state_t *state = (observer_state_t *)user_data;
    (void)candidate;
    (void)candidate_len;
    state->local_candidate_count++;
}

static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    observer_state_t *state = (observer_state_t *)user_data;
    if (data_len > sizeof(state->last_datagram)) {
        data_len = sizeof(state->last_datagram);
    }
    memcpy(state->last_datagram, data, data_len);
    state->last_datagram_len = data_len;
    state->datagram_count++;
}

static rtc_peer_connection_config_t test_config(unsigned char *arena,
                                                size_t arena_size,
                                                observer_state_t *state)
{
    rtc_peer_connection_config_t config;
    memset(&config, 0, sizeof(config));
    config.arena.data = arena;
    config.arena.size = arena_size;
    config.limits.sdp.max_description_bytes = 2048;
    config.limits.ice.max_candidates = 8;
    config.limits.ice.max_candidate_pairs = 16;
    config.limits.ice.max_transactions = 4;
    config.limits.ice.max_timer_slots = 8;
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
    config.platform = 0;
    config.executors.signaling = test_executor();
    config.executors.media = test_executor();
    config.executors.network = test_executor();
    config.observer.on_state = on_state;
    config.observer.on_error = on_error;
    config.observer.on_trace = on_trace;
    config.observer.on_local_candidate = on_local_candidate;
    config.observer.on_media_frame = 0;
    config.observer.on_datagram = on_datagram;
    config.observer.user_data = state;
    config.security_backend = test_security_backend();
    return config;
}

static void test_write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void test_write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void write_success_response(uint8_t *response, const uint8_t *request)
{
    memset(response, 0, 32);
    test_write_u16(response, RTC_STUN_BINDING_SUCCESS_RESPONSE);
    test_write_u16(response + 2, 12);
    test_write_u32(response + 4, RTC_STUN_MAGIC_COOKIE);
    memcpy(response + 8, request + 8, RTC_STUN_TRANSACTION_ID_BYTES);
    test_write_u16(response + 20, 0x0020);
    test_write_u16(response + 22, 8);
    response[24] = 0;
    response[25] = 0x01;
    test_write_u16(response + 26,
                   (uint16_t)(5000u ^ (RTC_STUN_MAGIC_COOKIE >> 16)));
    test_write_u32(response + 28, 0xc0000201u ^ RTC_STUN_MAGIC_COOKIE);
}

int rtc_test_observability(void)
{
    unsigned char arena[16384];
    observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;
    uint8_t response[32];

    memset(&state, 0, sizeof(state));
    state.last_error = RTC_STATUS_OK;
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_ASSERT(state.trace_count > 0);
    RTC_TEST_ASSERT(strcmp(state.last_trace, RTC_TRACE_PC_CREATE) == 0);

    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_create_answer(pc, 0, 0));
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE, state.last_error);
    RTC_TEST_ASSERT(strcmp(state.last_trace, RTC_TRACE_JSEP_REJECT) == 0);
    RTC_TEST_EQ_INT(0, state.local_candidate_count);

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(0, (int)counters.api.unsupported_api_calls);
    RTC_TEST_EQ_INT(0, (int)counters.ice.remote_candidates);

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host",
                        strlen("candidate:1 1 udp 1 192.0.2.1 5000 typ host")));
    RTC_TEST_ASSERT(strcmp(state.last_trace,
                           RTC_TRACE_ICE_CANDIDATE_REMOTE) == 0);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.ice.remote_candidates);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.gathering_complete") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_trace, RTC_TRACE_ICE_STATE) == 0 ||
                    strcmp(state.last_trace,
                           RTC_TRACE_ICE_CANDIDATE_LOCAL) == 0);

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_ASSERT(state.saw_checking);
    write_success_response(response, state.last_datagram);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_ice_handle_stun_response(pc, response,
                                                 sizeof(response)));
    write_success_response(response, state.last_datagram);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_ice_handle_stun_response(pc, response,
                                                 sizeof(response)));
    RTC_TEST_ASSERT(state.saw_connected);

    {
        uint8_t malformed[3] = {0xff, 0x00, 0x00};
        RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                        rtc_ice_handle_stun_response(pc, malformed,
                                                     sizeof(malformed)));
    }
    RTC_TEST_ASSERT(state.saw_failed);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_peer_connection_create_answer(pc, 0, 0));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.executor.affinity_errors);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    RTC_TEST_ASSERT(strcmp(state.last_trace, RTC_TRACE_PC_DESTROY) == 0);

    return 0;
}
