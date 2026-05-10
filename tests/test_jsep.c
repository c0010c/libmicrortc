#include "executor/executor.h"
#include "jsep/jsep.h"
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
    config.limits.ice.max_candidates = 2;
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
    config.executors.signaling = test_executor();
    config.executors.media = test_executor();
    config.executors.network = test_executor();
    return config;
}

int rtc_test_jsep(void)
{
    unsigned char arena[8192];
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    char offer[2048];
    char answer[2048];
    size_t offer_len;
    size_t answer_len;
    char candidate[128] = "candidate:1 1 udp 1 192.0.2.1 5000 typ host";

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_jsep_can_create_offer(RTC_JSEP_STABLE));
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_jsep_can_create_answer(RTC_JSEP_STABLE));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    config = test_config(arena, sizeof(arena));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    answer_len = sizeof(answer);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_create_answer(pc, answer,
                                                      &answer_len));
    offer_len = sizeof(offer);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create_offer(pc, offer, &offer_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_set_local_description(pc, offer,
                                                              offer_len));
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_set_remote_description(pc, offer,
                                                               offer_len));
    answer_len = sizeof(answer);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_sdp_write_answer(&config.sdp,
                                         RTC_SDP_DIRECTION_SENDRECV,
                                         RTC_SDP_DIRECTION_SENDRECV, answer,
                                         &answer_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_set_remote_description(pc, answer,
                                                               answer_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    config = test_config(arena, sizeof(arena));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_set_remote_description(pc, offer,
                                                               offer_len));
    answer_len = sizeof(answer);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create_answer(pc, answer,
                                                      &answer_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_set_local_description(pc, answer,
                                                              answer_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_add_ice_candidate(pc, candidate,
                                                          strlen(candidate)));
    candidate[10] = '9';
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "a=candidate:2 1 udp 1 192.0.2.2 5000 typ host",
                        strlen("a=candidate:2 1 udp 1 192.0.2.2 5000 typ host")));
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_ICE_CANDIDATES,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "candidate:3 1 udp 1 192.0.2.3 5000 typ host",
                        strlen("candidate:3 1 udp 1 192.0.2.3 5000 typ host")));
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_add_ice_candidate(pc, "bad", 3));
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    config = test_config(arena, sizeof(arena));
    config.limits.sdp.max_description_bytes = 16;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_SDP_BUFFER,
                    rtc_peer_connection_set_remote_description(pc, offer,
                                                               offer_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    return 0;
}
