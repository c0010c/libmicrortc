#include "rtc/rtc.h"

#include <stdio.h>
#include <string.h>

#include "test_sdp_fixtures.h"
#include "test_stun_helpers.h"

#define ASSERT_EQ_INT(expected, actual)                                                   \
  do {                                                                                    \
    int exp_val__ = (expected);                                                           \
    int act_val__ = (actual);                                                             \
    if (exp_val__ != act_val__) {                                                         \
      printf("ASSERT_EQ_INT failed at %s:%d expected=%d actual=%d\n", __FILE__, __LINE__, \
             exp_val__, act_val__);                                                       \
      return 1;                                                                           \
    }                                                                                     \
  } while (0)

#define ASSERT_TRUE(expr)                                                         \
  do {                                                                            \
    if (!(expr)) {                                                                \
      printf("ASSERT_TRUE failed at %s:%d expr=%s\n", __FILE__, __LINE__, #expr); \
      return 1;                                                                   \
    }                                                                             \
  } while (0)

typedef struct interop_log_capture {
  uint32_t infos;
  uint32_t warns;
  uint32_t errors;
  uint32_t seen_engine_created;
  uint32_t seen_peer_created;
  uint32_t seen_peer_start;
  uint32_t seen_ice_checking;
  uint32_t seen_dtls_handshake;
  uint32_t seen_media_connected;
  uint32_t seen_dtls_failed;
} interop_log_capture_t;

typedef struct interop_peer_capture {
  uint32_t state_changes;
  rtc_peer_state_t last_state;
  uint32_t local_description_count;
  uint32_t local_candidate_count;
  char last_local_sdp[RTC_CFG_MAX_SDP_LEN];
  char last_local_type[16];
} interop_peer_capture_t;

static int str_eq(const char *lhs, const char *rhs) {
  if (!lhs || !rhs) {
    return 0;
  }
  return strcmp(lhs, rhs) == 0;
}

static int str_contains(const char *text, const char *needle) {
  if (!text || !needle) {
    return 0;
  }
  return strstr(text, needle) != NULL;
}

static int interop_log_event_matches(const char *message, const char *evt) {
  char expected[64];
  if (!message || !evt) {
    return 0;
  }
  if (snprintf(expected, sizeof(expected), "evt=%s", evt) <= 0) {
    return 0;
  }
  return str_contains(message, expected) && str_contains(message, "state=");
}

static void interop_log_cb(rtc_log_level_t level, const char *module, uint32_t peer_id,
                           rtc_result_t code, const char *message, void *user_data) {
  interop_log_capture_t *cap = (interop_log_capture_t *)user_data;
  (void)peer_id;

  if (!cap) {
    return;
  }

  if (level == RTC_LOG_INFO) {
    cap->infos++;
    if (str_eq(module, "session.engine") &&
        interop_log_event_matches(message, "engine_created")) {
      cap->seen_engine_created++;
    } else if (str_eq(module, "session.peer") &&
               interop_log_event_matches(message, "peer_created")) {
      cap->seen_peer_created++;
    } else if (str_eq(module, "session.peer") &&
               interop_log_event_matches(message, "peer_start")) {
      cap->seen_peer_start++;
    } else if (str_eq(module, "session.peer") &&
               interop_log_event_matches(message, "ice_checking")) {
      cap->seen_ice_checking++;
    } else if (str_eq(module, "session.peer") &&
               interop_log_event_matches(message, "dtls_handshake")) {
      cap->seen_dtls_handshake++;
    } else if (str_eq(module, "session.peer") &&
               interop_log_event_matches(message, "media_connected")) {
      cap->seen_media_connected++;
    }
  } else if (level == RTC_LOG_WARN) {
    cap->warns++;
  } else if (level == RTC_LOG_ERROR) {
    cap->errors++;
    if (str_eq(module, "session.peer") &&
        interop_log_event_matches(message, "dtls_failed") &&
        code == RTC_ERR_DTLS_HANDSHAKE_FAILED) {
      cap->seen_dtls_failed++;
    }
  }
}

static void interop_state_cb(rtc_peer_t *peer, rtc_peer_state_t old_state,
                             rtc_peer_state_t new_state, void *user_data) {
  interop_peer_capture_t *cap = (interop_peer_capture_t *)user_data;
  (void)peer;
  (void)old_state;

  if (!cap) {
    return;
  }

  cap->state_changes++;
  cap->last_state = new_state;
}

static void interop_local_desc_cb(rtc_peer_t *peer, const char *sdp, const char *type,
                                  void *user_data) {
  interop_peer_capture_t *cap = (interop_peer_capture_t *)user_data;
  (void)peer;
  if (!cap || !sdp || !type) {
    return;
  }
  cap->local_description_count++;
  (void)snprintf(cap->last_local_sdp, sizeof(cap->last_local_sdp), "%s", sdp);
  (void)snprintf(cap->last_local_type, sizeof(cap->last_local_type), "%s", type);
}

static void interop_local_cand_cb(rtc_peer_t *peer, const char *candidate, void *user_data) {
  interop_peer_capture_t *cap = (interop_peer_capture_t *)user_data;
  (void)peer;
  if (!cap || !candidate) {
    return;
  }
  cap->local_candidate_count++;
}

static void fill_engine_cfg(rtc_engine_config_t *cfg, interop_log_capture_t *logs) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->version = RTC_API_VERSION;
  cfg->size = (uint16_t)sizeof(*cfg);
  cfg->min_log_level = RTC_LOG_DEBUG;
  cfg->log_cb = interop_log_cb;
  cfg->log_user_data = logs;
  cfg->active_peer_limit = RTC_CFG_MAX_PEERS;
}

static void fill_peer_cfg(rtc_peer_config_t *cfg, interop_peer_capture_t *cap) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->version = RTC_API_VERSION;
  cfg->size = (uint16_t)sizeof(*cfg);
  cfg->max_retries = 5;
  cfg->retry_interval_ms = 10;
  cfg->dtls_handshake_timeout_ms = 3000;
  cfg->dtls_handshake_max_retries = 16;
  cfg->on_state_change = interop_state_cb;
  cfg->on_local_description = interop_local_desc_cb;
  cfg->on_local_candidate = interop_local_cand_cb;
  cfg->user_data = cap;
}

static int interop_remote_stun_responder_step(int remote_fd, const char *offer_ufrag,
                                              const char *offer_pwd,
                                              const char *local_sdp) {
  uint8_t req[RTC_CFG_MTU];
  char local_ufrag[64];
  char username[160];

  if (remote_fd < 0 || !offer_ufrag || !offer_pwd || !local_sdp ||
      local_sdp[0] == '\0') {
    return 0;
  }
  if (!rtc_test_extract_sdp_attr(local_sdp, "a=ice-ufrag:", local_ufrag,
                                 (uint16_t)sizeof(local_ufrag))) {
    return 0;
  }
  if (snprintf(username, sizeof(username), "%s:%s", offer_ufrag, local_ufrag) <=
      0) {
    return 0;
  }

  for (;;) {
    rtc_platform_net_addr_t src;
    uint16_t recv_len = 0u;
    rtc_result_t r = rtc_platform_udp_recvfrom(remote_fd, &src, req,
                                               (uint16_t)sizeof(req), &recv_len);
    if (r == RTC_ERR_TIMEOUT) {
      break;
    }
    if (r != RTC_OK) {
      return 0;
    }
    if (!rtc_test_stun_is_binding_request(req, recv_len)) {
      continue;
    }

    {
      uint8_t tid[12];
      uint8_t rsp[RTC_CFG_MTU];
      uint16_t rsp_len = (uint16_t)sizeof(rsp);
      uint16_t sent_len = 0u;

      if (!rtc_test_stun_get_transaction_id(req, recv_len, tid)) {
        return 0;
      }
      if (!rtc_test_stun_build_binding_response(tid, username, offer_pwd, src.addr,
                                                src.port, rsp, &rsp_len)) {
        return 0;
      }
      if (rtc_platform_udp_sendto(remote_fd, &src, rsp, rsp_len, &sent_len) !=
              RTC_OK ||
          sent_len != rsp_len) {
        return 0;
      }
    }
  }

  return 1;
}

static int test_interop_smoke_skeleton(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  interop_log_capture_t logs;
  interop_peer_capture_t cap;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  int remote_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint16_t remote_port = 0u;
  char remote_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  char offer_ufrag[64];
  char offer_pwd[64];
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_TRUE(engine != NULL);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_TRUE(peer != NULL);

  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711,
                                        "a=ice-ufrag:", offer_ufrag,
                                        (uint16_t)sizeof(offer_ufrag)));
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-pwd:",
                                        offer_pwd, (uint16_t)sizeof(offer_pwd)));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&remote_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(remote_fd, 0u, &remote_port));
  ASSERT_TRUE(rtc_test_build_host_candidate(
      remote_candidate, (uint16_t)sizeof(remote_candidate), remote_port,
      2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 40; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    (void)interop_remote_stun_responder_step(remote_fd, offer_ufrag, offer_pwd,
                                             cap.last_local_sdp);
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_CONNECTED || state == RTC_PEER_STATE_FAILED) {
      break;
    }
  }

  ASSERT_TRUE(state != RTC_PEER_STATE_NEW);
  ASSERT_TRUE(cap.state_changes >= 1);
  ASSERT_TRUE(cap.local_description_count >= 1);
  ASSERT_TRUE(cap.local_candidate_count >= 1);
  ASSERT_TRUE(str_eq(cap.last_local_type, "answer"));
  ASSERT_TRUE(str_contains(cap.last_local_sdp, "a=ice-lite"));
  ASSERT_TRUE(str_contains(cap.last_local_sdp, "a=setup:passive"));
  ASSERT_TRUE(str_contains(cap.last_local_sdp, "a=group:BUNDLE"));
  ASSERT_TRUE(str_contains(cap.last_local_sdp, "a=rtcp-mux"));
  ASSERT_TRUE(str_contains(cap.last_local_sdp, "a=candidate:"));

  ASSERT_TRUE(logs.seen_engine_created >= 1);
  ASSERT_TRUE(logs.seen_peer_created >= 1);
  ASSERT_TRUE(logs.seen_peer_start >= 1);
  ASSERT_TRUE(logs.seen_ice_checking >= 1);
  ASSERT_TRUE(logs.seen_dtls_handshake >= 1);
  ASSERT_TRUE((logs.seen_media_connected + logs.seen_dtls_failed) >= 1);

  rtc_platform_udp_close(&remote_fd);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

int main(void) {
  int failures = 0;
  failures += test_interop_smoke_skeleton();

  if (failures != 0) {
    printf("interop smoke failures: %d\n", failures);
    return 1;
  }

  printf("interop smoke tests passed\n");
  return 0;
}
