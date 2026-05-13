#include "rtp_packet.h"

#include <string.h>

static uint16_t mrtc_read_u16_be(const uint8_t *bytes)
{
    return (uint16_t) (((uint16_t) bytes[0] << 8u) | (uint16_t) bytes[1]);
}

static uint32_t mrtc_read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t) bytes[0] << 24u) |
           ((uint32_t) bytes[1] << 16u) |
           ((uint32_t) bytes[2] << 8u) |
           (uint32_t) bytes[3];
}

static void mrtc_write_u16_be(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t) (value >> 8u);
    bytes[1] = (uint8_t) value;
}

static void mrtc_write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t) (value >> 24u);
    bytes[1] = (uint8_t) (value >> 16u);
    bytes[2] = (uint8_t) (value >> 8u);
    bytes[3] = (uint8_t) value;
}

MRTC_STATUS mrtc_rtp_packet_build(MRTC_RTP_PACKET *packet,
                                  uint8_t marker,
                                  uint8_t payload_type,
                                  uint16_t sequence_number,
                                  uint32_t timestamp,
                                  uint32_t ssrc,
                                  const uint8_t *payload,
                                  size_t payload_size)
{
    if (packet == 0 || (payload == 0 && payload_size > 0) || payload_type > 127u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(packet, 0, sizeof(*packet));
    packet->version = MRTC_RTP_VERSION;
    packet->marker = marker != 0;
    packet->payload_type = payload_type;
    packet->sequence_number = sequence_number;
    packet->timestamp = timestamp;
    packet->ssrc = ssrc;
    packet->payload = payload;
    packet->payload_size = payload_size;
    packet->header_size = MRTC_RTP_FIXED_HEADER_SIZE;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtp_packet_parse(const uint8_t *raw, size_t raw_size, MRTC_RTP_PACKET *packet)
{
    uint8_t first;
    uint8_t padding_len = 0;
    size_t header_size;
    size_t payload_size;
    uint8_t csrc_count;
    uint8_t has_extension;
    uint8_t has_padding;

    if (raw == 0 || packet == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (raw_size < MRTC_RTP_FIXED_HEADER_SIZE) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    first = raw[0];
    if (((first >> 6u) & 0x03u) != MRTC_RTP_VERSION) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    csrc_count = first & 0x0fu;
    has_padding = (uint8_t) ((first & 0x20u) != 0u);
    has_extension = (uint8_t) ((first & 0x10u) != 0u);
    header_size = MRTC_RTP_FIXED_HEADER_SIZE + ((size_t) csrc_count * 4u);
    if (raw_size < header_size) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    if (has_extension) {
        uint16_t extension_words;
        if (raw_size < header_size + 4u) {
            return MRTC_STATUS_PARSE_ERROR;
        }
        extension_words = mrtc_read_u16_be(raw + header_size + 2u);
        header_size += 4u + ((size_t) extension_words * 4u);
        if (raw_size < header_size) {
            return MRTC_STATUS_PARSE_ERROR;
        }
    }

    if (has_padding) {
        padding_len = raw[raw_size - 1u];
        if (padding_len == 0u || (size_t) padding_len > raw_size - header_size) {
            return MRTC_STATUS_PARSE_ERROR;
        }
    }

    payload_size = raw_size - header_size - (size_t) padding_len;
    memset(packet, 0, sizeof(*packet));
    packet->version = MRTC_RTP_VERSION;
    packet->marker = (uint8_t) ((raw[1] & 0x80u) != 0u);
    packet->payload_type = raw[1] & 0x7fu;
    packet->sequence_number = mrtc_read_u16_be(raw + 2u);
    packet->timestamp = mrtc_read_u32_be(raw + 4u);
    packet->ssrc = mrtc_read_u32_be(raw + 8u);
    packet->payload = raw + header_size;
    packet->payload_size = payload_size;
    packet->raw = raw;
    packet->raw_size = raw_size;
    packet->csrc_count = csrc_count;
    packet->has_extension = has_extension;
    packet->has_padding = has_padding;
    packet->header_size = header_size;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_rtp_packet_serialize(const MRTC_RTP_PACKET *packet,
                                      uint8_t *raw,
                                      size_t raw_capacity,
                                      size_t *raw_size)
{
    size_t required;

    if (packet == 0 || raw_size == 0 || (packet->payload == 0 && packet->payload_size > 0)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (packet->version != MRTC_RTP_VERSION || packet->payload_type > 127u ||
        packet->csrc_count != 0u || packet->has_extension != 0u || packet->has_padding != 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    required = mrtc_rtp_packet_serialized_size(packet);
    *raw_size = required;
    if (raw == 0) {
        return MRTC_STATUS_OK;
    }
    if (raw_capacity < required) {
        return MRTC_STATUS_INVALID_ARG;
    }

    raw[0] = (uint8_t) (MRTC_RTP_VERSION << 6u);
    raw[1] = packet->payload_type & 0x7fu;
    if (packet->marker != 0u) {
        raw[1] |= 0x80u;
    }
    mrtc_write_u16_be(raw + 2u, packet->sequence_number);
    mrtc_write_u32_be(raw + 4u, packet->timestamp);
    mrtc_write_u32_be(raw + 8u, packet->ssrc);
    if (packet->payload_size > 0) {
        memcpy(raw + MRTC_RTP_FIXED_HEADER_SIZE, packet->payload, packet->payload_size);
    }
    return MRTC_STATUS_OK;
}

size_t mrtc_rtp_packet_header_size(const MRTC_RTP_PACKET *packet)
{
    if (packet == 0) {
        return 0;
    }
    return packet->header_size == 0 ? MRTC_RTP_FIXED_HEADER_SIZE : packet->header_size;
}

size_t mrtc_rtp_packet_serialized_size(const MRTC_RTP_PACKET *packet)
{
    if (packet == 0) {
        return 0;
    }
    return MRTC_RTP_FIXED_HEADER_SIZE + packet->payload_size;
}

uint16_t mrtc_rtp_sequence_next(uint16_t sequence_number)
{
    return (uint16_t) (sequence_number + 1u);
}
