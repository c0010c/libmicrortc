#include "rtp/rtp_packet.h"

#include <stdio.h>
#include <string.h>

#define CHECK_TRUE(condition)                                                                                       \
    do {                                                                                                            \
        if (!(condition)) {                                                                                         \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);                       \
            return 1;                                                                                               \
        }                                                                                                           \
    } while (0)

int main(void)
{
    static const uint8_t payload[] = {0xde, 0xad, 0xbe, 0xef};
    uint8_t raw[64];
    size_t raw_size = 0;
    MRTC_RTP_PACKET packet;
    MRTC_RTP_PACKET parsed;
    static const uint8_t too_short[] = {0x80, 0x60, 0x00};
    static const uint8_t invalid_version[] = {
        0x40, 0x60, 0x00, 0x01, 0x00, 0x00, 0x03, 0xc0, 0x01, 0x02, 0x03, 0x04};

    CHECK_TRUE(mrtc_rtp_packet_build(&packet, 1, 96, 65535u, 960u, 0x01020304u, payload, sizeof(payload)) ==
               MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtp_packet_header_size(&packet) == MRTC_RTP_FIXED_HEADER_SIZE);
    CHECK_TRUE(mrtc_rtp_packet_serialize(&packet, raw, sizeof(raw), &raw_size) == MRTC_STATUS_OK);
    CHECK_TRUE(raw_size == MRTC_RTP_FIXED_HEADER_SIZE + sizeof(payload));
    CHECK_TRUE(mrtc_rtp_packet_parse(raw, raw_size, &parsed) == MRTC_STATUS_OK);
    CHECK_TRUE(parsed.version == MRTC_RTP_VERSION);
    CHECK_TRUE(parsed.marker == 1u);
    CHECK_TRUE(parsed.payload_type == 96u);
    CHECK_TRUE(parsed.sequence_number == 65535u);
    CHECK_TRUE(parsed.timestamp == 960u);
    CHECK_TRUE(parsed.ssrc == 0x01020304u);
    CHECK_TRUE(parsed.payload_size == sizeof(payload));
    CHECK_TRUE(memcmp(parsed.payload, payload, sizeof(payload)) == 0);
    CHECK_TRUE(parsed.raw == raw);
    CHECK_TRUE(parsed.raw_size == raw_size);
    CHECK_TRUE(mrtc_rtp_sequence_next(65535u) == 0u);
    CHECK_TRUE(mrtc_rtp_packet_parse(too_short, sizeof(too_short), &parsed) == MRTC_STATUS_PARSE_ERROR);
    CHECK_TRUE(mrtc_rtp_packet_parse(invalid_version, sizeof(invalid_version), &parsed) == MRTC_STATUS_PARSE_ERROR);

    return 0;
}
