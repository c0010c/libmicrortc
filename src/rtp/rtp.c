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
