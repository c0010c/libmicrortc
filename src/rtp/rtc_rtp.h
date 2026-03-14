#ifndef RTC_RTP_H_
#define RTC_RTP_H_

#include <stdint.h>

#include "rtc/rtc.h"

typedef enum rtc_packet_kind {
  RTC_PACKET_KIND_VIDEO = 1,
  RTC_PACKET_KIND_AUDIO = 2
} rtc_packet_kind_t;

typedef struct rtc_rtp_packet {
  rtc_packet_kind_t kind;
  rtc_audio_codec_t audio_codec;
  uint8_t marker;
  uint16_t payload_len;
  uint16_t wire_len;
  uint16_t seq;
  uint32_t timestamp;
  uint32_t ssrc;
  uint8_t payload[RTC_CFG_MAX_MEDIA_PAYLOAD];
  uint8_t wire[RTC_CFG_MTU + RTC_CFG_SRTP_MAX_TRAILER];
} rtc_rtp_packet_t;

typedef struct rtc_rtp_frame {
  rtc_packet_kind_t kind;
  rtc_audio_codec_t audio_codec;
  uint8_t marker;
  uint16_t payload_len;
  uint32_t timestamp;
  uint8_t payload[RTC_CFG_MAX_MEDIA_PAYLOAD];
} rtc_rtp_frame_t;

typedef struct rtc_rtp_ctx {
  uint16_t video_seq;
  uint16_t audio_seq;
  uint32_t video_ssrc;
  uint32_t audio_ssrc;
} rtc_rtp_ctx_t;

void rtc_rtp_init(rtc_rtp_ctx_t *ctx, uint32_t peer_id);

rtc_result_t rtc_rtp_encode_video_h264(rtc_rtp_ctx_t *ctx, const uint8_t *payload,
                                       uint16_t payload_len, uint32_t timestamp90k,
                                       uint8_t marker, rtc_rtp_packet_t *out_packet);

rtc_result_t rtc_rtp_encode_audio_g711(rtc_rtp_ctx_t *ctx, rtc_audio_codec_t codec,
                                       const uint8_t *payload, uint16_t payload_len,
                                       uint32_t timestamp8k,
                                       rtc_rtp_packet_t *out_packet);

rtc_result_t rtc_rtp_decode(const rtc_rtp_packet_t *packet, rtc_rtp_frame_t *out_frame);

#endif  // RTC_RTP_H_
