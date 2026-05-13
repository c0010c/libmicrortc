#include "rtcp_packet.h"

#include <string.h>

static uint16_t mrtc_read_u16_be(const uint8_t *bytes)
{
    return (uint16_t) (((uint16_t) bytes[0] << 8u) | (uint16_t) bytes[1]);
}

static uint32_t mrtc_read_u24_be(const uint8_t *bytes)
{
    return ((uint32_t) bytes[0] << 16u) | ((uint32_t) bytes[1] << 8u) | (uint32_t) bytes[2];
}

static uint32_t mrtc_read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t) bytes[0] << 24u) |
           ((uint32_t) bytes[1] << 16u) |
           ((uint32_t) bytes[2] << 8u) |
           (uint32_t) bytes[3];
}

static uint64_t mrtc_read_u64_be(const uint8_t *bytes)
{
    return ((uint64_t) mrtc_read_u32_be(bytes) << 32u) | (uint64_t) mrtc_read_u32_be(bytes + 4u);
}

static void mrtc_write_u16_be(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t) (value >> 8u);
    bytes[1] = (uint8_t) value;
}

static void mrtc_write_u24_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t) (value >> 16u);
    bytes[1] = (uint8_t) (value >> 8u);
    bytes[2] = (uint8_t) value;
}

static void mrtc_write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t) (value >> 24u);
    bytes[1] = (uint8_t) (value >> 16u);
    bytes[2] = (uint8_t) (value >> 8u);
    bytes[3] = (uint8_t) value;
}

static void mrtc_write_u64_be(uint8_t *bytes, uint64_t value)
{
    mrtc_write_u32_be(bytes, (uint32_t) (value >> 32u));
    mrtc_write_u32_be(bytes + 4u, (uint32_t) value);
}

static MRTC_STATUS mrtc_rtcp_write_header(uint8_t count,
                                          uint8_t packet_type,
                                          size_t packet_size,
                                          uint8_t *raw,
                                          size_t raw_capacity,
                                          size_t *raw_size)
{
    if (raw_size == 0 || packet_size < MRTC_RTCP_HEADER_SIZE || (packet_size % 4u) != 0u || count > 31u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    *raw_size = packet_size;
    if (raw == 0) {
        return MRTC_STATUS_OK;
    }
    if (raw_capacity < packet_size) {
        return MRTC_STATUS_INVALID_ARG;
    }
    raw[0] = (uint8_t) ((MRTC_RTCP_VERSION << 6u) | count);
    raw[1] = packet_type;
    mrtc_write_u16_be(raw + 2u, (uint16_t) ((packet_size / 4u) - 1u));
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_parse_header(const uint8_t *raw, size_t raw_size, MRTC_RTCP_HEADER *header)
{
    uint16_t length_words;
    size_t packet_size;

    if (raw == 0 || header == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (raw_size < MRTC_RTCP_HEADER_SIZE) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    if (((raw[0] >> 6u) & 0x03u) != MRTC_RTCP_VERSION) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    length_words = mrtc_read_u16_be(raw + 2u);
    packet_size = ((size_t) length_words + 1u) * 4u;
    if (packet_size < MRTC_RTCP_HEADER_SIZE || packet_size > raw_size) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    memset(header, 0, sizeof(*header));
    header->version = MRTC_RTCP_VERSION;
    header->count = raw[0] & 0x1fu;
    header->packet_type = raw[1];
    header->length_words = length_words;
    header->packet_size = packet_size;
    header->payload = raw + MRTC_RTCP_HEADER_SIZE;
    header->payload_size = packet_size - MRTC_RTCP_HEADER_SIZE;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_generate_sender_report(const MRTC_RTCP_SENDER_REPORT *report,
                                             uint8_t *raw,
                                             size_t raw_capacity,
                                             size_t *raw_size)
{
    MRTC_STATUS status;
    const size_t packet_size = 28u;

    if (report == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    status = mrtc_rtcp_write_header(0u, MRTC_RTCP_TYPE_SR, packet_size, raw, raw_capacity, raw_size);
    if (status != MRTC_STATUS_OK || raw == 0) {
        return status;
    }
    mrtc_write_u32_be(raw + 4u, report->sender_ssrc);
    mrtc_write_u64_be(raw + 8u, report->ntp_timestamp);
    mrtc_write_u32_be(raw + 16u, report->rtp_timestamp);
    mrtc_write_u32_be(raw + 20u, report->packet_count);
    mrtc_write_u32_be(raw + 24u, report->octet_count);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_parse_sender_report(const uint8_t *raw,
                                          size_t raw_size,
                                          MRTC_RTCP_SENDER_REPORT *report)
{
    MRTC_RTCP_HEADER header;

    if (report == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_rtcp_parse_header(raw, raw_size, &header) != MRTC_STATUS_OK ||
        header.packet_type != MRTC_RTCP_TYPE_SR || header.payload_size < 24u) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    report->sender_ssrc = mrtc_read_u32_be(header.payload);
    report->ntp_timestamp = mrtc_read_u64_be(header.payload + 4u);
    report->rtp_timestamp = mrtc_read_u32_be(header.payload + 12u);
    report->packet_count = mrtc_read_u32_be(header.payload + 16u);
    report->octet_count = mrtc_read_u32_be(header.payload + 20u);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_generate_receiver_report(const MRTC_RTCP_RECEIVER_REPORT *report,
                                               uint8_t *raw,
                                               size_t raw_capacity,
                                               size_t *raw_size)
{
    MRTC_STATUS status;
    const size_t packet_size = 32u;

    if (report == 0 || report->cumulative_lost > 0x00ffffffu) {
        return MRTC_STATUS_INVALID_ARG;
    }
    status = mrtc_rtcp_write_header(1u, MRTC_RTCP_TYPE_RR, packet_size, raw, raw_capacity, raw_size);
    if (status != MRTC_STATUS_OK || raw == 0) {
        return status;
    }
    mrtc_write_u32_be(raw + 4u, report->sender_ssrc);
    mrtc_write_u32_be(raw + 8u, report->report_ssrc);
    raw[12] = report->fraction_lost;
    mrtc_write_u24_be(raw + 13u, report->cumulative_lost);
    mrtc_write_u32_be(raw + 16u, report->highest_sequence_number);
    mrtc_write_u32_be(raw + 20u, report->jitter);
    mrtc_write_u32_be(raw + 24u, report->last_sender_report);
    mrtc_write_u32_be(raw + 28u, report->delay_since_last_sender_report);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_parse_receiver_report(const uint8_t *raw,
                                            size_t raw_size,
                                            MRTC_RTCP_RECEIVER_REPORT *report)
{
    MRTC_RTCP_HEADER header;

    if (report == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_rtcp_parse_header(raw, raw_size, &header) != MRTC_STATUS_OK ||
        header.packet_type != MRTC_RTCP_TYPE_RR || header.count < 1u || header.payload_size < 28u) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    report->sender_ssrc = mrtc_read_u32_be(header.payload);
    report->report_ssrc = mrtc_read_u32_be(header.payload + 4u);
    report->fraction_lost = header.payload[8];
    report->cumulative_lost = mrtc_read_u24_be(header.payload + 9u);
    report->highest_sequence_number = mrtc_read_u32_be(header.payload + 12u);
    report->jitter = mrtc_read_u32_be(header.payload + 16u);
    report->last_sender_report = mrtc_read_u32_be(header.payload + 20u);
    report->delay_since_last_sender_report = mrtc_read_u32_be(header.payload + 24u);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_parse_nack(const uint8_t *raw,
                                 size_t raw_size,
                                 uint32_t *sender_ssrc,
                                 uint32_t *media_ssrc,
                                 uint16_t *sequence_numbers,
                                 size_t *sequence_number_count)
{
    MRTC_RTCP_HEADER header;
    size_t capacity;
    size_t count = 0;
    size_t offset;

    if (sender_ssrc == 0 || media_ssrc == 0 || sequence_number_count == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_rtcp_parse_header(raw, raw_size, &header) != MRTC_STATUS_OK ||
        header.packet_type != MRTC_RTCP_TYPE_RTPFB || header.count != MRTC_RTCP_FMT_NACK ||
        header.payload_size < 12u || ((header.payload_size - 8u) % 4u) != 0u) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    capacity = *sequence_number_count;
    *sender_ssrc = mrtc_read_u32_be(header.payload);
    *media_ssrc = mrtc_read_u32_be(header.payload + 4u);
    for (offset = 8u; offset < header.payload_size; offset += 4u) {
        uint16_t pid = mrtc_read_u16_be(header.payload + offset);
        uint16_t blp = mrtc_read_u16_be(header.payload + offset + 2u);
        unsigned int bit;

        if (sequence_numbers != 0 && count < capacity) {
            sequence_numbers[count] = pid;
        }
        ++count;
        for (bit = 0; bit < 16u; ++bit) {
            if ((blp & (uint16_t) (1u << bit)) != 0u) {
                if (sequence_numbers != 0 && count < capacity) {
                    sequence_numbers[count] = (uint16_t) (pid + bit + 1u);
                }
                ++count;
            }
        }
    }
    *sequence_number_count = count;
    return (sequence_numbers != 0 && capacity < count) ? MRTC_STATUS_INVALID_ARG : MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_generate_pli(uint32_t sender_ssrc,
                                   uint32_t media_ssrc,
                                   uint8_t *raw,
                                   size_t raw_capacity,
                                   size_t *raw_size)
{
    MRTC_STATUS status;
    const size_t packet_size = 12u;

    status = mrtc_rtcp_write_header(MRTC_RTCP_FMT_PLI, MRTC_RTCP_TYPE_PSFB, packet_size, raw, raw_capacity, raw_size);
    if (status != MRTC_STATUS_OK || raw == 0) {
        return status;
    }
    mrtc_write_u32_be(raw + 4u, sender_ssrc);
    mrtc_write_u32_be(raw + 8u, media_ssrc);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtcp_parse_pli(const uint8_t *raw,
                                size_t raw_size,
                                uint32_t *sender_ssrc,
                                uint32_t *media_ssrc)
{
    MRTC_RTCP_HEADER header;

    if (sender_ssrc == 0 || media_ssrc == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_rtcp_parse_header(raw, raw_size, &header) != MRTC_STATUS_OK ||
        header.packet_type != MRTC_RTCP_TYPE_PSFB || header.count != MRTC_RTCP_FMT_PLI ||
        header.payload_size != 8u) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    *sender_ssrc = mrtc_read_u32_be(header.payload);
    *media_ssrc = mrtc_read_u32_be(header.payload + 4u);
    return MRTC_STATUS_OK;
}
