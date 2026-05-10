#include "executor/executor.h"
#include "rtc/rtc.h"
#include "rtc/security.h"
#include "test_runner.h"

#include <string.h>

typedef struct test_security_backend_state_t {
    int create_session_calls;
} test_security_backend_state_t;

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
    test_security_backend_state_t *state =
        (test_security_backend_state_t *)backend_user_data;
    (void)storage;
    (void)storage_len;
    (void)event_cb;
    (void)event_user_data;
    state->create_session_calls++;
    *out_session = state;
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
        "sha-256 12:34:56:78:90:AB:CD:EF:12:34:56:78:90:AB:CD:EF:"
        "12:34:56:78:90:AB:CD:EF:12:34:56:78:90:AB:CD:EF";
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
    static test_security_backend_state_t state;
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

static rtc_peer_connection_config_t test_config(unsigned char *arena,
                                                size_t arena_size)
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
    config.limits.rtp.max_payload_bytes = 256;
    config.limits.rtp.max_packets_per_frame = 4;
    config.limits.rtp.max_reassembly_bytes = 512;
    config.limits.rtp.max_media_queue_slots = 2;
    config.limits.rtcp.max_reports = 4;
    config.limits.rtcp.max_feedback_packets = 4;
    config.limits.rtcp.max_sdes_cname_bytes = 64;
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
    config.observer.on_state = 0;
    config.observer.on_error = 0;
    config.observer.on_trace = 0;
    config.observer.on_local_candidate = 0;
    config.observer.on_media_frame = 0;
    config.observer.on_datagram = 0;
    config.observer.user_data = 0;
    config.security_backend = test_security_backend();
    return config;
}

int rtc_test_peer_connection(void)
{
    unsigned char arena[16384];
    unsigned char small_arena[1];
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(0, &diag, &pc));

    config = test_config(arena, sizeof(arena));
    config.stun_server_count = 1;
    config.stun_server.ip = "192.0.2.1";
    config.stun_server.ip_len = strlen(config.stun_server.ip);
    config.stun_server.port = 3478;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    config = test_config(arena, sizeof(arena));
    config.stun_server_count = 2;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(&config, &diag, &pc));

    config = test_config(arena, sizeof(arena));
    config.stun_server_count = 1;
    config.stun_server.ip = "stun.example.test";
    config.stun_server.ip_len = strlen(config.stun_server.ip);
    config.stun_server.port = 3478;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(&config, &diag, &pc));

    config = test_config(arena, sizeof(arena));
    config.stun_server_count = 1;
    config.stun_server.ip = "2001:db8::1";
    config.stun_server.ip_len = strlen(config.stun_server.ip);
    config.stun_server.port = 0;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(&config, &diag, &pc));

    config = test_config(small_arena, sizeof(small_arena));
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_ARENA,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_CAPACITY_RESOURCE_ARENA, diag.resource);

    {
        unsigned char pair_arena[7200];
        config = test_config(pair_arena, sizeof(pair_arena));
        config.limits.ice.max_candidates = 1;
        RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_ICE_PAIRS,
                        rtc_peer_connection_create(&config, &diag, &pc));
        RTC_TEST_EQ_INT(RTC_CAPACITY_RESOURCE_ICE_PAIRS, diag.resource);
    }

    {
        unsigned char transaction_arena[7000];
        config = test_config(transaction_arena, sizeof(transaction_arena));
        config.limits.ice.max_candidates = 1;
        config.limits.ice.max_candidate_pairs = 1;
        RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_STUN_TRANSACTIONS,
                        rtc_peer_connection_create(&config, &diag, &pc));
        RTC_TEST_EQ_INT(RTC_CAPACITY_RESOURCE_STUN_TRANSACTIONS,
                        diag.resource);
    }

    config = test_config(arena, sizeof(arena));
    config.local_host_port = 0;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(&config, &diag, &pc));

    config = test_config(arena, sizeof(arena));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_ASSERT(pc != 0);
    {
        char offer[2048];
        size_t offer_len = sizeof(offer);
        RTC_TEST_EQ_INT(RTC_STATUS_OK,
                        rtc_peer_connection_create_offer(pc, offer,
                                                         &offer_len));
    }
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    config = test_config(arena, sizeof(arena));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_peer_connection_create(&config, &diag, &pc));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_receive_datagram(pc, 0, 0));
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_start_connectivity_checks(pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_peer_connection_start_connectivity_checks(pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    return 0;
}
