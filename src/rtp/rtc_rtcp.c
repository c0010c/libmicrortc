#include "rtp/rtc_rtcp.h"

#include <string.h>

static uint16_t rtc_rtcp_read_u16be(const uint8_t *src) {
  return (uint16_t)(((uint16_t)src[0] << 8) | (uint16_t)src[1]);
}

static uint32_t rtc_rtcp_read_u32be(const uint8_t *src) {
  return ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
         ((uint32_t)src[2] << 8) | (uint32_t)src[3];
}

static void rtc_rtcp_nack_push_seq(rtc_rtcp_event_t *event, uint16_t seq) {
  if (!event) {
    return;
  }
  if (event->nack_seq_count < RTC_CFG_RTX_CACHE) {
    event->nack_seq[event->nack_seq_count] = seq;
    event->nack_seq_count++;
  } else {
    event->nack_truncated = 1u;
  }
}

void rtc_rtcp_parser_init(rtc_rtcp_parser_t *parser, const uint8_t *packet,
                          uint16_t packet_len) {
  if (!parser) {
    return;
  }

  memset(parser, 0, sizeof(*parser));
  parser->packet = packet;
  parser->packet_len = packet_len;
}

rtc_result_t rtc_rtcp_parser_next(rtc_rtcp_parser_t *parser,
                                  rtc_rtcp_event_t *out_event) {
  const uint8_t *chunk;
  uint16_t length_words;
  uint32_t length_bytes;
  const uint8_t *payload;
  uint32_t payload_len;
  uint8_t version;

  if (!parser || !out_event || !parser->packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (parser->offset == parser->packet_len) {
    return RTC_ERR_TIMEOUT;
  }
  if (parser->offset > parser->packet_len ||
      (uint16_t)(parser->packet_len - parser->offset) < 4u) {
    parser->last_offset = parser->offset;
    return RTC_ERR_PROTOCOL;
  }

  chunk = parser->packet + parser->offset;
  parser->last_offset = parser->offset;
  parser->last_pt = chunk[1];
  parser->last_fmt = (uint8_t)(chunk[0] & 0x1Fu);

  version = (uint8_t)((chunk[0] >> 6) & 0x03u);
  if (version != 2u) {
    return RTC_ERR_PROTOCOL;
  }

  length_words = rtc_rtcp_read_u16be(chunk + 2u);
  length_bytes = ((uint32_t)length_words + 1u) * 4u;
  if (length_bytes < 4u) {
    return RTC_ERR_PROTOCOL;
  }
  if ((uint32_t)parser->offset + length_bytes > (uint32_t)parser->packet_len) {
    return RTC_ERR_PROTOCOL;
  }

  memset(out_event, 0, sizeof(*out_event));
  out_event->pt = parser->last_pt;
  out_event->fmt = parser->last_fmt;
  out_event->type = RTC_RTCP_EVENT_UNKNOWN;

  payload = chunk + 4u;
  payload_len = length_bytes - 4u;

  if (out_event->pt == 201u) {
    uint32_t rc;
    uint32_t report_bytes;

    if (payload_len < 4u) {
      return RTC_ERR_PROTOCOL;
    }
    rc = out_event->fmt;
    report_bytes = 4u + (rc * 24u);
    if (payload_len < report_bytes) {
      return RTC_ERR_PROTOCOL;
    }

    out_event->type = RTC_RTCP_EVENT_RR;
    out_event->sender_ssrc = rtc_rtcp_read_u32be(payload);
    if (rc > 0u) {
      out_event->media_ssrc = rtc_rtcp_read_u32be(payload + 4u);
    }
  } else if (out_event->pt == 206u && out_event->fmt == 1u) {
    if (payload_len != 8u) {
      return RTC_ERR_PROTOCOL;
    }

    out_event->type = RTC_RTCP_EVENT_PLI;
    out_event->sender_ssrc = rtc_rtcp_read_u32be(payload);
    out_event->media_ssrc = rtc_rtcp_read_u32be(payload + 4u);
  } else if (out_event->pt == 205u && out_event->fmt == 1u) {
    uint32_t pos;

    if (payload_len < 8u || ((payload_len - 8u) % 4u) != 0u) {
      return RTC_ERR_PROTOCOL;
    }

    out_event->type = RTC_RTCP_EVENT_NACK;
    out_event->sender_ssrc = rtc_rtcp_read_u32be(payload);
    out_event->media_ssrc = rtc_rtcp_read_u32be(payload + 4u);

    for (pos = 8u; pos + 4u <= payload_len; pos += 4u) {
      uint16_t pid = rtc_rtcp_read_u16be(payload + pos);
      uint16_t blp = rtc_rtcp_read_u16be(payload + pos + 2u);
      uint8_t bit;

      rtc_rtcp_nack_push_seq(out_event, pid);
      for (bit = 0u; bit < 16u; ++bit) {
        if ((blp & (uint16_t)(1u << bit)) != 0u) {
          rtc_rtcp_nack_push_seq(out_event, (uint16_t)(pid + bit + 1u));
        }
      }
    }
  }

  parser->offset = (uint16_t)(parser->offset + length_bytes);
  return RTC_OK;
}
