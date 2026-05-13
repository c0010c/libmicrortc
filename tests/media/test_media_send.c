#include "../../src/media/media_transceiver.h"
#include "../../src/rtp/rtp_packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_TRUE(expr) do { if (!(expr)) { return 1; } } while (0)

typedef struct SendCapture {
    size_t packet_count;
    size_t h264_packets;
    size_t opus_packets;
    uint8_t last_payload_type;
    uint16_t last_sequence_number;
    uint32_t last_ssrc;
    int saw_marker;
    int passthrough;
} SendCapture;

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
    MRTC_RTP_PACKET parsed;

    if (capture == 0 || peer_connection == 0 || transceiver == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    capture->packet_count++;
    if (transceiver->codec == MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1) {
        capture->h264_packets++;
    } else if (transceiver->codec == MRTC_CODEC_OPUS) {
        capture->opus_packets++;
    }

    if (capture->passthrough) {
        if (mrtc_rtp_packet_parse(packet, packet_size, &parsed) != MRTC_STATUS_OK) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        capture->last_payload_type = parsed.payload_type;
        capture->last_sequence_number = parsed.sequence_number;
        capture->last_ssrc = parsed.ssrc;
        capture->saw_marker = parsed.marker != 0u;
    }
    return MRTC_STATUS_OK;
}

static int connect_peer(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    char offer[2048];
    char answer[4096];
    size_t required_len = 0;

    if (!read_fixture(offer, sizeof(offer))) {
        return 0;
    }
    if (mrtc_peer_connection_set_remote_description(peer_connection, "offer", offer) != MRTC_STATUS_OK ||
        mrtc_peer_connection_create_answer(peer_connection, answer, sizeof(answer), &required_len) != MRTC_STATUS_OK ||
        mrtc_peer_connection_set_local_description(peer_connection, "answer", answer) != MRTC_STATUS_OK ||
        mrtc_peer_connection_add_ice_candidate(peer_connection, "candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host") != MRTC_STATUS_OK) {
        return 0;
    }
    return 1;
}

int main(void)
{
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_PEER_CONNECTION_HANDLE peer_connection = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE video = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE audio = 0;
    MRTC_TRANSCEIVER_INIT video_init = {0};
    MRTC_TRANSCEIVER_INIT audio_init = {0};
    SendCapture capture = {0};
    const uint8_t small_h264[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84};
    uint8_t large_h264[1508];
    const uint8_t opus_payload[] = {0x11, 0x22, 0x33, 0x44};
    MRTC_FRAME frame = {0};
    size_t before_packets;

    video_init.kind = MRTC_MEDIA_KIND_VIDEO;
    video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
    video_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    audio_init.kind = MRTC_MEDIA_KIND_AUDIO;
    audio_init.codec = MRTC_CODEC_OPUS;
    audio_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    CHECK_TRUE(mrtc_peer_connection_create(&config, 0, 0, &peer_connection) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_add_transceiver(peer_connection, &video_init, 0, &video) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_add_transceiver(peer_connection, &audio_init, 0, &audio) == MRTC_STATUS_OK);

    frame.data = small_h264;
    frame.size = sizeof(small_h264);
    frame.presentation_ts = 10000000ull;
    frame.decoding_ts = frame.presentation_ts;
    frame.index = 4;
    frame.flags = MRTC_FRAME_FLAG_KEY_FRAME;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_INVALID_STATE);

    CHECK_TRUE(connect_peer(peer_connection));
    capture.passthrough = mrtc_peer_connection_media_is_srtp_passthrough(peer_connection);
    CHECK_TRUE(mrtc_peer_connection_set_media_send_hook(peer_connection, capture_send, &capture) == MRTC_STATUS_OK);

    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK);
    CHECK_TRUE(capture.h264_packets == 1u);
    CHECK_TRUE(video->sequence_number == 1u);
    CHECK_TRUE(video->frames_sent == 1u);
    if (capture.passthrough) {
        CHECK_TRUE(capture.last_payload_type == 96u);
        CHECK_TRUE(capture.last_sequence_number == 0u);
        CHECK_TRUE(capture.last_ssrc == video->local_ssrc);
        CHECK_TRUE(capture.saw_marker);
    }

    memset(large_h264, 0x55, sizeof(large_h264));
    large_h264[0] = 0x00;
    large_h264[1] = 0x00;
    large_h264[2] = 0x00;
    large_h264[3] = 0x01;
    large_h264[4] = 0x65;
    frame.data = large_h264;
    frame.size = sizeof(large_h264);
    frame.index = 5;
    before_packets = capture.h264_packets;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK);
    CHECK_TRUE(capture.h264_packets > before_packets + 1u);
    CHECK_TRUE(video->sequence_number == capture.h264_packets);
    CHECK_TRUE(video->frames_sent == 2u);

    frame.data = opus_payload;
    frame.size = sizeof(opus_payload);
    frame.presentation_ts = 200000ull;
    frame.decoding_ts = frame.presentation_ts;
    frame.index = 9;
    frame.flags = MRTC_FRAME_FLAG_NONE;
    CHECK_TRUE(mrtc_transceiver_write_frame(audio, &frame) == MRTC_STATUS_OK);
    CHECK_TRUE(capture.opus_packets == 1u);
    CHECK_TRUE(audio->sequence_number == 1u);
    CHECK_TRUE(audio->frames_sent == 1u);
    if (capture.passthrough) {
        CHECK_TRUE(capture.last_payload_type == 111u);
        CHECK_TRUE(capture.last_sequence_number == 0u);
        CHECK_TRUE(capture.last_ssrc == audio->local_ssrc);
        CHECK_TRUE(capture.saw_marker);
    }

    mrtc_peer_connection_free(peer_connection);
    return 0;
}
