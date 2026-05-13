#include "rtp/codecs/opus.h"

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
    static const uint8_t opus_packet[] = {0xf8, 0xff, 0xfe, 0x11, 0x22, 0x33};
    uint8_t payload[32];
    uint8_t decoded[32];
    size_t payload_size = sizeof(payload);
    size_t decoded_size = sizeof(decoded);
    MRTC_FRAME frame;

    memset(&frame, 0, sizeof(frame));
    frame.data = opus_packet;
    frame.size = sizeof(opus_packet);
    frame.presentation_ts = 200000ull;

    CHECK_TRUE(mrtc_opus_payload_frame(&frame, payload, &payload_size) == MRTC_STATUS_OK);
    CHECK_TRUE(payload_size == sizeof(opus_packet));
    CHECK_TRUE(memcmp(payload, opus_packet, sizeof(opus_packet)) == 0);

    CHECK_TRUE(mrtc_opus_depayload(payload, payload_size, decoded, &decoded_size) == MRTC_STATUS_OK);
    CHECK_TRUE(decoded_size == sizeof(opus_packet));
    CHECK_TRUE(memcmp(decoded, opus_packet, sizeof(opus_packet)) == 0);
    CHECK_TRUE(mrtc_opus_rtp_timestamp_from_frame(&frame) == 960u);
    CHECK_TRUE(mrtc_opus_timestamp_100ns_to_rtp(200000ull) == 960u);

    decoded_size = sizeof(decoded);
    CHECK_TRUE(mrtc_opus_depayload(0, payload_size, decoded, &decoded_size) == MRTC_STATUS_INVALID_ARG);
    return 0;
}
