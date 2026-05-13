#include "../../src/media/media_transceiver.h"
#include "../../src/rtcp/rtcp_packet.h"

#include <stdio.h>
#include <string.h>

#define CHECK_TRUE(expr) do { if (!(expr)) { return 1; } } while (0)

typedef struct SendCapture {
    size_t packet_count;
    int passthrough;
} SendCapture;

typedef struct PictureLossCapture {
    size_t video_count;
    size_t audio_count;
} PictureLossCapture;

static int read_fixture(char *buffer, size_t buffer_len)
{
    FILE *file = fopen("tests/fixtures/minimal_offer.sdp", "rb");
    size_t read_len;

    if (file == 0) {
        return 0;
    }
    read_len = fread(buffer, 1, buffer_len - 1u, file);
    fclose(file);
    buffer[read_len] = '\0';
    return read_len > 0u;
}

static MRTC_STATUS capture_send(void *user_data,
                                MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                const uint8_t *packet,
                                size_t packet_size)
{
    SendCapture *capture = (SendCapture *) user_data;
    (void) peer_connection;
    (void) transceiver;

    if (capture == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    capture->packet_count++;
    return MRTC_STATUS_OK;
}

static void on_video_picture_loss(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    PictureLossCapture *capture = (PictureLossCapture *) user_data;
    if (capture != 0 && transceiver != 0) {
        capture->video_count++;
    }
}

static void on_audio_picture_loss(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    PictureLossCapture *capture = (PictureLossCapture *) user_data;
    if (capture != 0 && transceiver != 0) {
        capture->audio_count++;
    }
}

static int connect_peer(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    char offer[2048];
    char answer[4096];
    size_t required_len = 0;

    if (!read_fixture(offer, sizeof(offer))) {
        return 0;
    }
    return mrtc_peer_connection_set_remote_description(peer_connection, "offer", offer) == MRTC_STATUS_OK &&
           mrtc_peer_connection_create_answer(peer_connection, answer, sizeof(answer), &required_len) == MRTC_STATUS_OK &&
           mrtc_peer_connection_set_local_description(peer_connection, "answer", answer) == MRTC_STATUS_OK &&
           mrtc_peer_connection_add_ice_candidate(peer_connection, "candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host") == MRTC_STATUS_OK;
}

int main(void)
{
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_PEER_CONNECTION_HANDLE peer_connection = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE video = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE audio = 0;
    MRTC_TRANSCEIVER_INIT video_init = {0};
    MRTC_TRANSCEIVER_INIT audio_init = {0};
    MRTC_FRAME frame = {0};
    SendCapture send_capture = {0};
    PictureLossCapture pli_capture = {0};
    MRTC_TRANSCEIVER_CALLBACKS video_callbacks = {0};
    MRTC_TRANSCEIVER_CALLBACKS audio_callbacks = {0};
    const uint8_t small_h264[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84};
    uint8_t rtcp[64];
    size_t rtcp_size = sizeof(rtcp);
    MRTC_RTCP_HEADER header;
    MRTC_RTCP_SENDER_REPORT sr;
    MRTC_RTCP_RECEIVER_REPORT rr;

    video_init.kind = MRTC_MEDIA_KIND_VIDEO;
    video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
    video_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    audio_init.kind = MRTC_MEDIA_KIND_AUDIO;
    audio_init.codec = MRTC_CODEC_OPUS;
    audio_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    CHECK_TRUE(mrtc_peer_connection_create(&config, 0, 0, &peer_connection) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_add_transceiver(peer_connection, &video_init, 0, &video) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_add_transceiver(peer_connection, &audio_init, 0, &audio) == MRTC_STATUS_OK);
    CHECK_TRUE(connect_peer(peer_connection));
    send_capture.passthrough = mrtc_peer_connection_media_is_srtp_passthrough(peer_connection);
    CHECK_TRUE(mrtc_peer_connection_set_media_send_hook(peer_connection, capture_send, &send_capture) == MRTC_STATUS_OK);

    video_callbacks.on_picture_loss = on_video_picture_loss;
    audio_callbacks.on_picture_loss = on_audio_picture_loss;
    CHECK_TRUE(mrtc_transceiver_set_callbacks(video, &video_callbacks, &pli_capture) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_transceiver_set_callbacks(audio, &audio_callbacks, &pli_capture) == MRTC_STATUS_OK);

    frame.data = small_h264;
    frame.size = sizeof(small_h264);
    frame.presentation_ts = 10000000ull;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK);
    CHECK_TRUE(send_capture.packet_count == 1u);
    CHECK_TRUE(video->rtp_packets_sent == 1u);
    CHECK_TRUE(video->rtp_octets_sent == 3u);

    rtcp_size = sizeof(rtcp);
    CHECK_TRUE(mrtc_rtcp_generate_pli(0x01020304u, video->local_ssrc, rtcp, sizeof(rtcp), &rtcp_size) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection, rtcp, rtcp_size, 0) == MRTC_STATUS_OK);
    CHECK_TRUE(pli_capture.video_count == 1u);
    CHECK_TRUE(pli_capture.audio_count == 0u);
    CHECK_TRUE(video->picture_loss_count == 1u);

    rtcp_size = sizeof(rtcp);
    CHECK_TRUE(mrtc_rtcp_generate_pli(0x01020304u, 0x0badcafeu, rtcp, sizeof(rtcp), &rtcp_size) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection, rtcp, rtcp_size, 0) == MRTC_STATUS_INVALID_STATE);
    CHECK_TRUE(pli_capture.video_count == 1u);
    CHECK_TRUE(pli_capture.audio_count == 0u);

    rtcp_size = sizeof(rtcp);
    CHECK_TRUE(mrtc_transceiver_generate_sender_report(video, 0x0102030405060708ull, rtcp, sizeof(rtcp), &rtcp_size) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtcp_parse_header(rtcp, rtcp_size, &header) == MRTC_STATUS_OK);
    CHECK_TRUE(header.packet_type == MRTC_RTCP_TYPE_SR);
    CHECK_TRUE(mrtc_rtcp_parse_sender_report(rtcp, rtcp_size, &sr) == MRTC_STATUS_OK);
    CHECK_TRUE(sr.sender_ssrc == video->local_ssrc);
    CHECK_TRUE(sr.ntp_timestamp == 0x0102030405060708ull);
    CHECK_TRUE(sr.rtp_timestamp == video->last_rtp_timestamp);
    CHECK_TRUE(sr.packet_count == 1u);
    CHECK_TRUE(sr.octet_count == 3u);

    video->remote_ssrc = 0x11223344u;
    sr.sender_ssrc = video->remote_ssrc;
    sr.ntp_timestamp = 0x1111111122222222ull;
    sr.rtp_timestamp = 1234u;
    sr.packet_count = 5u;
    sr.octet_count = 99u;
    rtcp_size = sizeof(rtcp);
    CHECK_TRUE(mrtc_rtcp_generate_sender_report(&sr, rtcp, sizeof(rtcp), &rtcp_size) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection, rtcp, rtcp_size, 0) == MRTC_STATUS_OK);
    CHECK_TRUE(video->sender_reports_received == 1u);

    rtcp_size = sizeof(rtcp);
    CHECK_TRUE(mrtc_transceiver_generate_receiver_report(video, 0x01020304u, 7u, 2u, 55u, rtcp, sizeof(rtcp), &rtcp_size) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtcp_parse_header(rtcp, rtcp_size, &header) == MRTC_STATUS_OK);
    CHECK_TRUE(header.packet_type == MRTC_RTCP_TYPE_RR);
    CHECK_TRUE(mrtc_rtcp_parse_receiver_report(rtcp, rtcp_size, &rr) == MRTC_STATUS_OK);
    CHECK_TRUE(rr.sender_ssrc == 0x01020304u);
    CHECK_TRUE(rr.report_ssrc == video->local_ssrc);
    CHECK_TRUE(rr.fraction_lost == 7u);
    CHECK_TRUE(rr.cumulative_lost == 2u);
    CHECK_TRUE(rr.highest_sequence_number == 0u);
    CHECK_TRUE(rr.jitter == 55u);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection, rtcp, rtcp_size, 0) == MRTC_STATUS_OK);
    CHECK_TRUE(video->receiver_reports_received == 1u);
    CHECK_TRUE(video->last_receiver_fraction_lost == 7u);
    CHECK_TRUE(video->last_receiver_cumulative_lost == 2u);
    CHECK_TRUE(video->last_receiver_jitter == 55u);

    mrtc_peer_connection_free(peer_connection);
    return 0;
}
