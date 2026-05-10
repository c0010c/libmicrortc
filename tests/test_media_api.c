#include "executor/executor.h"
#include "api/peer_connection.h"
#include "observability/counters.h"
#include "rtc/media.h"
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
    config.limits.dtls.max_session_storage_bytes = 0;
    config.limits.rtp.max_packet_cache = 16;
    config.limits.rtp.max_payload_bytes = 1200;
    config.limits.rtp.max_packets_per_frame = 8;
    config.limits.rtp.max_reassembly_bytes = 4096;
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
    config.executors.signaling = test_executor();
    config.executors.media = test_executor();
    config.executors.network = test_executor();
    return config;
}

static int test_media_header_contract(void)
{
    rtc_media_frame_t frame;
    rtc_media_feedback_t feedback;

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_AUDIO_OPUS;
    frame.timestamp = 1234;
    frame.capture_time_us = 5678;
    frame.flags = 1;
    RTC_TEST_EQ_INT(RTC_MEDIA_KIND_AUDIO_OPUS, frame.kind);
    RTC_TEST_EQ_INT(1234, (int)frame.timestamp);
    RTC_TEST_EQ_INT(5678, (int)frame.capture_time_us);
    RTC_TEST_EQ_INT(1, (int)frame.flags);

    memset(&feedback, 0, sizeof(feedback));
    feedback.type = RTC_MEDIA_FEEDBACK_NACK;
    feedback.kind = RTC_MEDIA_KIND_VIDEO_H264;
    feedback.ssrc = 0x11223344u;
    feedback.pid = 12;
    feedback.blp = 3;
    feedback.lost_sequence_numbers[0] = 12;
    feedback.lost_sequence_number_count = 1;
    feedback.retransmit_performed = 0;
    RTC_TEST_EQ_INT(RTC_MEDIA_FEEDBACK_NACK, feedback.type);
    RTC_TEST_EQ_INT(RTC_MEDIA_KIND_VIDEO_H264, feedback.kind);
    RTC_TEST_EQ_INT(1, (int)feedback.lost_sequence_number_count);
    RTC_TEST_EQ_INT(0, feedback.retransmit_performed);
    return 0;
}

static int test_media_send_validation_and_affinity(void)
{
    unsigned char arena[32768];
    uint8_t payload[4] = {1, 2, 3, 4};
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_media_frame_t frame;

    config = test_config(arena, sizeof(arena));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));

    memset(&frame, 0, sizeof(frame));
    frame.kind = RTC_MEDIA_KIND_AUDIO_OPUS;
    frame.data = payload;
    frame.data_len = sizeof(payload);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_peer_connection_send_media_frame(pc, &frame));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    frame.kind = (rtc_media_kind_t)99;
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
                    rtc_peer_connection_send_media_frame(pc, &frame));

    frame.kind = RTC_MEDIA_KIND_AUDIO_OPUS;
    frame.data = 0;
    frame.data_len = sizeof(payload);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_send_media_frame(pc, &frame));

    frame.data = payload;
    frame.data_len = 0;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_send_media_frame(pc, &frame));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_request_keyframe_rejects_opus_kind(void)
{
    unsigned char arena[32768];
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    config = test_config(arena, sizeof(arena));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
                    rtc_peer_connection_request_keyframe(
                        pc, RTC_MEDIA_KIND_AUDIO_OPUS));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_media_limits_counters_trace_and_fixed_slots(void)
{
    unsigned char arena[32768];
    rtc_peer_connection_config_t config;
    rtc_peer_connection_counters_t counters;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    rtc_counters_init(&counters);
    RTC_TEST_EQ_INT(0, (int)counters.rtp.packets_sent);
    RTC_TEST_EQ_INT(0, (int)counters.rtp.packets_received);
    RTC_TEST_EQ_INT(0, (int)counters.rtp.packets_dropped);
    RTC_TEST_EQ_INT(0, (int)counters.rtcp.rtcp_sr_sent);
    RTC_TEST_EQ_INT(0, (int)counters.rtcp.rtcp_rr_received);
    RTC_TEST_EQ_INT(0, (int)counters.rtcp.pli_sent);
    RTC_TEST_EQ_INT(0, (int)counters.rtcp.pli_received);
    RTC_TEST_EQ_INT(0, (int)counters.rtcp.nack_received);
    RTC_TEST_EQ_INT(0, (int)counters.rtcp.nack_no_retransmit);
    RTC_TEST_EQ_INT(0, (int)counters.media.frames_sent);
    RTC_TEST_EQ_INT(0, (int)counters.media.frames_received);
    RTC_TEST_EQ_INT(0, (int)counters.media.h264_reassembly_drops);
    RTC_TEST_EQ_INT(0, (int)counters.media.media_queue_full);

    RTC_TEST_ASSERT(RTC_TRACE_MEDIA_FRAME[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_RTP_PACKET[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_RTCP_PACKET[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_MEDIA_FEEDBACK[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_FIELD_MEDIA_KIND[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_FIELD_SSRC[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_FIELD_SEQUENCE[0] != '\0');
    RTC_TEST_ASSERT(RTC_TRACE_FIELD_TIMESTAMP[0] != '\0');

    config = test_config(arena, sizeof(arena));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(4, (int)pc->media_queue_slot_count);
    RTC_TEST_ASSERT(pc->media_queue_slots != 0);
    RTC_TEST_EQ_INT(1200, (int)pc->media_max_payload_bytes);
    RTC_TEST_EQ_INT(8, (int)pc->media_max_packets_per_frame);
    RTC_TEST_EQ_INT(4096, (int)pc->media_max_reassembly_bytes);
    RTC_TEST_ASSERT(pc->h264_reassembly_buffer != 0);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    config = test_config(arena, sizeof(arena));
    config.limits.rtp.max_media_queue_slots = 0;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(&config, &diag, &pc));

    config = test_config(arena, sizeof(arena));
    config.limits.rtp.max_payload_bytes = 0;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(&config, &diag, &pc));

    config = test_config(arena, sizeof(arena));
    config.limits.rtcp.max_feedback_packets = 0;
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_peer_connection_create(&config, &diag, &pc));
    return 0;
}

int rtc_test_media_api(void)
{
    RTC_TEST_EQ_INT(0, test_media_header_contract());
    RTC_TEST_EQ_INT(0, test_media_send_validation_and_affinity());
    RTC_TEST_EQ_INT(0, test_request_keyframe_rejects_opus_kind());
    RTC_TEST_EQ_INT(0, test_media_limits_counters_trace_and_fixed_slots());
    return 0;
}
