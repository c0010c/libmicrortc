#include "rtc/rtc.h"

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#define RTC_SIGNAL_CLI_MAX_LINE 4096u
#define RTC_SIGNAL_CLI_DEFAULT_BUDGET_US 500u
#define RTC_SIGNAL_CLI_DEFAULT_RUN_STEP_MS 10u
#define RTC_SIGNAL_CLI_ANSWER_WAIT_MS 500u
#define RTC_SIGNAL_CLI_AUDIO_FRAME_MS 20u
#define RTC_SIGNAL_CLI_VIDEO_FRAME_MS 67u
#define RTC_SIGNAL_CLI_AUDIO_SAMPLES_PER_FRAME 160u
#define RTC_SIGNAL_CLI_VIDEO_TS_STEP_90K 6000u
#define RTC_SIGNAL_CLI_MEDIA_CATCHUP_MAX 16u
#define RTC_SIGNAL_CLI_STATS_LOG_INTERVAL_MS 1000u
#define RTC_SIGNAL_CLI_VIDEO_FILE_MAX_BYTES 131072u
#define RTC_SIGNAL_CLI_VIDEO_PACKET_MAX_BYTES 65535u
#define RTC_SIGNAL_CLI_VIDEO_PARAMSET_MAX_BYTES 2048u
#define RTC_SIGNAL_CLI_DTLS_HANDSHAKE_TIMEOUT_MS 15000u
#define RTC_SIGNAL_CLI_DTLS_HANDSHAKE_MAX_RETRIES 1500u
#define RTC_SIGNAL_CLI_OFFER_B64_MAX \
  ((((RTC_CFG_MAX_SDP_LEN) + 2u) / 3u) * 4u + 4u)
#define RTC_SIGNAL_CLI_ANSWER_B64_MAX \
  ((((RTC_CFG_MAX_SDP_LEN) + 2u) / 3u) * 4u + 4u)

static const uint8_t k_cli_audio_pcma_silence[RTC_SIGNAL_CLI_AUDIO_SAMPLES_PER_FRAME] = {
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u, 0xD5u,
    0xD5u, 0xD5u, 0xD5u, 0xD5u};

/* Minimal H264 Annex-B sample: SPS + PPS + IDR */
static const uint8_t k_cli_h264_idr_annexb[] = {
    0x00u, 0x00u, 0x00u, 0x01u, 0x67u, 0x42u, 0xE0u, 0x1Eu, 0x8Du, 0x68u, 0x54u,
    0x05u, 0x01u, 0xEDu, 0x00u, 0xF0u, 0x88u, 0x45u, 0x80u, 0x00u, 0x00u, 0x00u,
    0x01u, 0x68u, 0xCEu, 0x06u, 0xE2u, 0x00u, 0x00u, 0x00u, 0x01u, 0x65u, 0x88u,
    0x80u, 0x20u, 0x07u, 0xBFu, 0xFEu, 0xF7u, 0xD9u, 0x20u};

typedef struct rtc_signal_cli_video_source {
  uint8_t enabled;
  uint8_t file_ready;
  uint8_t fallback_active;
  uint8_t warned_fallback;
  char path[256];
  size_t file_len;
  size_t cursor;
  size_t sps_len;
  size_t pps_len;
  uint8_t file_buf[RTC_SIGNAL_CLI_VIDEO_FILE_MAX_BYTES];
  uint8_t sps_buf[RTC_SIGNAL_CLI_VIDEO_PARAMSET_MAX_BYTES];
  uint8_t pps_buf[RTC_SIGNAL_CLI_VIDEO_PARAMSET_MAX_BYTES];
  uint8_t packet_buf[RTC_SIGNAL_CLI_VIDEO_PACKET_MAX_BYTES];
} rtc_signal_cli_video_source_t;

typedef struct rtc_signal_cli_app {
  rtc_engine_t *engine;
  rtc_peer_t *peer;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  uint32_t now_ms;
  uint32_t run_ms;
  uint8_t run_ms_enabled;
  uint8_t has_answer;
  uint8_t should_quit;
  uint8_t media_enabled;
  uint32_t next_audio_send_ms;
  uint32_t next_video_send_ms;
  uint32_t next_stats_log_ms;
  uint32_t audio_ts8k;
  uint32_t video_ts90k;
  uint32_t audio_send_ok;
  uint32_t video_send_ok;
  uint32_t audio_send_invalid_state;
  uint32_t video_send_invalid_state;
  uint32_t audio_send_overflow;
  uint32_t video_send_overflow;
  size_t offer_len;
  size_t offer_b64_len;
  char offer[RTC_CFG_MAX_SDP_LEN];
  char offer_b64[RTC_SIGNAL_CLI_OFFER_B64_MAX];
  char answer[RTC_CFG_MAX_SDP_LEN];
  char answer_b64[RTC_SIGNAL_CLI_ANSWER_B64_MAX];
  char answer_type[16];
  rtc_signal_cli_video_source_t video;
} rtc_signal_cli_app_t;

static volatile sig_atomic_t g_cli_sigint = 0;

static void cli_signal_handler(int signum) {
  (void)signum;
  g_cli_sigint = 1;
}

static const char *cli_result_text(rtc_result_t code) {
  switch (code) {
    case RTC_OK:
      return "ok";
    case RTC_ERR_INVALID_ARG:
      return "invalid_arg";
    case RTC_ERR_INVALID_STATE:
      return "invalid_state";
    case RTC_ERR_RESOURCE_EXHAUSTED:
      return "resource_exhausted";
    case RTC_ERR_PROTOCOL:
      return "protocol_error";
    case RTC_ERR_TIMEOUT:
      return "timeout";
    case RTC_ERR_NOT_INIT:
      return "not_init";
    case RTC_ERR_NOT_SUPPORTED:
      return "not_supported";
    case RTC_ERR_BUFFER_TOO_SMALL:
      return "buffer_too_small";
    case RTC_ERR_OVERFLOW:
      return "overflow";
    case RTC_ERR_DTLS_HANDSHAKE_FAILED:
      return "dtls_handshake_failed";
    case RTC_ERR_SRTP_ACTIVATE_FAILED:
      return "srtp_activate_failed";
    case RTC_ERR_AUTH_FAILED:
      return "auth_failed";
    default:
      return "error";
  }
}

static const char *cli_peer_state_text(rtc_peer_state_t state) {
  switch (state) {
    case RTC_PEER_STATE_NEW:
      return "new";
    case RTC_PEER_STATE_STARTING:
      return "starting";
    case RTC_PEER_STATE_ICE_CHECKING:
      return "ice_checking";
    case RTC_PEER_STATE_DTLS_HANDSHAKE:
      return "dtls_handshake";
    case RTC_PEER_STATE_CONNECTED:
      return "connected";
    case RTC_PEER_STATE_STOPPED:
      return "stopped";
    case RTC_PEER_STATE_FAILED:
      return "failed";
    default:
      return "unknown";
  }
}

static const char *cli_log_level_text(rtc_log_level_t level) {
  switch (level) {
    case RTC_LOG_ERROR:
      return "ERROR";
    case RTC_LOG_WARN:
      return "WARN";
    case RTC_LOG_INFO:
      return "INFO";
    case RTC_LOG_DEBUG:
      return "DEBUG";
    default:
      return "UNKNOWN";
  }
}

static void cli_log_internal(const char *level, rtc_result_t code,
                             const char *message) {
  fprintf(stdout, "%s\tcli\tpeer=0\tcode=%d\t%s\n", level ? level : "INFO",
          (int)code, message ? message : "");
  fflush(stdout);
}

static void cli_log_cb(rtc_log_level_t level, const char *module, uint32_t peer_id,
                       rtc_result_t code, const char *message, void *user_data) {
  (void)user_data;
  fprintf(stdout, "%s\t%s\tpeer=%" PRIu32 "\tcode=%d\t%s\n",
          cli_log_level_text(level), module ? module : "core", peer_id, (int)code,
          message ? message : "");
  fflush(stdout);
}

static void cli_state_cb(rtc_peer_t *peer, rtc_peer_state_t old_state,
                         rtc_peer_state_t new_state, void *user_data) {
  uint32_t peer_id = 0u;
  rtc_result_t r;
  (void)user_data;
  r = rtc_peer_get_id(peer, &peer_id);
  if (r != RTC_OK) {
    peer_id = 0u;
  }
  fprintf(stdout, "INFO\tpeer\tpeer=%" PRIu32 "\tcode=0\tstate %s -> %s\n", peer_id,
          cli_peer_state_text(old_state), cli_peer_state_text(new_state));
  fflush(stdout);
}

static void cli_local_desc_cb(rtc_peer_t *peer, const char *sdp, const char *type,
                              void *user_data) {
  rtc_signal_cli_app_t *app = (rtc_signal_cli_app_t *)user_data;
  (void)peer;

  if (!app || !sdp || !type) {
    return;
  }

  (void)snprintf(app->answer, sizeof(app->answer), "%s", sdp);
  (void)snprintf(app->answer_type, sizeof(app->answer_type), "%s", type);
  app->has_answer = 1u;
  cli_log_internal("INFO", RTC_OK, "local_description_ready");
}

static void cli_local_candidate_cb(rtc_peer_t *peer, const char *candidate,
                                   void *user_data) {
  uint32_t peer_id = 0u;
  rtc_result_t r;
  (void)user_data;

  if (!peer || !candidate) {
    return;
  }

  r = rtc_peer_get_id(peer, &peer_id);
  if (r != RTC_OK) {
    peer_id = 0u;
  }

  fprintf(stdout, "DEBUG\tpeer\tpeer=%" PRIu32 "\tcode=0\tlocal_candidate %s\n", peer_id,
          candidate);
  fflush(stdout);
}

static int cli_parse_u32_arg(const char *input, uint32_t *value) {
  char *end = NULL;
  unsigned long parsed;

  if (!input || !value || input[0] == '\0') {
    return 0;
  }

  errno = 0;
  parsed = strtoul(input, &end, 10);
  if (errno != 0 || end == input || *end != '\0' || parsed > UINT32_MAX) {
    return 0;
  }

  *value = (uint32_t)parsed;
  return 1;
}

static size_t cli_base64_encode(const uint8_t *src, size_t src_len, char *dst,
                                size_t dst_cap) {
  static const char k_b64[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i;
  size_t o = 0u;
  size_t need;

  if (!src || !dst) {
    return 0u;
  }

  need = ((src_len + 2u) / 3u) * 4u + 1u;
  if (need > dst_cap) {
    return 0u;
  }

  for (i = 0u; i < src_len; i += 3u) {
    uint32_t chunk = ((uint32_t)src[i]) << 16;
    size_t rem = src_len - i;
    if (rem > 1u) {
      chunk |= ((uint32_t)src[i + 1u]) << 8;
    }
    if (rem > 2u) {
      chunk |= (uint32_t)src[i + 2u];
    }
    dst[o++] = k_b64[(chunk >> 18) & 0x3Fu];
    dst[o++] = k_b64[(chunk >> 12) & 0x3Fu];
    dst[o++] = (rem > 1u) ? k_b64[(chunk >> 6) & 0x3Fu] : '=';
    dst[o++] = (rem > 2u) ? k_b64[chunk & 0x3Fu] : '=';
  }

  dst[o] = '\0';
  return o;
}

static int cli_is_space_char(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

static int cli_base64_value(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A';
  }
  if (ch >= 'a' && ch <= 'z') {
    return ch - 'a' + 26;
  }
  if (ch >= '0' && ch <= '9') {
    return ch - '0' + 52;
  }
  if (ch == '+') {
    return 62;
  }
  if (ch == '/') {
    return 63;
  }
  return -1;
}

static rtc_result_t cli_base64_decode(const char *src, size_t src_len, uint8_t *dst,
                                      size_t dst_cap, size_t *out_len) {
  size_t i;
  size_t o = 0u;

  if (!src || !dst || !out_len) {
    return RTC_ERR_INVALID_ARG;
  }

  if ((src_len % 4u) != 0u) {
    return RTC_ERR_PROTOCOL;
  }

  for (i = 0u; i < src_len; i += 4u) {
    int v0;
    int v1;
    int v2 = 0;
    int v3 = 0;
    int pad = 0;
    char c0 = src[i];
    char c1 = src[i + 1u];
    char c2 = src[i + 2u];
    char c3 = src[i + 3u];

    v0 = cli_base64_value(c0);
    v1 = cli_base64_value(c1);
    if (v0 < 0 || v1 < 0) {
      return RTC_ERR_PROTOCOL;
    }

    if (c2 == '=') {
      if (c3 != '=') {
        return RTC_ERR_PROTOCOL;
      }
      pad = 2;
    } else {
      v2 = cli_base64_value(c2);
      if (v2 < 0) {
        return RTC_ERR_PROTOCOL;
      }
      if (c3 == '=') {
        pad = 1;
      } else {
        v3 = cli_base64_value(c3);
        if (v3 < 0) {
          return RTC_ERR_PROTOCOL;
        }
      }
    }

    if (o + 3u - (size_t)pad > dst_cap) {
      return RTC_ERR_BUFFER_TOO_SMALL;
    }

    dst[o++] = (uint8_t)(((uint32_t)v0 << 2) | (((uint32_t)v1 >> 4) & 0x03u));
    if (pad < 2) {
      dst[o++] = (uint8_t)((((uint32_t)v1 & 0x0Fu) << 4) | (((uint32_t)v2 >> 2) & 0x0Fu));
    }
    if (pad == 0) {
      dst[o++] = (uint8_t)((((uint32_t)v2 & 0x03u) << 6) | ((uint32_t)v3 & 0x3Fu));
    }

    if (pad > 0 && i + 4u != src_len) {
      return RTC_ERR_PROTOCOL;
    }
  }

  *out_len = o;
  return RTC_OK;
}

static uint64_t cli_now_monotonic_ms64(void) {
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    return 0u;
  }
  return (uint64_t)tv.tv_sec * 1000u + (uint64_t)tv.tv_usec / 1000u;
}

static void cli_sleep_ms(uint32_t sleep_ms) {
  struct timeval tv;
  if (sleep_ms == 0u) {
    return;
  }
  tv.tv_sec = (time_t)(sleep_ms / 1000u);
  tv.tv_usec = (suseconds_t)((sleep_ms % 1000u) * 1000u);
  (void)select(0, NULL, NULL, NULL, &tv);
}

static int cli_time_due(uint32_t now_ms, uint32_t target_ms) {
  return (int32_t)(now_ms - target_ms) >= 0;
}

static int cli_h264_start_code_size(const uint8_t *data, size_t remaining) {
  if (!data) {
    return 0;
  }
  if (remaining >= 4u && data[0] == 0x00u && data[1] == 0x00u && data[2] == 0x00u &&
      data[3] == 0x01u) {
    return 4;
  }
  if (remaining >= 3u && data[0] == 0x00u && data[1] == 0x00u && data[2] == 0x01u) {
    return 3;
  }
  return 0;
}

static int cli_h264_find_start_code(const uint8_t *data, size_t data_len, size_t from,
                                    size_t *out_pos, size_t *out_size) {
  size_t i;
  int sc_size;

  if (!data || !out_pos || !out_size || from >= data_len) {
    return 0;
  }

  for (i = from; i < data_len; ++i) {
    sc_size = cli_h264_start_code_size(data + i, data_len - i);
    if (sc_size > 0) {
      *out_pos = i;
      *out_size = (size_t)sc_size;
      return 1;
    }
  }

  return 0;
}

static void cli_video_enable_fallback(rtc_signal_cli_app_t *app, const char *reason) {
  rtc_signal_cli_video_source_t *video;
  if (!app) {
    return;
  }

  video = &app->video;
  video->fallback_active = 1u;
  video->file_ready = 0u;
  if (!video->warned_fallback) {
    char message[256];
    (void)snprintf(message, sizeof(message), "video_file_fallback reason=%s",
                   reason ? reason : "unknown");
    cli_log_internal("WARN", RTC_ERR_PROTOCOL, message);
    video->warned_fallback = 1u;
  }
}

static rtc_result_t cli_video_load_file(rtc_signal_cli_app_t *app, const char *path) {
  rtc_signal_cli_video_source_t *video;
  FILE *fp = NULL;
  size_t read_size;

  if (!app || !path || path[0] == '\0') {
    return RTC_ERR_INVALID_ARG;
  }

  video = &app->video;
  memset(video, 0, sizeof(*video));
  video->enabled = 1u;
  video->fallback_active = 1u;
  (void)snprintf(video->path, sizeof(video->path), "%s", path);

  fp = fopen(path, "rb");
  if (!fp) {
    cli_log_internal("WARN", RTC_ERR_RESOURCE_EXHAUSTED, "video_file_open_failed");
    return RTC_OK;
  }

  read_size = fread(video->file_buf, 1u, sizeof(video->file_buf), fp);
  if (ferror(fp)) {
    fclose(fp);
    cli_video_enable_fallback(app, "video_file_read_failed");
    return RTC_OK;
  }
  if (!feof(fp)) {
    fclose(fp);
    cli_video_enable_fallback(app, "video_file_too_large");
    return RTC_OK;
  }
  (void)fclose(fp);

  if (read_size == 0u) {
    cli_video_enable_fallback(app, "video_file_empty");
    return RTC_OK;
  }

  video->file_len = read_size;
  video->cursor = 0u;
  video->file_ready = 1u;
  video->fallback_active = 0u;
  video->warned_fallback = 0u;
  cli_log_internal("INFO", RTC_OK, "video_file_loaded");

  return RTC_OK;
}

static int cli_video_next_nal(rtc_signal_cli_app_t *app, const uint8_t **out_ptr,
                              uint16_t *out_len, uint8_t *out_marker) {
  rtc_signal_cli_video_source_t *video;
  size_t nal_pos = 0u;
  size_t nal_sc_size = 0u;
  size_t next_pos = 0u;
  size_t next_sc_size = 0u;
  size_t nal_end;
  size_t nal_len;
  uint8_t nal_type;

  if (!app || !out_ptr || !out_len || !out_marker) {
    return 0;
  }

  video = &app->video;
  if (!video->enabled || !video->file_ready || video->file_len == 0u) {
    return 0;
  }

  if (video->cursor >= video->file_len) {
    video->cursor = 0u;
  }

  if (!cli_h264_find_start_code(video->file_buf, video->file_len, video->cursor, &nal_pos,
                                &nal_sc_size)) {
    video->cursor = 0u;
    if (!cli_h264_find_start_code(video->file_buf, video->file_len, video->cursor, &nal_pos,
                                  &nal_sc_size)) {
      cli_video_enable_fallback(app, "video_file_no_start_code");
      return 0;
    }
  }

  if (cli_h264_find_start_code(video->file_buf, video->file_len, nal_pos + nal_sc_size,
                               &next_pos, &next_sc_size)) {
    (void)next_sc_size;
    nal_end = next_pos;
    video->cursor = next_pos;
  } else {
    nal_end = video->file_len;
    video->cursor = video->file_len;
  }

  if (nal_end <= nal_pos + nal_sc_size) {
    cli_video_enable_fallback(app, "video_file_empty_nal");
    return 0;
  }

  nal_len = nal_end - nal_pos;
  if (nal_len > RTC_SIGNAL_CLI_VIDEO_PACKET_MAX_BYTES || nal_len > UINT16_MAX) {
    cli_video_enable_fallback(app, "video_nal_too_large");
    return 0;
  }

  nal_type = video->file_buf[nal_pos + nal_sc_size] & 0x1Fu;

  if (nal_type == 7u && nal_len <= RTC_SIGNAL_CLI_VIDEO_PARAMSET_MAX_BYTES) {
    memcpy(video->sps_buf, video->file_buf + nal_pos, nal_len);
    video->sps_len = nal_len;
  } else if (nal_type == 8u && nal_len <= RTC_SIGNAL_CLI_VIDEO_PARAMSET_MAX_BYTES) {
    memcpy(video->pps_buf, video->file_buf + nal_pos, nal_len);
    video->pps_len = nal_len;
  }

  if (nal_type == 5u && video->sps_len > 0u && video->pps_len > 0u &&
      video->sps_len + video->pps_len + nal_len <= RTC_SIGNAL_CLI_VIDEO_PACKET_MAX_BYTES) {
    memcpy(video->packet_buf, video->sps_buf, video->sps_len);
    memcpy(video->packet_buf + video->sps_len, video->pps_buf, video->pps_len);
    memcpy(video->packet_buf + video->sps_len + video->pps_len, video->file_buf + nal_pos,
           nal_len);
    *out_ptr = video->packet_buf;
    *out_len = (uint16_t)(video->sps_len + video->pps_len + nal_len);
    *out_marker = 1u;
    return 1;
  }

  *out_ptr = video->file_buf + nal_pos;
  *out_len = (uint16_t)nal_len;
  *out_marker = 1u;
  return 1;
}

static rtc_result_t cli_create_peer(rtc_signal_cli_app_t *app) {
  rtc_result_t r;
  if (!app || !app->engine) {
    return RTC_ERR_INVALID_ARG;
  }

  app->peer = NULL;
  r = rtc_peer_create(app->engine, &app->peer_cfg, &app->peer);
  if (r != RTC_OK) {
    return r;
  }

  app->has_answer = 0u;
  app->should_quit = 0u;
  app->offer_len = 0u;
  app->offer_b64_len = 0u;
  app->offer[0] = '\0';
  app->offer_b64[0] = '\0';
  app->answer[0] = '\0';
  app->answer_b64[0] = '\0';
  app->answer_type[0] = '\0';
  app->now_ms = 0u;
  app->media_enabled = 0u;
  app->next_audio_send_ms = 0u;
  app->next_video_send_ms = 0u;
  app->next_stats_log_ms = RTC_SIGNAL_CLI_STATS_LOG_INTERVAL_MS;
  app->audio_ts8k = 0u;
  app->video_ts90k = 0u;
  app->audio_send_ok = 0u;
  app->video_send_ok = 0u;
  app->audio_send_invalid_state = 0u;
  app->video_send_invalid_state = 0u;
  app->audio_send_overflow = 0u;
  app->video_send_overflow = 0u;

  return RTC_OK;
}

static void cli_log_peer_stats(rtc_signal_cli_app_t *app, const char *reason) {
  rtc_peer_stats_t stats;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  rtc_result_t r;
  char msg[320];

  if (!app || !app->peer) {
    return;
  }

  memset(&stats, 0, sizeof(stats));
  r = rtc_peer_get_state(app->peer, &state);
  if (r != RTC_OK) {
    state = RTC_PEER_STATE_NEW;
  }
  r = rtc_peer_get_stats(app->peer, &stats);
  if (r != RTC_OK) {
    (void)snprintf(msg, sizeof(msg),
                   "stats_snapshot reason=%s state=%s stats_err=%d",
                   reason ? reason : "periodic", cli_peer_state_text(state), (int)r);
    cli_log_internal("WARN", r, msg);
    return;
  }

  (void)snprintf(msg, sizeof(msg),
                 "stats_snapshot reason=%s state=%s tx_video=%" PRIu32
                 " tx_audio=%" PRIu32 " rx_video=%" PRIu32 " rx_audio=%" PRIu32
                 " send_ok(v=%" PRIu32 ",a=%" PRIu32 ")"
                 " send_invalid_state(v=%" PRIu32 ",a=%" PRIu32 ")"
                 " send_overflow(v=%" PRIu32 ",a=%" PRIu32 ")"
                 " qtx=%u qrx=%u srtp=%u ice_ok=%" PRIu32 " ice_fail=%" PRIu32,
                 reason ? reason : "periodic", cli_peer_state_text(state),
                 stats.tx_video_frames, stats.tx_audio_frames, stats.rx_video_frames,
                 stats.rx_audio_frames, app->video_send_ok, app->audio_send_ok,
                 app->video_send_invalid_state, app->audio_send_invalid_state,
                 app->video_send_overflow, app->audio_send_overflow,
                 (unsigned)stats.rtp_tx_queue_depth, (unsigned)stats.rtp_rx_queue_depth,
                 (unsigned)stats.srtp_active, stats.ice_checks_ok, stats.ice_checks_failed);
  cli_log_internal("INFO", RTC_OK, msg);
}

static void cli_maybe_log_peer_stats(rtc_signal_cli_app_t *app) {
  if (!app) {
    return;
  }
  if (app->now_ms < app->next_stats_log_ms) {
    return;
  }
  cli_log_peer_stats(app, "periodic");
  app->next_stats_log_ms = app->now_ms + RTC_SIGNAL_CLI_STATS_LOG_INTERVAL_MS;
}

static rtc_result_t cli_media_send_once(rtc_signal_cli_app_t *app, uint8_t send_audio,
                                        uint8_t send_video) {
  rtc_result_t r = RTC_OK;

  if (!app || !app->peer) {
    return RTC_ERR_INVALID_ARG;
  }

  if (send_audio) {
    r = rtc_peer_send_audio_g711(app->peer, RTC_AUDIO_CODEC_PCMA,
                                 k_cli_audio_pcma_silence,
                                 (uint16_t)sizeof(k_cli_audio_pcma_silence),
                                 app->audio_ts8k);
    if (r == RTC_OK) {
      app->audio_send_ok++;
    } else if (r == RTC_ERR_INVALID_STATE) {
      app->audio_send_invalid_state++;
    } else if (r == RTC_ERR_OVERFLOW) {
      app->audio_send_overflow++;
    }
    if (r != RTC_OK && r != RTC_ERR_INVALID_STATE && r != RTC_ERR_OVERFLOW) {
      return r;
    }
    app->audio_ts8k += RTC_SIGNAL_CLI_AUDIO_SAMPLES_PER_FRAME;
  }

  if (send_video) {
    const uint8_t *payload = k_cli_h264_idr_annexb;
    uint16_t payload_len = (uint16_t)sizeof(k_cli_h264_idr_annexb);
    uint8_t marker = 1u;

    if (app->video.enabled && app->video.file_ready && !app->video.fallback_active) {
      if (!cli_video_next_nal(app, &payload, &payload_len, &marker)) {
        payload = k_cli_h264_idr_annexb;
        payload_len = (uint16_t)sizeof(k_cli_h264_idr_annexb);
        marker = 1u;
      }
    }

    r = rtc_peer_send_video_h264(app->peer, payload, payload_len, app->video_ts90k, marker);
    if (r == RTC_OK) {
      app->video_send_ok++;
    } else if (r == RTC_ERR_INVALID_STATE) {
      app->video_send_invalid_state++;
    } else if (r == RTC_ERR_OVERFLOW) {
      app->video_send_overflow++;
    }
    if (r != RTC_OK && r != RTC_ERR_INVALID_STATE && r != RTC_ERR_OVERFLOW) {
      return r;
    }
    app->video_ts90k += RTC_SIGNAL_CLI_VIDEO_TS_STEP_90K;
  }

  return RTC_OK;
}

static rtc_result_t cli_media_tick(rtc_signal_cli_app_t *app) {
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  rtc_result_t r;
  uint32_t audio_sent = 0u;
  uint32_t video_sent = 0u;

  if (!app || !app->peer) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!app->media_enabled) {
    return RTC_OK;
  }

  r = rtc_peer_get_state(app->peer, &state);
  if (r != RTC_OK) {
    return r;
  }
  if (state != RTC_PEER_STATE_CONNECTED) {
    app->next_audio_send_ms = app->now_ms + RTC_SIGNAL_CLI_AUDIO_FRAME_MS;
    app->next_video_send_ms = app->now_ms + RTC_SIGNAL_CLI_VIDEO_FRAME_MS;
    return RTC_OK;
  }

  while (cli_time_due(app->now_ms, app->next_audio_send_ms) &&
         audio_sent < RTC_SIGNAL_CLI_MEDIA_CATCHUP_MAX) {
    r = cli_media_send_once(app, 1u, 0u);
    if (r != RTC_OK) {
      return r;
    }
    app->next_audio_send_ms += RTC_SIGNAL_CLI_AUDIO_FRAME_MS;
    audio_sent++;
  }

  while (cli_time_due(app->now_ms, app->next_video_send_ms) &&
         video_sent < RTC_SIGNAL_CLI_MEDIA_CATCHUP_MAX) {
    r = cli_media_send_once(app, 0u, 1u);
    if (r != RTC_OK) {
      return r;
    }
    app->next_video_send_ms += RTC_SIGNAL_CLI_VIDEO_FRAME_MS;
    video_sent++;
  }

  return RTC_OK;
}

static int cli_init(rtc_signal_cli_app_t *app) {
  rtc_result_t r;
  if (!app) {
    return 1;
  }

  memset(app, 0, sizeof(*app));

  memset(&app->engine_cfg, 0, sizeof(app->engine_cfg));
  app->engine_cfg.version = RTC_API_VERSION;
  app->engine_cfg.size = (uint16_t)sizeof(app->engine_cfg);
  app->engine_cfg.min_log_level = RTC_LOG_INFO;
  app->engine_cfg.log_cb = cli_log_cb;
  app->engine_cfg.log_user_data = app;
  app->engine_cfg.active_peer_limit = RTC_CFG_DEFAULT_ACTIVE_PEERS;

  memset(&app->peer_cfg, 0, sizeof(app->peer_cfg));
  app->peer_cfg.version = RTC_API_VERSION;
  app->peer_cfg.size = (uint16_t)sizeof(app->peer_cfg);
  app->peer_cfg.max_retries = 5u;
  app->peer_cfg.retry_interval_ms = 10u;
  app->peer_cfg.dtls_handshake_timeout_ms = RTC_SIGNAL_CLI_DTLS_HANDSHAKE_TIMEOUT_MS;
  app->peer_cfg.dtls_handshake_max_retries = RTC_SIGNAL_CLI_DTLS_HANDSHAKE_MAX_RETRIES;
  app->peer_cfg.on_state_change = cli_state_cb;
  app->peer_cfg.on_local_description = cli_local_desc_cb;
  app->peer_cfg.on_local_candidate = cli_local_candidate_cb;
  app->peer_cfg.user_data = app;

  r = rtc_engine_create(&app->engine_cfg, &app->engine);
  if (r != RTC_OK) {
    fprintf(stdout, "rtc_engine_create failed: %d (%s)\n", (int)r, cli_result_text(r));
    return 1;
  }

  r = cli_create_peer(app);
  if (r != RTC_OK) {
    fprintf(stdout, "rtc_peer_create failed: %d (%s)\n", (int)r, cli_result_text(r));
    (void)rtc_engine_destroy(app->engine);
    app->engine = NULL;
    return 1;
  }

  return 0;
}

static void cli_deinit(rtc_signal_cli_app_t *app) {
  if (!app) {
    return;
  }
  if (app->peer) {
    (void)rtc_peer_destroy(app->peer);
    app->peer = NULL;
  }
  if (app->engine) {
    (void)rtc_engine_destroy(app->engine);
    app->engine = NULL;
  }
}

static void cli_usage(const char *prog) {
  fprintf(stderr,
          "usage: %s [--video-file <path>] [--run-ms <ms>] [-h|--help]\n"
          "\n"
          "stdin: offer SDP base64 text, terminated by EOF\n"
          "stdout: answer SDP base64 only (single line)\n"
          "stderr: logs and errors\n",
          prog ? prog : "rtc_signal_b64_cli");
}

static rtc_result_t cli_read_offer_b64_from_stdin(rtc_signal_cli_app_t *app) {
  char line[RTC_SIGNAL_CLI_MAX_LINE];
  char msg[160];
  size_t decoded_len = 0u;
  rtc_result_t r;
  size_t i;

  if (!app) {
    return RTC_ERR_INVALID_ARG;
  }

  app->offer_b64_len = 0u;
  app->offer_b64[0] = '\0';
  app->offer_len = 0u;
  app->offer[0] = '\0';

  while (fgets(line, sizeof(line), stdin) != NULL) {
    size_t len = strlen(line);

    for (i = 0u; i < len; ++i) {
      char ch = line[i];
      if (cli_is_space_char(ch)) {
        continue;
      }
      if (app->offer_b64_len + 1u >= sizeof(app->offer_b64)) {
        cli_log_internal("ERROR", RTC_ERR_BUFFER_TOO_SMALL, "offer_b64_too_large");
        return RTC_ERR_BUFFER_TOO_SMALL;
      }
      app->offer_b64[app->offer_b64_len++] = ch;
    }
  }

  if (ferror(stdin)) {
    cli_log_internal("ERROR", RTC_ERR_PROTOCOL, "offer_b64_read_error");
    return RTC_ERR_PROTOCOL;
  }

  if (app->offer_b64_len == 0u) {
    cli_log_internal("ERROR", RTC_ERR_INVALID_ARG, "offer_b64_empty");
    return RTC_ERR_INVALID_ARG;
  }

  app->offer_b64[app->offer_b64_len] = '\0';
  r = cli_base64_decode(app->offer_b64, app->offer_b64_len, (uint8_t *)app->offer,
                        sizeof(app->offer) - 1u, &decoded_len);
  if (r != RTC_OK) {
    if (r == RTC_ERR_PROTOCOL) {
      cli_log_internal("ERROR", r, "offer_b64_invalid_char_or_padding");
    } else if (r == RTC_ERR_BUFFER_TOO_SMALL) {
      cli_log_internal("ERROR", r, "offer_b64_decode_overflow");
    }
    return r;
  }

  if (decoded_len == 0u) {
    cli_log_internal("ERROR", RTC_ERR_INVALID_ARG, "offer_b64_decoded_empty");
    return RTC_ERR_INVALID_ARG;
  }
  if (decoded_len >= sizeof(app->offer)) {
    cli_log_internal("ERROR", RTC_ERR_BUFFER_TOO_SMALL, "offer_b64_decoded_too_large");
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  if (memchr(app->offer, '\0', decoded_len) != NULL) {
    cli_log_internal("ERROR", RTC_ERR_PROTOCOL, "offer_b64_decoded_contains_nul");
    return RTC_ERR_PROTOCOL;
  }

  app->offer[decoded_len] = '\0';
  app->offer_len = decoded_len;
  (void)snprintf(msg, sizeof(msg), "offer_b64_decoded b64_len=%" PRIu32 " sdp_len=%" PRIu32,
                 (uint32_t)app->offer_b64_len, (uint32_t)decoded_len);
  cli_log_internal("INFO", RTC_OK, msg);

  return RTC_OK;
}

static rtc_result_t cli_update_runtime_tick(rtc_signal_cli_app_t *app,
                                            uint64_t start_ms64) {
  uint64_t now64;
  uint64_t elapsed;
  rtc_result_t r;

  if (!app || !app->engine) {
    return RTC_ERR_INVALID_ARG;
  }

  now64 = cli_now_monotonic_ms64();
  elapsed = (now64 >= start_ms64) ? (now64 - start_ms64) : 0u;
  if (elapsed > UINT32_MAX) {
    app->now_ms = UINT32_MAX;
  } else {
    app->now_ms = (uint32_t)elapsed;
  }

  r = cli_media_tick(app);
  if (r != RTC_OK) {
    cli_log_peer_stats(app, "media_tick_error");
    return r;
  }

  cli_maybe_log_peer_stats(app);

  return rtc_engine_poll(app->engine, app->now_ms, RTC_SIGNAL_CLI_DEFAULT_BUDGET_US);
}

static rtc_result_t cli_wait_for_answer(rtc_signal_cli_app_t *app,
                                        uint64_t start_ms64) {
  rtc_result_t r;

  if (!app) {
    return RTC_ERR_INVALID_ARG;
  }

  while (!app->has_answer) {
    r = cli_update_runtime_tick(app, start_ms64);
    if (r != RTC_OK) {
      return r;
    }

    if (app->has_answer) {
      return RTC_OK;
    }

    if (app->now_ms >= RTC_SIGNAL_CLI_ANSWER_WAIT_MS) {
      return RTC_ERR_TIMEOUT;
    }

    if (g_cli_sigint) {
      return RTC_ERR_TIMEOUT;
    }

    cli_sleep_ms(RTC_SIGNAL_CLI_DEFAULT_RUN_STEP_MS);
  }

  return RTC_OK;
}

static rtc_result_t cli_run_after_answer(rtc_signal_cli_app_t *app,
                                         uint64_t start_ms64) {
  rtc_result_t r;

  if (!app) {
    return RTC_ERR_INVALID_ARG;
  }

  while (!g_cli_sigint && !app->should_quit) {
    r = cli_update_runtime_tick(app, start_ms64);
    if (r != RTC_OK) {
      return r;
    }

    if (app->run_ms_enabled && app->now_ms >= app->run_ms) {
      return RTC_OK;
    }

    cli_sleep_ms(RTC_SIGNAL_CLI_DEFAULT_RUN_STEP_MS);
  }

  return RTC_OK;
}

int main(int argc, char **argv) {
  static rtc_signal_cli_app_t app;
  rtc_result_t r;
  const char *video_path = NULL;
  uint32_t run_ms = 0u;
  uint8_t run_ms_enabled = 0u;
  uint64_t start_ms64;
  int i;

  for (i = 1; i < argc; ++i) {
    const char *arg = argv[i];
    if (strcmp(arg, "--video-file") == 0) {
      if (i + 1 >= argc) {
        cli_usage(argv[0]);
        return 2;
      }
      video_path = argv[++i];
      continue;
    }
    if (strcmp(arg, "--run-ms") == 0) {
      if (i + 1 >= argc || !cli_parse_u32_arg(argv[i + 1], &run_ms)) {
        cli_usage(argv[0]);
        return 2;
      }
      run_ms_enabled = 1u;
      ++i;
      continue;
    }
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      cli_usage(argv[0]);
      return 0;
    }
    cli_usage(argv[0]);
    return 2;
  }

  if (cli_init(&app) != 0) {
    return 1;
  }

  app.run_ms_enabled = run_ms_enabled;
  app.run_ms = run_ms;

  if (video_path) {
    r = cli_video_load_file(&app, video_path);
    if (r != RTC_OK) {
      fprintf(stdout, "video init failed: %d (%s)\n", (int)r, cli_result_text(r));
      cli_deinit(&app);
      return 1;
    }
  }
  cli_log_internal("INFO", RTC_OK, "connectivity_only_mode media_tx_disabled");

  (void)signal(SIGINT, cli_signal_handler);

  r = cli_read_offer_b64_from_stdin(&app);
  if (r != RTC_OK) {
    fprintf(stdout, "read offer b64 failed: %d (%s)\n", (int)r, cli_result_text(r));
    cli_deinit(&app);
    return 1;
  }

  r = rtc_peer_set_remote_description(app.peer, app.offer, "offer");
  if (r != RTC_OK) {
    fprintf(stdout, "set remote description failed: %d (%s)\n", (int)r,
            cli_result_text(r));
    cli_deinit(&app);
    return 1;
  }

  r = rtc_peer_start(app.peer);
  if (r != RTC_OK) {
    fprintf(stdout, "peer start failed: %d (%s)\n", (int)r, cli_result_text(r));
    cli_deinit(&app);
    return 1;
  }

  start_ms64 = cli_now_monotonic_ms64();

  r = cli_wait_for_answer(&app, start_ms64);
  if (r != RTC_OK || !app.has_answer || strcmp(app.answer_type, "answer") != 0) {
    fprintf(stdout, "answer not ready: %d (%s)\n", (int)r, cli_result_text(r));
    cli_deinit(&app);
    return 1;
  }

  {
    char msg[160];
    size_t answer_len = strlen(app.answer);
    size_t encoded_len =
        cli_base64_encode((const uint8_t *)app.answer, answer_len, app.answer_b64,
                          sizeof(app.answer_b64));
    if (encoded_len == 0u) {
      fprintf(stdout, "answer base64 encode failed: %d (%s)\n",
              (int)RTC_ERR_BUFFER_TOO_SMALL, cli_result_text(RTC_ERR_BUFFER_TOO_SMALL));
      cli_deinit(&app);
      return 1;
    }
    (void)snprintf(msg, sizeof(msg), "answer_b64_encoded sdp_len=%" PRIu32 " b64_len=%" PRIu32,
                   (uint32_t)answer_len, (uint32_t)encoded_len);
    cli_log_internal("INFO", RTC_OK, msg);
    fputs(app.answer_b64, stdout);
    fputc('\n', stdout);
  }
  fflush(stdout);

  r = cli_run_after_answer(&app, start_ms64);
  if (r != RTC_OK) {
    fprintf(stdout, "run loop failed: %d (%s)\n", (int)r, cli_result_text(r));
    cli_deinit(&app);
    return 1;
  }

  cli_deinit(&app);
  return 0;
}
