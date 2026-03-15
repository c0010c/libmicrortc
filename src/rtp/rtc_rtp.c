#include "rtp/rtc_rtp.h"

#include <string.h>

#define RTC_RTP_PT_PCMU 0u
#define RTC_RTP_PT_PCMA 8u
#define RTC_RTP_PT_H264 96u

#define RTC_H264_NAL_TYPE_MASK 0x1Fu
#define RTC_H264_NAL_TYPE_FU_A 28u
#define RTC_H264_FU_HEADER_SIZE 2u

typedef struct rtc_rtp_packet_store {
  rtc_rtp_packet_t *packet;
  uint8_t seen;
} rtc_rtp_packet_store_t;

typedef struct rtc_h264_emit_ctx {
  rtc_rtp_ctx_t *ctx;
  uint32_t timestamp;
  uint8_t input_marker;
  uint16_t total_packets;
  uint16_t emitted_packets;
  rtc_rtp_packet_handler_t handler;
  void *user_data;
} rtc_h264_emit_ctx_t;

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

static uint16_t rtc_read_u16be(const uint8_t *src) {
  return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

static uint32_t rtc_read_u32be(const uint8_t *src) {
  return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
         ((uint32_t)src[2] << 8) | src[3];
}

static uint8_t rtc_h264_has_start_code(const uint8_t *payload, uint16_t payload_len) {
  if (!payload || payload_len < 3u) {
    return 0u;
  }
  if (payload_len >= 4u && payload[0] == 0x00u && payload[1] == 0x00u &&
      payload[2] == 0x00u && payload[3] == 0x01u) {
    return 1u;
  }
  if (payload[0] == 0x00u && payload[1] == 0x00u && payload[2] == 0x01u) {
    return 1u;
  }
  return 0u;
}

static uint8_t rtc_h264_find_start_code(const uint8_t *payload, uint16_t payload_len,
                                        uint16_t from, uint16_t *out_pos,
                                        uint8_t *out_code_len) {
  uint16_t i;

  if (!payload || !out_pos || !out_code_len || from >= payload_len) {
    return 0u;
  }

  for (i = from; i + 2u < payload_len; ++i) {
    if (payload[i] != 0x00u || payload[i + 1u] != 0x00u) {
      continue;
    }
    if (payload[i + 2u] == 0x01u) {
      *out_pos = i;
      *out_code_len = 3u;
      return 1u;
    }
    if (i + 3u < payload_len && payload[i + 2u] == 0x00u &&
        payload[i + 3u] == 0x01u) {
      *out_pos = i;
      *out_code_len = 4u;
      return 1u;
    }
  }
  return 0u;
}

static rtc_result_t rtc_rtp_encode_common_pt(rtc_rtp_ctx_t *ctx,
                                              rtc_packet_kind_t kind,
                                              rtc_audio_codec_t codec,
                                              uint8_t payload_type,
                                              const uint8_t *payload,
                                              uint16_t payload_len,
                                              uint32_t timestamp,
                                              uint8_t marker,
                                              rtc_rtp_packet_t *out_packet) {
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

  out_packet->wire[0] = 0x80u;
  out_packet->wire[1] = (uint8_t)((marker ? 0x80u : 0u) | payload_type);
  rtc_write_u16be(out_packet->wire + 2u, out_packet->seq);
  rtc_write_u32be(out_packet->wire + 4u, out_packet->timestamp);
  rtc_write_u32be(out_packet->wire + 8u, out_packet->ssrc);
  memcpy(out_packet->wire + RTC_CFG_RTP_HEADER_LEN, payload, payload_len);
  out_packet->wire_len = (uint16_t)(RTC_CFG_RTP_HEADER_LEN + payload_len);

  return RTC_OK;
}

static rtc_result_t rtc_h264_count_nalu_packets(uint16_t nalu_len,
                                                 uint16_t *io_total_packets) {
  uint16_t chunk;
  uint16_t payload_without_header;
  uint16_t packets;

  if (!io_total_packets || nalu_len == 0u) {
    return RTC_ERR_PROTOCOL;
  }

  if (nalu_len <= RTC_CFG_MAX_MEDIA_PAYLOAD) {
    *io_total_packets = (uint16_t)(*io_total_packets + 1u);
    return RTC_OK;
  }

  if (RTC_CFG_MAX_MEDIA_PAYLOAD <= RTC_H264_FU_HEADER_SIZE || nalu_len <= 1u) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  chunk = (uint16_t)(RTC_CFG_MAX_MEDIA_PAYLOAD - RTC_H264_FU_HEADER_SIZE);
  payload_without_header = (uint16_t)(nalu_len - 1u);
  packets = (uint16_t)((payload_without_header + chunk - 1u) / chunk);
  *io_total_packets = (uint16_t)(*io_total_packets + packets);
  return RTC_OK;
}

static rtc_result_t rtc_h264_for_each_nalu(const uint8_t *payload, uint16_t payload_len,
                                            rtc_result_t (*on_nalu)(const uint8_t *,
                                                                    uint16_t,
                                                                    void *),
                                            void *user_data,
                                            uint16_t *out_nalu_count) {
  uint16_t nalu_count = 0u;

  if (!payload || payload_len == 0u || !on_nalu) {
    return RTC_ERR_INVALID_ARG;
  }

  if (!rtc_h264_has_start_code(payload, payload_len)) {
    rtc_result_t r = on_nalu(payload, payload_len, user_data);
    if (r != RTC_OK) {
      return r;
    }
    if (out_nalu_count) {
      *out_nalu_count = 1u;
    }
    return RTC_OK;
  }

  {
    uint16_t scan = 0u;
    while (scan < payload_len) {
      uint16_t start_pos = 0u;
      uint16_t next_pos = 0u;
      uint16_t nalu_start;
      uint16_t nalu_end;
      uint8_t start_len = 0u;
      uint8_t next_len = 0u;

      if (!rtc_h264_find_start_code(payload, payload_len, scan, &start_pos, &start_len)) {
        break;
      }

      nalu_start = (uint16_t)(start_pos + start_len);
      if (nalu_start >= payload_len) {
        break;
      }

      if (rtc_h264_find_start_code(payload, payload_len, nalu_start, &next_pos, &next_len)) {
        nalu_end = next_pos;
        scan = next_pos;
        (void)next_len;
      } else {
        nalu_end = payload_len;
        while (nalu_end > nalu_start && payload[nalu_end - 1u] == 0x00u) {
          nalu_end--;
        }
        scan = payload_len;
      }

      if (nalu_end <= nalu_start) {
        continue;
      }

      {
        rtc_result_t r = on_nalu(&payload[nalu_start],
                                 (uint16_t)(nalu_end - nalu_start), user_data);
        if (r != RTC_OK) {
          return r;
        }
      }
      nalu_count++;
    }
  }

  if (nalu_count == 0u) {
    return RTC_ERR_PROTOCOL;
  }
  if (out_nalu_count) {
    *out_nalu_count = nalu_count;
  }
  return RTC_OK;
}

static rtc_result_t rtc_h264_count_nalu_cb(const uint8_t *nalu, uint16_t nalu_len,
                                            void *user_data) {
  uint16_t *total_packets = (uint16_t *)user_data;
  (void)nalu;
  return rtc_h264_count_nalu_packets(nalu_len, total_packets);
}

static rtc_result_t rtc_h264_emit_single_nalu(rtc_h264_emit_ctx_t *emit_ctx,
                                               const uint8_t *nalu,
                                               uint16_t nalu_len,
                                               uint8_t marker,
                                               rtc_rtp_packet_t *packet) {
  if (!emit_ctx || !nalu || !packet) {
    return RTC_ERR_INVALID_ARG;
  }
  return rtc_rtp_encode_common_pt(emit_ctx->ctx, RTC_PACKET_KIND_VIDEO,
                                  RTC_AUDIO_CODEC_PCMA,
                                  (uint8_t)emit_ctx->ctx->video_pt, nalu,
                                  nalu_len, emit_ctx->timestamp, marker,
                                  packet);
}

static rtc_result_t rtc_h264_emit_fu_a(rtc_h264_emit_ctx_t *emit_ctx,
                                        const uint8_t *nalu,
                                        uint16_t nalu_len,
                                        rtc_rtp_packet_t *packet) {
  uint16_t chunk;
  uint16_t remaining;
  uint16_t offset;
  uint8_t indicator;
  uint8_t unit_type;

  if (!emit_ctx || !nalu || !packet || nalu_len <= RTC_CFG_MAX_MEDIA_PAYLOAD ||
      nalu_len <= 1u) {
    return RTC_ERR_INVALID_ARG;
  }

  if (RTC_CFG_MAX_MEDIA_PAYLOAD <= RTC_H264_FU_HEADER_SIZE) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  chunk = (uint16_t)(RTC_CFG_MAX_MEDIA_PAYLOAD - RTC_H264_FU_HEADER_SIZE);
  remaining = (uint16_t)(nalu_len - 1u);
  offset = 1u;
  indicator = (uint8_t)((nalu[0] & 0xE0u) | RTC_H264_NAL_TYPE_FU_A);
  unit_type = (uint8_t)(nalu[0] & RTC_H264_NAL_TYPE_MASK);

  while (remaining > 0u) {
    uint16_t copy_len = remaining > chunk ? chunk : remaining;
    uint8_t fu_payload[RTC_CFG_MAX_MEDIA_PAYLOAD];
    uint8_t is_start = (uint8_t)(offset == 1u ? 1u : 0u);
    uint8_t is_end = (uint8_t)(remaining == copy_len ? 1u : 0u);
    uint8_t marker = 0u;

    fu_payload[0] = indicator;
    fu_payload[1] = unit_type;
    if (is_start) {
      fu_payload[1] |= 0x80u;
    }
    if (is_end) {
      fu_payload[1] |= 0x40u;
    }
    memcpy(&fu_payload[RTC_H264_FU_HEADER_SIZE], &nalu[offset], copy_len);

    if ((uint16_t)(emit_ctx->emitted_packets + 1u) == emit_ctx->total_packets &&
        emit_ctx->input_marker) {
      marker = 1u;
    }

    {
      rtc_result_t r = rtc_rtp_encode_common_pt(
          emit_ctx->ctx, RTC_PACKET_KIND_VIDEO, RTC_AUDIO_CODEC_PCMA,
          (uint8_t)emit_ctx->ctx->video_pt, fu_payload,
          (uint16_t)(RTC_H264_FU_HEADER_SIZE + copy_len), emit_ctx->timestamp,
          marker, packet);
      if (r != RTC_OK) {
        return r;
      }
      emit_ctx->emitted_packets++;
      r = emit_ctx->handler(packet, emit_ctx->user_data);
      if (r != RTC_OK) {
        return r;
      }
    }

    offset = (uint16_t)(offset + copy_len);
    remaining = (uint16_t)(remaining - copy_len);
  }

  return RTC_OK;
}

static rtc_result_t rtc_h264_emit_nalu_cb(const uint8_t *nalu, uint16_t nalu_len,
                                          void *user_data) {
  rtc_h264_emit_ctx_t *emit_ctx = (rtc_h264_emit_ctx_t *)user_data;
  rtc_rtp_packet_t packet;

  if (!emit_ctx || !nalu) {
    return RTC_ERR_INVALID_ARG;
  }

  if (nalu_len == 0u) {
    return RTC_ERR_PROTOCOL;
  }

  if (nalu_len <= RTC_CFG_MAX_MEDIA_PAYLOAD) {
    uint8_t marker = 0u;
    rtc_result_t r;

    if ((uint16_t)(emit_ctx->emitted_packets + 1u) == emit_ctx->total_packets &&
        emit_ctx->input_marker) {
      marker = 1u;
    }

    r = rtc_h264_emit_single_nalu(emit_ctx, nalu, nalu_len, marker, &packet);
    if (r != RTC_OK) {
      return r;
    }
    emit_ctx->emitted_packets++;
    return emit_ctx->handler(&packet, emit_ctx->user_data);
  }

  return rtc_h264_emit_fu_a(emit_ctx, nalu, nalu_len, &packet);
}

static rtc_result_t rtc_rtp_store_packet_cb(rtc_rtp_packet_t *packet, void *user_data) {
  rtc_rtp_packet_store_t *store = (rtc_rtp_packet_store_t *)user_data;
  if (!store || !store->packet || !packet || store->seen) {
    return RTC_ERR_INVALID_STATE;
  }
  *store->packet = *packet;
  store->seen = 1u;
  return RTC_OK;
}

static void rtc_rtp_reset_h264_fu_state(rtc_rtp_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  ctx->h264_fu_active = 0u;
  ctx->h264_fu_next_seq = 0u;
  ctx->h264_fu_timestamp = 0u;
  ctx->h264_fu_ssrc = 0u;
  ctx->h264_fu_len = 0u;
}

static rtc_result_t rtc_rtp_decode_video_h264(rtc_rtp_ctx_t *ctx,
                                               const uint8_t *payload,
                                               uint16_t payload_len,
                                               uint16_t seq,
                                               uint32_t timestamp,
                                               uint32_t ssrc,
                                               uint8_t marker,
                                               rtc_rtp_frame_t *out_frame) {
  uint8_t nal_type;

  if (!ctx || !payload || !out_frame || payload_len == 0u) {
    return RTC_ERR_INVALID_ARG;
  }

  nal_type = (uint8_t)(payload[0] & RTC_H264_NAL_TYPE_MASK);

  if (nal_type > 0u && nal_type < 24u) {
    if (ctx->h264_fu_active) {
      ctx->rx_h264_chain_breaks++;
      rtc_rtp_reset_h264_fu_state(ctx);
    }
    if (payload_len > RTC_CFG_MAX_MEDIA_PAYLOAD) {
      return RTC_ERR_PROTOCOL;
    }
    out_frame->kind = RTC_PACKET_KIND_VIDEO;
    out_frame->audio_codec = RTC_AUDIO_CODEC_PCMA;
    out_frame->marker = marker;
    out_frame->timestamp = timestamp;
    out_frame->payload_len = payload_len;
    memcpy(out_frame->payload, payload, payload_len);
    out_frame->payload_ptr = out_frame->payload;
    return RTC_OK;
  }

  if (nal_type != RTC_H264_NAL_TYPE_FU_A) {
    if (ctx->h264_fu_active) {
      ctx->rx_h264_chain_breaks++;
      rtc_rtp_reset_h264_fu_state(ctx);
    }
    return RTC_ERR_PROTOCOL;
  }

  if (payload_len < RTC_H264_FU_HEADER_SIZE) {
    return RTC_ERR_PROTOCOL;
  }

  {
    uint8_t fu_header = payload[1];
    uint8_t is_start = (uint8_t)((fu_header & 0x80u) ? 1u : 0u);
    uint8_t is_end = (uint8_t)((fu_header & 0x40u) ? 1u : 0u);
    uint8_t reconstructed_header;
    const uint8_t *fragment_payload = &payload[RTC_H264_FU_HEADER_SIZE];
    uint16_t fragment_len =
        (uint16_t)(payload_len - RTC_H264_FU_HEADER_SIZE);

    if (is_start) {
      if (ctx->h264_fu_active) {
        ctx->rx_h264_chain_breaks++;
      }
      rtc_rtp_reset_h264_fu_state(ctx);
      ctx->h264_fu_active = 1u;
      ctx->h264_fu_next_seq = (uint16_t)(seq + 1u);
      ctx->h264_fu_timestamp = timestamp;
      ctx->h264_fu_ssrc = ssrc;
      reconstructed_header = (uint8_t)((payload[0] & 0xE0u) |
                                       (fu_header & RTC_H264_NAL_TYPE_MASK));
      if (ctx->h264_fu_len >= RTC_CFG_H264_REASSEMBLY_MAX) {
        ctx->rx_h264_reassembly_overflows++;
        rtc_rtp_reset_h264_fu_state(ctx);
        return RTC_ERR_OVERFLOW;
      }
      ctx->h264_fu_buf[ctx->h264_fu_len++] = reconstructed_header;
    } else {
      if (!ctx->h264_fu_active) {
        ctx->rx_h264_chain_breaks++;
        return RTC_ERR_TIMEOUT;
      }
      if (seq != ctx->h264_fu_next_seq || timestamp != ctx->h264_fu_timestamp ||
          ssrc != ctx->h264_fu_ssrc) {
        ctx->rx_h264_chain_breaks++;
        rtc_rtp_reset_h264_fu_state(ctx);
        return RTC_ERR_TIMEOUT;
      }
      ctx->h264_fu_next_seq = (uint16_t)(seq + 1u);
    }

    if ((uint32_t)ctx->h264_fu_len + fragment_len > RTC_CFG_H264_REASSEMBLY_MAX) {
      ctx->rx_h264_reassembly_overflows++;
      rtc_rtp_reset_h264_fu_state(ctx);
      return RTC_ERR_OVERFLOW;
    }
    if (fragment_len > 0u) {
      memcpy(&ctx->h264_fu_buf[ctx->h264_fu_len], fragment_payload, fragment_len);
      ctx->h264_fu_len = (uint16_t)(ctx->h264_fu_len + fragment_len);
    }

    if (!is_end) {
      return RTC_ERR_TIMEOUT;
    }

    out_frame->kind = RTC_PACKET_KIND_VIDEO;
    out_frame->audio_codec = RTC_AUDIO_CODEC_PCMA;
    out_frame->marker = marker;
    out_frame->timestamp = timestamp;
    out_frame->payload_len = ctx->h264_fu_len;
    out_frame->payload_ptr = ctx->h264_fu_buf;
    rtc_rtp_reset_h264_fu_state(ctx);
    return RTC_OK;
  }
}

void rtc_rtp_init(rtc_rtp_ctx_t *ctx, uint32_t peer_id) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->video_ssrc = 0x10000000u | (peer_id & 0x00FFFFFFu);
  ctx->audio_ssrc = 0x20000000u | (peer_id & 0x00FFFFFFu);
  ctx->video_pt = (int16_t)RTC_RTP_PT_H264;
  ctx->audio_pcma_pt = (int16_t)RTC_RTP_PT_PCMA;
  ctx->audio_pcmu_pt = (int16_t)RTC_RTP_PT_PCMU;
}

void rtc_rtp_set_payload_types(rtc_rtp_ctx_t *ctx, int16_t video_pt,
                               int16_t audio_pcma_pt, int16_t audio_pcmu_pt) {
  if (!ctx) {
    return;
  }
  ctx->video_pt = video_pt;
  ctx->audio_pcma_pt = audio_pcma_pt;
  ctx->audio_pcmu_pt = audio_pcmu_pt;
}

void rtc_rtp_set_expected_remote_ssrc(rtc_rtp_ctx_t *ctx, uint8_t expect_video,
                                      uint32_t video_ssrc, uint8_t expect_audio,
                                      uint32_t audio_ssrc) {
  if (!ctx) {
    return;
  }
  ctx->expect_video_ssrc = expect_video ? 1u : 0u;
  ctx->expect_audio_ssrc = expect_audio ? 1u : 0u;
  ctx->remote_video_ssrc = video_ssrc;
  ctx->remote_audio_ssrc = audio_ssrc;
}

rtc_result_t rtc_rtp_count_video_h264_packets(const uint8_t *payload,
                                              uint16_t payload_len,
                                              uint16_t *out_packet_count) {
  uint16_t total_packets = 0u;
  rtc_result_t r;

  if (!payload || payload_len == 0u || !out_packet_count) {
    return RTC_ERR_INVALID_ARG;
  }

  r = rtc_h264_for_each_nalu(payload, payload_len, rtc_h264_count_nalu_cb,
                             &total_packets, NULL);
  if (r != RTC_OK) {
    return r;
  }
  if (total_packets == 0u) {
    return RTC_ERR_PROTOCOL;
  }

  *out_packet_count = total_packets;
  return RTC_OK;
}

rtc_result_t rtc_rtp_packetize_video_h264(rtc_rtp_ctx_t *ctx,
                                          const uint8_t *payload,
                                          uint16_t payload_len,
                                          uint32_t timestamp90k, uint8_t marker,
                                          rtc_rtp_packet_handler_t handler,
                                          void *user_data,
                                          uint16_t *out_packet_count) {
  rtc_h264_emit_ctx_t emit_ctx;
  rtc_result_t r;
  uint16_t total_packets = 0u;

  if (!ctx || !payload || payload_len == 0u || !out_packet_count) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->video_pt < 0 || ctx->video_pt > 127) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  r = rtc_rtp_count_video_h264_packets(payload, payload_len, &total_packets);
  if (r != RTC_OK) {
    return r;
  }
  *out_packet_count = total_packets;

  if (!handler) {
    return RTC_OK;
  }

  memset(&emit_ctx, 0, sizeof(emit_ctx));
  emit_ctx.ctx = ctx;
  emit_ctx.timestamp = timestamp90k;
  emit_ctx.input_marker = marker ? 1u : 0u;
  emit_ctx.total_packets = total_packets;
  emit_ctx.handler = handler;
  emit_ctx.user_data = user_data;

  return rtc_h264_for_each_nalu(payload, payload_len, rtc_h264_emit_nalu_cb,
                                &emit_ctx, NULL);
}

rtc_result_t rtc_rtp_encode_video_h264(rtc_rtp_ctx_t *ctx,
                                       const uint8_t *payload,
                                       uint16_t payload_len,
                                       uint32_t timestamp90k,
                                       uint8_t marker,
                                       rtc_rtp_packet_t *out_packet) {
  uint16_t packet_count = 0u;
  rtc_rtp_packet_store_t store;
  rtc_result_t r;

  if (!ctx || !payload || !out_packet || payload_len == 0u) {
    return RTC_ERR_INVALID_ARG;
  }

  r = rtc_rtp_count_video_h264_packets(payload, payload_len, &packet_count);
  if (r != RTC_OK) {
    return r;
  }
  if (packet_count != 1u) {
    return RTC_ERR_OVERFLOW;
  }

  memset(&store, 0, sizeof(store));
  store.packet = out_packet;
  r = rtc_rtp_packetize_video_h264(ctx, payload, payload_len, timestamp90k, marker,
                                   rtc_rtp_store_packet_cb, &store,
                                   &packet_count);
  if (r != RTC_OK) {
    return r;
  }
  if (!store.seen) {
    return RTC_ERR_PROTOCOL;
  }
  return RTC_OK;
}

rtc_result_t rtc_rtp_encode_audio_g711(rtc_rtp_ctx_t *ctx,
                                       rtc_audio_codec_t codec,
                                       const uint8_t *payload,
                                       uint16_t payload_len,
                                       uint32_t timestamp8k,
                                       rtc_rtp_packet_t *out_packet) {
  int16_t pt;

  if (!ctx || !payload || !out_packet || payload_len == 0u) {
    return RTC_ERR_INVALID_ARG;
  }
  if (codec == RTC_AUDIO_CODEC_PCMA) {
    pt = ctx->audio_pcma_pt;
  } else if (codec == RTC_AUDIO_CODEC_PCMU) {
    pt = ctx->audio_pcmu_pt;
  } else {
    return RTC_ERR_INVALID_ARG;
  }
  if (pt < 0 || pt > 127) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  return rtc_rtp_encode_common_pt(ctx, RTC_PACKET_KIND_AUDIO, codec,
                                  (uint8_t)pt, payload, payload_len,
                                  timestamp8k, 1u, out_packet);
}

static rtc_result_t rtc_rtp_decode_wire(rtc_rtp_ctx_t *ctx,
                                        const rtc_rtp_packet_t *packet,
                                        rtc_rtp_frame_t *out_frame) {
  uint8_t version;
  uint8_t cc;
  uint8_t payload_type;
  uint8_t marker;
  uint8_t padding;
  uint16_t header_len;
  uint16_t payload_len;
  uint16_t seq;
  uint32_t timestamp;
  uint32_t ssrc;
  rtc_packet_kind_t kind;
  rtc_audio_codec_t audio_codec;

  if (!ctx || !packet || !out_frame) {
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

  cc = (uint8_t)(packet->wire[0] & 0x0Fu);
  header_len = (uint16_t)(RTC_CFG_RTP_HEADER_LEN + ((uint16_t)cc * 4u));
  if (packet->wire_len < header_len) {
    return RTC_ERR_PROTOCOL;
  }

  if ((packet->wire[0] & 0x10u) != 0u) {
    uint16_t ext_len_words;
    if ((uint32_t)header_len + 4u > (uint32_t)packet->wire_len) {
      return RTC_ERR_PROTOCOL;
    }
    ext_len_words = rtc_read_u16be(packet->wire + header_len + 2u);
    if ((uint32_t)header_len + 4u + ((uint32_t)ext_len_words * 4u) >
        (uint32_t)packet->wire_len) {
      return RTC_ERR_PROTOCOL;
    }
    header_len = (uint16_t)(header_len + 4u + ext_len_words * 4u);
  }

  padding = 0u;
  if ((packet->wire[0] & 0x20u) != 0u) {
    padding = packet->wire[packet->wire_len - 1u];
  }
  if ((uint32_t)packet->wire_len < (uint32_t)header_len + (uint32_t)padding) {
    return RTC_ERR_PROTOCOL;
  }

  payload_len = (uint16_t)(packet->wire_len - header_len - padding);
  if (payload_len == 0u || payload_len > RTC_CFG_MAX_MEDIA_PAYLOAD) {
    return RTC_ERR_PROTOCOL;
  }

  payload_type = (uint8_t)(packet->wire[1] & 0x7Fu);
  marker = (uint8_t)((packet->wire[1] & 0x80u) ? 1u : 0u);
  seq = rtc_read_u16be(packet->wire + 2u);
  timestamp = rtc_read_u32be(packet->wire + 4u);
  ssrc = rtc_read_u32be(packet->wire + 8u);

  if (ctx->video_pt >= 0 && payload_type == (uint8_t)ctx->video_pt) {
    kind = RTC_PACKET_KIND_VIDEO;
    audio_codec = RTC_AUDIO_CODEC_PCMA;
  } else if (ctx->audio_pcma_pt >= 0 &&
             payload_type == (uint8_t)ctx->audio_pcma_pt) {
    kind = RTC_PACKET_KIND_AUDIO;
    audio_codec = RTC_AUDIO_CODEC_PCMA;
  } else if (ctx->audio_pcmu_pt >= 0 &&
             payload_type == (uint8_t)ctx->audio_pcmu_pt) {
    kind = RTC_PACKET_KIND_AUDIO;
    audio_codec = RTC_AUDIO_CODEC_PCMU;
  } else {
    return RTC_ERR_PROTOCOL;
  }

  if (kind == RTC_PACKET_KIND_VIDEO && ctx->expect_video_ssrc &&
      ssrc != ctx->remote_video_ssrc) {
    ctx->rx_ssrc_mismatch_drops++;
    return RTC_ERR_PROTOCOL;
  }
  if (kind == RTC_PACKET_KIND_AUDIO && ctx->expect_audio_ssrc &&
      ssrc != ctx->remote_audio_ssrc) {
    ctx->rx_ssrc_mismatch_drops++;
    return RTC_ERR_PROTOCOL;
  }

  if (kind == RTC_PACKET_KIND_AUDIO) {
    out_frame->kind = RTC_PACKET_KIND_AUDIO;
    out_frame->audio_codec = audio_codec;
    out_frame->marker = marker;
    out_frame->timestamp = timestamp;
    out_frame->payload_len = payload_len;
    memcpy(out_frame->payload, packet->wire + header_len, payload_len);
    out_frame->payload_ptr = out_frame->payload;
    return RTC_OK;
  }

  return rtc_rtp_decode_video_h264(ctx, packet->wire + header_len, payload_len,
                                   seq, timestamp, ssrc, marker, out_frame);
}

rtc_result_t rtc_rtp_decode(rtc_rtp_ctx_t *ctx, const rtc_rtp_packet_t *packet,
                            rtc_rtp_frame_t *out_frame) {
  if (!ctx || !packet || !out_frame) {
    return RTC_ERR_INVALID_ARG;
  }

  memset(out_frame, 0, sizeof(*out_frame));

  if (packet->wire_len >= RTC_CFG_RTP_HEADER_LEN) {
    return rtc_rtp_decode_wire(ctx, packet, out_frame);
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
  out_frame->payload_ptr = out_frame->payload;
  return RTC_OK;
}
