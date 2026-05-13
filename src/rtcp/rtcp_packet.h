#ifndef MRTC_RTCP_PACKET_H
#define MRTC_RTCP_PACKET_H

#include <micrortc/peer_connection.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MRTC_RTCP_VERSION 2u
#define MRTC_RTCP_HEADER_SIZE 4u
#define MRTC_RTCP_TYPE_SR 200u
#define MRTC_RTCP_TYPE_RR 201u
#define MRTC_RTCP_TYPE_RTPFB 205u
#define MRTC_RTCP_TYPE_PSFB 206u
#define MRTC_RTCP_FMT_NACK 1u
#define MRTC_RTCP_FMT_PLI 1u

typedef struct MRTC_RTCP_HEADER {
    uint8_t version;
    uint8_t count;
    uint8_t packet_type;
    uint16_t length_words;
    size_t packet_size;
    const uint8_t *payload;
    size_t payload_size;
} MRTC_RTCP_HEADER;

typedef struct MRTC_RTCP_SENDER_REPORT {
    uint32_t sender_ssrc;
    uint64_t ntp_timestamp;
    uint32_t rtp_timestamp;
    uint32_t packet_count;
    uint32_t octet_count;
} MRTC_RTCP_SENDER_REPORT;

typedef struct MRTC_RTCP_RECEIVER_REPORT {
    uint32_t sender_ssrc;
    uint32_t report_ssrc;
    uint8_t fraction_lost;
    uint32_t cumulative_lost;
    uint32_t highest_sequence_number;
    uint32_t jitter;
    uint32_t last_sender_report;
    uint32_t delay_since_last_sender_report;
} MRTC_RTCP_RECEIVER_REPORT;

MRTC_STATUS mrtc_rtcp_parse_header(const uint8_t *raw, size_t raw_size, MRTC_RTCP_HEADER *header);

MRTC_STATUS mrtc_rtcp_generate_sender_report(const MRTC_RTCP_SENDER_REPORT *report,
                                             uint8_t *raw,
                                             size_t raw_capacity,
                                             size_t *raw_size);
MRTC_STATUS mrtc_rtcp_parse_sender_report(const uint8_t *raw,
                                          size_t raw_size,
                                          MRTC_RTCP_SENDER_REPORT *report);

MRTC_STATUS mrtc_rtcp_generate_receiver_report(const MRTC_RTCP_RECEIVER_REPORT *report,
                                               uint8_t *raw,
                                               size_t raw_capacity,
                                               size_t *raw_size);
MRTC_STATUS mrtc_rtcp_parse_receiver_report(const uint8_t *raw,
                                            size_t raw_size,
                                            MRTC_RTCP_RECEIVER_REPORT *report);

MRTC_STATUS mrtc_rtcp_parse_nack(const uint8_t *raw,
                                 size_t raw_size,
                                 uint32_t *sender_ssrc,
                                 uint32_t *media_ssrc,
                                 uint16_t *sequence_numbers,
                                 size_t *sequence_number_count);

MRTC_STATUS mrtc_rtcp_generate_pli(uint32_t sender_ssrc,
                                   uint32_t media_ssrc,
                                   uint8_t *raw,
                                   size_t raw_capacity,
                                   size_t *raw_size);
MRTC_STATUS mrtc_rtcp_parse_pli(const uint8_t *raw,
                                size_t raw_size,
                                uint32_t *sender_ssrc,
                                uint32_t *media_ssrc);

#ifdef __cplusplus
}
#endif

#endif /* MRTC_RTCP_PACKET_H */
