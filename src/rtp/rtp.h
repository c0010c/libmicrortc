#ifndef RTC_RTP_INTERNAL_H
#define RTC_RTP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/media.h"
#include "rtc/status.h"

#define RTC_RTP_HEADER_BYTES 12u
#define RTC_RTP_VERSION 2u
#define RTC_RTP_PAYLOAD_TYPE_OPUS 111u
#define RTC_RTP_PAYLOAD_TYPE_H264 103u
#define RTC_RTP_OPUS_CLOCK_INCREMENT 960u
#define RTC_RTP_SRTP_MAX_TRAILER_BYTES 64u

typedef struct rtc_rtp_packetizer_state_t {
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
} rtc_rtp_packetizer_state_t;

typedef struct rtc_rtp_packet_buffer_t {
    uint8_t *data;
    size_t capacity;
    size_t len;
    uint16_t sequence;
    uint32_t timestamp;
    int marker;
} rtc_rtp_packet_buffer_t;

typedef struct rtc_rtp_header_t {
    uint8_t payload_type;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
    size_t header_len;
    int marker;
} rtc_rtp_header_t;

rtc_status_t rtc_rtp_write_header(uint8_t *out, size_t capacity, int marker,
                                  uint8_t payload_type, uint16_t sequence,
                                  uint32_t timestamp, uint32_t ssrc);
rtc_status_t rtc_rtp_parse_header(const uint8_t *packet, size_t packet_len,
                                  rtc_rtp_header_t *out_header);
rtc_status_t rtc_rtp_packetize_opus(const rtc_media_frame_t *frame,
                                    rtc_rtp_packetizer_state_t *state,
                                    size_t max_payload_bytes, uint8_t *out,
                                    size_t capacity, size_t *out_len);
rtc_status_t rtc_rtp_packetize_h264(const rtc_media_frame_t *frame,
                                    rtc_rtp_packetizer_state_t *state,
                                    size_t max_payload_bytes,
                                    size_t max_packets_per_frame,
                                    rtc_rtp_packet_buffer_t *packets,
                                    size_t *inout_packet_count);

#endif
