#include "../../src/media/media_transceiver.h"
#include "../../src/rtcp/retransmitter.h"
#include "../../src/rtcp/rtcp_packet.h"
#include "../../src/rtp/rtp_packet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_TRUE(expr, message)                                                                                   \
    do {                                                                                                            \
        if (!(expr)) {                                                                                             \
            fprintf(stderr, "phase5 media verifier: %s\n", message);                                              \
            return 0;                                                                                              \
        }                                                                                                           \
    } while (0)

typedef struct FixtureBytes {
    unsigned char *data;
    size_t size;
} FixtureBytes;

typedef struct OpusFixture {
    FixtureBytes packets[8];
    size_t packet_count;
} OpusFixture;

typedef struct SendCapture {
    uint8_t packets[128][1600];
    size_t packet_sizes[128];
    size_t packet_count;
    size_t h264_packets;
    size_t opus_packets;
    uint16_t sequence_numbers[128];
    uint8_t payload_types[128];
    uint32_t timestamps[128];
    int passthrough;
} SendCapture;

typedef struct FrameCapture {
    uint8_t data[8192];
    size_t size;
    size_t frame_count;
    uint64_t presentation_ts[8];
    uint32_t flags;
    size_t picture_loss_count;
} FrameCapture;

static const char *argument_value(int argc, char **argv, const char *name)
{
    int i;

    for (i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return 0;
}

static int path_join(char *buffer, size_t buffer_len, const char *dir, const char *name)
{
    size_t dir_len;
    int written;

    if (buffer == 0 || buffer_len == 0u || dir == 0 || name == 0 || dir[0] == '\0') {
        return 0;
    }
    dir_len = strlen(dir);
    written = snprintf(buffer, buffer_len, "%s%s%s", dir, dir[dir_len - 1u] == '/' ? "" : "/", name);
    return written > 0 && (size_t) written < buffer_len;
}

static int read_file(const char *path, FixtureBytes *bytes)
{
    FILE *file;
    long file_size;
    unsigned char *buffer;
    size_t read_size;

    if (path == 0 || bytes == 0) {
        return 0;
    }
    memset(bytes, 0, sizeof(*bytes));

    file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return 0;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    buffer = (unsigned char *) malloc((size_t) file_size);
    if (buffer == 0) {
        fclose(file);
        return 0;
    }
    read_size = fread(buffer, 1, (size_t) file_size, file);
    fclose(file);
    if (read_size != (size_t) file_size) {
        free(buffer);
        return 0;
    }
    bytes->data = buffer;
    bytes->size = read_size;
    return 1;
}

static int h264_start_code_size_at(const unsigned char *data, size_t size, size_t offset)
{
    if (offset + 3u <= size && data[offset] == 0u && data[offset + 1u] == 0u && data[offset + 2u] == 1u) {
        return 3;
    }
    if (offset + 4u <= size && data[offset] == 0u && data[offset + 1u] == 0u &&
        data[offset + 2u] == 0u && data[offset + 3u] == 1u) {
        return 4;
    }
    return 0;
}

static int h264_fixture_is_valid(const FixtureBytes *h264)
{
    size_t offset = 0;
    int saw_sps = 0;
    int saw_pps = 0;
    int saw_idr = 0;

    if (h264 == 0 || h264->data == 0 || h264->size < 5u) {
        return 0;
    }

    while (offset < h264->size) {
        int start_code_size = h264_start_code_size_at(h264->data, h264->size, offset);
        size_t nalu_start;
        size_t next_start;
        unsigned int nalu_type;

        if (start_code_size == 0) {
            return 0;
        }
        nalu_start = offset + (size_t) start_code_size;
        if (nalu_start >= h264->size) {
            return 0;
        }
        next_start = nalu_start + 1u;
        while (next_start < h264->size && h264_start_code_size_at(h264->data, h264->size, next_start) == 0) {
            ++next_start;
        }
        nalu_type = h264->data[nalu_start] & 0x1fu;
        if (nalu_type == 7u) {
            if (saw_idr) {
                return 0;
            }
            saw_sps = 1;
        } else if (nalu_type == 8u) {
            if (!saw_sps || saw_idr) {
                return 0;
            }
            saw_pps = 1;
        } else if (nalu_type == 5u) {
            if (!saw_sps || !saw_pps) {
                return 0;
            }
            saw_idr = 1;
        }
        offset = next_start;
    }

    return saw_sps && saw_pps && saw_idr;
}

static int read_opus_fixture(const char *path, OpusFixture *fixture)
{
    FixtureBytes raw;
    size_t offset = 0;

    if (fixture == 0 || !read_file(path, &raw)) {
        return 0;
    }
    memset(fixture, 0, sizeof(*fixture));

    while (offset < raw.size) {
        size_t packet_len;

        if (offset + 2u > raw.size || fixture->packet_count >= 8u) {
            free(raw.data);
            return 0;
        }
        packet_len = ((size_t) raw.data[offset] << 8u) | (size_t) raw.data[offset + 1u];
        offset += 2u;
        if (packet_len == 0u || offset + packet_len > raw.size) {
            free(raw.data);
            return 0;
        }
        fixture->packets[fixture->packet_count].data = (unsigned char *) malloc(packet_len);
        if (fixture->packets[fixture->packet_count].data == 0) {
            free(raw.data);
            return 0;
        }
        memcpy(fixture->packets[fixture->packet_count].data, raw.data + offset, packet_len);
        fixture->packets[fixture->packet_count].size = packet_len;
        offset += packet_len;
        fixture->packet_count++;
    }

    free(raw.data);
    return fixture->packet_count >= 2u;
}

static void free_opus_fixture(OpusFixture *fixture)
{
    size_t i;

    if (fixture == 0) {
        return;
    }
    for (i = 0; i < fixture->packet_count; ++i) {
        free(fixture->packets[i].data);
        fixture->packets[i].data = 0;
        fixture->packets[i].size = 0;
    }
    fixture->packet_count = 0;
}

static int read_sdp_fixture(char *buffer, size_t buffer_len)
{
    FILE *file = fopen("tests/fixtures/minimal_offer.sdp", "rb");
    size_t read_len;

    if (file == 0 || buffer == 0 || buffer_len == 0u) {
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
    size_t index;

    if (capture == 0 || peer_connection == 0 || transceiver == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    index = capture->packet_count;
    if (index < 128u && packet_size <= sizeof(capture->packets[0])) {
        memcpy(capture->packets[index], packet, packet_size);
        capture->packet_sizes[index] = packet_size;
    }
    if (transceiver->codec == MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1) {
        capture->h264_packets++;
    } else if (transceiver->codec == MRTC_CODEC_OPUS) {
        capture->opus_packets++;
    }
    if (capture->passthrough && index < 32u &&
        mrtc_rtp_packet_parse(packet, packet_size, &parsed) == MRTC_STATUS_OK) {
        capture->sequence_numbers[index] = parsed.sequence_number;
        capture->payload_types[index] = parsed.payload_type;
        capture->timestamps[index] = parsed.timestamp;
    }
    capture->packet_count++;
    return MRTC_STATUS_OK;
}

static void reset_send_capture(SendCapture *capture)
{
    int passthrough = capture->passthrough;
    memset(capture, 0, sizeof(*capture));
    capture->passthrough = passthrough;
}

static void capture_frame(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver, const MRTC_FRAME *frame)
{
    FrameCapture *capture = (FrameCapture *) user_data;
    (void) transceiver;

    if (capture == 0 || frame == 0 || frame->data == 0 || frame->size == 0u || frame->size > sizeof(capture->data)) {
        return;
    }
    memcpy(capture->data, frame->data, frame->size);
    capture->size = frame->size;
    if (capture->frame_count < 8u) {
        capture->presentation_ts[capture->frame_count] = frame->presentation_ts;
    }
    capture->flags = frame->flags;
    capture->frame_count++;
}

static void on_video_picture_loss(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    FrameCapture *capture = (FrameCapture *) user_data;
    if (capture != 0 && transceiver != 0) {
        capture->picture_loss_count++;
    }
}

static void on_audio_picture_loss(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    FrameCapture *capture = (FrameCapture *) user_data;
    if (capture != 0 && transceiver != 0) {
        capture->picture_loss_count++;
    }
}

static int connect_peer(MRTC_PEER_CONNECTION_HANDLE peer_connection)
{
    char offer[2048];
    char answer[4096];
    size_t required_len = 0;

    CHECK_TRUE(read_sdp_fixture(offer, sizeof(offer)), "minimal SDP fixture missing");
    CHECK_TRUE(mrtc_peer_connection_set_remote_description(peer_connection, "offer", offer) == MRTC_STATUS_OK,
               "remote offer rejected");
    CHECK_TRUE(mrtc_peer_connection_create_answer(peer_connection, answer, sizeof(answer), &required_len) == MRTC_STATUS_OK,
               "answer creation failed");
    CHECK_TRUE(mrtc_peer_connection_set_local_description(peer_connection, "answer", answer) == MRTC_STATUS_OK,
               "local answer rejected");
    CHECK_TRUE(mrtc_peer_connection_add_ice_candidate(peer_connection,
                                                      "candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host") ==
                   MRTC_STATUS_OK,
               "host candidate rejected");
    return 1;
}

static void write_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t) (value >> 8u);
    bytes[1] = (uint8_t) value;
}

static void write_u32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t) (value >> 24u);
    bytes[1] = (uint8_t) (value >> 16u);
    bytes[2] = (uint8_t) (value >> 8u);
    bytes[3] = (uint8_t) value;
}

static void build_nack(uint32_t media_ssrc, uint16_t pid, uint16_t blp, uint8_t *raw)
{
    raw[0] = 0x81u;
    raw[1] = 0xcdu;
    raw[2] = 0x00u;
    raw[3] = 0x03u;
    write_u32(raw + 4u, 0x01020304u);
    write_u32(raw + 8u, media_ssrc);
    write_u16(raw + 12u, pid);
    write_u16(raw + 14u, blp);
}

static int verify_media_path(const FixtureBytes *h264, const OpusFixture *opus)
{
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_PEER_CONNECTION_HANDLE peer_connection = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE video = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE audio = 0;
    MRTC_TRANSCEIVER_INIT video_init = {0};
    MRTC_TRANSCEIVER_INIT audio_init = {0};
    MRTC_TRANSCEIVER_CALLBACKS video_callbacks = {0};
    MRTC_TRANSCEIVER_CALLBACKS audio_callbacks = {0};
    MRTC_FRAME frame = {0};
    SendCapture send_capture = {0};
    FrameCapture video_frames = {0};
    FrameCapture audio_frames = {0};
    MRTC_RTCP_RETRANSMIT_RESULT retransmit_result;
    uint8_t rtcp[64];
    size_t rtcp_size = sizeof(rtcp);
    size_t i;

    video_init.kind = MRTC_MEDIA_KIND_VIDEO;
    video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
    video_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    audio_init.kind = MRTC_MEDIA_KIND_AUDIO;
    audio_init.codec = MRTC_CODEC_OPUS;
    audio_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    CHECK_TRUE(mrtc_peer_connection_create(&config, 0, 0, &peer_connection) == MRTC_STATUS_OK,
               "peer connection create failed");
    CHECK_TRUE(mrtc_peer_connection_add_transceiver(peer_connection, &video_init, 0, &video) == MRTC_STATUS_OK,
               "video transceiver create failed");
    CHECK_TRUE(mrtc_peer_connection_add_transceiver(peer_connection, &audio_init, 0, &audio) == MRTC_STATUS_OK,
               "audio transceiver create failed");
    printf("public media api ok\n");

    CHECK_TRUE(connect_peer(peer_connection), "peer connection media setup failed");
    send_capture.passthrough = mrtc_peer_connection_media_is_srtp_passthrough(peer_connection);
    CHECK_TRUE(mrtc_peer_connection_set_media_send_hook(peer_connection, capture_send, &send_capture) == MRTC_STATUS_OK,
               "media send hook failed");

    frame.data = h264->data;
    frame.size = h264->size;
    frame.presentation_ts = 10000000ull;
    frame.decoding_ts = frame.presentation_ts;
    frame.flags = MRTC_FRAME_FLAG_KEY_FRAME;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK, "H264 write_frame failed");
    CHECK_TRUE(send_capture.h264_packets >= 3u, "H264 fixture did not produce expected RTP packets");
    CHECK_TRUE(video->frames_sent == 1u && video->rtp_packets_sent >= 3u, "H264 send counters missing");
    printf("h264 send ok\n");
    printf("srtp media path ok\n");

    video_callbacks.on_frame = capture_frame;
    audio_callbacks.on_frame = capture_frame;
    video_callbacks.on_picture_loss = on_video_picture_loss;
    audio_callbacks.on_picture_loss = on_audio_picture_loss;
    CHECK_TRUE(mrtc_transceiver_set_callbacks(video, &video_callbacks, &video_frames) == MRTC_STATUS_OK,
               "video callbacks failed");
    CHECK_TRUE(mrtc_transceiver_set_callbacks(audio, &audio_callbacks, &audio_frames) == MRTC_STATUS_OK,
               "audio callbacks failed");
    video->user_data = &video_frames;
    audio->user_data = &audio_frames;

    for (i = 0; i < send_capture.packet_count; ++i) {
        CHECK_TRUE(mrtc_peer_connection_receive_protected_media_packet(peer_connection,
                                                                       send_capture.packets[i],
                                                                       send_capture.packet_sizes[i]) == MRTC_STATUS_OK,
                   "H264 receive packet failed");
    }
    CHECK_TRUE(video_frames.frame_count == 1u, "H264 on_frame not called");
    {
        FixtureBytes received_h264;

        received_h264.data = video_frames.data;
        received_h264.size = video_frames.size;
        CHECK_TRUE(h264_fixture_is_valid(&received_h264), "H264 received Annex-B invalid");
    }
    CHECK_TRUE((video_frames.flags & MRTC_FRAME_FLAG_KEY_FRAME) != 0u, "H264 keyframe flag missing");
    printf("h264 receive ok\n");

    reset_send_capture(&send_capture);
    frame.data = opus->packets[0].data;
    frame.size = opus->packets[0].size;
    frame.presentation_ts = 200000ull;
    frame.decoding_ts = frame.presentation_ts;
    frame.flags = MRTC_FRAME_FLAG_NONE;
    CHECK_TRUE(mrtc_transceiver_write_frame(audio, &frame) == MRTC_STATUS_OK, "Opus first write_frame failed");
    frame.data = opus->packets[1].data;
    frame.size = opus->packets[1].size;
    frame.presentation_ts = 400000ull;
    frame.decoding_ts = frame.presentation_ts;
    CHECK_TRUE(mrtc_transceiver_write_frame(audio, &frame) == MRTC_STATUS_OK, "Opus second write_frame failed");
    CHECK_TRUE(send_capture.opus_packets == 2u, "Opus did not produce two RTP packets");
    CHECK_TRUE(audio->frames_sent == 2u && audio->rtp_packets_sent == 2u, "Opus send counters missing");
    if (send_capture.passthrough) {
        CHECK_TRUE(send_capture.payload_types[0] == 111u && send_capture.payload_types[1] == 111u,
                   "Opus payload type mismatch");
        CHECK_TRUE(send_capture.timestamps[1] > send_capture.timestamps[0], "Opus RTP timestamp is not monotonic");
    }
    printf("opus send ok\n");

    for (i = 0; i < send_capture.packet_count; ++i) {
        CHECK_TRUE(mrtc_peer_connection_receive_protected_media_packet(peer_connection,
                                                                       send_capture.packets[i],
                                                                       send_capture.packet_sizes[i]) == MRTC_STATUS_OK,
                   "Opus receive packet failed");
    }
    CHECK_TRUE(audio_frames.frame_count == 2u, "Opus on_frame count mismatch");
    CHECK_TRUE(audio_frames.size == opus->packets[1].size, "Opus last received frame size mismatch");
    CHECK_TRUE(memcmp(audio_frames.data, opus->packets[1].data, opus->packets[1].size) == 0,
               "Opus received payload mismatch");
    CHECK_TRUE(audio_frames.presentation_ts[1] > audio_frames.presentation_ts[0],
               "Opus received timestamp is not monotonic");
    printf("opus receive ok\n");

    reset_send_capture(&send_capture);
    frame.data = h264->data;
    frame.size = h264->size;
    frame.presentation_ts = 30000000ull;
    frame.flags = MRTC_FRAME_FLAG_KEY_FRAME;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK, "H264 retransmit seed write failed");
    CHECK_TRUE(send_capture.packet_count >= 2u, "H264 retransmit seed did not send enough packets");
    build_nack(video->local_ssrc, send_capture.sequence_numbers[0], 0x0001u, rtcp);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection,
                                                                  rtcp,
                                                                  16u,
                                                                  &retransmit_result) == MRTC_STATUS_OK,
               "NACK RTCP receive failed");
    CHECK_TRUE(retransmit_result.requested_count == 2u, "NACK requested count mismatch");
    CHECK_TRUE(retransmit_result.retransmitted_count == 2u, "NACK retransmit count mismatch");
    CHECK_TRUE(video->nack_packets_received > 0u && video->retransmitted_packets_sent >= 2u,
               "NACK behavior counters missing");
    printf("rtcp nack retransmit ok\n");

    rtcp_size = sizeof(rtcp);
    CHECK_TRUE(mrtc_rtcp_generate_pli(0x01020304u, video->local_ssrc, rtcp, sizeof(rtcp), &rtcp_size) == MRTC_STATUS_OK,
               "PLI generate failed");
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection, rtcp, rtcp_size, 0) == MRTC_STATUS_OK,
               "PLI receive failed");
    CHECK_TRUE(video_frames.picture_loss_count == 1u && audio_frames.picture_loss_count == 0u &&
                   video->picture_loss_count == 1u,
               "PLI callback behavior missing");
    printf("pli callback ok\n");

    mrtc_peer_connection_free(peer_connection);
    return 1;
}

int main(int argc, char **argv)
{
    const char *fixture_dir = argument_value(argc, argv, "--fixtures");
    char h264_path[512];
    char opus_path[512];
    FixtureBytes h264;
    OpusFixture opus;
    int ok;

    memset(&h264, 0, sizeof(h264));
    memset(&opus, 0, sizeof(opus));

    if (fixture_dir == 0 || fixture_dir[0] == '\0') {
        fprintf(stderr, "phase5 media verifier: missing --fixtures path\n");
        return 2;
    }
    if (!path_join(h264_path, sizeof(h264_path), fixture_dir, "h264_annexb_sample.h264") ||
        !path_join(opus_path, sizeof(opus_path), fixture_dir, "opus_packets.bin")) {
        fprintf(stderr, "phase5 media verifier: invalid --fixtures path\n");
        return 2;
    }
    if (!read_file(h264_path, &h264) || !h264_fixture_is_valid(&h264)) {
        fprintf(stderr, "phase5 media verifier: --fixtures must contain readable h264_annexb_sample.h264 with SPS/PPS before IDR\n");
        free(h264.data);
        return 3;
    }
    if (!read_opus_fixture(opus_path, &opus)) {
        fprintf(stderr, "phase5 media verifier: --fixtures must contain readable opus_packets.bin with non-empty length-prefixed packets\n");
        free(h264.data);
        return 3;
    }

    ok = verify_media_path(&h264, &opus);
    free(h264.data);
    free_opus_fixture(&opus);
    if (!ok) {
        return 4;
    }

    printf("phase5 media verifier complete\n");
    return 0;
}
