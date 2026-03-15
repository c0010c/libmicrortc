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
  const uint8_t *payload_ptr;
  uint8_t payload[RTC_CFG_MAX_MEDIA_PAYLOAD];
} rtc_rtp_frame_t;

typedef struct rtc_rtp_ctx {
  uint16_t video_seq;
  uint16_t audio_seq;
  uint32_t video_ssrc;
  uint32_t audio_ssrc;
  int16_t video_pt;
  int16_t audio_pcma_pt;
  int16_t audio_pcmu_pt;
  uint8_t expect_video_ssrc;
  uint8_t expect_audio_ssrc;
  uint32_t remote_video_ssrc;
  uint32_t remote_audio_ssrc;
  uint8_t h264_fu_active;
  uint16_t h264_fu_next_seq;
  uint32_t h264_fu_timestamp;
  uint32_t h264_fu_ssrc;
  uint16_t h264_fu_len;
  uint8_t h264_fu_buf[RTC_CFG_H264_REASSEMBLY_MAX];
  uint32_t rx_h264_chain_breaks;
  uint32_t rx_h264_reassembly_overflows;
  uint32_t rx_ssrc_mismatch_drops;
} rtc_rtp_ctx_t;

typedef rtc_result_t (*rtc_rtp_packet_handler_t)(rtc_rtp_packet_t *packet,
                                                  void *user_data);

void rtc_rtp_init(rtc_rtp_ctx_t *ctx, uint32_t peer_id);
void rtc_rtp_set_payload_types(rtc_rtp_ctx_t *ctx, int16_t video_pt,
                               int16_t audio_pcma_pt, int16_t audio_pcmu_pt);
void rtc_rtp_set_expected_remote_ssrc(rtc_rtp_ctx_t *ctx, uint8_t expect_video,
                                      uint32_t video_ssrc, uint8_t expect_audio,
                                      uint32_t audio_ssrc);

rtc_result_t rtc_rtp_count_video_h264_packets(const uint8_t *payload,
                                              uint16_t payload_len,
                                              uint16_t *out_packet_count);
rtc_result_t rtc_rtp_packetize_video_h264(rtc_rtp_ctx_t *ctx,
                                          const uint8_t *payload,
                                          uint16_t payload_len,
                                          uint32_t timestamp90k, uint8_t marker,
                                          rtc_rtp_packet_handler_t handler,
                                          void *user_data,
                                          uint16_t *out_packet_count);

rtc_result_t rtc_rtp_encode_video_h264(rtc_rtp_ctx_t *ctx, const uint8_t *payload,
                                       uint16_t payload_len, uint32_t timestamp90k,
                                       uint8_t marker, rtc_rtp_packet_t *out_packet);

rtc_result_t rtc_rtp_encode_audio_g711(rtc_rtp_ctx_t *ctx, rtc_audio_codec_t codec,
                                       const uint8_t *payload, uint16_t payload_len,
                                       uint32_t timestamp8k,
                                       rtc_rtp_packet_t *out_packet);

rtc_result_t rtc_rtp_decode(rtc_rtp_ctx_t *ctx, const rtc_rtp_packet_t *packet,
                            rtc_rtp_frame_t *out_frame);

#endif  // RTC_RTP_H_
