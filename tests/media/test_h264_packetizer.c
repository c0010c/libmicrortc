#include "rtp/codecs/h264.h"

#include <stdio.h>
#include <string.h>

#define CHECK_TRUE(condition)                                                                                       \
    do {                                                                                                            \
        if (!(condition)) {                                                                                         \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);                       \
            return 1;                                                                                               \
        }                                                                                                           \
    } while (0)

static int test_single_nalu(void)
{
    static const uint8_t annexb[] = {0x00, 0x00, 0x01, 0x65, 0x11, 0x22};
    uint8_t payloads[32];
    size_t payload_lengths[4];
    size_t payloads_size = sizeof(payloads);
    size_t payload_count = 4;
    uint8_t output[32];
    size_t output_size = sizeof(output);
    int is_start = 0;

    CHECK_TRUE(mrtc_h264_packetize_annexb(annexb, sizeof(annexb), 1200u, payloads, &payloads_size,
                                          payload_lengths, &payload_count) == MRTC_STATUS_OK);
    CHECK_TRUE(payload_count == 1u);
    CHECK_TRUE(payload_lengths[0] == 3u);
    CHECK_TRUE(payloads_size == 3u);
    CHECK_TRUE(memcmp(payloads, annexb + 3u, 3u) == 0);

    CHECK_TRUE(mrtc_h264_depacketize_payload(payloads, payload_lengths[0], output, &output_size, &is_start) ==
               MRTC_STATUS_OK);
    CHECK_TRUE(is_start == 1);
    CHECK_TRUE(output_size == 7u);
    CHECK_TRUE(output[0] == 0x00 && output[1] == 0x00 && output[2] == 0x00 && output[3] == 0x01);
    CHECK_TRUE(memcmp(output + 4u, annexb + 3u, 3u) == 0);
    return 0;
}

static int test_fu_a(void)
{
    static const uint8_t annexb[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01, 0x02,
                                     0x03, 0x04, 0x05, 0x06, 0x07};
    static const uint8_t expected_rebuilt[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01,
                                               0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    uint8_t payloads[64];
    size_t payload_lengths[8];
    size_t payloads_size = sizeof(payloads);
    size_t payload_count = 8;
    uint8_t rebuilt[64];
    size_t rebuilt_size = 0;
    size_t offset = 0;
    size_t i;

    CHECK_TRUE(mrtc_h264_packetize_annexb(annexb, sizeof(annexb), 5u, payloads, &payloads_size,
                                          payload_lengths, &payload_count) == MRTC_STATUS_OK);
    CHECK_TRUE(payload_count == 3u);
    CHECK_TRUE(payload_lengths[0] == 5u);
    CHECK_TRUE(payload_lengths[1] == 5u);
    CHECK_TRUE(payload_lengths[2] == 3u);
    CHECK_TRUE(payloads[0] == 0x7cu);
    CHECK_TRUE(payloads[1] == 0x85u);
    CHECK_TRUE(payloads[payload_lengths[0]] == 0x7cu);
    CHECK_TRUE(payloads[payload_lengths[0] + 1u] == 0x05u);
    CHECK_TRUE(payloads[payload_lengths[0] + payload_lengths[1]] == 0x7cu);
    CHECK_TRUE(payloads[payload_lengths[0] + payload_lengths[1] + 1u] == 0x45u);

    for (i = 0; i < payload_count; ++i) {
        size_t out_size = sizeof(rebuilt) - rebuilt_size;
        int is_start = 0;
        CHECK_TRUE(mrtc_h264_depacketize_payload(payloads + offset, payload_lengths[i], rebuilt + rebuilt_size,
                                                  &out_size, &is_start) == MRTC_STATUS_OK);
        if (i == 0u) {
            CHECK_TRUE(is_start == 1);
            CHECK_TRUE(out_size > MRTC_H264_START_CODE_SIZE);
            CHECK_TRUE(rebuilt[0] == 0x00 && rebuilt[1] == 0x00 && rebuilt[2] == 0x00 && rebuilt[3] == 0x01);
        } else {
            CHECK_TRUE(is_start == 0);
        }
        rebuilt_size += out_size;
        offset += payload_lengths[i];
    }

    CHECK_TRUE(rebuilt_size == sizeof(expected_rebuilt));
    CHECK_TRUE(memcmp(rebuilt, expected_rebuilt, sizeof(expected_rebuilt)) == 0);
    return 0;
}

static int test_invalid_input(void)
{
    static const uint8_t avcc_like[] = {0x00, 0x00, 0x00, 0x03, 0x65, 0x11, 0x22};
    uint8_t payloads[32];
    size_t payload_lengths[4];
    size_t payloads_size = sizeof(payloads);
    size_t payload_count = 4;

    CHECK_TRUE(mrtc_h264_packetize_annexb(avcc_like, sizeof(avcc_like), 1200u, payloads, &payloads_size,
                                          payload_lengths, &payload_count) == MRTC_STATUS_PARSE_ERROR);
    CHECK_TRUE(payloads_size == 0u);
    CHECK_TRUE(payload_count == 0u);
    return 0;
}

int main(void)
{
    CHECK_TRUE(test_single_nalu() == 0);
    CHECK_TRUE(test_fu_a() == 0);
    CHECK_TRUE(test_invalid_input() == 0);
    return 0;
}
