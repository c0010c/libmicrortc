#include "rtc/rtc.h"
#include "rtc/rtc_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../datachannel/datachannel.h"
#include "../datachannel/sctp_adapter.h"
#include "../transport/dtls.h"
#include "../transport/ice.h"
#include "../transport/stun.h"
#include "mempool.h"

#define RTC_INBUF_CAP 1500

typedef struct {
  int used;
  char value[128];
} remote_candidate_t;

struct rtc_session {
  rtc_config_t cfg;
  rtc_platform_ops_t ops;
  void *platform_user;
  rtc_callbacks_t cbs;

  int started;
  int closed;
  int udp_fd;

  uint8_t *pool_mem;
  rtc_mempool_t pool;

  rtc_ice_agent_t ice;
  rtc_dtls_t dtls;
  rtc_dtls_state_t dtls_reported_state;
  int has_dtls_reported_state;
  rtc_sctp_adapter_t sctp;
  rtc_dc_manager_t dcm;
  rtc_channel_t *channels;

  char remote_offer[2048];
  int has_remote_offer;
  int remote_offer_legacy_sctp;
  remote_candidate_t remote_candidates[16];
  char local_ufrag[33];
  char local_pwd[65];
  char remote_ufrag[33];
  char remote_pwd[65];
  char remote_ip[64];
  uint16_t remote_port;
  int has_remote_addr;
  int remote_addr_locked;
  char provisional_remote_ip[64];
  uint16_t provisional_remote_port;
  int has_provisional_remote_addr;
  int seen_ice_check;
  int logged_first_non_stun;

  uint8_t inbuf[RTC_INBUF_CAP];
};

static void emit_error(rtc_session_t *s, rtc_result_t err, const char *msg);
static void emit_log(rtc_session_t *s, rtc_log_level_t lvl, const char *msg);
static void emit_local_candidate(rtc_session_t *s,
                                 const char *foundation,
                                 uint32_t priority,
                                 const char *ip,
                                 uint16_t port,
                                 const char *type);
static void sync_dtls_peer_id(rtc_session_t *s);
static int lock_remote_addr(rtc_session_t *s, const char *ip, uint16_t port, const char *reason);

static void bytes_to_hex(const uint8_t *in, size_t n, char *out, size_t out_len) {
  static const char h[] = "0123456789abcdef";
  size_t i;
  if (!in || !out || out_len == 0) {
    return;
  }
  if (out_len < (n * 2u + 1u)) {
    return;
  }
  for (i = 0; i < n; ++i) {
    out[i * 2 + 0] = h[(in[i] >> 4) & 0x0F];
    out[i * 2 + 1] = h[in[i] & 0x0F];
  }
  out[n * 2] = '\0';
}

static int try_extract_sdp_from_json(const char *in, char *out, size_t out_cap) {
  const char *p;
  size_t w = 0;
  if (!in || !out || out_cap == 0) {
    return -1;
  }
  p = strstr(in, "\"sdp\"");
  if (!p) {
    return -1;
  }
  p = strchr(p, ':');
  if (!p) {
    return -1;
  }
  while (*p && *p != '\"') {
    ++p;
  }
  if (*p != '\"') {
    return -1;
  }
  ++p;
  while (*p) {
    char c = *p++;
    if (c == '\"') {
      break;
    }
    if (c == '\\') {
      char e = *p++;
      if (!e) {
        break;
      }
      if (e == 'n') {
        c = '\n';
      } else if (e == 'r') {
        c = '\r';
      } else if (e == 't') {
        c = '\t';
      } else if (e == '\"' || e == '\\' || e == '/') {
        c = e;
      } else {
        c = e;
      }
    }
    if (w + 1 >= out_cap) {
      return -1;
    }
    out[w++] = c;
  }
  if (w == 0) {
    return -1;
  }
  out[w] = '\0';
  return 0;
}

static int parse_candidate_ip_port(const char *line, char *out_ip, size_t out_ip_len, uint16_t *out_port) {
  char ip[64];
  unsigned port = 0;
  if (!line || !out_ip || !out_port) {
    return -1;
  }
  if (sscanf(line, "candidate:%*s %*u %*s %*u %63s %u typ %*s", ip, &port) != 2) {
    return -1;
  }
  if (port > 65535u) {
    return -1;
  }
  (void)snprintf(out_ip, out_ip_len, "%s", ip);
  *out_port = (uint16_t)port;
  return 0;
}

static int remote_addr_is_locked_to(rtc_session_t *s, const char *ip, uint16_t port) {
  if (!s || !s->remote_addr_locked || !s->has_remote_addr || !ip || port == 0) {
    return 0;
  }
  return strcmp(s->remote_ip, ip) == 0 && s->remote_port == port;
}

static void emit_logf(rtc_session_t *s, rtc_log_level_t lvl, const char *fmt, ...) {
  char line[256];
  va_list ap;
  if (!s || !fmt) {
    return;
  }
  va_start(ap, fmt);
  (void)vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  emit_log(s, lvl, line);
}

static void set_provisional_remote_addr(rtc_session_t *s, const char *ip, uint16_t port, const char *reason) {
  if (!s || !ip || port == 0) {
    return;
  }
  (void)snprintf(s->provisional_remote_ip, sizeof(s->provisional_remote_ip), "%s", ip);
  s->provisional_remote_port = port;
  s->has_provisional_remote_addr = 1;
  emit_logf(s,
            RTC_LOG_DEBUG,
            "remote tuple provisional=%s:%u (%s)",
            s->provisional_remote_ip,
            (unsigned)s->provisional_remote_port,
            reason ? reason : "unspecified");
}

static void maybe_update_remote_addr(rtc_session_t *s, const char *candidate_line) {
  char cand_ip[64];
  uint16_t cand_port = 0;
  if (!s || !candidate_line) {
    return;
  }
  if (parse_candidate_ip_port(candidate_line, cand_ip, sizeof(cand_ip), &cand_port) == 0) {
    set_provisional_remote_addr(s, cand_ip, cand_port, "remote candidate");
  }
}

static int ice_username_matches(rtc_session_t *s, const char *username) {
  char expected[96];
  if (!s || !username || !s->local_ufrag[0] || !s->remote_ufrag[0]) {
    return 0;
  }
  (void)snprintf(expected, sizeof(expected), "%s:%s", s->local_ufrag, s->remote_ufrag);
  return strcmp(expected, username) == 0;
}

static const char *guess_packet_kind(const uint8_t *data, size_t len) {
  if (!data || len < 2) {
    return "unknown";
  }
  if (data[0] >= 20u && data[0] <= 64u && data[1] == 0xFEu) {
    return "DTLS";
  }
  return "unknown";
}

static int dtls_send_packet_cb(void *user, const uint8_t *data, size_t len) {
  rtc_session_t *s = (rtc_session_t *)user;
  if (!s || s->udp_fd < 0 || !s->has_remote_addr || !data || len == 0) {
    return -1;
  }
  if (s->ops.udp_send(s->platform_user, s->udp_fd, s->remote_ip, s->remote_port, data, len) < 0) {
    return -1;
  }
  return 0;
}

static int sctp_send_packet_cb(void *user, const uint8_t *data, size_t len) {
  rtc_session_t *s = (rtc_session_t *)user;
  if (!s || !data || len == 0) {
    return -1;
  }
  return rtc_dtls_send_application_data(&s->dtls, data, len);
}

static void sctp_on_open_cb(void *user, uint16_t stream_id, const char *label) {
  rtc_session_t *s = (rtc_session_t *)user;
  rtc_channel_t *ch = 0;
  int created = 0;
  if (!s) {
    return;
  }
  if (!rtc_dc_find(&s->dcm, stream_id)) {
    const char *name = (label && label[0]) ? label : "dc";
    (void)rtc_dc_add(&s->dcm, stream_id, name, &ch);
    created = 1;
  } else {
    ch = rtc_dc_find(&s->dcm, stream_id);
  }
  if (ch && s->cbs.on_channel_open && (created || (label && label[0]))) {
    s->cbs.on_channel_open(s->cbs.user, ch->id, ch->label);
  }
}

static void sctp_on_message_cb(void *user, uint16_t stream_id, const uint8_t *data, size_t len) {
  rtc_session_t *s = (rtc_session_t *)user;
  if (!s || !s->cbs.on_channel_message) {
    return;
  }
  s->cbs.on_channel_message(s->cbs.user, stream_id, data, len);
}

static void sctp_on_error_cb(void *user, const char *msg) {
  rtc_session_t *s = (rtc_session_t *)user;
  if (!s) {
    return;
  }
  emit_error(s, RTC_ERR_IO, msg ? msg : "sctp error");
}

static void dtls_on_appdata_cb(void *user, const uint8_t *data, size_t len) {
  rtc_session_t *s = (rtc_session_t *)user;
  if (!s || !data || len == 0) {
    return;
  }
  rtc_sctp_handle_incoming(&s->sctp, data, len);
}

static void dtls_on_error_cb(void *user, const char *msg) {
  rtc_session_t *s = (rtc_session_t *)user;
  if (!s) {
    return;
  }
  emit_error(s, RTC_ERR_IO, msg ? msg : "dtls error");
}

static void emit_log(rtc_session_t *s, rtc_log_level_t lvl, const char *msg) {
  if (s && s->ops.log && lvl <= s->cfg.log_level) {
    s->ops.log(s->platform_user, lvl, msg);
  }
}

static void emit_error(rtc_session_t *s, rtc_result_t err, const char *msg) {
  emit_log(s, RTC_LOG_ERROR, msg);
  if (s && s->cbs.on_error) {
    s->cbs.on_error(s->cbs.user, err, msg);
  }
}

static void emit_local_candidate(rtc_session_t *s,
                                 const char *foundation,
                                 uint32_t priority,
                                 const char *ip,
                                 uint16_t port,
                                 const char *type) {
  char line[192];
  if (!s || !s->cbs.on_local_candidate || !foundation || !ip || !type || port == 0) {
    return;
  }
  (void)snprintf(line,
                 sizeof(line),
                 "candidate:%s 1 UDP %u %s %u typ %s",
                 foundation,
                 (unsigned)priority,
                 ip,
                 (unsigned)port,
                 type);
  s->cbs.on_local_candidate(s->cbs.user, line);
}

static void sync_dtls_peer_id(rtc_session_t *s) {
  char idbuf[96];
  int n;
  if (!s || !s->has_remote_addr) {
    return;
  }
  n = snprintf(idbuf, sizeof(idbuf), "%s:%u", s->remote_ip, (unsigned)s->remote_port);
  if (n <= 0 || (size_t)n >= sizeof(idbuf)) {
    return;
  }
  (void)rtc_dtls_set_peer_transport_id(&s->dtls, (const uint8_t *)idbuf, (size_t)n);
}

static int lock_remote_addr(rtc_session_t *s, const char *ip, uint16_t port, const char *reason) {
  if (!s || !ip || port == 0) {
    return 0;
  }
  if (s->remote_addr_locked) {
    if (!remote_addr_is_locked_to(s, ip, port)) {
      emit_logf(s,
                RTC_LOG_WARN,
                "remote tuple switch ignored src=%s:%u locked=%s:%u",
                ip,
                (unsigned)port,
                s->remote_ip,
                (unsigned)s->remote_port);
      return 0;
    }
    return 1;
  }
  (void)snprintf(s->remote_ip, sizeof(s->remote_ip), "%s", ip);
  s->remote_port = port;
  s->has_remote_addr = 1;
  s->remote_addr_locked = 1;
  sync_dtls_peer_id(s);
  emit_logf(s,
            RTC_LOG_INFO,
            "remote tuple locked %s:%u (%s)",
            s->remote_ip,
            (unsigned)s->remote_port,
            reason ? reason : "unspecified");
  return 1;
}

static void set_ice_state(rtc_session_t *s, rtc_ice_state_t st) {
  if (!s || s->ice.state == st) {
    return;
  }
  rtc_ice_set_state(&s->ice, st);
  if (s->cbs.on_ice_state) {
    s->cbs.on_ice_state(s->cbs.user, st);
  }
}

static void set_dtls_state(rtc_session_t *s, rtc_dtls_state_t st) {
  if (!s) {
    return;
  }
  s->dtls.state = st;
  if (!s->has_dtls_reported_state || s->dtls_reported_state != st) {
    s->has_dtls_reported_state = 1;
    s->dtls_reported_state = st;
    if (s->cbs.on_dtls_state) {
      s->cbs.on_dtls_state(s->cbs.user, st);
    }
  }
}

rtc_session_t *rtc_session_create(const rtc_config_t *cfg,
                                  const rtc_platform_ops_t *ops,
                                  void *platform_user,
                                  const rtc_callbacks_t *cbs) {
  rtc_session_t *s;
  size_t pool_size;
  if (!ops || !ops->udp_open || !ops->udp_send || !ops->udp_recv || !ops->udp_close || !ops->now_ms ||
      !ops->rand_bytes) {
    return 0;
  }
  s = (rtc_session_t *)calloc(1, sizeof(*s));
  if (!s) {
    return 0;
  }

  s->cfg.bind_ip = "0.0.0.0";
  s->cfg.bind_port = 0;
  s->cfg.stun_server_ip = 0;
  s->cfg.stun_server_port = 3478;
  s->cfg.max_channels = RTC_DEFAULT_MAX_CHANNELS;
  s->cfg.max_message_size = RTC_DEFAULT_MAX_MESSAGE_SIZE;
  s->cfg.mempool_bytes = RTC_DEFAULT_MEMPOOL_BYTES;
  s->cfg.log_level = RTC_LOG_INFO;

  if (cfg) {
    s->cfg = *cfg;
    if (!s->cfg.bind_ip) {
      s->cfg.bind_ip = "0.0.0.0";
    }
    if (s->cfg.max_channels == 0) {
      s->cfg.max_channels = RTC_DEFAULT_MAX_CHANNELS;
    }
    if (s->cfg.max_message_size == 0) {
      s->cfg.max_message_size = RTC_DEFAULT_MAX_MESSAGE_SIZE;
    }
    if (s->cfg.mempool_bytes == 0) {
      s->cfg.mempool_bytes = RTC_DEFAULT_MEMPOOL_BYTES;
    }
    if (s->cfg.stun_server_port == 0) {
      s->cfg.stun_server_port = 3478;
    }
  }

  s->ops = *ops;
  s->platform_user = platform_user;
  if (cbs) {
    s->cbs = *cbs;
  }

  pool_size = s->cfg.mempool_bytes;
  s->pool_mem = (uint8_t *)calloc(1, pool_size);
  if (!s->pool_mem || rtc_mempool_init(&s->pool, s->pool_mem, pool_size) != 0) {
    rtc_session_destroy(s);
    return 0;
  }

  s->channels = (rtc_channel_t *)rtc_mempool_alloc(&s->pool, sizeof(rtc_channel_t) * s->cfg.max_channels);
  if (!s->channels) {
    rtc_session_destroy(s);
    return 0;
  }

  if (rtc_dc_init(&s->dcm, s->channels, s->cfg.max_channels) != 0) {
    rtc_session_destroy(s);
    return 0;
  }

  rtc_ice_init(&s->ice);
  rtc_dtls_init(&s->dtls, s->cfg.dtls_cert_pem, s->cfg.dtls_key_pem);
  rtc_dtls_set_callbacks(&s->dtls, dtls_on_appdata_cb, dtls_on_error_cb, s);
  rtc_sctp_init(&s->sctp);
  rtc_sctp_set_io(&s->sctp, sctp_send_packet_cb, s);
  rtc_sctp_set_callbacks(&s->sctp, sctp_on_open_cb, sctp_on_message_cb, sctp_on_error_cb, s);
  {
    uint8_t tmp[16];
    if (s->ops.rand_bytes(s->platform_user, tmp, 8) == 0) {
      bytes_to_hex(tmp, 8, s->local_ufrag, sizeof(s->local_ufrag));
    } else {
      snprintf(s->local_ufrag, sizeof(s->local_ufrag), "rtcufrag");
    }
    if (s->ops.rand_bytes(s->platform_user, tmp, sizeof(tmp)) == 0) {
      bytes_to_hex(tmp, sizeof(tmp), s->local_pwd, sizeof(s->local_pwd));
    } else {
      snprintf(s->local_pwd, sizeof(s->local_pwd), "rtcpwd0000000000000000000000");
    }
  }
  s->udp_fd = -1;
  return s;
}

rtc_result_t rtc_session_set_remote_offer(rtc_session_t *s, const char *sdp_offer) {
  size_t n;
  const char *cursor;
  const char *effective = sdp_offer;
  char decoded[2048];
  if (!s || !sdp_offer) {
    return RTC_ERR_INVALID_ARG;
  }
  if (sdp_offer[0] == '{' && try_extract_sdp_from_json(sdp_offer, decoded, sizeof(decoded)) == 0) {
    effective = decoded;
  }
  n = strlen(effective);
  if (n >= sizeof(s->remote_offer)) {
    return RTC_ERR_OVERFLOW;
  }
  memcpy(s->remote_offer, effective, n + 1);
  s->has_remote_offer = 1;
  s->remote_ufrag[0] = '\0';
  s->remote_pwd[0] = '\0';
  s->remote_ip[0] = '\0';
  s->remote_port = 0;
  s->has_remote_addr = 0;
  s->remote_addr_locked = 0;
  s->provisional_remote_ip[0] = '\0';
  s->provisional_remote_port = 0;
  s->has_provisional_remote_addr = 0;
  s->seen_ice_check = 0;
  s->logged_first_non_stun = 0;
  memset(s->remote_candidates, 0, sizeof(s->remote_candidates));
  /* Prefer modern syntax unless offer explicitly uses legacy DTLS/SCTP + sctpmap. */
  s->remote_offer_legacy_sctp = 0;
  if (strstr(effective, "a=sctp-port:") || strstr(effective, "UDP/DTLS/SCTP")) {
    s->remote_offer_legacy_sctp = 0;
  } else if (strstr(effective, "a=sctpmap:") || strstr(effective, " DTLS/SCTP ")) {
    s->remote_offer_legacy_sctp = 1;
  }

  cursor = s->remote_offer;
  while (cursor && *cursor) {
    const char *line_end = strstr(cursor, "\n");
    size_t line_len = line_end ? (size_t)(line_end - cursor) : strlen(cursor);
    while (line_len > 0 && cursor[line_len - 1] == '\r') {
      --line_len;
    }
    if (line_len > 12 && strncmp(cursor, "a=ice-ufrag:", 12) == 0) {
      size_t ncopy = line_len - 12;
      if (ncopy >= sizeof(s->remote_ufrag)) {
        ncopy = sizeof(s->remote_ufrag) - 1;
      }
      memcpy(s->remote_ufrag, cursor + 12, ncopy);
      s->remote_ufrag[ncopy] = '\0';
    } else if (line_len > 10 && strncmp(cursor, "a=ice-pwd:", 10) == 0) {
      size_t ncopy = line_len - 10;
      if (ncopy >= sizeof(s->remote_pwd)) {
        ncopy = sizeof(s->remote_pwd) - 1;
      }
      memcpy(s->remote_pwd, cursor + 10, ncopy);
      s->remote_pwd[ncopy] = '\0';
    }
    if (line_len > 12 && strncmp(cursor, "a=candidate:", 12) == 0) {
      char tmp[128];
      size_t ncopy = line_len - 2;
      if (ncopy >= sizeof(tmp)) {
        ncopy = sizeof(tmp) - 1;
      }
      memcpy(tmp, cursor + 2, ncopy);
      tmp[ncopy] = '\0';
      maybe_update_remote_addr(s, tmp);
    }
    if (!line_end) {
      break;
    }
    cursor = line_end + 1;
  }
  return RTC_OK;
}

rtc_result_t rtc_session_add_remote_candidate(rtc_session_t *s, const char *candidate_line) {
  size_t i;
  size_t n;
  if (!s || !candidate_line) {
    return RTC_ERR_INVALID_ARG;
  }
  for (i = 0; i < sizeof(s->remote_candidates) / sizeof(s->remote_candidates[0]); ++i) {
    if (!s->remote_candidates[i].used) {
      n = strlen(candidate_line);
      if (n >= sizeof(s->remote_candidates[i].value)) {
        return RTC_ERR_OVERFLOW;
      }
      memcpy(s->remote_candidates[i].value, candidate_line, n + 1);
      s->remote_candidates[i].used = 1;
      maybe_update_remote_addr(s, candidate_line);
      return RTC_OK;
    }
  }
  return RTC_ERR_NO_MEMORY;
}

rtc_result_t rtc_session_create_answer(rtc_session_t *s,
                                       char *out_sdp,
                                       size_t out_len,
                                       size_t *written) {
  int n;
  char fp[128];
  int legacy_sctp;
  const char *mline;
  const char *sctp_attr;
  if (!s || !out_sdp || out_len == 0 || !written) {
    return RTC_ERR_INVALID_ARG;
  }
  if (rtc_dtls_get_local_fingerprint_sha256(&s->dtls, fp, sizeof(fp)) != 0) {
    return RTC_ERR_STATE;
  }

  /* Mirror remote offer flavor to maximize browser compatibility. */
  legacy_sctp = s->remote_offer_legacy_sctp;
  if (legacy_sctp) {
    mline = "m=application 9 DTLS/SCTP 5000\r\n";
    sctp_attr = "a=sctpmap:5000 webrtc-datachannel 1024\r\n";
  } else {
    mline = "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n";
    sctp_attr = "a=sctp-port:5000\r\n";
  }

  n = snprintf(out_sdp,
               out_len,
               "v=0\r\n"
               "o=- 0 0 IN IP4 127.0.0.1\r\n"
               "s=rtc-s1\r\n"
               "t=0 0\r\n"
               "a=group:BUNDLE 0\r\n"
               "%s"
               "c=IN IP4 0.0.0.0\r\n"
               "a=mid:0\r\n"
               "a=setup:passive\r\n"
               "a=fingerprint:sha-256 %s\r\n"
               "a=ice-ufrag:%s\r\n"
               "a=ice-pwd:%s\r\n"
               "%s"
               "",
               mline,
               fp,
               s->local_ufrag,
               s->local_pwd,
               sctp_attr);
  if (n > 0 && s->cfg.bind_port != 0 && s->cfg.bind_ip && strcmp(s->cfg.bind_ip, "0.0.0.0") != 0) {
    int n2 = snprintf(out_sdp + n,
                      out_len - (size_t)n,
                      "a=candidate:1 1 UDP 2130706431 %s %u typ host\r\n"
                      "a=end-of-candidates\r\n",
                      s->cfg.bind_ip,
                      s->cfg.bind_port);
    if (n2 < 0 || (size_t)n + (size_t)n2 >= out_len) {
      return RTC_ERR_OVERFLOW;
    }
    n += n2;
  }
  if (n < 0 || (size_t)n >= out_len) {
    return RTC_ERR_OVERFLOW;
  }
  *written = (size_t)n;
  return RTC_OK;
}

rtc_result_t rtc_session_start(rtc_session_t *s) {
  uint8_t txn[12];
  uint8_t req[64];
  size_t req_len = 0;
  if (!s) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!s->has_remote_offer) {
    return RTC_ERR_STATE;
  }
  if (s->started) {
    return RTC_OK;
  }
  s->udp_fd = s->ops.udp_open(s->platform_user, s->cfg.bind_ip, s->cfg.bind_port);
  if (s->udp_fd < 0) {
    return RTC_ERR_IO;
  }
  s->started = 1;
  rtc_dtls_set_io(&s->dtls, dtls_send_packet_cb, s);
  emit_logf(s, RTC_LOG_INFO, "DTLS backend=%s", rtc_dtls_backend_name(&s->dtls));
  set_ice_state(s, RTC_ICE_GATHERING);

  if (s->cfg.stun_server_ip) {
    if (s->ops.rand_bytes(s->platform_user, txn, sizeof(txn)) != 0) {
      emit_error(s, RTC_ERR_IO, "rand_bytes failed");
      return RTC_ERR_IO;
    }
    if (rtc_ice_build_stun_request(&s->ice, txn, req, sizeof(req), &req_len) != 0) {
      return RTC_ERR_STATE;
    }
    if (s->ops.udp_send(s->platform_user,
                        s->udp_fd,
                        s->cfg.stun_server_ip,
                        s->cfg.stun_server_port,
                        req,
                        req_len) < 0) {
      emit_log(s, RTC_LOG_WARN, "STUN send failed");
    }
    set_ice_state(s, RTC_ICE_CHECKING);
  } else {
    if (!s->remote_addr_locked && s->has_provisional_remote_addr) {
      (void)lock_remote_addr(s,
                             s->provisional_remote_ip,
                             s->provisional_remote_port,
                             "no STUN server; using provisional candidate");
    }
    set_ice_state(s, RTC_ICE_CONNECTED);
  }
  if (s->cfg.bind_ip && strcmp(s->cfg.bind_ip, "0.0.0.0") != 0 && s->cfg.bind_port != 0) {
    emit_local_candidate(s, "1", 2130706431u, s->cfg.bind_ip, s->cfg.bind_port, "host");
  }
  return RTC_OK;
}

static void handle_udp(rtc_session_t *s) {
  char src_ip[64];
  uint16_t src_port = 0;
  size_t in_len = 0;
  int from_stun_server = 0;
  int is_stun_msg;
  int rc = s->ops.udp_recv(s->platform_user,
                           s->udp_fd,
                           src_ip,
                           sizeof(src_ip),
                           &src_port,
                           s->inbuf,
                           sizeof(s->inbuf),
                           &in_len);
  if (rc <= 0 || in_len == 0) {
    return;
  }
  if (s->cfg.stun_server_ip && strcmp(src_ip, s->cfg.stun_server_ip) == 0 && src_port == s->cfg.stun_server_port) {
    from_stun_server = 1;
  }
  is_stun_msg = rtc_stun_is_message(s->inbuf, in_len);

  if (is_stun_msg) {
    char mapped_ip[64];
    uint16_t mapped_port = 0;
    if (from_stun_server) {
      if (rtc_ice_handle_stun_response(&s->ice,
                                       s->inbuf,
                                       in_len,
                                       mapped_ip,
                                       sizeof(mapped_ip),
                                       &mapped_port) == 0) {
        snprintf(s->ice.local_srflx.ip, sizeof(s->ice.local_srflx.ip), "%s", mapped_ip);
        s->ice.local_srflx.port = mapped_port;
        s->ice.local_srflx.is_srflx = 1;
        emit_local_candidate(s, "2", 1694498815u, mapped_ip, mapped_port, "srflx");
        emit_log(s, RTC_LOG_INFO, "ICE srflx candidate learned");
      }
    } else {
      uint8_t txn[12];
      if (rtc_stun_is_binding_request(s->inbuf, in_len, txn)) {
        int use_candidate = rtc_stun_has_use_candidate(s->inbuf, in_len);
        int mi_present = rtc_stun_has_message_integrity(s->inbuf, in_len);
        int mi_status = 0;
        int username_ok = 0;
        int auth_ok = 0;
        uint8_t resp[128];
        size_t resp_len = 0;
        const char *integrity_key = s->local_pwd;
        char username[128];
        username[0] = '\0';
        s->seen_ice_check = 1;
        if (!s->remote_addr_locked) {
          set_provisional_remote_addr(s,
                                      src_ip,
                                      src_port,
                                      use_candidate ? "ICE check request (use-candidate)" : "ICE check request");
        }
        if (rtc_stun_parse_username(s->inbuf, in_len, username, sizeof(username)) == 0) {
          username_ok = ice_username_matches(s, username);
        } else {
          (void)snprintf(username, sizeof(username), "<missing>");
          username_ok = 0;
        }
        mi_status = rtc_stun_verify_message_integrity(s->inbuf, in_len, s->local_pwd);
        if (mi_status == -2) {
          emit_log(s, RTC_LOG_WARN, "ICE check MESSAGE-INTEGRITY verify unsupported; using username-only auth");
        }
        auth_ok = username_ok && (mi_status == 1 || mi_status == -2);
        emit_logf(s,
                  RTC_LOG_DEBUG,
                  "ICE check auth username=%s username-ok=%d mi-present=%d mi-status=%d auth=%s",
                  username,
                  username_ok,
                  mi_present,
                  mi_status,
                  auth_ok ? "ok" : "fail");
        if (mi_present && mi_status <= 0 && mi_status != -2) {
          emit_logf(s, RTC_LOG_WARN, "ICE check MESSAGE-INTEGRITY invalid src=%s:%u", src_ip, (unsigned)src_port);
        }
        if (rtc_stun_build_binding_success_response(txn,
                                                    src_ip,
                                                    src_port,
                                                    integrity_key,
                                                    resp,
                                                    sizeof(resp),
                                                    &resp_len) == 0) {
          if (s->ops.udp_send(s->platform_user, s->udp_fd, src_ip, src_port, resp, resp_len) < 0) {
            emit_log(s, RTC_LOG_WARN, "ICE check response send failed");
          } else {
            if (auth_ok && s->remote_addr_locked && remote_addr_is_locked_to(s, src_ip, src_port)) {
              emit_logf(s,
                        RTC_LOG_DEBUG,
                        "ICE keepalive handled src=%s:%u use-candidate=%d auth=ok",
                        src_ip,
                        (unsigned)src_port,
                        use_candidate);
            } else {
              emit_logf(s,
                        RTC_LOG_INFO,
                        "ICE check request handled src=%s:%u use-candidate=%d auth=%s",
                        src_ip,
                        (unsigned)src_port,
                        use_candidate,
                        auth_ok ? "ok" : "fail");
            }
            if (use_candidate && auth_ok && lock_remote_addr(s, src_ip, src_port, "ICE request with USE-CANDIDATE")) {
              set_ice_state(s, RTC_ICE_CONNECTED);
            } else if (use_candidate && !auth_ok) {
              emit_logf(s,
                        RTC_LOG_WARN,
                        "ICE nomination ignored src=%s:%u use-candidate=1 auth=fail",
                        src_ip,
                        (unsigned)src_port);
            }
          }
        } else {
          emit_log(s, RTC_LOG_WARN, "ICE check response build failed");
        }
      }
    }
    return;
  }

  if (from_stun_server) {
    return;
  }

  if (!s->logged_first_non_stun) {
    emit_logf(s,
              RTC_LOG_INFO,
              "first non-STUN packet src=%s:%u kind=%s",
              src_ip,
              (unsigned)src_port,
              guess_packet_kind(s->inbuf, in_len));
    s->logged_first_non_stun = 1;
  }

  if (!s->remote_addr_locked) {
    if (s->seen_ice_check) {
      emit_logf(s,
                RTC_LOG_WARN,
                "ignoring non-STUN packet before nominated tuple src=%s:%u kind=%s",
                src_ip,
                (unsigned)src_port,
                guess_packet_kind(s->inbuf, in_len));
      return;
    }
    if (!lock_remote_addr(s, src_ip, src_port, "first non-STUN packet")) {
      return;
    }
    set_ice_state(s, RTC_ICE_CONNECTED);
  } else if (!remote_addr_is_locked_to(s, src_ip, src_port)) {
    emit_logf(s,
              RTC_LOG_WARN,
              "non-STUN packet from unselected tuple src=%s:%u locked=%s:%u",
              src_ip,
              (unsigned)src_port,
              s->remote_ip,
              (unsigned)s->remote_port);
    return;
  }
  if (s->dtls.state == RTC_DTLS_NEW || s->dtls.state == RTC_DTLS_CONNECTING) {
    emit_log(s, RTC_LOG_INFO, "DTLS packet received from peer");
  }
  rtc_dtls_handle_incoming(&s->dtls, s->inbuf, in_len, s->ops.now_ms(s->platform_user));
}

rtc_result_t rtc_session_poll(rtc_session_t *s) {
  uint64_t now;
  if (!s) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!s->started || s->closed) {
    return RTC_OK;
  }

  now = s->ops.now_ms(s->platform_user);
  handle_udp(s);

  if (s->ice.state == RTC_ICE_CONNECTED && s->dtls.state == RTC_DTLS_NEW) {
    emit_log(s, RTC_LOG_INFO, "Starting DTLS handshake");
    rtc_dtls_start(&s->dtls, now);
    set_dtls_state(s, RTC_DTLS_CONNECTING);
  }
  rtc_dtls_poll(&s->dtls, now);
  if (s->dtls.state == RTC_DTLS_FAILED) {
    set_dtls_state(s, RTC_DTLS_FAILED);
  }
  if (s->dtls.state == RTC_DTLS_CONNECTED && !s->sctp.started) {
    rtc_sctp_start(&s->sctp);
    set_dtls_state(s, RTC_DTLS_CONNECTED);
  }
  return RTC_OK;
}

rtc_result_t rtc_channel_open(rtc_session_t *s, const char *label, uint16_t *channel_id) {
  uint16_t id = 0;
  rtc_channel_t *ch = 0;
  if (!s || !label || !channel_id) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!s->sctp.started) {
    return RTC_ERR_STATE;
  }
  if (rtc_sctp_open_channel(&s->sctp, label, &id) != 0 || rtc_dc_add(&s->dcm, id, label, &ch) != 0) {
    return RTC_ERR_STATE;
  }
  *channel_id = id;
  if (s->cbs.on_channel_open) {
    s->cbs.on_channel_open(s->cbs.user, ch->id, ch->label);
  }
  return RTC_OK;
}

rtc_result_t rtc_channel_send(rtc_session_t *s,
                              uint16_t channel_id,
                              const uint8_t *data,
                              size_t len) {
  rtc_channel_t *ch;
  if (!s || (!data && len > 0)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (len > s->cfg.max_message_size) {
    return RTC_ERR_OVERFLOW;
  }
  ch = rtc_dc_find(&s->dcm, channel_id);
  if (!ch || ch->state != RTC_CHANNEL_OPEN) {
    return RTC_ERR_NOT_FOUND;
  }
  if (rtc_sctp_send(&s->sctp, channel_id, data, len) != 0) {
    return RTC_ERR_IO;
  }
  return RTC_OK;
}

rtc_result_t rtc_channel_close(rtc_session_t *s, uint16_t channel_id) {
  if (!s) {
    return RTC_ERR_INVALID_ARG;
  }
  if (rtc_dc_close(&s->dcm, channel_id) != 0) {
    return RTC_ERR_NOT_FOUND;
  }
  if (rtc_sctp_close_channel(&s->sctp, channel_id) != 0) {
    return RTC_ERR_IO;
  }
  return RTC_OK;
}

rtc_result_t rtc_session_close(rtc_session_t *s) {
  if (!s) {
    return RTC_ERR_INVALID_ARG;
  }
  if (s->closed) {
    return RTC_OK;
  }
  s->closed = 1;
  if (s->udp_fd >= 0) {
    s->ops.udp_close(s->platform_user, s->udp_fd);
    s->udp_fd = -1;
  }
  set_ice_state(s, RTC_ICE_CLOSED);
  set_dtls_state(s, RTC_DTLS_CLOSED);
  rtc_dtls_deinit(&s->dtls);
  rtc_sctp_deinit(&s->sctp);
  return RTC_OK;
}

void rtc_session_destroy(rtc_session_t *s) {
  if (!s) {
    return;
  }
  (void)rtc_session_close(s);
  if (s->pool_mem) {
    free(s->pool_mem);
    s->pool_mem = 0;
  }
  free(s);
}
