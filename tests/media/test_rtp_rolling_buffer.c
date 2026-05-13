#include "rtcp/rtp_rolling_buffer.h"

#include <string.h>

#define CHECK_TRUE(expr) do { if (!(expr)) { return 1; } } while (0)

int main(void)
{
    MRTC_RTP_ROLLING_BUFFER *buffer = 0;
    const uint8_t packet_a[] = {0x80, 0x60, 0x00, 0x0a, 0xaa};
    const uint8_t packet_b[] = {0x80, 0x60, 0x00, 0x0b, 0xbb, 0xbb};
    const uint8_t packet_c[] = {0x80, 0x60, 0x00, 0x0c, 0xcc, 0xcc, 0xcc};
    const uint8_t packet_d[] = {0x80, 0x60, 0x00, 0x0d, 0xdd};
    const uint8_t packet_b2[] = {0x80, 0x60, 0x00, 0x0b, 0x22};
    const uint8_t *found = 0;
    size_t found_size = 0;
    uint8_t copy[16];

    CHECK_TRUE(mrtc_rtp_rolling_buffer_create(3u, &buffer) == MRTC_STATUS_OK);
    CHECK_TRUE(buffer != 0);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_add(buffer, 10u, packet_a, sizeof(packet_a)) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_add(buffer, 11u, packet_b, sizeof(packet_b)) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_add(buffer, 12u, packet_c, sizeof(packet_c)) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_size(buffer) == 3u);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_lookup(buffer, 11u, &found, &found_size) == MRTC_STATUS_OK);
    CHECK_TRUE(found_size == sizeof(packet_b));
    CHECK_TRUE(memcmp(found, packet_b, sizeof(packet_b)) == 0);

    CHECK_TRUE(mrtc_rtp_rolling_buffer_add(buffer, 13u, packet_d, sizeof(packet_d)) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_size(buffer) == 3u);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_lookup(buffer, 10u, &found, &found_size) == MRTC_STATUS_INVALID_STATE);
    CHECK_TRUE(found == 0);
    CHECK_TRUE(found_size == 0u);

    found_size = sizeof(copy);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_copy(buffer, 12u, copy, sizeof(copy), &found_size) == MRTC_STATUS_OK);
    CHECK_TRUE(found_size == sizeof(packet_c));
    CHECK_TRUE(memcmp(copy, packet_c, sizeof(packet_c)) == 0);

    CHECK_TRUE(mrtc_rtp_rolling_buffer_add(buffer, 11u, packet_b2, sizeof(packet_b2)) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_size(buffer) == 3u);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_lookup(buffer, 11u, &found, &found_size) == MRTC_STATUS_OK);
    CHECK_TRUE(found_size == sizeof(packet_b2));
    CHECK_TRUE(memcmp(found, packet_b2, sizeof(packet_b2)) == 0);

    CHECK_TRUE(mrtc_rtp_rolling_buffer_remove(buffer, 11u) == MRTC_STATUS_OK);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_lookup(buffer, 11u, &found, &found_size) == MRTC_STATUS_INVALID_STATE);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_remove(buffer, 42u) == MRTC_STATUS_INVALID_STATE);
    CHECK_TRUE(mrtc_rtp_rolling_buffer_size(buffer) == 2u);

    mrtc_rtp_rolling_buffer_free(buffer);
    return 0;
}
