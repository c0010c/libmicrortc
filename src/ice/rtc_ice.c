#include "ice/rtc_ice.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/rtc_platform.h"

#define RTC_SDP_MAX_LINE 384
#define RTC_MEDIA_NONE 0u
#define RTC_MEDIA_AUDIO 1u
#define RTC_MEDIA_VIDEO 2u
#define RTC_MEDIA_OTHER 3u

typedef struct rtc_h264_entry {
  int16_t pt;
  uint8_t has_fmtp;
  uint8_t packetization_mode_1;
  char fmtp[128];
} rtc_h264_entry_t;

static char rtc_ascii_tolower(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return (char)(ch - 'A' + 'a');
  }
  return ch;
}

static int rtc_ascii_case_eq(const char *lhs, const char *rhs) {
  size_t i = 0u;
  if (!lhs || !rhs) {
    return 0;
  }
  while (lhs[i] != '\0' && rhs[i] != '\0') {
    if (rtc_ascii_tolower(lhs[i]) != rtc_ascii_tolower(rhs[i])) {
      return 0;
    }
    i++;
  }
  return lhs[i] == '\0' && rhs[i] == '\0';
}

static int rtc_ascii_case_has_prefix(const char *text, const char *prefix) {
  size_t i = 0u;
  if (!text || !prefix) {
    return 0;
  }
  while (prefix[i] != '\0') {
    if (text[i] == '\0') {
      return 0;
    }
    if (rtc_ascii_tolower(text[i]) != rtc_ascii_tolower(prefix[i])) {
      return 0;
    }
    i++;
  }
  return 1;
}

static int rtc_ice_set_string(char *dst, uint16_t cap, const char *src) {
  if (!dst || !src || cap == 0u) {
    return 0;
  }
  return rtc_platform_copy_string(dst, cap, src);
}

static void rtc_ice_reset_offer_fields(rtc_ice_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  ctx->remote_description_set = 0u;
  ctx->answer_ready = 0u;
  ctx->local_description_emitted = 0u;
  ctx->local_candidate_emitted = 0u;
  ctx->media_order_count = 0u;
  memset(ctx->media_order, 0, sizeof(ctx->media_order));

  ctx->audio_offer_present = 0u;
  ctx->video_offer_present = 0u;
  ctx->audio_accepted = 0u;
  ctx->video_accepted = 0u;
  ctx->audio_rtcp_mux = 0u;
  ctx->video_rtcp_mux = 0u;
  ctx->video_h264_found = 0u;
  ctx->video_h264_pm1_found = 0u;
  ctx->audio_pcma_pt = -1;
  ctx->audio_pcmu_pt = -1;
  ctx->audio_selected_pt = -1;
  ctx->audio_first_pt = -1;
  ctx->video_h264_pt = -1;
  ctx->video_h264_pm1_pt = -1;
  ctx->video_selected_pt = -1;
  ctx->video_first_pt = -1;

  ctx->local_candidate_count = 0u;
  ctx->remote_candidate_count = 0u;
  ctx->connect_ticks = 0u;
  ctx->retry_count = 0u;
  memset(ctx->local_sdp, 0, sizeof(ctx->local_sdp));
  memset(ctx->local_candidate, 0, sizeof(ctx->local_candidate));
  memset(ctx->remote_ice_ufrag, 0, sizeof(ctx->remote_ice_ufrag));
  memset(ctx->remote_ice_pwd, 0, sizeof(ctx->remote_ice_pwd));
  memset(ctx->remote_fingerprint, 0, sizeof(ctx->remote_fingerprint));
  memset(ctx->remote_setup, 0, sizeof(ctx->remote_setup));
  memset(ctx->audio_mid, 0, sizeof(ctx->audio_mid));
  memset(ctx->video_mid, 0, sizeof(ctx->video_mid));
  memset(ctx->video_fmtp, 0, sizeof(ctx->video_fmtp));
  memset(ctx->remote_candidates, 0, sizeof(ctx->remote_candidates));
}

static void rtc_ice_prepare_local_credentials(rtc_ice_ctx_t *ctx) {
  if (!ctx || ctx->peer_id == 0u) {
    return;
  }
  if (ctx->local_ice_ufrag[0] == '\0') {
    (void)snprintf(ctx->local_ice_ufrag, sizeof(ctx->local_ice_ufrag), "u%u",
                   ctx->peer_id);
  }
  if (ctx->local_ice_pwd[0] == '\0') {
    (void)snprintf(ctx->local_ice_pwd, sizeof(ctx->local_ice_pwd),
                   "pw%08u%08u%08u", ctx->peer_id, ctx->peer_id * 3u + 7u,
                   ctx->peer_id * 11u + 3u);
  }
}

static void rtc_ice_add_media_order(rtc_ice_ctx_t *ctx, uint8_t media_kind) {
  uint8_t i;
  if (!ctx || media_kind == RTC_MEDIA_NONE || media_kind == RTC_MEDIA_OTHER) {
    return;
  }
  for (i = 0u; i < ctx->media_order_count; ++i) {
    if (ctx->media_order[i] == media_kind) {
      return;
    }
  }
  if (ctx->media_order_count < sizeof(ctx->media_order)) {
    ctx->media_order[ctx->media_order_count] = media_kind;
    ctx->media_order_count++;
  }
}

static rtc_result_t rtc_ice_sdp_append(char *dst, uint16_t cap, uint16_t *io_len,
                                       const char *fmt, ...) {
  va_list ap;
  int n;
  uint16_t offset;

  if (!dst || !io_len || !fmt) {
    return RTC_ERR_INVALID_ARG;
  }
  offset = *io_len;
  if (offset >= cap) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }

  va_start(ap, fmt);
  n = vsnprintf(dst + offset, (size_t)(cap - offset), fmt, ap);
  va_end(ap);
  if (n < 0 || n >= (int)(cap - offset)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  offset = (uint16_t)(offset + (uint16_t)n);
  if ((uint16_t)(offset + 2u) >= cap) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  dst[offset++] = '\r';
  dst[offset++] = '\n';
  dst[offset] = '\0';
  *io_len = offset;
  return RTC_OK;
}

static int rtc_ice_parse_int16(const char *text, int16_t *out_value) {
  long value;
  char *end_ptr = NULL;
  if (!text || !out_value) {
    return 0;
  }
  value = strtol(text, &end_ptr, 10);
  if (end_ptr == text || value < -32768L || value > 32767L) {
    return 0;
  }
  *out_value = (int16_t)value;
  return 1;
}

static int rtc_ice_parse_m_first_pt(const char *mline_content, int16_t *out_first_pt) {
  char media[16];
  char proto[40];
  unsigned port = 0u;
  int first_pt = -1;
  int parsed;

  if (!mline_content || !out_first_pt) {
    return 0;
  }
  *out_first_pt = -1;

  parsed = sscanf(mline_content, "%15s %u %39s %d", media, &port, proto, &first_pt);
  if (parsed < 3) {
    return 0;
  }
  if (parsed >= 4 && first_pt >= 0 && first_pt <= 127) {
    *out_first_pt = (int16_t)first_pt;
  }
  return 1;
}

static int rtc_ice_parse_rtpmap(const char *rtpmap_value, int16_t *out_pt,
                                char *codec, uint16_t codec_cap, int *out_clock_rate) {
  const char *space;
  const char *slash;
  int16_t pt = -1;
  size_t codec_len;
  long clock_rate;
  char *clock_end = NULL;

  if (!rtpmap_value || !out_pt || !codec || codec_cap == 0u || !out_clock_rate) {
    return 0;
  }
  *out_pt = -1;
  codec[0] = '\0';
  *out_clock_rate = 0;

  if (!rtc_ice_parse_int16(rtpmap_value, &pt)) {
    return 0;
  }
  space = strchr(rtpmap_value, ' ');
  if (!space || *(space + 1) == '\0') {
    return 0;
  }
  slash = strchr(space + 1, '/');
  if (!slash || slash == space + 1) {
    return 0;
  }

  codec_len = (size_t)(slash - (space + 1));
  if (codec_len + 1u > codec_cap) {
    return 0;
  }
  memcpy(codec, space + 1, codec_len);
  codec[codec_len] = '\0';

  clock_rate = strtol(slash + 1, &clock_end, 10);
  if (clock_end == slash + 1 || clock_rate <= 0 || clock_rate > 192000L) {
    return 0;
  }

  *out_pt = pt;
  *out_clock_rate = (int)clock_rate;
  return 1;
}

static int rtc_ice_parse_fmtp(const char *fmtp_value, int16_t *out_pt,
                              const char **out_params) {
  const char *space;
  int16_t pt = -1;

  if (!fmtp_value || !out_pt || !out_params) {
    return 0;
  }
  *out_pt = -1;
  *out_params = NULL;

  if (!rtc_ice_parse_int16(fmtp_value, &pt)) {
    return 0;
  }
  space = strchr(fmtp_value, ' ');
  if (!space || *(space + 1) == '\0') {
    return 0;
  }
  *out_pt = pt;
  *out_params = space + 1;
  return 1;
}

static int rtc_ice_find_h264_entry(rtc_h264_entry_t *entries, uint8_t count,
                                   int16_t pt) {
  uint8_t i;
  for (i = 0u; i < count; ++i) {
    if (entries[i].pt == pt) {
      return (int)i;
    }
  }
  return -1;
}

static int rtc_ice_ensure_h264_entry(rtc_h264_entry_t *entries, uint8_t *io_count,
                                     int16_t pt) {
  int idx;
  if (!entries || !io_count) {
    return -1;
  }
  idx = rtc_ice_find_h264_entry(entries, *io_count, pt);
  if (idx >= 0) {
    return idx;
  }
  if (*io_count >= 8u) {
    return -1;
  }
  entries[*io_count].pt = pt;
  entries[*io_count].has_fmtp = 0u;
  entries[*io_count].packetization_mode_1 = 0u;
  entries[*io_count].fmtp[0] = '\0';
  (*io_count)++;
  return (int)(*io_count - 1u);
}

static rtc_result_t rtc_ice_build_answer(rtc_ice_ctx_t *ctx) {
  uint16_t sdp_len = 0u;
  uint8_t i;
  uint8_t candidate_written = 0u;
  char bundle[48];
  uint8_t accepted_media_count = 0u;
  rtc_result_t r;

  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!ctx->local_host_ready || !ctx->local_fingerprint_ready) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (!ctx->remote_description_set) {
    return RTC_ERR_INVALID_STATE;
  }
  if (rtc_ascii_case_eq(ctx->remote_setup, "passive")) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (!rtc_ascii_case_eq(ctx->remote_setup, "actpass") &&
      !rtc_ascii_case_eq(ctx->remote_setup, "active")) {
    return RTC_ERR_PROTOCOL;
  }

  memset(bundle, 0, sizeof(bundle));
  for (i = 0u; i < ctx->media_order_count; ++i) {
    if (ctx->media_order[i] == RTC_MEDIA_AUDIO && ctx->audio_accepted) {
      const char *mid = ctx->audio_mid[0] != '\0' ? ctx->audio_mid : "0";
      if (bundle[0] != '\0') {
        (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle),
                       " ");
      }
      (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle), "%s",
                     mid);
      accepted_media_count++;
    } else if (ctx->media_order[i] == RTC_MEDIA_VIDEO && ctx->video_accepted) {
      const char *mid = ctx->video_mid[0] != '\0' ? ctx->video_mid : "1";
      if (bundle[0] != '\0') {
        (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle),
                       " ");
      }
      (void)snprintf(bundle + strlen(bundle), sizeof(bundle) - strlen(bundle), "%s",
                     mid);
      accepted_media_count++;
    }
  }
  if (accepted_media_count == 0u) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  (void)snprintf(ctx->local_candidate, sizeof(ctx->local_candidate),
                 "candidate:%u 1 udp 2130706431 %u.%u.%u.%u %u typ host",
                 (unsigned int)ctx->peer_id, ctx->local_host_ip[0], ctx->local_host_ip[1],
                 ctx->local_host_ip[2], ctx->local_host_ip[3], ctx->local_host_port);

  memset(ctx->local_sdp, 0, sizeof(ctx->local_sdp));
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "v=0");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                         "o=- %u 2 IN IP4 %u.%u.%u.%u", (unsigned int)ctx->peer_id,
                         ctx->local_host_ip[0], ctx->local_host_ip[1], ctx->local_host_ip[2],
                         ctx->local_host_ip[3]);
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "s=-");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "t=0 0");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                         "a=group:BUNDLE %s", bundle);
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "a=ice-lite");
  if (r != RTC_OK) {
    return r;
  }
  r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                         "a=fingerprint:sha-256 %s", ctx->local_fingerprint);
  if (r != RTC_OK) {
    return r;
  }

  for (i = 0u; i < ctx->media_order_count; ++i) {
    uint8_t media_kind = ctx->media_order[i];
    uint8_t accepted = 0u;
    int16_t selected_pt = -1;
    int16_t fallback_pt = -1;
    const char *mid = NULL;

    if (media_kind == RTC_MEDIA_AUDIO) {
      accepted = ctx->audio_accepted;
      selected_pt = ctx->audio_selected_pt;
      fallback_pt = ctx->audio_first_pt;
      mid = ctx->audio_mid[0] != '\0' ? ctx->audio_mid : "0";
      if (fallback_pt < 0) {
        fallback_pt = 0;
      }

      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "m=audio %u UDP/TLS/RTP/SAVPF %d",
                             accepted ? 9u : 0u, accepted ? selected_pt : fallback_pt);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "c=IN IP4 0.0.0.0");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "a=mid:%s",
                             mid);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=setup:passive");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-ufrag:%s", ctx->local_ice_ufrag);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-pwd:%s", ctx->local_ice_pwd);
      if (r != RTC_OK) {
        return r;
      }

      if (accepted) {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=rtcp-mux");
        if (r != RTC_OK) {
          return r;
        }
        if (selected_pt == ctx->audio_pcma_pt) {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=rtpmap:%d PCMA/8000", selected_pt);
        } else {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=rtpmap:%d PCMU/8000", selected_pt);
        }
        if (r != RTC_OK) {
          return r;
        }
        if (!candidate_written) {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=%s", ctx->local_candidate);
          if (r != RTC_OK) {
            return r;
          }
          candidate_written = 1u;
        }
      } else {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=inactive");
        if (r != RTC_OK) {
          return r;
        }
      }
    } else if (media_kind == RTC_MEDIA_VIDEO) {
      accepted = ctx->video_accepted;
      selected_pt = ctx->video_selected_pt;
      fallback_pt = ctx->video_first_pt;
      mid = ctx->video_mid[0] != '\0' ? ctx->video_mid : "1";
      if (fallback_pt < 0) {
        fallback_pt = 96;
      }

      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "m=video %u UDP/TLS/RTP/SAVPF %d",
                             accepted ? 9u : 0u, accepted ? selected_pt : fallback_pt);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "c=IN IP4 0.0.0.0");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len, "a=mid:%s",
                             mid);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=setup:passive");
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-ufrag:%s", ctx->local_ice_ufrag);
      if (r != RTC_OK) {
        return r;
      }
      r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                             "a=ice-pwd:%s", ctx->local_ice_pwd);
      if (r != RTC_OK) {
        return r;
      }

      if (accepted) {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=rtcp-mux");
        if (r != RTC_OK) {
          return r;
        }
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=rtpmap:%d H264/90000", selected_pt);
        if (r != RTC_OK) {
          return r;
        }
        if (ctx->video_fmtp[0] != '\0') {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=fmtp:%d %s", selected_pt, ctx->video_fmtp);
          if (r != RTC_OK) {
            return r;
          }
        }
        if (!candidate_written) {
          r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                                 "a=%s", ctx->local_candidate);
          if (r != RTC_OK) {
            return r;
          }
          candidate_written = 1u;
        }
      } else {
        r = rtc_ice_sdp_append(ctx->local_sdp, sizeof(ctx->local_sdp), &sdp_len,
                               "a=inactive");
        if (r != RTC_OK) {
          return r;
        }
      }
    }
  }

  if (!candidate_written) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  (void)snprintf(ctx->local_description_type, sizeof(ctx->local_description_type), "%s",
                 "answer");
  ctx->local_candidate_count = 1u;
  ctx->answer_ready = 1u;
  return RTC_OK;
}

static rtc_result_t rtc_ice_parse_offer(rtc_ice_ctx_t *ctx, const char *sdp) {
  const char *cursor = NULL;
  uint8_t current_media = RTC_MEDIA_NONE;
  rtc_h264_entry_t h264_entries[8];
  uint8_t h264_count = 0u;
  uint8_t h264_selected_idx = 0xFFu;

  if (!ctx || !sdp) {
    return RTC_ERR_INVALID_ARG;
  }
  memset(h264_entries, 0, sizeof(h264_entries));

  cursor = sdp;
  while (cursor && *cursor != '\0') {
    const char *nl = strchr(cursor, '\n');
    size_t line_len = nl ? (size_t)(nl - cursor) : strlen(cursor);
    char line[RTC_SDP_MAX_LINE];

    if (line_len > 0u && cursor[line_len - 1u] == '\r') {
      line_len--;
    }
    if (line_len >= sizeof(line)) {
      return RTC_ERR_PROTOCOL;
    }
    memcpy(line, cursor, line_len);
    line[line_len] = '\0';

    if (line[0] == 'm' && line[1] == '=') {
      if (strncmp(line + 2, "audio ", 6u) == 0) {
        current_media = RTC_MEDIA_AUDIO;
        ctx->audio_offer_present = 1u;
        rtc_ice_add_media_order(ctx, RTC_MEDIA_AUDIO);
        (void)rtc_ice_parse_m_first_pt(line + 2, &ctx->audio_first_pt);
        if (ctx->audio_mid[0] == '\0') {
          (void)snprintf(ctx->audio_mid, sizeof(ctx->audio_mid), "%s", "0");
        }
      } else if (strncmp(line + 2, "video ", 6u) == 0) {
        current_media = RTC_MEDIA_VIDEO;
        ctx->video_offer_present = 1u;
        rtc_ice_add_media_order(ctx, RTC_MEDIA_VIDEO);
        (void)rtc_ice_parse_m_first_pt(line + 2, &ctx->video_first_pt);
        if (ctx->video_mid[0] == '\0') {
          (void)snprintf(ctx->video_mid, sizeof(ctx->video_mid), "%s", "1");
        }
      } else {
        current_media = RTC_MEDIA_OTHER;
      }
    } else if (line[0] == 'a' && line[1] == '=') {
      const char *attr = line + 2;

      if (strncmp(attr, "ice-ufrag:", 10u) == 0) {
        if (!rtc_ice_set_string(ctx->remote_ice_ufrag, sizeof(ctx->remote_ice_ufrag),
                                attr + 10u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "ice-pwd:", 8u) == 0) {
        if (!rtc_ice_set_string(ctx->remote_ice_pwd, sizeof(ctx->remote_ice_pwd),
                                attr + 8u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "fingerprint:", 12u) == 0) {
        const char *value = attr + 12u;
        if (!rtc_ascii_case_has_prefix(value, "sha-256 ")) {
          return RTC_ERR_NOT_SUPPORTED;
        }
        if (!rtc_ice_set_string(ctx->remote_fingerprint, sizeof(ctx->remote_fingerprint),
                                value + 8u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "setup:", 6u) == 0) {
        if (!rtc_ice_set_string(ctx->remote_setup, sizeof(ctx->remote_setup), attr + 6u)) {
          return RTC_ERR_BUFFER_TOO_SMALL;
        }
      } else if (strncmp(attr, "candidate:", 10u) == 0) {
        rtc_result_t r = rtc_ice_add_remote_candidate(ctx, attr);
        if (r != RTC_OK) {
          return r;
        }
      } else if (strncmp(attr, "mid:", 4u) == 0) {
        if (current_media == RTC_MEDIA_AUDIO) {
          if (!rtc_ice_set_string(ctx->audio_mid, sizeof(ctx->audio_mid), attr + 4u)) {
            return RTC_ERR_BUFFER_TOO_SMALL;
          }
        } else if (current_media == RTC_MEDIA_VIDEO) {
          if (!rtc_ice_set_string(ctx->video_mid, sizeof(ctx->video_mid), attr + 4u)) {
            return RTC_ERR_BUFFER_TOO_SMALL;
          }
        }
      } else if (rtc_ascii_case_eq(attr, "rtcp-mux")) {
        if (current_media == RTC_MEDIA_AUDIO) {
          ctx->audio_rtcp_mux = 1u;
        } else if (current_media == RTC_MEDIA_VIDEO) {
          ctx->video_rtcp_mux = 1u;
        }
      } else if (strncmp(attr, "rtpmap:", 7u) == 0) {
        int16_t pt = -1;
        char codec[24];
        int clock_rate = 0;

        if (!rtc_ice_parse_rtpmap(attr + 7u, &pt, codec, sizeof(codec), &clock_rate)) {
          return RTC_ERR_PROTOCOL;
        }
        if (current_media == RTC_MEDIA_AUDIO) {
          if (clock_rate == 8000 && rtc_ascii_case_eq(codec, "PCMA")) {
            ctx->audio_pcma_pt = pt;
          } else if (clock_rate == 8000 && rtc_ascii_case_eq(codec, "PCMU")) {
            ctx->audio_pcmu_pt = pt;
          }
        } else if (current_media == RTC_MEDIA_VIDEO) {
          if (clock_rate == 90000 && rtc_ascii_case_eq(codec, "H264")) {
            int idx = rtc_ice_ensure_h264_entry(h264_entries, &h264_count, pt);
            if (idx < 0) {
              return RTC_ERR_PROTOCOL;
            }
            ctx->video_h264_found = 1u;
          }
        }
      } else if (strncmp(attr, "fmtp:", 5u) == 0) {
        int16_t pt = -1;
        const char *params = NULL;

        if (!rtc_ice_parse_fmtp(attr + 5u, &pt, &params)) {
          return RTC_ERR_PROTOCOL;
        }
        if (current_media == RTC_MEDIA_VIDEO) {
          int idx = rtc_ice_find_h264_entry(h264_entries, h264_count, pt);
          if (idx >= 0) {
            rtc_h264_entry_t *entry = &h264_entries[idx];
            if (!rtc_ice_set_string(entry->fmtp, sizeof(entry->fmtp), params)) {
              return RTC_ERR_BUFFER_TOO_SMALL;
            }
            entry->has_fmtp = 1u;
            if (strstr(params, "packetization-mode=1") != NULL) {
              entry->packetization_mode_1 = 1u;
            }
          }
        }
      }
    }

    if (!nl) {
      break;
    }
    cursor = nl + 1;
  }

  if (ctx->remote_ice_ufrag[0] == '\0' || ctx->remote_ice_pwd[0] == '\0' ||
      ctx->remote_fingerprint[0] == '\0' || ctx->remote_setup[0] == '\0') {
    return RTC_ERR_PROTOCOL;
  }

  if (ctx->audio_pcma_pt >= 0) {
    ctx->audio_selected_pt = ctx->audio_pcma_pt;
  } else if (ctx->audio_pcmu_pt >= 0) {
    ctx->audio_selected_pt = ctx->audio_pcmu_pt;
  } else {
    ctx->audio_selected_pt = -1;
  }

  if (ctx->video_h264_found && h264_count > 0u) {
    uint8_t i;
    h264_selected_idx = 0u;
    for (i = 0u; i < h264_count; ++i) {
      if (h264_entries[i].packetization_mode_1) {
        h264_selected_idx = i;
        ctx->video_h264_pm1_found = 1u;
        break;
      }
    }
    ctx->video_selected_pt = h264_entries[h264_selected_idx].pt;
    ctx->video_h264_pt = h264_entries[0].pt;
    if (ctx->video_h264_pm1_found) {
      ctx->video_h264_pm1_pt = h264_entries[h264_selected_idx].pt;
    }
    if (h264_entries[h264_selected_idx].has_fmtp) {
      (void)snprintf(ctx->video_fmtp, sizeof(ctx->video_fmtp), "%s",
                     h264_entries[h264_selected_idx].fmtp);
    }
  }

  ctx->audio_accepted =
      (uint8_t)(ctx->audio_offer_present && ctx->audio_selected_pt >= 0 &&
                ctx->audio_rtcp_mux);
  ctx->video_accepted =
      (uint8_t)(ctx->video_offer_present && ctx->video_selected_pt >= 0 &&
                ctx->video_rtcp_mux);

  if (!ctx->audio_accepted && !ctx->video_accepted) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  return RTC_OK;
}

void rtc_ice_init(rtc_ice_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->state = RTC_ICE_STATE_NEW;
  ctx->audio_pcma_pt = -1;
  ctx->audio_pcmu_pt = -1;
  ctx->audio_selected_pt = -1;
  ctx->audio_first_pt = -1;
  ctx->video_h264_pt = -1;
  ctx->video_h264_pm1_pt = -1;
  ctx->video_selected_pt = -1;
  ctx->video_first_pt = -1;
}

void rtc_ice_set_peer_id(rtc_ice_ctx_t *ctx, uint32_t peer_id) {
  if (!ctx) {
    return;
  }
  ctx->peer_id = peer_id;
  rtc_ice_prepare_local_credentials(ctx);
}

rtc_result_t rtc_ice_set_local_host(rtc_ice_ctx_t *ctx, const uint8_t ip[4],
                                    uint16_t port) {
  if (!ctx || !ip || port == 0u) {
    return RTC_ERR_INVALID_ARG;
  }
  memcpy(ctx->local_host_ip, ip, 4u);
  ctx->local_host_port = port;
  ctx->local_host_ready = 1u;
  return RTC_OK;
}

rtc_result_t rtc_ice_set_local_fingerprint(rtc_ice_ctx_t *ctx,
                                           const char *fingerprint_sha256) {
  if (!ctx || !fingerprint_sha256 || fingerprint_sha256[0] == '\0') {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_ice_set_string(ctx->local_fingerprint, sizeof(ctx->local_fingerprint),
                          fingerprint_sha256)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  ctx->local_fingerprint_ready = 1u;
  return RTC_OK;
}

rtc_result_t rtc_ice_start(rtc_ice_ctx_t *ctx, uint32_t peer_id, uint32_t now_ms) {
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }

  if (peer_id != 0u) {
    ctx->peer_id = peer_id;
  }
  rtc_ice_prepare_local_credentials(ctx);

  ctx->local_description_emitted = 0u;
  ctx->local_candidate_emitted = 0u;
  ctx->connect_ticks = 0u;
  ctx->retry_count = 0u;
  ctx->state = RTC_ICE_STATE_GATHERING;
  ctx->last_tick_ms = now_ms;

  return RTC_OK;
}

rtc_result_t rtc_ice_set_remote_description(rtc_ice_ctx_t *ctx, const char *sdp,
                                            const char *type) {
  if (!ctx || !sdp || !type) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_ascii_case_eq(type, "offer")) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  if (!ctx->local_host_ready || !ctx->local_fingerprint_ready) {
    return RTC_ERR_NOT_SUPPORTED;
  }
  rtc_ice_reset_offer_fields(ctx);
  if (!rtc_platform_copy_string(ctx->remote_sdp, sizeof(ctx->remote_sdp), sdp)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  if (!rtc_platform_copy_string(ctx->remote_description_type,
                                sizeof(ctx->remote_description_type), type)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }

  {
    rtc_result_t r = rtc_ice_parse_offer(ctx, sdp);
    if (r != RTC_OK) {
      return r;
    }
    ctx->remote_description_set = 1u;
    r = rtc_ice_build_answer(ctx);
    if (r != RTC_OK) {
      rtc_ice_reset_offer_fields(ctx);
      return r;
    }
  }

  return RTC_OK;
}

rtc_result_t rtc_ice_add_remote_candidate(rtc_ice_ctx_t *ctx, const char *candidate) {
  uint16_t i;
  if (!ctx || !candidate) {
    return RTC_ERR_INVALID_ARG;
  }

  for (i = 0u; i < ctx->remote_candidate_count; ++i) {
    if (strcmp(ctx->remote_candidates[i], candidate) == 0) {
      return RTC_OK;
    }
  }

  if (ctx->remote_candidate_count >= RTC_CFG_MAX_REMOTE_CANDIDATES) {
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }
  if (!rtc_platform_copy_string(
          ctx->remote_candidates[ctx->remote_candidate_count], RTC_CFG_MAX_CANDIDATE_LEN,
          candidate)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  ctx->remote_candidate_count++;
  return RTC_OK;
}

void rtc_ice_tick(rtc_ice_ctx_t *ctx, uint32_t now_ms, uint16_t retry_interval_ms,
                  uint16_t max_retries, rtc_ice_event_t *out_event) {
  uint32_t elapsed = 0;

  if (!ctx || !out_event) {
    return;
  }
  memset(out_event, 0, sizeof(*out_event));

  if (ctx->state == RTC_ICE_STATE_GATHERING) {
    if (ctx->answer_ready && !ctx->local_description_emitted) {
      out_event->emit_local_description = 1u;
      ctx->local_description_emitted = 1u;
    }
    if (ctx->answer_ready && !ctx->local_candidate_emitted) {
      out_event->emit_local_candidate = 1u;
      ctx->local_candidate_emitted = 1u;
    }
    ctx->state = RTC_ICE_STATE_CHECKING;
    ctx->last_tick_ms = now_ms;
    return;
  }

  if (ctx->state != RTC_ICE_STATE_CHECKING) {
    return;
  }

  if (ctx->answer_ready && !ctx->local_description_emitted) {
    out_event->emit_local_description = 1u;
    ctx->local_description_emitted = 1u;
  }
  if (ctx->answer_ready && !ctx->local_candidate_emitted) {
    out_event->emit_local_candidate = 1u;
    ctx->local_candidate_emitted = 1u;
  }

  if (!ctx->remote_description_set) {
    return;
  }

  if (ctx->remote_candidate_count > 0u) {
    if (ctx->connect_ticks < RTC_CFG_ICE_CONNECT_TICKS) {
      ctx->connect_ticks++;
    }
    if (ctx->connect_ticks >= RTC_CFG_ICE_CONNECT_TICKS) {
      ctx->state = RTC_ICE_STATE_CONNECTED;
      out_event->connected = 1u;
    }
    return;
  }

  elapsed = now_ms - ctx->last_tick_ms;
  if (elapsed < retry_interval_ms) {
    return;
  }

  ctx->last_tick_ms = now_ms;
  if (ctx->retry_count < max_retries) {
    ctx->retry_count++;
    out_event->retry_performed = 1u;
    return;
  }

  ctx->state = RTC_ICE_STATE_FAILED;
  out_event->failed = 1u;
  out_event->error = RTC_ERR_TIMEOUT;
}
