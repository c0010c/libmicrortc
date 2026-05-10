#include "test_runner.h"

#include "api/peer_connection.h"
#include "executor/executor.h"
#include "rtc/media.h"
#include "rtc/rtc.h"
#include "rtc/security.h"
#include "media/media.h"
#include "rtcp/rtcp.h"

#include <string.h>

typedef struct rtcp_test_state_t {
    int create_session_calls;
    int destroy_session_calls;
    int srtcp_protect_calls;
    int srtcp_unprotect_calls;
    int datagram_count;
    int feedback_count;
    const char *last_trace_reason;
    rtc_status_t srtcp_protect_status;
    rtc_status_t srtcp_unprotect_status;
    rtc_media_feedback_t last_feedback;
    uint8_t datagrams[4][256];
    size_t datagram_lens[4];
} rtcp_test_state_t;

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
        *out_timer_id = 9;
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
    rtcp_test_state_t *state = (rtcp_test_state_t *)user_data;
    int index = state->datagram_count;

    if (index >= 4) {
        index = 3;
    }
    state->datagram_count++;
    if (data_len > sizeof(state->datagrams[index])) {
        data_len = sizeof(state->datagrams[index]);
    }
    memcpy(state->datagrams[index], data, data_len);
    state->datagram_lens[index] = data_len;
}

static void on_media_feedback(void *user_data,
                              const rtc_media_feedback_t *feedback)
{
    rtcp_test_state_t *state = (rtcp_test_state_t *)user_data;

    state->feedback_count++;
    if (feedback != 0) {
        state->last_feedback = *feedback;
    }
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    rtcp_test_state_t *state = (rtcp_test_state_t *)user_data;
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
    rtcp_test_state_t *state = (rtcp_test_state_t *)backend_user_data;

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
    rtcp_test_state_t *state = (rtcp_test_state_t *)session;
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

static rtc_status_t test_srtcp_protect(void *session, uint8_t *packet,
                                       size_t *inout_len, size_t capacity)
{
    rtcp_test_state_t *state = (rtcp_test_state_t *)session;

    state->srtcp_protect_calls++;
    if (state->srtcp_protect_status != RTC_STATUS_OK) {
        return state->srtcp_protect_status;
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

static rtc_status_t test_srtcp_unprotect(void *session, uint8_t *packet,
                                         size_t *inout_len)
{
    rtcp_test_state_t *state = (rtcp_test_state_t *)session;

    state->srtcp_unprotect_calls++;
    if (state->srtcp_unprotect_status != RTC_STATUS_OK) {
        return state->srtcp_unprotect_status;
    }
    if (packet == 0 || inout_len == 0 || *inout_len < 8u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *inout_len -= 4u;
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
    vtable.srtcp_protect = test_srtcp_protect;
    vtable.srtcp_unprotect = test_srtcp_unprotect;
    return vtable;
}

static rtc_peer_connection_config_t test_config(
    unsigned char *arena, size_t arena_size, rtcp_test_state_t *state,
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
    config.observer.on_media_feedback = on_media_feedback;
    config.observer.on_trace = on_trace;
    config.observer.user_data = state;
    config.security_backend = backend;
    return config;
}

static int test_rtcp_sr_rr_sdes_codec_round_trip_updates_counters(void)
{
    unsigned char arena[32768];
    uint8_t packet[192];
    size_t offset = 0;
    size_t written = 0;
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));

    pc->rtcp_audio.ssrc = 0x11111111u;
    pc->rtcp_audio.packets_sent = 3;
    pc->rtcp_audio.octets_sent = 33;
    pc->rtcp_audio.packets_received = 2;
    pc->rtcp_audio.last_sequence = 0x1234;
    pc->rtcp_audio.jitter = 5;
    pc->rtcp_audio.last_sr_ntp = 0x0102030405060708ULL;
    pc->rtcp_audio.last_sr_rtp = 0x11223344u;

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_rtcp_write_sender_report(&pc->rtcp_audio, packet,
                                                 sizeof(packet), &written));
    offset += written;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_rtcp_write_receiver_report(&pc->rtcp_audio,
                                                   packet + offset,
                                                   sizeof(packet) - offset,
                                                   &written));
    offset += written;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_rtcp_write_sdes(pc->rtcp_audio.ssrc, "rtc-cname", 9,
                                        pc->limits.rtcp.max_sdes_cname_bytes,
                                        packet + offset,
                                        sizeof(packet) - offset, &written));
    offset += written;

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_rtcp_parse_compound(pc, packet, offset));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.rtcp_sr_received);
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.rtcp_rr_received);
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.rtcp_sdes_received);
    RTC_TEST_EQ_INT(0x0102030405060708ULL,
                    pc->rtcp_audio.last_sr_ntp);
    RTC_TEST_EQ_INT(0x11223344u, pc->rtcp_audio.last_sr_rtp);

    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_rtcp_rejects_sdes_cname_over_limit_and_unknown_type(void)
{
    uint8_t packet[64];
    size_t written = 0;
    rtc_rtcp_media_stats_t stats;
    uint8_t unknown[] = {0x80, 199, 0, 1, 0, 0, 0, 1};

    memset(&stats, 0, sizeof(stats));
    stats.ssrc = 0x22222222u;

    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY,
                    rtc_rtcp_write_sdes(stats.ssrc, "too-long-name", 13, 4,
                                        packet, sizeof(packet), &written));
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
                    rtc_rtcp_parse_compound(0, unknown, sizeof(unknown)));
    return 0;
}

static void write_rr(uint8_t *packet, uint32_t ssrc)
{
    packet[0] = 0x80;
    packet[1] = 201;
    packet[2] = 0;
    packet[3] = 1;
    packet[4] = (uint8_t)(ssrc >> 24);
    packet[5] = (uint8_t)((ssrc >> 16) & 0xffu);
    packet[6] = (uint8_t)((ssrc >> 8) & 0xffu);
    packet[7] = (uint8_t)(ssrc & 0xffu);
    packet[8] = 0xAA;
    packet[9] = 0xBB;
    packet[10] = 0xCC;
    packet[11] = 0xDD;
}

static int test_rtcp_receive_rr_uses_srtcp_unprotect_before_parse(void)
{
    unsigned char arena[32768];
    uint8_t packet[12];
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;
    write_rr(packet, pc->rtcp_audio.ssrc);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, packet,
                                                         sizeof(packet)));
    RTC_TEST_EQ_INT(1, state.srtcp_unprotect_calls);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.rtcp_rr_received);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_rtcp_unprotect_failure_does_not_parse_rr(void)
{
    unsigned char arena[32768];
    uint8_t packet[12];
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    state.srtcp_unprotect_status = RTC_STATUS_PROTOCOL_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;
    write_rr(packet, pc->rtcp_audio.ssrc);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_receive_datagram(pc, packet,
                                                         sizeof(packet)));
    RTC_TEST_EQ_INT(1, state.srtcp_unprotect_calls);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(0, (int)counters.rtcp.rtcp_rr_received);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_rtcp_send_reports_protects_and_outputs_datagram(void)
{
    unsigned char arena[32768];
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;
    pc->rtcp_audio.packets_sent = 2;
    pc->rtcp_audio.octets_sent = 20;
    pc->rtcp_audio.last_sr_ntp = 0x0102030405060708ULL;
    pc->rtcp_audio.last_sr_rtp = 0x11223344u;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_media_send_rtcp_reports(pc));
    RTC_TEST_EQ_INT(1, state.srtcp_protect_calls);
    RTC_TEST_EQ_INT(1, state.datagram_count);
    RTC_TEST_EQ_INT(0x80, state.datagrams[0][0]);
    RTC_TEST_EQ_INT(200, state.datagrams[0][1]);
    RTC_TEST_EQ_INT(0xAA, state.datagrams[0][state.datagram_lens[0] - 4]);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.rtcp_sr_sent);
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.rtcp_sdes_sent);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_rtcp_protect_failure_does_not_output_datagram(void)
{
    unsigned char arena[32768];
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    state.srtcp_protect_status = RTC_STATUS_BACKEND_ERROR;
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_BACKEND_ERROR,
                    rtc_media_send_rtcp_reports(pc));
    RTC_TEST_EQ_INT(1, state.srtcp_protect_calls);
    RTC_TEST_EQ_INT(0, state.datagram_count);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_rtcp_request_keyframe_sends_protected_pli_datagram(void)
{
    unsigned char arena[32768];
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_request_keyframe(
                        pc, RTC_MEDIA_KIND_VIDEO_H264));
    RTC_TEST_EQ_INT(1, state.srtcp_protect_calls);
    RTC_TEST_EQ_INT(1, state.datagram_count);
    RTC_TEST_EQ_INT(0x81, state.datagrams[0][0]);
    RTC_TEST_EQ_INT(206, state.datagrams[0][1]);
    RTC_TEST_EQ_INT(0xAA, state.datagrams[0][state.datagram_lens[0] - 4]);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.pli_sent);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_rtcp_receive_pli_reports_media_feedback(void)
{
    unsigned char arena[32768];
    uint8_t packet[16];
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;
    size_t packet_len = 0;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_rtcp_write_pli(0x11111111u, pc->rtcp_video.ssrc,
                                       packet, sizeof(packet), &packet_len));
    packet[packet_len++] = 0xAA;
    packet[packet_len++] = 0xBB;
    packet[packet_len++] = 0xCC;
    packet[packet_len++] = 0xDD;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, packet,
                                                         packet_len));
    RTC_TEST_EQ_INT(1, state.srtcp_unprotect_calls);
    RTC_TEST_EQ_INT(1, state.feedback_count);
    RTC_TEST_EQ_INT(RTC_MEDIA_FEEDBACK_PLI, state.last_feedback.type);
    RTC_TEST_EQ_INT(RTC_MEDIA_KIND_VIDEO_H264, state.last_feedback.kind);
    RTC_TEST_EQ_INT(0, state.last_feedback.retransmit_performed);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.pli_received);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_rtcp_parse_nack_expands_pid_only(void)
{
    rtc_media_feedback_t feedback;
    uint8_t fci[] = {0x12, 0x34, 0x00, 0x00};

    memset(&feedback, 0, sizeof(feedback));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_rtcp_parse_nack(fci, sizeof(fci), &feedback));
    RTC_TEST_EQ_INT(RTC_MEDIA_FEEDBACK_NACK, feedback.type);
    RTC_TEST_EQ_INT(0x1234, feedback.pid);
    RTC_TEST_EQ_INT(0, feedback.blp);
    RTC_TEST_EQ_INT(1, (int)feedback.lost_sequence_number_count);
    RTC_TEST_EQ_INT(0x1234,
                    (int)feedback.lost_sequence_numbers[0]);
    RTC_TEST_EQ_INT(0, feedback.retransmit_performed);
    return 0;
}

static int test_rtcp_parse_nack_expands_pid_and_blp(void)
{
    rtc_media_feedback_t feedback;
    uint8_t fci[] = {0x01, 0x2C, 0x00, 0x05};

    memset(&feedback, 0, sizeof(feedback));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_rtcp_parse_nack(fci, sizeof(fci), &feedback));
    RTC_TEST_EQ_INT(0x012C, feedback.pid);
    RTC_TEST_EQ_INT(0x0005, feedback.blp);
    RTC_TEST_EQ_INT(3, (int)feedback.lost_sequence_number_count);
    RTC_TEST_EQ_INT(300, (int)feedback.lost_sequence_numbers[0]);
    RTC_TEST_EQ_INT(301, (int)feedback.lost_sequence_numbers[1]);
    RTC_TEST_EQ_INT(303, (int)feedback.lost_sequence_numbers[2]);
    RTC_TEST_EQ_INT(0, feedback.retransmit_performed);
    return 0;
}

static void write_nack(uint8_t *packet, uint32_t sender_ssrc,
                       uint32_t media_ssrc, uint16_t pid, uint16_t blp)
{
    packet[0] = 0x81;
    packet[1] = 205;
    packet[2] = 0;
    packet[3] = 3;
    packet[4] = (uint8_t)(sender_ssrc >> 24);
    packet[5] = (uint8_t)((sender_ssrc >> 16) & 0xffu);
    packet[6] = (uint8_t)((sender_ssrc >> 8) & 0xffu);
    packet[7] = (uint8_t)(sender_ssrc & 0xffu);
    packet[8] = (uint8_t)(media_ssrc >> 24);
    packet[9] = (uint8_t)((media_ssrc >> 16) & 0xffu);
    packet[10] = (uint8_t)((media_ssrc >> 8) & 0xffu);
    packet[11] = (uint8_t)(media_ssrc & 0xffu);
    packet[12] = (uint8_t)(pid >> 8);
    packet[13] = (uint8_t)(pid & 0xffu);
    packet[14] = (uint8_t)(blp >> 8);
    packet[15] = (uint8_t)(blp & 0xffu);
    packet[16] = 0xAA;
    packet[17] = 0xBB;
    packet[18] = 0xCC;
    packet[19] = 0xDD;
}

static int test_rtcp_receive_nack_reports_no_retransmit_feedback(void)
{
    unsigned char arena[32768];
    uint8_t packet[20];
    rtcp_test_state_t state;
    rtc_security_backend_config_t backend;
    rtc_security_backend_vtable_t vtable;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state, &backend, &vtable);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->srtp_ready = 1;

    write_nack(packet, 0x11111111u, pc->rtcp_video.ssrc, 300, 0x0005);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, packet,
                                                         sizeof(packet)));
    RTC_TEST_EQ_INT(1, state.srtcp_unprotect_calls);
    RTC_TEST_EQ_INT(1, state.feedback_count);
    RTC_TEST_EQ_INT(RTC_MEDIA_FEEDBACK_NACK, state.last_feedback.type);
    RTC_TEST_EQ_INT(RTC_MEDIA_KIND_VIDEO_H264, state.last_feedback.kind);
    RTC_TEST_EQ_INT(3, (int)state.last_feedback.lost_sequence_number_count);
    RTC_TEST_EQ_INT(300, (int)state.last_feedback.lost_sequence_numbers[0]);
    RTC_TEST_EQ_INT(301, (int)state.last_feedback.lost_sequence_numbers[1]);
    RTC_TEST_EQ_INT(303, (int)state.last_feedback.lost_sequence_numbers[2]);
    RTC_TEST_EQ_INT(0, state.last_feedback.retransmit_performed);
    RTC_TEST_ASSERT(state.last_trace_reason != 0);
    RTC_TEST_ASSERT(strcmp(state.last_trace_reason, "nack_no_retransmit") ==
                    0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.nack_received);
    RTC_TEST_EQ_INT(1, (int)counters.rtcp.nack_no_retransmit);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

int rtc_test_rtcp(void)
{
    int status;

    status = test_rtcp_sr_rr_sdes_codec_round_trip_updates_counters();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_rejects_sdes_cname_over_limit_and_unknown_type();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_receive_rr_uses_srtcp_unprotect_before_parse();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_unprotect_failure_does_not_parse_rr();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_send_reports_protects_and_outputs_datagram();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_protect_failure_does_not_output_datagram();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_request_keyframe_sends_protected_pli_datagram();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_receive_pli_reports_media_feedback();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_parse_nack_expands_pid_only();
    if (status != 0) {
        return status;
    }
    status = test_rtcp_parse_nack_expands_pid_and_blp();
    if (status != 0) {
        return status;
    }
    return test_rtcp_receive_nack_reports_no_retransmit_feedback();
}
