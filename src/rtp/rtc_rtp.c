#include "rtp/rtc_rtp.h"

#include <string.h>

#define RTC_RTP_PT_PCMU 0u
#define RTC_RTP_PT_PCMA 8u
#define RTC_RTP_PT_H264 96u

static void rtc_write_u16be(uint8_t *dst, uint16_t v) {
  dst[0] = (uint8_t)((v >> 8) & 0xFFu);
  dst[1] = (uint8_t)(v & 0xFFu);
}

static void rtc_write_u32be(uint8_t *dst, uint32_t v) {
  dst[0] = (uint8_t)((v >> 24) & 0xFFu);
  dst[1] = (uint8_t)((v >> 16) & 0xFFu);
  dst[2] = (uint8_t)((v >> 8) & 0xFFu);
  dst[3] = (uint8_t)(v & 0xFFu);
}

static uint32_t rtc_read_u32be(const uint8_t *src) {
  return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
         ((uint32_t)src[2] << 8) | src[3];
}

void rtc_rtp_init(rtc_rtp_ctx_t *ctx, uint32_t peer_id) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->video_ssrc = 0x10000000u | (peer_id & 0x00FFFFFFu);
  ctx->audio_ssrc = 0x20000000u | (peer_id & 0x00FFFFFFu);
}

static rtc_result_t rtc_rtp_encode_common(rtc_rtp_ctx_t *ctx,
                                          rtc_packet_kind_t kind,
                                          rtc_audio_codec_t codec,
                                          const uint8_t *payload,
                                          uint16_t payload_len,
                                          uint32_t timestamp,
                                          uint8_t marker,
                                          rtc_rtp_packet_t *out_packet) {
  uint8_t payload_type;

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
    payload_type = RTC_RTP_PT_H264;
  } else {
    out_packet->seq = ctx->audio_seq++;
    out_packet->ssrc = ctx->audio_ssrc;
    payload_type = (codec == RTC_AUDIO_CODEC_PCMU) ? RTC_RTP_PT_PCMU : RTC_RTP_PT_PCMA;
  }

  memcpy(out_packet->payload, payload, payload_len);

  out_packet->wire[0] = 0x80u;
  out_packet->wire[1] = (uint8_t)((marker ? 0x80u : 0u) | payload_type);
  rtc_write_u16be(out_packet->wire + 2u, out_packet->seq);
  rtc_write_u32be(out_packet->wire + 4u, out_packet->timestamp);
  rtc_write_u32be(out_packet->wire + 8u, out_packet->ssrc);
  memcpy(out_packet->wire + RTC_CFG_RTP_HEADER_LEN, payload, payload_len);
  out_packet->wire_len = (uint16_t)(RTC_CFG_RTP_HEADER_LEN + payload_len);

  return RTC_OK;
}

rtc_result_t rtc_rtp_encode_video_h264(rtc_rtp_ctx_t *ctx,
                                       const uint8_t *payload,
                                       uint16_t payload_len,
                                       uint32_t timestamp90k,
                                       uint8_t marker,
                                       rtc_rtp_packet_t *out_packet) {
  return rtc_rtp_encode_common(ctx, RTC_PACKET_KIND_VIDEO, RTC_AUDIO_CODEC_PCMA,
                               payload, payload_len, timestamp90k, marker,
                               out_packet);
}

rtc_result_t rtc_rtp_encode_audio_g711(rtc_rtp_ctx_t *ctx,
                                       rtc_audio_codec_t codec,
                                       const uint8_t *payload,
                                       uint16_t payload_len,
                                       uint32_t timestamp8k,
                                       rtc_rtp_packet_t *out_packet) {
  if (codec != RTC_AUDIO_CODEC_PCMA && codec != RTC_AUDIO_CODEC_PCMU) {
    return RTC_ERR_INVALID_ARG;
  }
  return rtc_rtp_encode_common(ctx, RTC_PACKET_KIND_AUDIO, codec, payload,
                               payload_len, timestamp8k, 1u, out_packet);
}

static rtc_result_t rtc_rtp_decode_wire(const rtc_rtp_packet_t *packet,
                                        rtc_rtp_frame_t *out_frame) {
  uint8_t version;
  uint8_t payload_type;
  uint16_t payload_len;

  if (!packet || !out_frame) {
    return RTC_ERR_INVALID_ARG;
  }
  if (packet->wire_len < RTC_CFG_RTP_HEADER_LEN ||
      packet->wire_len > sizeof(packet->wire)) {
    return RTC_ERR_PROTOCOL;
  }

  version = (uint8_t)(packet->wire[0] >> 6);
  if (version != 2u) {
    return RTC_ERR_PROTOCOL;
  }

  payload_type = (uint8_t)(packet->wire[1] & 0x7Fu);
  if (payload_type == RTC_RTP_PT_H264) {
    out_frame->kind = RTC_PACKET_KIND_VIDEO;
    out_frame->audio_codec = RTC_AUDIO_CODEC_PCMA;
  } else if (payload_type == RTC_RTP_PT_PCMA) {
    out_frame->kind = RTC_PACKET_KIND_AUDIO;
    out_frame->audio_codec = RTC_AUDIO_CODEC_PCMA;
  } else if (payload_type == RTC_RTP_PT_PCMU) {
    out_frame->kind = RTC_PACKET_KIND_AUDIO;
    out_frame->audio_codec = RTC_AUDIO_CODEC_PCMU;
  } else {
    return RTC_ERR_PROTOCOL;
  }

  payload_len = (uint16_t)(packet->wire_len - RTC_CFG_RTP_HEADER_LEN);
  if (payload_len == 0u || payload_len > RTC_CFG_MAX_MEDIA_PAYLOAD) {
    return RTC_ERR_PROTOCOL;
  }

  out_frame->marker = (uint8_t)((packet->wire[1] & 0x80u) ? 1u : 0u);
  out_frame->timestamp = rtc_read_u32be(packet->wire + 4u);
  out_frame->payload_len = payload_len;
  memcpy(out_frame->payload, packet->wire + RTC_CFG_RTP_HEADER_LEN, payload_len);

  return RTC_OK;
}

rtc_result_t rtc_rtp_decode(const rtc_rtp_packet_t *packet, rtc_rtp_frame_t *out_frame) {
  if (!packet || !out_frame) {
    return RTC_ERR_INVALID_ARG;
  }

  memset(out_frame, 0, sizeof(*out_frame));

  if (packet->wire_len >= RTC_CFG_RTP_HEADER_LEN) {
    return rtc_rtp_decode_wire(packet, out_frame);
  }

  if (packet->payload_len == 0u || packet->payload_len > RTC_CFG_MAX_MEDIA_PAYLOAD) {
    return RTC_ERR_PROTOCOL;
  }

  out_frame->kind = packet->kind;
  out_frame->audio_codec = packet->audio_codec;
  out_frame->marker = packet->marker;
  out_frame->payload_len = packet->payload_len;
  out_frame->timestamp = packet->timestamp;
  memcpy(out_frame->payload, packet->payload, packet->payload_len);
  return RTC_OK;
}
