#include "../../src/media/media_transceiver.h"
#include "../../src/rtp/rtp_packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_TRUE(expr) do { if (!(expr)) { return 1; } } while (0)

typedef struct SendCapture {
    uint8_t packets[8][1600];
    size_t packet_sizes[8];
    size_t packet_count;
    size_t h264_packets;
    size_t opus_packets;
    uint8_t last_payload_type;
    uint16_t last_sequence_number;
    uint32_t last_ssrc;
    int saw_marker;
    int passthrough;
} SendCapture;

typedef struct FrameCapture {
    uint8_t data[2048];
    size_t size;
    uint64_t presentation_ts;
    uint64_t index;
    uint32_t flags;
    size_t frame_count;
} FrameCapture;

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
    if (capture->packet_count <= 8u && packet_size <= sizeof(capture->packets[0])) {
        memcpy(capture->packets[capture->packet_count - 1u], packet, packet_size);
        capture->packet_sizes[capture->packet_count - 1u] = packet_size;
    }
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

static void capture_frame(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver, const MRTC_FRAME *frame)
{
    FrameCapture *capture = (FrameCapture *) user_data;
    (void) transceiver;

    if (capture == 0 || frame == 0 || frame->data == 0 || frame->size > sizeof(capture->data)) {
        return;
    }
    memcpy(capture->data, frame->data, frame->size);
    capture->size = frame->size;
    capture->presentation_ts = frame->presentation_ts;
    capture->index = frame->index;
    capture->flags = frame->flags;
    capture->frame_count++;
}

static void reset_capture(SendCapture *capture)
{
    int passthrough = capture->passthrough;
    memset(capture, 0, sizeof(*capture));
    capture->passthrough = passthrough;
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
    FrameCapture video_frames = {0};
    FrameCapture audio_frames = {0};
    MRTC_TRANSCEIVER_CALLBACKS video_callbacks = {0};
    MRTC_TRANSCEIVER_CALLBACKS audio_callbacks = {0};
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

    video_callbacks.on_frame = capture_frame;
    audio_callbacks.on_frame = capture_frame;
    CHECK_TRUE(mrtc_transceiver_set_callbacks(video, &video_callbacks, &video_frames) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_transceiver_set_callbacks(audio, &audio_callbacks, &audio_frames) == MRTC_STATUS_OK);

    reset_capture(&capture);
    frame.data = large_h264;
    frame.size = sizeof(large_h264);
    frame.presentation_ts = 30000000ull;
    frame.decoding_ts = frame.presentation_ts;
    frame.index = 10;
    frame.flags = MRTC_FRAME_FLAG_KEY_FRAME;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK);
    CHECK_TRUE(capture.packet_count > 1u);
    for (before_packets = 0; before_packets < capture.packet_count; ++before_packets) {
        CHECK_TRUE(mrtc_peer_connection_receive_protected_media_packet(peer_connection,
                                                                       capture.packets[before_packets],
                                                                       capture.packet_sizes[before_packets]) == MRTC_STATUS_OK);
    }
    CHECK_TRUE(video_frames.frame_count == 1u);
    CHECK_TRUE(video_frames.size == sizeof(large_h264));
    CHECK_TRUE(memcmp(video_frames.data, large_h264, sizeof(large_h264)) == 0);
    CHECK_TRUE(video_frames.presentation_ts == 30000000ull);
    CHECK_TRUE(video_frames.index == 0u);
    CHECK_TRUE((video_frames.flags & MRTC_FRAME_FLAG_KEY_FRAME) != 0u);
    CHECK_TRUE(video->frames_received == 1u);

    reset_capture(&capture);
    frame.data = opus_payload;
    frame.size = sizeof(opus_payload);
    frame.presentation_ts = 400000ull;
    frame.decoding_ts = frame.presentation_ts;
    frame.index = 11;
    frame.flags = MRTC_FRAME_FLAG_NONE;
    CHECK_TRUE(mrtc_transceiver_write_frame(audio, &frame) == MRTC_STATUS_OK);
    CHECK_TRUE(capture.packet_count == 1u);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_media_packet(peer_connection,
                                                                   capture.packets[0],
                                                                   capture.packet_sizes[0]) == MRTC_STATUS_OK);
    CHECK_TRUE(audio_frames.frame_count == 1u);
    CHECK_TRUE(audio_frames.size == sizeof(opus_payload));
    CHECK_TRUE(memcmp(audio_frames.data, opus_payload, sizeof(opus_payload)) == 0);
    CHECK_TRUE(audio_frames.presentation_ts == 400000ull);
    CHECK_TRUE(audio_frames.index == 0u);
    CHECK_TRUE(audio->frames_received == 1u);

    if (capture.passthrough) {
        MRTC_RTP_PACKET wrong_packet;
        uint8_t wrong_raw[64];
        size_t wrong_size = 0;
        CHECK_TRUE(mrtc_rtp_packet_build(&wrong_packet,
                                         1,
                                         120,
                                         77,
                                         900,
                                         0x01020304u,
                                         opus_payload,
                                         sizeof(opus_payload)) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_rtp_packet_serialize(&wrong_packet, wrong_raw, sizeof(wrong_raw), &wrong_size) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_peer_connection_receive_protected_media_packet(peer_connection, wrong_raw, wrong_size) == MRTC_STATUS_INVALID_STATE);
        CHECK_TRUE(video_frames.frame_count == 1u);
        CHECK_TRUE(audio_frames.frame_count == 1u);

        CHECK_TRUE(mrtc_rtp_packet_build(&wrong_packet,
                                         1,
                                         111,
                                         78,
                                         960,
                                         0x01020304u,
                                         opus_payload,
                                         sizeof(opus_payload)) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_rtp_packet_serialize(&wrong_packet, wrong_raw, sizeof(wrong_raw), &wrong_size) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_peer_connection_receive_protected_media_packet(peer_connection, wrong_raw, wrong_size) == MRTC_STATUS_INVALID_STATE);
        CHECK_TRUE(audio_frames.frame_count == 1u);
    }

    mrtc_peer_connection_free(peer_connection);

    if (capture.passthrough) {
        MRTC_PEER_CONNECTION_HANDLE receive_peer = 0;
        MRTC_RTP_TRANSCEIVER_HANDLE sendonly_video = 0;
        MRTC_RTP_TRANSCEIVER_HANDLE recvonly_video = 0;
        MRTC_TRANSCEIVER_INIT sendonly_init = {0};
        MRTC_TRANSCEIVER_INIT recvonly_init = {0};
        MRTC_TRANSCEIVER_CALLBACKS recv_callbacks = {0};
        FrameCapture recv_frames = {0};
        MRTC_RTP_PACKET inbound_packet;
        uint8_t inbound_raw[64];
        size_t inbound_size = 0;
        const uint8_t inbound_payload[] = {0x65, 0x88, 0x84};

        sendonly_init.kind = MRTC_MEDIA_KIND_VIDEO;
        sendonly_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
        sendonly_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
        recvonly_init = sendonly_init;
        recvonly_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;
        recv_callbacks.on_frame = capture_frame;

        CHECK_TRUE(mrtc_peer_connection_create(&config, 0, 0, &receive_peer) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_peer_connection_add_transceiver(receive_peer, &sendonly_init, 0, &sendonly_video) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_peer_connection_add_transceiver(receive_peer, &recvonly_init, &recv_frames, &recvonly_video) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_transceiver_set_callbacks(recvonly_video, &recv_callbacks, &recv_frames) == MRTC_STATUS_OK);
        CHECK_TRUE(connect_peer(receive_peer));
        CHECK_TRUE(mrtc_peer_connection_media_is_srtp_passthrough(receive_peer));

        CHECK_TRUE(mrtc_rtp_packet_build(&inbound_packet,
                                         1,
                                         96,
                                         10,
                                         90000,
                                         0x22223333u,
                                         inbound_payload,
                                         sizeof(inbound_payload)) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_rtp_packet_serialize(&inbound_packet, inbound_raw, sizeof(inbound_raw), &inbound_size) == MRTC_STATUS_OK);
        CHECK_TRUE(mrtc_peer_connection_receive_protected_media_packet(receive_peer, inbound_raw, inbound_size) == MRTC_STATUS_OK);
        CHECK_TRUE(recv_frames.frame_count == 1u);
        CHECK_TRUE(sendonly_video->remote_ssrc == 0u);
        CHECK_TRUE(recvonly_video->remote_ssrc == 0x22223333u);
        mrtc_peer_connection_free(receive_peer);
    }
    return 0;
}
