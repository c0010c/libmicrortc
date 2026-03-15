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

typedef struct test_log_capture {
  uint32_t errors;
  uint32_t warns;
  uint32_t infos;
  uint32_t debugs;
  uint32_t seen_engine_created;
  uint32_t seen_engine_destroyed;
  uint32_t seen_peer_created;
  uint32_t seen_remote_description_set;
  uint32_t seen_remote_candidate_added;
  uint32_t seen_peer_start;
  uint32_t seen_ice_checking;
  uint32_t seen_dtls_handshake;
  uint32_t seen_media_connected;
  uint32_t seen_dtls_failed;
  rtc_result_t last_code;
  uint32_t last_peer_id;
  char last_module[24];
  char last_message[96];
} test_log_capture_t;

typedef struct test_peer_capture {
  uint32_t state_changes;
  rtc_peer_state_t old_state;
  rtc_peer_state_t new_state;
  uint32_t local_description_count;
  uint32_t local_candidate_count;
  char last_local_sdp[RTC_CFG_MAX_SDP_LEN];
  char last_local_type[16];
  uint32_t video_frames;
  uint32_t audio_frames;
  uint8_t last_video[64];
  uint16_t last_video_len;
  uint8_t last_audio[64];
  uint16_t last_audio_len;
  rtc_audio_codec_t last_audio_codec;
} test_peer_capture_t;

typedef enum test_log_profile {
  TEST_LOG_PROFILE_CONNECTED = 0,
  TEST_LOG_PROFILE_DTLS_FAIL = 1
} test_log_profile_t;

static int test_str_eq(const char *lhs, const char *rhs) {
  if (!lhs || !rhs) {
    return 0;
  }
  return strcmp(lhs, rhs) == 0;
}

static int test_str_contains(const char *text, const char *needle) {
  if (!text || !needle) {
    return 0;
  }
  return strstr(text, needle) != NULL;
}

static void test_log_cb(rtc_log_level_t level, const char *module, uint32_t peer_id,
                        rtc_result_t code, const char *message, void *user_data) {
  test_log_capture_t *cap = (test_log_capture_t *)user_data;
  if (!cap) {
    return;
  }
  if (level == RTC_LOG_ERROR) {
    cap->errors++;
  } else if (level == RTC_LOG_WARN) {
    cap->warns++;
  } else if (level == RTC_LOG_INFO) {
    cap->infos++;
  } else if (level == RTC_LOG_DEBUG) {
    cap->debugs++;
  }
  cap->last_code = code;
  cap->last_peer_id = peer_id;
  if (module) {
    (void)snprintf(cap->last_module, sizeof(cap->last_module), "%s", module);
  }
  if (message) {
    (void)snprintf(cap->last_message, sizeof(cap->last_message), "%s", message);
  }

  if (level == RTC_LOG_INFO) {
    if (test_str_eq(module, "session.engine") && test_str_eq(message, "engine created")) {
      cap->seen_engine_created++;
    } else if (test_str_eq(module, "session.engine") && test_str_eq(message, "engine destroyed")) {
      cap->seen_engine_destroyed++;
    } else if (test_str_eq(module, "session.peer") && test_str_eq(message, "peer created")) {
      cap->seen_peer_created++;
    } else if (test_str_eq(module, "ice") && test_str_eq(message, "remote description set")) {
      cap->seen_remote_description_set++;
    } else if (test_str_eq(module, "ice") && test_str_eq(message, "remote candidate added")) {
      cap->seen_remote_candidate_added++;
    } else if (test_str_eq(module, "session.peer") && test_str_eq(message, "peer start")) {
      cap->seen_peer_start++;
    } else if (test_str_eq(module, "session.peer") && test_str_eq(message, "ice checking")) {
      cap->seen_ice_checking++;
    } else if (test_str_eq(module, "session.peer") && test_str_eq(message, "dtls handshake")) {
      cap->seen_dtls_handshake++;
    } else if (test_str_eq(module, "session.peer") && test_str_eq(message, "media connected")) {
      cap->seen_media_connected++;
    }
  } else if (level == RTC_LOG_ERROR) {
    if (test_str_eq(module, "session.peer") && test_str_eq(message, "dtls failed") &&
        code == RTC_ERR_DTLS_HANDSHAKE_FAILED) {
      cap->seen_dtls_failed++;
    }
  }
}

static int assert_log_baseline_minimum(const test_log_capture_t *logs,
                                       test_log_profile_t profile) {
  ASSERT_TRUE(logs != NULL);
  ASSERT_TRUE(logs->seen_engine_created >= 1);
  ASSERT_TRUE(logs->seen_engine_destroyed >= 1);
  ASSERT_TRUE(logs->seen_peer_created >= 1);
  ASSERT_TRUE(logs->seen_remote_description_set >= 1);
  ASSERT_TRUE(logs->seen_remote_candidate_added >= 1);
  ASSERT_TRUE(logs->seen_peer_start >= 1);
  ASSERT_TRUE(logs->seen_ice_checking >= 1);
  ASSERT_TRUE(logs->seen_dtls_handshake >= 1);

  if (profile == TEST_LOG_PROFILE_CONNECTED) {
    ASSERT_TRUE(logs->seen_media_connected >= 1);
    ASSERT_TRUE(logs->seen_dtls_failed == 0);
  } else {
    ASSERT_TRUE(logs->seen_dtls_failed >= 1);
    ASSERT_TRUE(logs->seen_media_connected == 0);
  }

  return 0;
}

static void test_state_cb(rtc_peer_t *peer, rtc_peer_state_t old_state,
                          rtc_peer_state_t new_state, void *user_data) {
  test_peer_capture_t *cap = (test_peer_capture_t *)user_data;
  (void)peer;
  if (!cap) {
    return;
  }
  cap->state_changes++;
  cap->old_state = old_state;
  cap->new_state = new_state;
}

static void test_local_desc_cb(rtc_peer_t *peer, const char *sdp, const char *type,
                               void *user_data) {
  test_peer_capture_t *cap = (test_peer_capture_t *)user_data;
  (void)peer;
  if (!cap) {
    return;
  }
  if (!sdp || !type) {
    return;
  }
  cap->local_description_count++;
  (void)snprintf(cap->last_local_sdp, sizeof(cap->last_local_sdp), "%s", sdp);
  (void)snprintf(cap->last_local_type, sizeof(cap->last_local_type), "%s", type);
}

static void test_local_cand_cb(rtc_peer_t *peer, const char *candidate, void *user_data) {
  test_peer_capture_t *cap = (test_peer_capture_t *)user_data;
  (void)peer;
  if (!cap) {
    return;
  }
  if (!candidate) {
    return;
  }
  cap->local_candidate_count++;
}

static void test_video_cb(rtc_peer_t *peer, const uint8_t *payload, uint16_t payload_len,
                          uint32_t timestamp90k, void *user_data) {
  test_peer_capture_t *cap = (test_peer_capture_t *)user_data;
  (void)peer;
  (void)timestamp90k;
  if (!cap || !payload) {
    return;
  }
  cap->video_frames++;
  cap->last_video_len = payload_len > sizeof(cap->last_video) ? sizeof(cap->last_video) : payload_len;
  memcpy(cap->last_video, payload, cap->last_video_len);
}

static void test_audio_cb(rtc_peer_t *peer, rtc_audio_codec_t codec, const uint8_t *payload,
                          uint16_t payload_len, uint32_t timestamp8k, void *user_data) {
  test_peer_capture_t *cap = (test_peer_capture_t *)user_data;
  (void)peer;
  (void)timestamp8k;
  if (!cap || !payload) {
    return;
  }
  cap->audio_frames++;
  cap->last_audio_codec = codec;
  cap->last_audio_len = payload_len > sizeof(cap->last_audio) ? sizeof(cap->last_audio) : payload_len;
  memcpy(cap->last_audio, payload, cap->last_audio_len);
}

static void fill_engine_cfg(rtc_engine_config_t *cfg, test_log_capture_t *logs) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->version = RTC_API_VERSION;
  cfg->size = (uint16_t)sizeof(*cfg);
  cfg->min_log_level = RTC_LOG_DEBUG;
  cfg->log_cb = test_log_cb;
  cfg->log_user_data = logs;
  cfg->active_peer_limit = RTC_CFG_MAX_PEERS;
}

static void fill_peer_cfg(rtc_peer_config_t *cfg, test_peer_capture_t *cap) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->version = RTC_API_VERSION;
  cfg->size = (uint16_t)sizeof(*cfg);
  cfg->max_retries = 5;
  cfg->retry_interval_ms = 10;
  cfg->dtls_handshake_timeout_ms = 3000;
  cfg->dtls_handshake_max_retries = 16;
  cfg->on_state_change = test_state_cb;
  cfg->on_local_description = test_local_desc_cb;
  cfg->on_local_candidate = test_local_cand_cb;
  cfg->on_video_frame = test_video_cb;
  cfg->on_audio_frame = test_audio_cb;
  cfg->user_data = cap;
}

static int test_remote_stun_responder_step(int remote_fd, const char *offer_ufrag,
                                           const char *offer_pwd,
                                           const char *local_sdp) {
  uint8_t req[RTC_CFG_MTU];
  char local_ufrag[64];
  char username[160];

  if (remote_fd < 0 || !offer_ufrag || !offer_pwd || !local_sdp || local_sdp[0] == '\0') {
    return 0;
  }
  if (!rtc_test_extract_sdp_attr(local_sdp, "a=ice-ufrag:", local_ufrag,
                                 (uint16_t)sizeof(local_ufrag))) {
    return 0;
  }
  if (snprintf(username, sizeof(username), "%s:%s", offer_ufrag, local_ufrag) <= 0) {
    return 0;
  }

  for (;;) {
    rtc_platform_net_addr_t src;
    uint16_t recv_len = 0u;
    rtc_result_t r = rtc_platform_udp_recvfrom(remote_fd, &src, req, (uint16_t)sizeof(req),
                                               &recv_len);
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
      if (!rtc_test_stun_build_binding_response(tid, username, offer_pwd, src.addr, src.port,
                                                rsp, &rsp_len)) {
        return 0;
      }
      if (rtc_platform_udp_sendto(remote_fd, &src, rsp, rsp_len, &sent_len) != RTC_OK ||
          sent_len != rsp_len) {
        return 0;
      }
    }
  }

  return 1;
}

static int poll_until_connected(rtc_engine_t *engine, rtc_peer_t *peer,
                                test_peer_capture_t *cap) {
  uint32_t now_ms = 0;
  rtc_peer_state_t state;
  int remote_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint16_t remote_port = 0u;
  char remote_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  char offer_ufrag[64];
  char offer_pwd[64];
  int i;

  ASSERT_TRUE(cap != NULL);
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-ufrag:",
                                        offer_ufrag, (uint16_t)sizeof(offer_ufrag)));
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-pwd:",
                                        offer_pwd, (uint16_t)sizeof(offer_pwd)));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&remote_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(remote_fd, 0u, &remote_port));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_port, 2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 40; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    (void)test_remote_stun_responder_step(remote_fd, offer_ufrag, offer_pwd, cap->last_local_sdp);
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_CONNECTED) {
      rtc_platform_udp_close(&remote_fd);
      return 0;
    }
  }

  rtc_platform_udp_close(&remote_fd);
  printf("peer did not reach connected state\n");
  return 1;
}

static int test_lifecycle_and_connection(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_state_t state;
  rtc_engine_stats_t engine_stats;
  rtc_peer_stats_t peer_stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_TRUE(engine != NULL);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_TRUE(peer != NULL);

  ASSERT_EQ_INT(0, poll_until_connected(engine, peer, &cap));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &peer_stats));
  ASSERT_TRUE(peer_stats.local_candidate_count >= 1);
  ASSERT_TRUE(peer_stats.remote_candidate_count >= 1);
  ASSERT_EQ_INT(RTC_DTLS_STATE_CONNECTED, peer_stats.dtls_state);
  ASSERT_TRUE(peer_stats.srtp_active == 1);
  ASSERT_TRUE(peer_stats.dtls_handshake_elapsed_ms > 0);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_get_stats(engine, &engine_stats));
  ASSERT_TRUE(engine_stats.poll_count >= 1);
  ASSERT_TRUE(cap.local_description_count >= 1);
  ASSERT_TRUE(cap.local_candidate_count >= 1);
  ASSERT_TRUE(cap.state_changes >= 1);
  ASSERT_TRUE(test_str_eq(cap.last_local_type, "answer"));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=ice-lite"));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=setup:passive"));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=rtcp-mux"));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=group:BUNDLE"));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=candidate:"));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_stop(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_stop(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));

  ASSERT_EQ_INT(0, assert_log_baseline_minimum(&logs, TEST_LOG_PROFILE_CONNECTED));
  ASSERT_TRUE(logs.infos >= 1);
  return 0;
}

static int test_invalid_args_and_boundaries(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_engine_create(NULL, &engine));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_engine_create(&engine_cfg, NULL));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_create(engine, NULL, &peer));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_create(engine, &peer_cfg, NULL));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));

  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_get_state(peer, NULL));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_set_remote_description(peer, NULL, "offer"));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_set_remote_description(peer, "v=0", NULL));
  ASSERT_EQ_INT(RTC_ERR_NOT_SUPPORTED, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "answer"));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_add_remote_candidate(peer, NULL));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_send_video_h264(peer, NULL, 1, 0, 1));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_peer_send_audio_g711(peer, RTC_AUDIO_CODEC_PCMA, NULL, 1, 0));

  ASSERT_EQ_INT(RTC_ERR_NOT_SUPPORTED, rtc_peer_datachannel_open(peer, "dc", NULL));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  ASSERT_TRUE(logs.errors >= 1);
  return 0;
}

static int test_remote_candidate_parse_failure_is_explicit(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "offer"));
  ASSERT_EQ_INT(
      RTC_ERR_NOT_SUPPORTED,
      rtc_peer_add_remote_candidate(
          peer, "candidate:1 1 udp 2130706431 127.0.0.1 5000 typ srflx"));
  ASSERT_EQ_INT(0, logs.seen_remote_candidate_added);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_partial_accept_rejects_unsupported_audio(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_opus_only, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 12; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    if (cap.local_description_count > 0u) {
      break;
    }
  }

  ASSERT_TRUE(cap.local_description_count >= 1);
  ASSERT_TRUE(test_str_eq(cap.last_local_type, "answer"));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "m=audio 0 "));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "m=video 9 "));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_reject_all_unsupported_codecs(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_ERR_NOT_SUPPORTED, rtc_peer_set_remote_description(peer, g_test_chrome_offer_unsupported, "offer"));
  ASSERT_TRUE(logs.errors >= 1);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_missing_required_attr_fails(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_ERR_PROTOCOL, rtc_peer_set_remote_description(peer, g_test_chrome_offer_missing_ice_pwd, "offer"));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_offer_without_candidate_then_add_candidate(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;
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
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_no_candidate, "a=ice-ufrag:",
                                        offer_ufrag, (uint16_t)sizeof(offer_ufrag)));
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_no_candidate, "a=ice-pwd:",
                                        offer_pwd, (uint16_t)sizeof(offer_pwd)));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&remote_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(remote_fd, 0u, &remote_port));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_port, 2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_no_candidate, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 4; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    (void)test_remote_stun_responder_step(remote_fd, offer_ufrag, offer_pwd, cap.last_local_sdp);
  }
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
  ASSERT_TRUE(state != RTC_PEER_STATE_FAILED);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));

  for (i = 0; i < 40; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    (void)test_remote_stun_responder_step(remote_fd, offer_ufrag, offer_pwd, cap.last_local_sdp);
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_CONNECTED) {
      break;
    }
  }
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);

  rtc_platform_udp_close(&remote_fd);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_candidate_before_offer_is_preserved(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;
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
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_no_candidate, "a=ice-ufrag:",
                                        offer_ufrag, (uint16_t)sizeof(offer_ufrag)));
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_no_candidate, "a=ice-pwd:",
                                        offer_pwd, (uint16_t)sizeof(offer_pwd)));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&remote_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(remote_fd, 0u, &remote_port));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_port, 2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_no_candidate, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 40; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    (void)test_remote_stun_responder_step(remote_fd, offer_ufrag, offer_pwd, cap.last_local_sdp);
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_CONNECTED) {
      break;
    }
  }
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);

  rtc_platform_udp_close(&remote_fd);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_h264_packetization_mode_requires_exact_one(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_pm10_then_pm1, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 20; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    if (cap.local_description_count > 0u) {
      break;
    }
  }

  ASSERT_TRUE(cap.local_description_count >= 1);
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=rtpmap:97 H264/90000"));
  ASSERT_TRUE(!test_str_contains(cap.last_local_sdp, "a=rtpmap:96 H264/90000"));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_start_before_offer_waits_and_then_emits_answer(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 2; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
  }
  ASSERT_EQ_INT(0, cap.local_description_count);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, g_test_remote_candidate_host));

  for (i = 0; i < 20; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    if (cap.local_description_count > 0u) {
      break;
    }
  }

  ASSERT_TRUE(cap.local_description_count >= 1);
  ASSERT_TRUE(test_str_eq(cap.last_local_type, "answer"));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_resource_exhaustion(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peers[RTC_CFG_MAX_PEERS + 1];
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  uint32_t i;

  memset(peers, 0, sizeof(peers));
  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));

  for (i = 0; i < RTC_CFG_MAX_PEERS; ++i) {
    ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peers[i]));
    ASSERT_TRUE(peers[i] != NULL);
  }

  ASSERT_EQ_INT(RTC_ERR_RESOURCE_EXHAUSTED,
                rtc_peer_create(engine, &peer_cfg, &peers[RTC_CFG_MAX_PEERS]));

  for (i = 0; i < RTC_CFG_MAX_PEERS; ++i) {
    ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peers[i]));
  }

  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_media_loopback_and_stats(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_stats_t stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  uint8_t video_payload[] = {0x65, 0x88, 0x84, 0x21, 0xA0};
  uint8_t audio_payload[] = {0x7F, 0x80, 0x81, 0x82};
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(0, poll_until_connected(engine, peer, &cap));

  ASSERT_EQ_INT(RTC_OK,
                rtc_peer_send_video_h264(peer, video_payload, (uint16_t)sizeof(video_payload),
                                         90000, 1));
  ASSERT_EQ_INT(RTC_OK,
                rtc_peer_send_audio_g711(peer, RTC_AUDIO_CODEC_PCMA, audio_payload,
                                         (uint16_t)sizeof(audio_payload), 8000));

  for (i = 0; i < 10; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
  }

  ASSERT_TRUE(cap.video_frames >= 1);
  ASSERT_TRUE(cap.audio_frames >= 1);
  ASSERT_EQ_INT((int)sizeof(video_payload), cap.last_video_len);
  ASSERT_EQ_INT((int)sizeof(audio_payload), cap.last_audio_len);
  ASSERT_TRUE(memcmp(video_payload, cap.last_video, sizeof(video_payload)) == 0);
  ASSERT_TRUE(memcmp(audio_payload, cap.last_audio, sizeof(audio_payload)) == 0);
  ASSERT_EQ_INT(RTC_AUDIO_CODEC_PCMA, cap.last_audio_codec);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_TRUE(stats.tx_video_frames >= 1);
  ASSERT_TRUE(stats.tx_audio_frames >= 1);
  ASSERT_TRUE(stats.rx_video_frames >= 1);
  ASSERT_TRUE(stats.rx_audio_frames >= 1);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_queue_overflow_and_datachannel_stub(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_stats_t stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  uint8_t video_payload[] = {0x65, 0x44, 0x12};
  int overflow_seen = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(0, poll_until_connected(engine, peer, &cap));

  for (i = 0; i < 64; ++i) {
    rtc_result_t r = rtc_peer_send_video_h264(peer, video_payload,
                                              (uint16_t)sizeof(video_payload),
                                              (uint32_t)(90000 + i * 3000), 1);
    if (r == RTC_ERR_OVERFLOW) {
      overflow_seen = 1;
      break;
    }
    ASSERT_EQ_INT(RTC_OK, r);
  }

  ASSERT_TRUE(overflow_seen == 1);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_TRUE(stats.dropped_packets >= 1);

  ASSERT_EQ_INT(RTC_ERR_NOT_SUPPORTED, rtc_peer_datachannel_open(peer, "dc", NULL));
  ASSERT_EQ_INT(RTC_ERR_NOT_SUPPORTED,
                rtc_peer_datachannel_send(peer, 0, video_payload,
                                          (uint16_t)sizeof(video_payload)));
  ASSERT_EQ_INT(RTC_ERR_NOT_SUPPORTED, rtc_peer_datachannel_close(peer, 0));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  ASSERT_TRUE(logs.warns >= 1);
  return 0;
}

static int test_dtls_timeout_and_error_observability(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  rtc_peer_stats_t stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  int remote_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint16_t remote_port = 0u;
  char remote_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  char offer_ufrag[64];
  char offer_pwd[64];
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-ufrag:",
                                        offer_ufrag, (uint16_t)sizeof(offer_ufrag)));
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-pwd:",
                                        offer_pwd, (uint16_t)sizeof(offer_pwd)));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&remote_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(remote_fd, 0u, &remote_port));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_port, 2130706431u));

  peer_cfg.dtls_handshake_timeout_ms = 1;
  peer_cfg.dtls_handshake_max_retries = 1;

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 40; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    (void)test_remote_stun_responder_step(remote_fd, offer_ufrag, offer_pwd, cap.last_local_sdp);
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_FAILED) {
      break;
    }
  }

  ASSERT_EQ_INT(RTC_PEER_STATE_FAILED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_EQ_INT(RTC_ERR_DTLS_HANDSHAKE_FAILED, stats.dtls_last_error);
  ASSERT_EQ_INT(RTC_DTLS_STATE_FAILED, stats.dtls_state);
  ASSERT_TRUE(logs.errors >= 1);

  rtc_platform_udp_close(&remote_fd);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  ASSERT_EQ_INT(0, assert_log_baseline_minimum(&logs, TEST_LOG_PROFILE_DTLS_FAIL));
  return 0;
}

int main(void) {
  int failures = 0;

  failures += test_lifecycle_and_connection();
  failures += test_invalid_args_and_boundaries();
  failures += test_remote_candidate_parse_failure_is_explicit();
  failures += test_partial_accept_rejects_unsupported_audio();
  failures += test_reject_all_unsupported_codecs();
  failures += test_missing_required_attr_fails();
  failures += test_offer_without_candidate_then_add_candidate();
  failures += test_candidate_before_offer_is_preserved();
  failures += test_h264_packetization_mode_requires_exact_one();
  failures += test_start_before_offer_waits_and_then_emits_answer();
  failures += test_resource_exhaustion();
  failures += test_media_loopback_and_stats();
  failures += test_queue_overflow_and_datachannel_stub();
  failures += test_dtls_timeout_and_error_observability();

  if (failures != 0) {
    printf("test failures: %d\n", failures);
    return 1;
  }

  printf("all tests passed\n");
  return 0;
}
