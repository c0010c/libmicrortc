#include "test_runner.h"

#include "api/peer_connection.h"
#include "executor/executor.h"
#include "rtc/media.h"
#include "rtc/rtc.h"
#include "rtc/security.h"

#include <string.h>

typedef struct rtp_test_state_t {
    int create_session_calls;
    int destroy_session_calls;
    int srtp_protect_rtp_calls;
    int datagram_count;
    rtc_status_t srtp_protect_rtp_status;
    uint8_t datagrams[8][256];
    size_t datagram_lens[8];
} rtp_test_state_t;

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
        *out_timer_id = 7;
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

static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    rtp_test_state_t *state = (rtp_test_state_t *)user_data;
    int index = state->datagram_count;

    if (index >= 8) {
        index = 7;
    }
    state->datagram_count++;
    if (data_len > sizeof(state->datagrams[index])) {
        data_len = sizeof(state->datagrams[index]);
    }
    memcpy(state->datagrams[index], data, data_len);
    state->datagram_lens[index] = data_len;
}

static rtc_status_t test_create_session(
    void *backend_user_data, void *storage, size_t storage_len,
    rtc_security_backend_event_cb event_cb, void *event_user_data,
    void **out_session)
{
    rtp_test_state_t *state = (rtp_test_state_t *)backend_user_data;

    (void)storage;
    (void)storage_len;
    (void)event_cb;
    (void)event_user_data;
    state->create_session_calls++;
    *out_session = state;
    return RTC_STATUS_OK;
}

static void test_destroy_session(void *session)
{
    rtp_test_state_t *state = (rtp_test_state_t *)session;
    state->destroy_session_calls++;
}

static rtc_status_t test_get_local_fingerprint(void *session, char *out,
                                               size_t *inout_len)
{
    static const char fingerprint[] =
        "sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    size_t len = sizeof(fingerprint) - 1u;

    (void)session;
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
    (void)session;
    (void)role;
    return RTC_STATUS_OK;
}

static rtc_status_t test_handle_dtls_datagram(void *session,
                                              const uint8_t *packet,
                                              size_t packet_len)
{
    (void)session;
    (void)packet;
    (void)packet_len;
    return RTC_STATUS_OK;
}

static rtc_status_t test_srtp_protect_rtp(void *session, uint8_t *packet,
                                          size_t *inout_len, size_t capacity)
{
    rtp_test_state_t *state = (rtp_test_state_t *)session;

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

static rtc_security_backend_vtable_t test_backend_vtable(void)
{
    rtc_security_backend_vtable_t vtable;
    memset(&vtable, 0, sizeof(vtable));
    vtable.create_session = test_create_session;
    vtable.destroy_session = test_destroy_session;
    vtable.get_local_fingerprint = test_get_local_fingerprint;
    vtable.start_dtls = test_start_dtls;
    vtable.handle_dtls_datagram = test_handle_dtls_datagram;
    vtable.srtp_protect_rtp = test_srtp_protect_rtp;
    return vtable;
}

static rtc_peer_connection_config_t test_config(
    unsigned char *arena, size_t arena_size, rtp_test_state_t *state,
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
    config.limits.rtp.max_payload_bytes = 128;
    config.limits.rtp.max_packets_per_frame = 4;
    config.limits.rtp.max_reassembly_bytes = 512;
    config.limits.rtp.max_media_queue_slots = 4;
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
    config.executors.signaling = test_executor(state);
    config.executors.media = test_executor(state);
    config.executors.network = test_executor(state);
    config.observer.on_datagram = on_datagram;
    config.observer.user_data = state;
    config.security_backend = backend;
    return config;
}

static int test_opus_send_outputs_protected_rtp_datagram(void)
{
    unsigned char arena[32768];
    uint8_t opus[] = {0x11, 0x22, 0x33};
    rtp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_media_frame_t frame;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_AUDIO_OPUS;
    frame.data = opus;
    frame.data_len = sizeof(opus);
    frame.timestamp = 48000;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_send_media_frame(pc, &frame));
    RTC_TEST_EQ_INT(1, state.srtp_protect_rtp_calls);
    RTC_TEST_EQ_INT(1, state.datagram_count);
    RTC_TEST_EQ_INT(12 + (int)sizeof(opus) + 4,
                    (int)state.datagram_lens[0]);
    RTC_TEST_EQ_INT(0x80, state.datagrams[0][0]);
    RTC_TEST_EQ_INT(111, state.datagrams[0][1] & 0x7F);
    RTC_TEST_EQ_INT(0x00, state.datagrams[0][2]);
    RTC_TEST_EQ_INT(0x01, state.datagrams[0][3]);
    RTC_TEST_EQ_INT(0x00, state.datagrams[0][4]);
    RTC_TEST_EQ_INT(0x00, state.datagrams[0][5]);
    RTC_TEST_EQ_INT(0xBB, state.datagrams[0][6]);
    RTC_TEST_EQ_INT(0x80, state.datagrams[0][7]);
    RTC_TEST_ASSERT(memcmp(state.datagrams[0] + 12, opus, sizeof(opus)) ==
                    0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_opus_protect_failure_does_not_output_plaintext(void)
{
    unsigned char arena[32768];
    uint8_t opus[] = {0x44, 0x55};
    rtp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_media_frame_t frame;

    memset(&state, 0, sizeof(state));
    state.srtp_protect_rtp_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_AUDIO_OPUS;
    frame.data = opus;
    frame.data_len = sizeof(opus);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_BACKEND_ERROR,
                    rtc_peer_connection_send_media_frame(pc, &frame));
    RTC_TEST_EQ_INT(1, state.srtp_protect_rtp_calls);
    RTC_TEST_EQ_INT(0, state.datagram_count);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_h264_single_nalu_outputs_marker_packet(void)
{
    unsigned char arena[32768];
    uint8_t au[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x99};
    uint8_t expected_nalu[] = {0x65, 0x88, 0x99};
    rtp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_media_frame_t frame;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_VIDEO_H264;
    frame.data = au;
    frame.data_len = sizeof(au);
    frame.timestamp = 90000;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_send_media_frame(pc, &frame));
    RTC_TEST_EQ_INT(1, state.srtp_protect_rtp_calls);
    RTC_TEST_EQ_INT(1, state.datagram_count);
    RTC_TEST_EQ_INT(103, state.datagrams[0][1] & 0x7F);
    RTC_TEST_ASSERT((state.datagrams[0][1] & 0x80u) != 0); /* marker */
    RTC_TEST_ASSERT(memcmp(state.datagrams[0] + 12, expected_nalu,
                           sizeof(expected_nalu)) == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_h264_large_nalu_uses_fu_a_and_marker_on_last_packet(void)
{
    unsigned char arena[32768];
    uint8_t au[] = {0x00, 0x00, 0x01, 0x65, 0x01, 0x02, 0x03,
                    0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    rtp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_media_frame_t frame;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    config.limits.rtp.max_payload_bytes = 6;
    config.limits.rtp.max_packets_per_frame = 3;
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_VIDEO_H264;
    frame.data = au;
    frame.data_len = sizeof(au);
    frame.timestamp = 180000;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_send_media_frame(pc, &frame));
    RTC_TEST_EQ_INT(3, state.srtp_protect_rtp_calls);
    RTC_TEST_EQ_INT(3, state.datagram_count);
    RTC_TEST_EQ_INT(0, state.datagrams[0][1] & 0x80); /* marker */
    RTC_TEST_EQ_INT(0, state.datagrams[1][1] & 0x80); /* marker */
    RTC_TEST_ASSERT((state.datagrams[2][1] & 0x80u) != 0); /* marker */
    RTC_TEST_EQ_INT(0x7c, state.datagrams[0][12]); /* FU-A indicator */
    RTC_TEST_EQ_INT(0x85, state.datagrams[0][13]);
    RTC_TEST_EQ_INT(0x05, state.datagrams[1][13]);
    RTC_TEST_EQ_INT(0x45, state.datagrams[2][13]);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_h264_max_packets_per_frame_capacity(void)
{
    unsigned char arena[32768];
    uint8_t au[] = {0x00, 0x00, 0x01, 0x65, 0x01, 0x02, 0x03,
                    0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    rtp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_media_frame_t frame;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    config.limits.rtp.max_payload_bytes = 6;
    config.limits.rtp.max_packets_per_frame = 2;
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_VIDEO_H264;
    frame.data = au;
    frame.data_len = sizeof(au);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_PACKET_CACHE,
                    rtc_peer_connection_send_media_frame(pc, &frame));
    RTC_TEST_EQ_INT(0, state.srtp_protect_rtp_calls);
    RTC_TEST_EQ_INT(0, state.datagram_count);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_h264_protect_failure_does_not_output_plaintext(void)
{
    unsigned char arena[32768];
    uint8_t au[] = {0x00, 0x00, 0x01, 0x65, 0x77};
    rtp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_media_frame_t frame;

    memset(&state, 0, sizeof(state));
    state.srtp_protect_rtp_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_VIDEO_H264;
    frame.data = au;
    frame.data_len = sizeof(au);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_BACKEND_ERROR,
                    rtc_peer_connection_send_media_frame(pc, &frame));
    RTC_TEST_EQ_INT(1, state.srtp_protect_rtp_calls);
    RTC_TEST_EQ_INT(0, state.datagram_count);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

int rtc_test_rtp(void)
{
    RTC_TEST_EQ_INT(0, test_opus_send_outputs_protected_rtp_datagram());
    RTC_TEST_EQ_INT(0, test_opus_protect_failure_does_not_output_plaintext());
    RTC_TEST_EQ_INT(0, test_h264_single_nalu_outputs_marker_packet());
    RTC_TEST_EQ_INT(0,
                    test_h264_large_nalu_uses_fu_a_and_marker_on_last_packet());
    RTC_TEST_EQ_INT(0, test_h264_max_packets_per_frame_capacity());
    RTC_TEST_EQ_INT(0, test_h264_protect_failure_does_not_output_plaintext());
    return 0;
}
