#include "rtp/rtc_rtp.h"

#include <string.h>

void rtc_rtp_init(rtc_rtp_ctx_t *ctx, uint32_t peer_id) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->video_ssrc = 0x10000000u | (peer_id & 0x00FFFFFFu);
  ctx->audio_ssrc = 0x20000000u | (peer_id & 0x00FFFFFFu);
}

static rtc_result_t rtc_rtp_encode_common(rtc_rtp_ctx_t *ctx, rtc_packet_kind_t kind,
                                          rtc_audio_codec_t codec, const uint8_t *payload,
                                          uint16_t payload_len, uint32_t timestamp,
                                          uint8_t marker, rtc_rtp_packet_t *out_packet) {
  if (!ctx || !payload || !out_packet || payload_len == 0u) {
    return RTC_ERR_INVALID_ARG;
  }
  if (payload_len > RTC_CFG_MAX_MEDIA_PAYLOAD) {
    return RTC_ERR_OVERFLOW;
  }

  memset(out_packet, 0, sizeof(*out_packet));
  out_packet->kind = kind;
  out_packet->audio_codec = codec;
  out_packet->marker = marker;
  out_packet->payload_len = payload_len;
  out_packet->timestamp = timestamp;
  if (kind == RTC_PACKET_KIND_VIDEO) {
    out_packet->seq = ctx->video_seq++;
    out_packet->ssrc = ctx->video_ssrc;
  } else {
    out_packet->seq = ctx->audio_seq++;
    out_packet->ssrc = ctx->audio_ssrc;
  }
  memcpy(out_packet->payload, payload, payload_len);
  return RTC_OK;
}

rtc_result_t rtc_rtp_encode_video_h264(rtc_rtp_ctx_t *ctx, const uint8_t *payload,
                                       uint16_t payload_len, uint32_t timestamp90k,
                                       uint8_t marker, rtc_rtp_packet_t *out_packet) {
  return rtc_rtp_encode_common(ctx, RTC_PACKET_KIND_VIDEO, RTC_AUDIO_CODEC_PCMA, payload,
                               payload_len, timestamp90k, marker, out_packet);
}

rtc_result_t rtc_rtp_encode_audio_g711(rtc_rtp_ctx_t *ctx, rtc_audio_codec_t codec,
                                       const uint8_t *payload, uint16_t payload_len,
                                       uint32_t timestamp8k,
                                       rtc_rtp_packet_t *out_packet) {
  if (codec != RTC_AUDIO_CODEC_PCMA && codec != RTC_AUDIO_CODEC_PCMU) {
    return RTC_ERR_INVALID_ARG;
  }
  return rtc_rtp_encode_common(ctx, RTC_PACKET_KIND_AUDIO, codec, payload, payload_len,
                               timestamp8k, 1u, out_packet);
}

rtc_result_t rtc_rtp_decode(const rtc_rtp_packet_t *packet, rtc_rtp_frame_t *out_frame) {
  if (!packet || !out_frame) {
    return RTC_ERR_INVALID_ARG;
  }
  if (packet->payload_len == 0u || packet->payload_len > RTC_CFG_MAX_MEDIA_PAYLOAD) {
    return RTC_ERR_PROTOCOL;
  }

  memset(out_frame, 0, sizeof(*out_frame));
  out_frame->kind = packet->kind;
  out_frame->audio_codec = packet->audio_codec;
  out_frame->marker = packet->marker;
  out_frame->payload_len = packet->payload_len;
  out_frame->timestamp = packet->timestamp;
  memcpy(out_frame->payload, packet->payload, packet->payload_len);
  return RTC_OK;
}
