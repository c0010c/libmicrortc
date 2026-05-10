#include "rtp/rtp.h"

#include <string.h>

static void rtc_write_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)(value & 0xffu);
}

static void rtc_write_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)((value >> 16) & 0xffu);
    out[2] = (uint8_t)((value >> 8) & 0xffu);
    out[3] = (uint8_t)(value & 0xffu);
}

static uint16_t rtc_read_u16(const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

static uint32_t rtc_read_u32(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | in[3];
}

rtc_status_t rtc_rtp_write_header(uint8_t *out, size_t capacity, int marker,
                                  uint8_t payload_type, uint16_t sequence,
                                  uint32_t timestamp, uint32_t ssrc)
{
    if (out == 0 || capacity < RTC_RTP_HEADER_BYTES ||
        payload_type > 127u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    out[0] = (uint8_t)(RTC_RTP_VERSION << 6);
    out[1] = (uint8_t)((marker ? 0x80u : 0u) | payload_type);
    rtc_write_u16(out + 2, sequence);
    rtc_write_u32(out + 4, timestamp);
    rtc_write_u32(out + 8, ssrc);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtp_parse_header(const uint8_t *packet, size_t packet_len,
                                  rtc_rtp_header_t *out_header)
{
    size_t csrc_count;
    size_t header_len;

    if (packet == 0 || out_header == 0 ||
        packet_len < RTC_RTP_HEADER_BYTES) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if ((packet[0] >> 6) != RTC_RTP_VERSION) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if ((packet[0] & 0x10u) != 0u) {
        return RTC_STATUS_UNSUPPORTED;
    }
    csrc_count = packet[0] & 0x0fu;
    header_len = RTC_RTP_HEADER_BYTES + csrc_count * 4u;
    if (packet_len < header_len || packet_len == header_len) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    out_header->payload_type = (uint8_t)(packet[1] & 0x7fu);
    out_header->marker = (packet[1] & 0x80u) != 0u;
    out_header->sequence = rtc_read_u16(packet + 2);
    out_header->timestamp = rtc_read_u32(packet + 4);
    out_header->ssrc = rtc_read_u32(packet + 8);
    out_header->header_len = header_len;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtp_packetize_opus(const rtc_media_frame_t *frame,
                                    rtc_rtp_packetizer_state_t *state,
                                    size_t max_payload_bytes, uint8_t *out,
                                    size_t capacity, size_t *out_len)
{
    uint32_t timestamp;
    rtc_status_t status;

    if (frame == 0 || state == 0 || out == 0 || out_len == 0 ||
        frame->data == 0 || frame->data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (frame->data_len > max_payload_bytes ||
        capacity < RTC_RTP_HEADER_BYTES + frame->data_len) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    timestamp = frame->timestamp != 0 ? frame->timestamp : state->timestamp;
    status = rtc_rtp_write_header(out, capacity, 0, RTC_RTP_PAYLOAD_TYPE_OPUS,
                                  state->sequence, timestamp, state->ssrc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    memcpy(out + RTC_RTP_HEADER_BYTES, frame->data, frame->data_len);
    *out_len = RTC_RTP_HEADER_BYTES + frame->data_len;
    state->sequence = (uint16_t)(state->sequence + 1u);
    state->timestamp = timestamp + RTC_RTP_OPUS_CLOCK_INCREMENT;
    return RTC_STATUS_OK;
}

static int rtc_h264_start_code_len(const uint8_t *data, size_t len,
                                   size_t offset, size_t *out_len)
{
    if (offset + 3u <= len && data[offset] == 0x00 &&
        data[offset + 1u] == 0x00 && data[offset + 2u] == 0x01) {
        *out_len = 3;
        return 1;
    }
    if (offset + 4u <= len && data[offset] == 0x00 &&
        data[offset + 1u] == 0x00 && data[offset + 2u] == 0x00 &&
        data[offset + 3u] == 0x01) {
        *out_len = 4;
        return 1;
    }
    return 0;
}

static int rtc_h264_find_start_code(const uint8_t *data, size_t len,
                                    size_t offset, size_t *out_offset,
                                    size_t *out_start_code_len)
{
    size_t i;

    for (i = offset; i < len; ++i) {
        if (rtc_h264_start_code_len(data, len, i, out_start_code_len)) {
            *out_offset = i;
            return 1;
        }
    }
    return 0;
}

static rtc_status_t rtc_h264_write_packet(rtc_rtp_packet_buffer_t *packet,
                                          uint16_t sequence,
                                          uint32_t timestamp, uint32_t ssrc,
                                          int marker, const uint8_t *payload,
                                          size_t payload_len)
{
    rtc_status_t status;

    if (packet == 0 || packet->data == 0 || payload == 0 ||
        packet->capacity < RTC_RTP_HEADER_BYTES + payload_len) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    status = rtc_rtp_write_header(packet->data, packet->capacity, marker,
                                  RTC_RTP_PAYLOAD_TYPE_H264, sequence,
                                  timestamp, ssrc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    memcpy(packet->data + RTC_RTP_HEADER_BYTES, payload, payload_len);
    packet->len = RTC_RTP_HEADER_BYTES + payload_len;
    packet->sequence = sequence;
    packet->timestamp = timestamp;
    packet->marker = marker;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_h264_emit_packet(rtc_rtp_packet_buffer_t *packets,
                                         size_t packet_capacity,
                                         size_t *packet_count,
                                         uint16_t *sequence,
                                         uint32_t timestamp, uint32_t ssrc,
                                         int marker, const uint8_t *payload,
                                         size_t payload_len)
{
    rtc_status_t status;

    if (*packet_count >= packet_capacity) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    status = rtc_h264_write_packet(&packets[*packet_count], *sequence,
                                   timestamp, ssrc, marker, payload,
                                   payload_len);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    *sequence = (uint16_t)(*sequence + 1u);
    (*packet_count)++;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_h264_emit_fua(rtc_rtp_packet_buffer_t *packets,
                                      size_t packet_capacity,
                                      size_t *packet_count,
                                      uint16_t *sequence, uint32_t timestamp,
                                      uint32_t ssrc, int marker,
                                      const uint8_t *nalu, size_t nalu_len,
                                      size_t max_payload_bytes)
{
    uint8_t nalu_header;
    uint8_t nri;
    uint8_t nalu_type;
    size_t fragment_capacity;
    size_t offset;

    if (max_payload_bytes <= 2u) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    nalu_header = nalu[0];
    nri = (uint8_t)(nalu_header & 0x60u);
    nalu_type = (uint8_t)(nalu_header & 0x1fu);
    fragment_capacity = max_payload_bytes - 2u;
    offset = 1u;

    while (offset < nalu_len) {
        size_t fragment_len = nalu_len - offset;
        int start = offset == 1u;
        int end;
        rtc_status_t status;
        rtc_rtp_packet_buffer_t *packet;

        if (fragment_len > fragment_capacity) {
            fragment_len = fragment_capacity;
        }
        end = offset + fragment_len >= nalu_len;
        if (*packet_count >= packet_capacity) {
            return RTC_STATUS_CAPACITY_PACKET_CACHE;
        }
        packet = &packets[*packet_count];
        if (packet->data == 0 ||
            packet->capacity < RTC_RTP_HEADER_BYTES + fragment_len + 2u) {
            return RTC_STATUS_CAPACITY_PACKET_CACHE;
        }
        status = rtc_rtp_write_header(packet->data, packet->capacity,
                                      end && marker, RTC_RTP_PAYLOAD_TYPE_H264,
                                      *sequence, timestamp, ssrc);
        if (status != RTC_STATUS_OK) {
            return status;
        }
        packet->data[RTC_RTP_HEADER_BYTES] = (uint8_t)(nri | 28u);
        packet->data[RTC_RTP_HEADER_BYTES + 1u] =
            (uint8_t)((start ? 0x80u : 0u) | (end ? 0x40u : 0u) |
                      nalu_type);
        memcpy(packet->data + RTC_RTP_HEADER_BYTES + 2u, nalu + offset,
               fragment_len);
        packet->len = RTC_RTP_HEADER_BYTES + fragment_len + 2u;
        packet->sequence = *sequence;
        packet->timestamp = timestamp;
        packet->marker = end && marker;
        *sequence = (uint16_t)(*sequence + 1u);
        (*packet_count)++;
        offset += fragment_len;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_h264_count_nalu_packets(size_t nalu_len,
                                                size_t max_payload_bytes,
                                                size_t *inout_count)
{
    size_t payload_bytes;
    size_t fragment_capacity;
    size_t fragments;

    if (nalu_len == 0) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (nalu_len <= max_payload_bytes) {
        (*inout_count)++;
        return RTC_STATUS_OK;
    }
    if (max_payload_bytes <= 2u) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    payload_bytes = nalu_len - 1u;
    fragment_capacity = max_payload_bytes - 2u;
    fragments = (payload_bytes + fragment_capacity - 1u) / fragment_capacity;
    *inout_count += fragments;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtp_packetize_h264(const rtc_media_frame_t *frame,
                                    rtc_rtp_packetizer_state_t *state,
                                    size_t max_payload_bytes,
                                    size_t max_packets_per_frame,
                                    rtc_rtp_packet_buffer_t *packets,
                                    size_t *inout_packet_count)
{
    size_t packet_capacity;
    size_t packet_count = 0;
    size_t start_code;
    size_t start_code_len;
    size_t offset;
    uint16_t sequence;
    uint32_t timestamp;

    if (frame == 0 || state == 0 || packets == 0 ||
        inout_packet_count == 0 || frame->data == 0 || frame->data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (max_payload_bytes == 0 || max_packets_per_frame == 0) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    packet_capacity = *inout_packet_count;
    if (packet_capacity == 0 || packet_capacity > max_packets_per_frame) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    if (!rtc_h264_find_start_code(frame->data, frame->data_len, 0,
                                  &start_code, &start_code_len) ||
        start_code != 0) {
        return RTC_STATUS_UNSUPPORTED;
    }

    offset = start_code + start_code_len;
    while (offset < frame->data_len) {
        size_t next_start;
        size_t next_start_len = 0;
        size_t nalu_end = frame->data_len;
        size_t nalu_len;
        rtc_status_t status;

        if (rtc_h264_find_start_code(frame->data, frame->data_len, offset,
                                     &next_start, &next_start_len)) {
            nalu_end = next_start;
        }
        nalu_len = nalu_end - offset;
        status = rtc_h264_count_nalu_packets(nalu_len, max_payload_bytes,
                                             &packet_count);
        if (status != RTC_STATUS_OK) {
            return status;
        }
        if (packet_count > max_packets_per_frame ||
            packet_count > packet_capacity) {
            return RTC_STATUS_CAPACITY_PACKET_CACHE;
        }
        if (nalu_end == frame->data_len) {
            break;
        }
        offset = nalu_end + next_start_len;
    }

    packet_count = 0;
    sequence = state->sequence;
    timestamp = frame->timestamp != 0 ? frame->timestamp : state->timestamp;
    offset = start_code + start_code_len;
    while (offset < frame->data_len) {
        size_t next_start;
        size_t next_start_len = 0;
        size_t nalu_end = frame->data_len;
        size_t nalu_len;
        int last_nalu;
        rtc_status_t status;

        if (rtc_h264_find_start_code(frame->data, frame->data_len, offset,
                                     &next_start, &next_start_len)) {
            nalu_end = next_start;
        }
        nalu_len = nalu_end - offset;
        last_nalu = nalu_end == frame->data_len;
        if (nalu_len <= max_payload_bytes) {
            status = rtc_h264_emit_packet(
                packets, packet_capacity, &packet_count, &sequence, timestamp,
                state->ssrc, last_nalu, frame->data + offset, nalu_len);
        } else {
            status = rtc_h264_emit_fua(
                packets, packet_capacity, &packet_count, &sequence, timestamp,
                state->ssrc, last_nalu, frame->data + offset, nalu_len,
                max_payload_bytes);
        }
        if (status != RTC_STATUS_OK) {
            return status;
        }
        if (last_nalu) {
            break;
        }
        offset = nalu_end + next_start_len;
    }

    *inout_packet_count = packet_count;
    state->sequence = sequence;
    state->timestamp = timestamp + 3000u;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_h264_append_reassembly(
    uint8_t *reassembly_buffer, size_t max_reassembly_bytes,
    size_t *inout_reassembly_len, const uint8_t *data, size_t data_len,
    const char **out_drop_reason)
{
    if (*inout_reassembly_len + data_len > max_reassembly_bytes) {
        *inout_reassembly_len = 0;
        if (out_drop_reason != 0) {
            *out_drop_reason = "h264_reassembly_capacity";
        }
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    memcpy(reassembly_buffer + *inout_reassembly_len, data, data_len);
    *inout_reassembly_len += data_len;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_h264_depacketize_stap_a(
    const uint8_t *payload, size_t payload_len, uint8_t *reassembly_buffer,
    size_t max_reassembly_bytes, size_t *inout_reassembly_len,
    const char **out_drop_reason)
{
    size_t offset = 1u;

    while (offset < payload_len) {
        uint16_t nalu_len;
        rtc_status_t status;

        if (offset + 2u > payload_len) {
            if (out_drop_reason != 0) {
                *out_drop_reason = "h264_stap_a_length";
            }
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        nalu_len = rtc_read_u16(payload + offset);
        offset += 2u;
        if (nalu_len == 0 || offset + nalu_len > payload_len) {
            if (out_drop_reason != 0) {
                *out_drop_reason = "h264_stap_a_length";
            }
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        status = rtc_h264_append_reassembly(
            reassembly_buffer, max_reassembly_bytes, inout_reassembly_len,
            payload + offset, nalu_len, out_drop_reason);
        if (status != RTC_STATUS_OK) {
            return status;
        }
        offset += nalu_len;
    }

    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtp_depacketize_h264(
    const uint8_t *payload, size_t payload_len, uint16_t sequence,
    uint32_t timestamp, int marker, uint8_t *reassembly_buffer,
    size_t max_reassembly_bytes, size_t *inout_reassembly_len,
    uint16_t *inout_expected_sequence, int *inout_active,
    uint32_t *inout_timestamp, int *out_frame_ready,
    const char **out_drop_reason)
{
    uint8_t nalu_type;
    rtc_status_t status;

    if (out_frame_ready != 0) {
        *out_frame_ready = 0;
    }
    if (out_drop_reason != 0) {
        *out_drop_reason = 0;
    }
    if (payload == 0 || payload_len == 0 || reassembly_buffer == 0 ||
        inout_reassembly_len == 0 || inout_expected_sequence == 0 ||
        inout_active == 0 || inout_timestamp == 0 ||
        out_frame_ready == 0 || max_reassembly_bytes == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    nalu_type = (uint8_t)(payload[0] & 0x1fu);
    if (nalu_type >= 1u && nalu_type <= 23u) {
        if (*inout_active) {
            *inout_active = 0;
            if (out_drop_reason != 0) {
                *out_drop_reason = "h264_interrupted_au";
            }
        }
        *inout_reassembly_len = 0;
        status = rtc_h264_append_reassembly(
            reassembly_buffer, max_reassembly_bytes, inout_reassembly_len,
            payload, payload_len, out_drop_reason);
        if (status != RTC_STATUS_OK) {
            *inout_active = 0;
            return status;
        }
        *inout_active = !marker;
        *inout_expected_sequence = (uint16_t)(sequence + 1u);
        *inout_timestamp = timestamp;
        if (marker) {
            *out_frame_ready = 1;
        }
        return RTC_STATUS_OK;
    }

    if (nalu_type == 24u) { /* STAP-A */
        if (*inout_active) {
            *inout_active = 0;
            if (out_drop_reason != 0) {
                *out_drop_reason = "h264_interrupted_au";
            }
        }
        *inout_reassembly_len = 0;
        status = rtc_h264_depacketize_stap_a(
            payload, payload_len, reassembly_buffer, max_reassembly_bytes,
            inout_reassembly_len, out_drop_reason);
        if (status != RTC_STATUS_OK) {
            *inout_active = 0;
            *inout_reassembly_len = 0;
            return status;
        }
        *inout_active = !marker;
        *inout_expected_sequence = (uint16_t)(sequence + 1u);
        *inout_timestamp = timestamp;
        if (marker) {
            *out_frame_ready = 1;
        }
        return RTC_STATUS_OK;
    }

    if (nalu_type == 28u) { /* FU-A */
        uint8_t fu_header;
        uint8_t reconstructed_header;
        int start;
        int end;

        if (payload_len < 3u) {
            if (out_drop_reason != 0) {
                *out_drop_reason = "h264_fu_a_short";
            }
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        fu_header = payload[1];
        start = (fu_header & 0x80u) != 0u;
        end = (fu_header & 0x40u) != 0u;
        if (start) {
            *inout_reassembly_len = 0;
            *inout_active = 1;
            *inout_timestamp = timestamp;
            reconstructed_header =
                (uint8_t)((payload[0] & 0xe0u) | (fu_header & 0x1fu));
            status = rtc_h264_append_reassembly(
                reassembly_buffer, max_reassembly_bytes,
                inout_reassembly_len, &reconstructed_header, 1u,
                out_drop_reason);
            if (status != RTC_STATUS_OK) {
                *inout_active = 0;
                return status;
            }
        } else if (!*inout_active) {
            if (out_drop_reason != 0) {
                *out_drop_reason = "h264_missing_start";
            }
            return RTC_STATUS_OK;
        } else if (sequence != *inout_expected_sequence) {
            *inout_active = 0;
            *inout_reassembly_len = 0;
            if (out_drop_reason != 0) {
                *out_drop_reason = "h264_sequence_gap";
            }
            return RTC_STATUS_OK;
        }

        status = rtc_h264_append_reassembly(
            reassembly_buffer, max_reassembly_bytes, inout_reassembly_len,
            payload + 2u, payload_len - 2u, out_drop_reason);
        if (status != RTC_STATUS_OK) {
            *inout_active = 0;
            return status;
        }
        *inout_expected_sequence = (uint16_t)(sequence + 1u);
        if (end) {
            *inout_active = 0;
            if (marker) {
                *out_frame_ready = 1;
            }
        }
        return RTC_STATUS_OK;
    }

    return RTC_STATUS_UNSUPPORTED;
}
