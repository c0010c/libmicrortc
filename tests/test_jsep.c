#include "executor/executor.h"
#include "jsep/jsep.h"
#include "rtc/rtc.h"
#include "rtc/security.h"
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
        "sha-256 FE:DC:BA:98:76:54:32:10:FE:DC:BA:98:76:54:32:10:"
        "FE:DC:BA:98:76:54:32:10:FE:DC:BA:98:76:54:32:10";
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

static rtc_peer_connection_config_t test_config(unsigned char *arena,
                                                size_t arena_size)
{
    rtc_peer_connection_config_t config;
    memset(&config, 0, sizeof(config));
    config.arena.data = arena;
    config.arena.size = arena_size;
    config.limits.sdp.max_description_bytes = 2048;
    config.limits.ice.max_candidates = 2;
    config.limits.ice.max_candidate_pairs = 4;
    config.limits.ice.max_transactions = 2;
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
    config.executors.signaling = test_executor();
    config.executors.media = test_executor();
    config.executors.network = test_executor();
    config.security_backend = test_security_backend();
    return config;
}

int rtc_test_jsep(void)
{
    unsigned char arena[16384];
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
                        pc, "a=candidate:2 1 udp 2 192.0.2.2 5001 typ srflx",
                        strlen("a=candidate:2 1 udp 2 192.0.2.2 5001 typ srflx")));
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_ICE_CANDIDATES,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "candidate:3 1 udp 1 192.0.2.3 5000 typ host",
                        strlen("candidate:3 1 udp 1 192.0.2.3 5000 typ host")));
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "candidate:4 1 tcp 1 192.0.2.4 5000 typ host",
                        strlen("candidate:4 1 tcp 1 192.0.2.4 5000 typ host")));
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "candidate:5 2 udp 1 192.0.2.5 5000 typ host",
                        strlen("candidate:5 2 udp 1 192.0.2.5 5000 typ host")));
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "candidate:6 1 udp 1 192.0.2.6 70000 typ host",
                        strlen("candidate:6 1 udp 1 192.0.2.6 70000 typ host")));
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
                    rtc_peer_connection_add_ice_candidate(
                        pc, "candidate:7 1 udp 1 192.0.2.7 5000 typ relay",
                        strlen("candidate:7 1 udp 1 192.0.2.7 5000 typ relay")));
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
