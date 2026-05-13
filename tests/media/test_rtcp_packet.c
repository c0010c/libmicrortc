#include "rtcp/rtcp_packet.h"

#include <stdio.h>

#define CHECK_TRUE(condition)                                                                                       \
    do {                                                                                                            \
        if (!(condition)) {                                                                                         \
            fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition);                       \
            return 1;                                                                                               \
        }                                                                                                           \
    } while (0)

static int test_sr_rr(void)
{
    uint8_t raw[64];
    size_t raw_size = sizeof(raw);
    MRTC_RTCP_HEADER header;
    MRTC_RTCP_SENDER_REPORT sr;
    MRTC_RTCP_SENDER_REPORT parsed_sr;
    MRTC_RTCP_RECEIVER_REPORT rr;
    MRTC_RTCP_RECEIVER_REPORT parsed_rr;

    sr.sender_ssrc = 0x01020304u;
    sr.ntp_timestamp = 0x0102030405060708ull;
    sr.rtp_timestamp = 0x10203040u;
    sr.packet_count = 33u;
    sr.octet_count = 44u;
    CHECK_TRUE(mrtc_rtcp_generate_sender_report(&sr, raw, sizeof(raw), &raw_size) == MRTC_STATUS_OK);
    CHECK_TRUE(raw_size == 28u);
    CHECK_TRUE(mrtc_rtcp_parse_header(raw, raw_size, &header) == MRTC_STATUS_OK);
    CHECK_TRUE(header.packet_type == MRTC_RTCP_TYPE_SR);
    CHECK_TRUE(mrtc_rtcp_parse_sender_report(raw, raw_size, &parsed_sr) == MRTC_STATUS_OK);
    CHECK_TRUE(parsed_sr.sender_ssrc == sr.sender_ssrc);
    CHECK_TRUE(parsed_sr.ntp_timestamp == sr.ntp_timestamp);
    CHECK_TRUE(parsed_sr.rtp_timestamp == sr.rtp_timestamp);
    CHECK_TRUE(parsed_sr.packet_count == sr.packet_count);
    CHECK_TRUE(parsed_sr.octet_count == sr.octet_count);

    rr.sender_ssrc = 0x11111111u;
    rr.report_ssrc = 0x22222222u;
    rr.fraction_lost = 3u;
    rr.cumulative_lost = 0x00010203u;
    rr.highest_sequence_number = 0x33333333u;
    rr.jitter = 0x44444444u;
    rr.last_sender_report = 0x55555555u;
    rr.delay_since_last_sender_report = 0x66666666u;
    raw_size = sizeof(raw);
    CHECK_TRUE(mrtc_rtcp_generate_receiver_report(&rr, raw, sizeof(raw), &raw_size) == MRTC_STATUS_OK);
    CHECK_TRUE(raw_size == 32u);
    CHECK_TRUE(mrtc_rtcp_parse_header(raw, raw_size, &header) == MRTC_STATUS_OK);
    CHECK_TRUE(header.packet_type == MRTC_RTCP_TYPE_RR);
    CHECK_TRUE(mrtc_rtcp_parse_receiver_report(raw, raw_size, &parsed_rr) == MRTC_STATUS_OK);
    CHECK_TRUE(parsed_rr.sender_ssrc == rr.sender_ssrc);
    CHECK_TRUE(parsed_rr.report_ssrc == rr.report_ssrc);
    CHECK_TRUE(parsed_rr.fraction_lost == rr.fraction_lost);
    CHECK_TRUE(parsed_rr.cumulative_lost == rr.cumulative_lost);
    CHECK_TRUE(parsed_rr.delay_since_last_sender_report == rr.delay_since_last_sender_report);
    return 0;
}

static int test_nack_pli(void)
{
    static const uint8_t nack[] = {
        0x81, 0xcd, 0x00, 0x03,
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x00, 0x64, 0x00, 0x05};
    uint16_t sequence_numbers[8];
    size_t sequence_number_count = 8;
    uint32_t sender_ssrc = 0;
    uint32_t media_ssrc = 0;
    uint8_t pli[16];
    size_t pli_size = sizeof(pli);

    CHECK_TRUE(mrtc_rtcp_parse_nack(nack, sizeof(nack), &sender_ssrc, &media_ssrc,
                                    sequence_numbers, &sequence_number_count) == MRTC_STATUS_OK);
    CHECK_TRUE(sender_ssrc == 0x01020304u);
    CHECK_TRUE(media_ssrc == 0x05060708u);
    CHECK_TRUE(sequence_number_count == 3u);
    CHECK_TRUE(sequence_numbers[0] == 100u);
    CHECK_TRUE(sequence_numbers[1] == 101u);
    CHECK_TRUE(sequence_numbers[2] == 103u);

    CHECK_TRUE(mrtc_rtcp_generate_pli(0x0a0b0c0du, 0x01020304u, pli, sizeof(pli), &pli_size) == MRTC_STATUS_OK);
    CHECK_TRUE(pli_size == 12u);
    CHECK_TRUE(mrtc_rtcp_parse_pli(pli, pli_size, &sender_ssrc, &media_ssrc) == MRTC_STATUS_OK);
    CHECK_TRUE(sender_ssrc == 0x0a0b0c0du);
    CHECK_TRUE(media_ssrc == 0x01020304u);
    return 0;
}

int main(void)
{
    CHECK_TRUE(test_sr_rr() == 0);
    CHECK_TRUE(test_nack_pli() == 0);
    return 0;
}
