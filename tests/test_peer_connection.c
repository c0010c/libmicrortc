#include "executor/executor.h"
#include "rtc/rtc.h"
#include "test_runner.h"

#include <string.h>

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
    config.observer.on_state = 0;
    config.observer.on_error = 0;
    config.observer.on_trace = 0;
    config.observer.on_local_candidate = 0;
    config.observer.on_media_frame = 0;
    config.observer.on_datagram = 0;
    config.observer.user_data = 0;
    config.security_backend = 0;
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
        unsigned char pair_arena[6600];
        config = test_config(pair_arena, sizeof(pair_arena));
        config.limits.ice.max_candidates = 1;
        RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_ICE_PAIRS,
                        rtc_peer_connection_create(&config, &diag, &pc));
        RTC_TEST_EQ_INT(RTC_CAPACITY_RESOURCE_ICE_PAIRS, diag.resource);
    }

    {
        unsigned char transaction_arena[6500];
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
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
                    rtc_peer_connection_receive_datagram(pc, 0, 0));
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
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
