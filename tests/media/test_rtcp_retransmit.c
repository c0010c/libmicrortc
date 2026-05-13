#include "../../src/media/media_transceiver.h"
#include "../../src/rtcp/retransmitter.h"
#include "../../src/rtp/rtp_packet.h"

#include <stdio.h>
#include <string.h>

#define CHECK_TRUE(expr) do { if (!(expr)) { return 1; } } while (0)

typedef struct SendCapture {
    uint8_t packets[8][1600];
    size_t packet_sizes[8];
    size_t packet_count;
    uint16_t sequence_numbers[8];
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
    size_t index;

    if (capture == 0 || peer_connection == 0 || transceiver == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    index = capture->packet_count;
    if (index < 8u && packet_size <= sizeof(capture->packets[0])) {
        memcpy(capture->packets[index], packet, packet_size);
        capture->packet_sizes[index] = packet_size;
    }
    if (capture->passthrough && index < 8u) {
        if (mrtc_rtp_packet_parse(packet, packet_size, &parsed) != MRTC_STATUS_OK) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        capture->sequence_numbers[index] = parsed.sequence_number;
    }
    capture->packet_count++;
    return MRTC_STATUS_OK;
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
    return mrtc_peer_connection_set_remote_description(peer_connection, "offer", offer) == MRTC_STATUS_OK &&
           mrtc_peer_connection_create_answer(peer_connection, answer, sizeof(answer), &required_len) == MRTC_STATUS_OK &&
           mrtc_peer_connection_set_local_description(peer_connection, "answer", answer) == MRTC_STATUS_OK &&
           mrtc_peer_connection_add_ice_candidate(peer_connection, "candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host") == MRTC_STATUS_OK;
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

int main(void)
{
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_PEER_CONNECTION_HANDLE peer_connection = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE video = 0;
    MRTC_TRANSCEIVER_INIT video_init = {0};
    MRTC_FRAME frame = {0};
    SendCapture capture = {0};
    MRTC_RTCP_RETRANSMIT_RESULT result;
    const uint8_t small_h264[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84};
    uint8_t nack[16];

    video_init.kind = MRTC_MEDIA_KIND_VIDEO;
    video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
    video_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    CHECK_TRUE(mrtc_peer_connection_create(&config, 0, 0, &peer_connection) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_peer_connection_add_transceiver(peer_connection, &video_init, 0, &video) == MRTC_STATUS_OK);
    CHECK_TRUE(connect_peer(peer_connection));
    capture.passthrough = mrtc_peer_connection_media_is_srtp_passthrough(peer_connection);
    CHECK_TRUE(mrtc_peer_connection_set_media_send_hook(peer_connection, capture_send, &capture) == MRTC_STATUS_OK);

    frame.data = small_h264;
    frame.size = sizeof(small_h264);
    frame.presentation_ts = 10000000ull;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK);
    frame.presentation_ts = 20000000ull;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK);
    video->sequence_number = 3u;
    frame.presentation_ts = 30000000ull;
    CHECK_TRUE(mrtc_transceiver_write_frame(video, &frame) == MRTC_STATUS_OK);
    CHECK_TRUE(capture.packet_count == 3u);
    if (capture.passthrough) {
        CHECK_TRUE(capture.sequence_numbers[0] == 0u);
        CHECK_TRUE(capture.sequence_numbers[1] == 1u);
        CHECK_TRUE(capture.sequence_numbers[2] == 3u);
    }

    reset_capture(&capture);
    build_nack(video->local_ssrc, 0u, 0x0001u, nack);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection,
                                                                  nack,
                                                                  sizeof(nack),
                                                                  &result) == MRTC_STATUS_OK);
    CHECK_TRUE(result.requested_count == 2u);
    CHECK_TRUE(result.retransmitted_count == 2u);
    CHECK_TRUE(result.missing_count == 0u);
    CHECK_TRUE(capture.packet_count == 2u);
    CHECK_TRUE(video->nack_packets_received == 1u);
    CHECK_TRUE(video->retransmitted_packets_sent == 2u);
    if (capture.passthrough) {
        CHECK_TRUE(capture.sequence_numbers[0] == 0u);
        CHECK_TRUE(capture.sequence_numbers[1] == 1u);
    }

    reset_capture(&capture);
    build_nack(0x0badcafeu, 0u, 0u, nack);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection,
                                                                  nack,
                                                                  sizeof(nack),
                                                                  &result) == MRTC_STATUS_INVALID_STATE);
    CHECK_TRUE(capture.packet_count == 0u);

    build_nack(video->local_ssrc, 2u, 0u, nack);
    CHECK_TRUE(mrtc_peer_connection_receive_protected_rtcp_packet(peer_connection,
                                                                  nack,
                                                                  sizeof(nack),
                                                                  &result) == MRTC_STATUS_OK);
    CHECK_TRUE(result.requested_count == 1u);
    CHECK_TRUE(result.retransmitted_count == 0u);
    CHECK_TRUE(result.missing_count == 1u);
    CHECK_TRUE(capture.packet_count == 0u);
    CHECK_TRUE(video->retransmit_packets_missing == 1u);

    mrtc_peer_connection_free(peer_connection);
    return 0;
}
