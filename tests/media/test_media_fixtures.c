#include "rtp/codecs/h264.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_TRUE(expr)                                                                                           \
    do {                                                                                                            \
        if (!(expr)) {                                                                                             \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #expr);                           \
            return 1;                                                                                              \
        }                                                                                                           \
    } while (0)

static int read_file(const char *path, unsigned char **data, size_t *size)
{
    FILE *file;
    long file_size;
    unsigned char *buffer;
    size_t read_size;

    if (path == 0 || data == 0 || size == 0) {
        return 0;
    }

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

    *data = buffer;
    *size = read_size;
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

static int test_h264_fixture(void)
{
    unsigned char *annexb = 0;
    size_t annexb_size = 0;
    size_t offset = 0;
    int saw_sps = 0;
    int saw_pps = 0;
    int saw_idr = 0;
    uint8_t *payloads = 0;
    size_t *payload_lengths = 0;
    size_t payloads_size = 0;
    size_t payload_count = 0;

    CHECK_TRUE(read_file("tests/fixtures/h264_annexb_sample.h264", &annexb, &annexb_size));
    CHECK_TRUE(annexb_size > 0u);

    while (offset < annexb_size) {
        int start_code_size = h264_start_code_size_at(annexb, annexb_size, offset);
        size_t nalu_start;
        size_t next_start;
        unsigned int nalu_type;

        CHECK_TRUE(start_code_size != 0);
        nalu_start = offset + (size_t) start_code_size;
        CHECK_TRUE(nalu_start < annexb_size);
        next_start = nalu_start + 1u;
        while (next_start < annexb_size && h264_start_code_size_at(annexb, annexb_size, next_start) == 0) {
            ++next_start;
        }

        nalu_type = annexb[nalu_start] & MRTC_H264_NAL_TYPE_MASK;
        if (nalu_type == 7u) {
            CHECK_TRUE(!saw_idr);
            saw_sps = 1;
        } else if (nalu_type == 8u) {
            CHECK_TRUE(saw_sps);
            CHECK_TRUE(!saw_idr);
            saw_pps = 1;
        } else if (nalu_type == 5u) {
            CHECK_TRUE(saw_sps);
            CHECK_TRUE(saw_pps);
            saw_idr = 1;
        }

        offset = next_start;
    }

    CHECK_TRUE(saw_sps);
    CHECK_TRUE(saw_pps);
    CHECK_TRUE(saw_idr);
    CHECK_TRUE(mrtc_h264_packetize_annexb(annexb, annexb_size, 1200u, 0, &payloads_size,
                                          0, &payload_count) == MRTC_STATUS_OK);
    payloads = (uint8_t *) malloc(payloads_size);
    payload_lengths = (size_t *) calloc(payload_count, sizeof(*payload_lengths));
    CHECK_TRUE(payloads != 0);
    CHECK_TRUE(payload_lengths != 0);
    CHECK_TRUE(mrtc_h264_packetize_annexb(annexb, annexb_size, 1200u, payloads, &payloads_size,
                                          payload_lengths, &payload_count) == MRTC_STATUS_OK);
    CHECK_TRUE(payload_count >= 3u);
    CHECK_TRUE(payloads_size > 0u);

    free(payload_lengths);
    free(payloads);
    free(annexb);
    return 0;
}

static int test_opus_fixture(void)
{
    unsigned char *raw = 0;
    size_t raw_size = 0;
    size_t offset = 0;
    size_t packet_count = 0;

    CHECK_TRUE(read_file("tests/fixtures/opus_packets.bin", &raw, &raw_size));
    CHECK_TRUE(raw_size > 2u);

    while (offset < raw_size) {
        size_t packet_len;

        CHECK_TRUE(offset + 2u <= raw_size);
        packet_len = ((size_t) raw[offset] << 8u) | (size_t) raw[offset + 1u];
        offset += 2u;
        CHECK_TRUE(packet_len > 0u);
        CHECK_TRUE(offset + packet_len <= raw_size);
        CHECK_TRUE(raw[offset] != 0u || packet_len > 1u);
        offset += packet_len;
        ++packet_count;
    }

    CHECK_TRUE(offset == raw_size);
    CHECK_TRUE(packet_count >= 2u);

    free(raw);
    return 0;
}

int main(void)
{
    CHECK_TRUE(test_h264_fixture() == 0);
    CHECK_TRUE(test_opus_fixture() == 0);
    return 0;
}
