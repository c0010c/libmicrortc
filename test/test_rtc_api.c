#include "rtc/rtc.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>
#include <mbedtls/x509_crt.h>
#include <stdio.h>
#include <string.h>

#include "dtls/rtc_dtls_local_cert.h"
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
  uint32_t seen_ice_timeout_failed;
  uint32_t seen_ice_protocol_failed;
  uint32_t seen_ice_resource_failed;
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
  uint8_t state_seq_step;
  uint8_t seen_starting;
  uint8_t seen_ice_checking;
  uint8_t seen_dtls_handshake;
  uint8_t seen_connected;
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
    } else if (test_str_eq(module, "session.peer") &&
               test_str_eq(message, "ice protocol failure") &&
               code == RTC_ERR_PROTOCOL) {
      cap->seen_ice_protocol_failed++;
    } else if (test_str_eq(module, "session.peer") &&
               test_str_eq(message, "ice resource exhausted") &&
               code == RTC_ERR_RESOURCE_EXHAUSTED) {
      cap->seen_ice_resource_failed++;
    }
  } else if (level == RTC_LOG_WARN) {
    if (test_str_eq(module, "session.peer") && test_str_eq(message, "ice timeout") &&
        code == RTC_ERR_TIMEOUT) {
      cap->seen_ice_timeout_failed++;
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
  if (new_state == RTC_PEER_STATE_STARTING) {
    cap->seen_starting = 1u;
    if (cap->state_seq_step == 0u) {
      cap->state_seq_step = 1u;
    }
  } else if (new_state == RTC_PEER_STATE_ICE_CHECKING) {
    cap->seen_ice_checking = 1u;
    if (cap->state_seq_step == 1u) {
      cap->state_seq_step = 2u;
    }
  } else if (new_state == RTC_PEER_STATE_DTLS_HANDSHAKE) {
    cap->seen_dtls_handshake = 1u;
    if (cap->state_seq_step == 2u) {
      cap->state_seq_step = 3u;
    }
  } else if (new_state == RTC_PEER_STATE_CONNECTED) {
    cap->seen_connected = 1u;
    if (cap->state_seq_step == 3u) {
      cap->state_seq_step = 4u;
    }
  }
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

typedef struct test_dtls_in_packet {
  uint16_t len;
  uint8_t data[RTC_CFG_DTLS_MAX_DATAGRAM];
} test_dtls_in_packet_t;

typedef struct test_remote_dtls_peer {
  int remote_fd;
  uint16_t remote_port;
  rtc_platform_net_addr_t peer_addr;
  uint8_t peer_addr_valid;
  uint32_t dtls_in_drop_count;
  uint16_t dtls_in_head;
  uint16_t dtls_in_tail;
  uint16_t dtls_in_count;
  test_dtls_in_packet_t dtls_in[RTC_CFG_DTLS_MAILBOX_CAP];
  char offer_ufrag[64];
  char offer_pwd[64];
  char local_ufrag[64];
  uint8_t have_local_ufrag;
  uint8_t started;
  uint8_t connected;
  uint8_t failed;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cert;
  mbedtls_pk_context key;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context drbg;
  mbedtls_timing_delay_context timer;
} test_remote_dtls_peer_t;

static const char *k_test_dtls_local_fingerprint =
    "B3:7D:98:A3:34:67:93:66:D0:17:9B:08:C5:0E:8B:94:57:B8:DA:E7:F2:A9:05:16:8D:A2:E6:F1:E0:49:A1:EB";

static const mbedtls_ssl_srtp_profile k_test_dtls_profiles[] = {
    MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80,
    MBEDTLS_TLS_SRTP_UNSET,
};

static int test_dtls_is_packet(const uint8_t *buf, uint16_t len) {
  if (!buf || len == 0u) {
    return 0;
  }
  return buf[0] >= 20u && buf[0] <= 63u;
}

static int test_dtls_in_push(test_remote_dtls_peer_t *peer, const uint8_t *buf,
                             uint16_t len) {
  test_dtls_in_packet_t *slot;

  if (!peer || !buf || len == 0u || len > RTC_CFG_DTLS_MAX_DATAGRAM) {
    return 0;
  }
  if (peer->dtls_in_count >= RTC_CFG_DTLS_MAILBOX_CAP) {
    peer->dtls_in_drop_count++;
    return 0;
  }

  slot = &peer->dtls_in[peer->dtls_in_tail];
  slot->len = len;
  memcpy(slot->data, buf, len);
  peer->dtls_in_tail =
      (uint16_t)((peer->dtls_in_tail + 1u) % RTC_CFG_DTLS_MAILBOX_CAP);
  peer->dtls_in_count++;
  return 1;
}

static int test_dtls_in_pop(test_remote_dtls_peer_t *peer, uint8_t *out,
                            uint16_t cap, uint16_t *out_len) {
  test_dtls_in_packet_t *slot;

  if (!peer || !out || !out_len || peer->dtls_in_count == 0u) {
    return 0;
  }
  slot = &peer->dtls_in[peer->dtls_in_head];
  if (slot->len > cap) {
    return 0;
  }
  memcpy(out, slot->data, slot->len);
  *out_len = slot->len;
  peer->dtls_in_head =
      (uint16_t)((peer->dtls_in_head + 1u) % RTC_CFG_DTLS_MAILBOX_CAP);
  peer->dtls_in_count--;
  return 1;
}

static int test_remote_dtls_send_cb(void *ctx, const unsigned char *buf, size_t len) {
  test_remote_dtls_peer_t *peer = (test_remote_dtls_peer_t *)ctx;
  uint16_t sent_len = 0u;
  rtc_result_t r;

  if (!peer || !buf || len == 0u || len > RTC_CFG_DTLS_MAX_DATAGRAM) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  if (peer->remote_fd < 0 || !peer->peer_addr_valid) {
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }

  r = rtc_platform_udp_sendto(peer->remote_fd, &peer->peer_addr, buf, (uint16_t)len,
                              &sent_len);
  if (r == RTC_ERR_TIMEOUT) {
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }
  if (r != RTC_OK || sent_len != len) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }

  return (int)len;
}

static int test_remote_dtls_recv_cb(void *ctx, unsigned char *buf, size_t len) {
  test_remote_dtls_peer_t *peer = (test_remote_dtls_peer_t *)ctx;
  uint16_t packet_len = 0u;

  if (!peer || !buf || len == 0u || len > UINT16_MAX) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  if (!test_dtls_in_pop(peer, buf, (uint16_t)len, &packet_len)) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  return (int)packet_len;
}

static int test_replace_offer_fingerprint(const char *offer_sdp,
                                          const char *fingerprint,
                                          char *out_offer,
                                          uint16_t out_offer_cap) {
  const char *needle = "a=fingerprint:sha-256 ";
  const char *line_start = NULL;
  const char *line_end = NULL;
  size_t prefix_len;
  size_t suffix_len;
  size_t fp_len;
  size_t total_len;

  if (!offer_sdp || !fingerprint || !out_offer || out_offer_cap == 0u) {
    return 0;
  }

  line_start = strstr(offer_sdp, needle);
  if (!line_start) {
    return 0;
  }
  line_end = strchr(line_start, '\n');
  if (!line_end) {
    line_end = offer_sdp + strlen(offer_sdp);
  }

  prefix_len = (size_t)(line_start - offer_sdp) + strlen(needle);
  suffix_len = strlen(line_end);
  fp_len = strlen(fingerprint);
  total_len = prefix_len + fp_len + suffix_len + 1u;
  if (total_len > out_offer_cap) {
    return 0;
  }

  memcpy(out_offer, offer_sdp, prefix_len);
  memcpy(out_offer + prefix_len, fingerprint, fp_len);
  memcpy(out_offer + prefix_len + fp_len, line_end, suffix_len + 1u);
  return 1;
}

static int test_remote_dtls_peer_init(test_remote_dtls_peer_t *peer,
                                      const char *offer_sdp) {
  const char *pers = "test-remote-dtls-client";
  int rc;

  if (!peer || !offer_sdp) {
    return 0;
  }

  memset(peer, 0, sizeof(*peer));
  peer->remote_fd = RTC_PLATFORM_INVALID_SOCKET;

  if (!rtc_test_extract_sdp_attr(offer_sdp, "a=ice-ufrag:", peer->offer_ufrag,
                                 (uint16_t)sizeof(peer->offer_ufrag))) {
    return 0;
  }
  if (!rtc_test_extract_sdp_attr(offer_sdp, "a=ice-pwd:", peer->offer_pwd,
                                 (uint16_t)sizeof(peer->offer_pwd))) {
    return 0;
  }
  if (rtc_platform_udp_create_nonblock(&peer->remote_fd) != RTC_OK) {
    return 0;
  }
  if (rtc_platform_udp_bind(peer->remote_fd, 0u, &peer->remote_port) != RTC_OK) {
    rtc_platform_udp_close(&peer->remote_fd);
    return 0;
  }

  mbedtls_ssl_init(&peer->ssl);
  mbedtls_ssl_config_init(&peer->conf);
  mbedtls_x509_crt_init(&peer->cert);
  mbedtls_pk_init(&peer->key);
  mbedtls_entropy_init(&peer->entropy);
  mbedtls_ctr_drbg_init(&peer->drbg);
  memset(&peer->timer, 0, sizeof(peer->timer));

  rc = mbedtls_ctr_drbg_seed(&peer->drbg, mbedtls_entropy_func, &peer->entropy,
                             (const unsigned char *)pers, strlen(pers));
  if (rc != 0) {
    return 0;
  }
  rc = mbedtls_x509_crt_parse(&peer->cert,
                              (const unsigned char *)g_rtc_dtls_local_cert_pem,
                              sizeof(g_rtc_dtls_local_cert_pem));
  if (rc != 0) {
    return 0;
  }
  rc = mbedtls_pk_parse_key(&peer->key,
                            (const unsigned char *)g_rtc_dtls_local_key_pem,
                            sizeof(g_rtc_dtls_local_key_pem), NULL, 0,
                            mbedtls_ctr_drbg_random, &peer->drbg);
  if (rc != 0) {
    return 0;
  }
  rc = mbedtls_ssl_config_defaults(&peer->conf, MBEDTLS_SSL_IS_CLIENT,
                                   MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                   MBEDTLS_SSL_PRESET_DEFAULT);
  if (rc != 0) {
    return 0;
  }

  mbedtls_ssl_conf_authmode(&peer->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_rng(&peer->conf, mbedtls_ctr_drbg_random, &peer->drbg);
  if (mbedtls_ssl_conf_own_cert(&peer->conf, &peer->cert, &peer->key) != 0) {
    return 0;
  }
  if (mbedtls_ssl_conf_dtls_srtp_protection_profiles(&peer->conf,
                                                      k_test_dtls_profiles) != 0) {
    return 0;
  }
  mbedtls_ssl_conf_dtls_cookies(&peer->conf, NULL, NULL, NULL);
  if (mbedtls_ssl_setup(&peer->ssl, &peer->conf) != 0) {
    return 0;
  }
  mbedtls_ssl_set_bio(&peer->ssl, peer, test_remote_dtls_send_cb,
                      test_remote_dtls_recv_cb, NULL);
  mbedtls_ssl_set_timer_cb(&peer->ssl, &peer->timer, mbedtls_timing_set_delay,
                           mbedtls_timing_get_delay);
  mbedtls_ssl_set_mtu(&peer->ssl, RTC_CFG_MTU);
  return 1;
}

static void test_remote_dtls_peer_deinit(test_remote_dtls_peer_t *peer) {
  if (!peer) {
    return;
  }
  rtc_platform_udp_close(&peer->remote_fd);
  mbedtls_ssl_free(&peer->ssl);
  mbedtls_ssl_config_free(&peer->conf);
  mbedtls_x509_crt_free(&peer->cert);
  mbedtls_pk_free(&peer->key);
  mbedtls_ctr_drbg_free(&peer->drbg);
  mbedtls_entropy_free(&peer->entropy);
  memset(peer, 0, sizeof(*peer));
  peer->remote_fd = RTC_PLATFORM_INVALID_SOCKET;
}

static int test_remote_dtls_peer_step(test_remote_dtls_peer_t *peer,
                                      const char *local_sdp) {
  uint8_t packet[RTC_CFG_MTU];
  char username[160];

  if (!peer) {
    return 0;
  }

  if (!peer->have_local_ufrag && local_sdp && local_sdp[0] != '\0') {
    if (rtc_test_extract_sdp_attr(local_sdp, "a=ice-ufrag:", peer->local_ufrag,
                                  (uint16_t)sizeof(peer->local_ufrag))) {
      peer->have_local_ufrag = 1u;
    }
  }

  for (;;) {
    rtc_platform_net_addr_t src;
    uint16_t recv_len = 0u;
    rtc_result_t r = rtc_platform_udp_recvfrom(peer->remote_fd, &src, packet,
                                               (uint16_t)sizeof(packet), &recv_len);
    if (r == RTC_ERR_TIMEOUT) {
      break;
    }
    if (r != RTC_OK) {
      return 0;
    }

    if (rtc_test_stun_is_binding_request(packet, recv_len)) {
      uint8_t tid[12];
      uint8_t rsp[RTC_CFG_MTU];
      uint16_t rsp_len = (uint16_t)sizeof(rsp);
      uint16_t sent_len = 0u;

      peer->peer_addr = src;
      peer->peer_addr_valid = 1u;
      if (!peer->have_local_ufrag) {
        continue;
      }
      if (!rtc_test_stun_get_transaction_id(packet, recv_len, tid)) {
        return 0;
      }
      if (snprintf(username, sizeof(username), "%s:%s", peer->offer_ufrag,
                   peer->local_ufrag) <= 0) {
        return 0;
      }
      if (!rtc_test_stun_build_binding_response(tid, username, peer->offer_pwd,
                                                src.addr, src.port, rsp, &rsp_len)) {
        return 0;
      }
      if (rtc_platform_udp_sendto(peer->remote_fd, &src, rsp, rsp_len,
                                  &sent_len) != RTC_OK ||
          sent_len != rsp_len) {
        return 0;
      }
      continue;
    }

    if (test_dtls_is_packet(packet, recv_len)) {
      peer->peer_addr = src;
      peer->peer_addr_valid = 1u;
      (void)test_dtls_in_push(peer, packet, recv_len);
    }
  }

  if (!peer->started && peer->have_local_ufrag && peer->peer_addr_valid) {
    peer->started = 1u;
  }

  if (peer->started && !peer->connected && !peer->failed) {
    int hr = mbedtls_ssl_handshake(&peer->ssl);
    if (hr == 0) {
      peer->connected = 1u;
    } else if (hr != MBEDTLS_ERR_SSL_WANT_READ &&
               hr != MBEDTLS_ERR_SSL_WANT_WRITE &&
               hr != MBEDTLS_ERR_SSL_TIMEOUT) {
#ifdef MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS
      if (hr != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
        peer->failed = 1u;
      }
#else
      peer->failed = 1u;
#endif
    }
  }
  return 1;
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

static int test_extract_local_ice_creds(const char *local_sdp, char *out_local_ufrag,
                                        uint16_t out_local_ufrag_cap,
                                        char *out_local_pwd,
                                        uint16_t out_local_pwd_cap) {
  if (!local_sdp) {
    return 0;
  }
  if (out_local_ufrag && out_local_ufrag_cap > 0u) {
    if (!rtc_test_extract_sdp_attr(local_sdp, "a=ice-ufrag:", out_local_ufrag,
                                   out_local_ufrag_cap)) {
      return 0;
    }
  }
  if (out_local_pwd && out_local_pwd_cap > 0u) {
    if (!rtc_test_extract_sdp_attr(local_sdp, "a=ice-pwd:", out_local_pwd,
                                   out_local_pwd_cap)) {
      return 0;
    }
  }
  return 1;
}

static int test_capture_binding_request_source(int remote_fd,
                                               rtc_platform_net_addr_t *out_src) {
  uint8_t req[RTC_CFG_MTU];
  for (;;) {
    rtc_platform_net_addr_t src;
    uint16_t recv_len = 0u;
    rtc_result_t r = rtc_platform_udp_recvfrom(remote_fd, &src, req,
                                               (uint16_t)sizeof(req), &recv_len);
    if (r == RTC_ERR_TIMEOUT) {
      return 0;
    }
    if (r != RTC_OK) {
      return 0;
    }
    if (!rtc_test_stun_is_binding_request(req, recv_len)) {
      continue;
    }
    if (out_src) {
      *out_src = src;
    }
    return 1;
  }
}

static int test_send_binding_response_from_remote(int remote_fd,
                                                  const rtc_platform_net_addr_t *dst,
                                                  const char *username,
                                                  const char *password,
                                                  const uint8_t tid[12]) {
  uint8_t rsp[RTC_CFG_MTU];
  uint16_t rsp_len = (uint16_t)sizeof(rsp);
  uint16_t sent_len = 0u;

  if (remote_fd < 0 || !dst || !username || !password || !tid) {
    return 0;
  }
  if (!rtc_test_stun_build_binding_response(tid, username, password, dst->addr, dst->port,
                                            rsp, &rsp_len)) {
    return 0;
  }
  if (rtc_platform_udp_sendto(remote_fd, dst, rsp, rsp_len, &sent_len) != RTC_OK ||
      sent_len != rsp_len) {
    return 0;
  }
  return 1;
}

static int test_send_binding_request_from_remote(int remote_fd,
                                                 const rtc_platform_net_addr_t *dst,
                                                 const char *username,
                                                 const char *password,
                                                 uint32_t priority,
                                                 const uint8_t tid[12]) {
  uint8_t req[RTC_CFG_MTU];
  uint16_t req_len = (uint16_t)sizeof(req);
  uint16_t sent_len = 0u;

  if (remote_fd < 0 || !dst || !username || !password || !tid) {
    return 0;
  }
  if (!rtc_test_stun_build_binding_request(tid, username, password, priority, req,
                                           &req_len)) {
    return 0;
  }
  if (rtc_platform_udp_sendto(remote_fd, dst, req, req_len, &sent_len) != RTC_OK ||
      sent_len != req_len) {
    return 0;
  }
  return 1;
}

static int poll_until_connected_with_remote(rtc_engine_t *engine, rtc_peer_t *peer,
                                            test_peer_capture_t *cap,
                                            test_remote_dtls_peer_t *remote_peer,
                                            uint32_t *inout_now_ms,
                                            const char *offer_template) {
  uint32_t now_ms = 0u;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  char offer_sdp[RTC_CFG_MAX_SDP_LEN];
  char remote_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  int i;

  ASSERT_TRUE(cap != NULL);
  ASSERT_TRUE(remote_peer != NULL);
  if (inout_now_ms) {
    now_ms = *inout_now_ms;
  }
  ASSERT_TRUE(offer_template != NULL);
  ASSERT_TRUE(test_replace_offer_fingerprint(offer_template,
                                             k_test_dtls_local_fingerprint,
                                             offer_sdp,
                                             (uint16_t)sizeof(offer_sdp)));
  ASSERT_TRUE(test_remote_dtls_peer_init(remote_peer, offer_sdp));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_peer->remote_port, 2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, offer_sdp, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 300; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    ASSERT_TRUE(test_remote_dtls_peer_step(remote_peer, cap->last_local_sdp));
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_CONNECTED) {
      if (inout_now_ms) {
        *inout_now_ms = now_ms;
      }
      return 0;
    }
  }

  printf("peer did not reach connected state\n");
  return 1;
}

static int poll_until_connected(rtc_engine_t *engine, rtc_peer_t *peer,
                                test_peer_capture_t *cap) {
  test_remote_dtls_peer_t remote_peer;
  uint32_t now_ms = 0u;
  memset(&remote_peer, 0, sizeof(remote_peer));
  remote_peer.remote_fd = RTC_PLATFORM_INVALID_SOCKET;
  int r = poll_until_connected_with_remote(engine, peer, cap, &remote_peer, &now_ms,
                                           g_test_chrome_offer_h264_g711);
  test_remote_dtls_peer_deinit(&remote_peer);
  return r;
}

static int poll_until_connected_with_offer(rtc_engine_t *engine, rtc_peer_t *peer,
                                           test_peer_capture_t *cap,
                                           const char *offer_template) {
  test_remote_dtls_peer_t remote_peer;
  uint32_t now_ms = 0u;
  memset(&remote_peer, 0, sizeof(remote_peer));
  remote_peer.remote_fd = RTC_PLATFORM_INVALID_SOCKET;
  int r = poll_until_connected_with_remote(engine, peer, cap, &remote_peer, &now_ms,
                                           offer_template);
  test_remote_dtls_peer_deinit(&remote_peer);
  return r;
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
  ASSERT_TRUE(peer_stats.ice_checks_sent >= 1u);
  ASSERT_TRUE(peer_stats.ice_checks_ok >= 1u);
  ASSERT_EQ_INT(RTC_OK, peer_stats.ice_last_error);
  ASSERT_EQ_INT(RTC_DTLS_STATE_CONNECTED, peer_stats.dtls_state);
  ASSERT_TRUE(peer_stats.srtp_active == 1);
  ASSERT_TRUE(peer_stats.dtls_handshake_elapsed_ms > 0);
  ASSERT_TRUE(peer_stats.dtls_rx_pkts > 0u);
  ASSERT_TRUE(peer_stats.dtls_tx_pkts > 0u);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_get_stats(engine, &engine_stats));
  ASSERT_TRUE(engine_stats.poll_count >= 1);
  ASSERT_TRUE(cap.local_description_count >= 1);
  ASSERT_TRUE(cap.local_candidate_count >= 1);
  ASSERT_TRUE(cap.state_changes >= 1);
  ASSERT_TRUE(cap.seen_starting == 1u);
  ASSERT_TRUE(cap.seen_ice_checking == 1u);
  ASSERT_TRUE(cap.seen_dtls_handshake == 1u);
  ASSERT_TRUE(cap.seen_connected == 1u);
  ASSERT_TRUE(cap.state_seq_step >= 4u);
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

static int test_missing_required_ssrc_fails(void) {
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
  ASSERT_EQ_INT(RTC_ERR_PROTOCOL,
                rtc_peer_set_remote_description(peer, g_test_chrome_offer_missing_ssrc,
                                                "offer"));

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
  test_remote_dtls_peer_t remote_peer;
  char offer_sdp[RTC_CFG_MAX_SDP_LEN];
  char remote_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);
  ASSERT_TRUE(test_replace_offer_fingerprint(g_test_chrome_offer_no_candidate,
                                             k_test_dtls_local_fingerprint,
                                             offer_sdp,
                                             (uint16_t)sizeof(offer_sdp)));
  ASSERT_TRUE(test_remote_dtls_peer_init(&remote_peer, offer_sdp));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_peer.remote_port, 2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, offer_sdp, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 4; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    ASSERT_TRUE(test_remote_dtls_peer_step(&remote_peer, cap.last_local_sdp));
  }
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
  ASSERT_TRUE(state != RTC_PEER_STATE_FAILED);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));

  for (i = 0; i < 300; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    ASSERT_TRUE(test_remote_dtls_peer_step(&remote_peer, cap.last_local_sdp));
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_CONNECTED) {
      break;
    }
  }
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);

  test_remote_dtls_peer_deinit(&remote_peer);
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
  test_remote_dtls_peer_t remote_peer;
  char offer_sdp[RTC_CFG_MAX_SDP_LEN];
  char remote_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);
  ASSERT_TRUE(test_replace_offer_fingerprint(g_test_chrome_offer_no_candidate,
                                             k_test_dtls_local_fingerprint,
                                             offer_sdp,
                                             (uint16_t)sizeof(offer_sdp)));
  ASSERT_TRUE(test_remote_dtls_peer_init(&remote_peer, offer_sdp));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_peer.remote_port, 2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));

  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, offer_sdp, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 300; ++i) {
    now_ms += 10;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500));
    ASSERT_TRUE(test_remote_dtls_peer_step(&remote_peer, cap.last_local_sdp));
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_CONNECTED) {
      break;
    }
  }
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);

  test_remote_dtls_peer_deinit(&remote_peer);
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

static int test_ice_timeout_error_observability(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  rtc_peer_stats_t stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);
  peer_cfg.max_retries = 2u;
  peer_cfg.retry_interval_ms = 10u;

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_no_candidate, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 40; ++i) {
    now_ms += 10u;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500u));
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_FAILED) {
      break;
    }
  }

  ASSERT_EQ_INT(RTC_PEER_STATE_FAILED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_EQ_INT(RTC_ERR_TIMEOUT, stats.ice_last_error);
  ASSERT_TRUE(logs.seen_ice_timeout_failed >= 1u);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_ice_protocol_error_observability(void) {
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
  char local_ufrag[64];
  char username[160];
  rtc_platform_net_addr_t dst;
  uint8_t bad_tid[12];
  int have_local_ufrag = 0;
  int injected = 0;
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  memset(bad_tid, 0xAA, sizeof(bad_tid));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);
  peer_cfg.max_retries = 20u;
  peer_cfg.retry_interval_ms = 10u;
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-ufrag:",
                                        offer_ufrag, (uint16_t)sizeof(offer_ufrag)));
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-pwd:",
                                        offer_pwd, (uint16_t)sizeof(offer_pwd)));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&remote_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(remote_fd, 0u, &remote_port));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_port, 2130707000u));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 400; ++i) {
    now_ms += 10u;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500u));
    if (!have_local_ufrag && cap.local_description_count > 0u) {
      ASSERT_TRUE(test_extract_local_ice_creds(cap.last_local_sdp, local_ufrag,
                                               (uint16_t)sizeof(local_ufrag), NULL,
                                               0u));
      have_local_ufrag = 1;
    }
    if (!injected && have_local_ufrag &&
        test_capture_binding_request_source(remote_fd, &dst)) {
      ASSERT_TRUE(snprintf(username, sizeof(username), "%s:%s", offer_ufrag,
                           local_ufrag) > 0);
      ASSERT_TRUE(test_send_binding_response_from_remote(remote_fd, &dst, username,
                                                         offer_pwd, bad_tid));
      injected = 1;
    }
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_FAILED) {
      break;
    }
  }

  ASSERT_TRUE(injected == 1);
  ASSERT_EQ_INT(RTC_PEER_STATE_FAILED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_EQ_INT(RTC_ERR_PROTOCOL, stats.ice_last_error);
  ASSERT_TRUE(logs.seen_ice_protocol_failed >= 1u);

  rtc_platform_udp_close(&remote_fd);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_ice_resource_exhausted_error_observability(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  rtc_peer_stats_t stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  int known_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint16_t known_port = 0u;
  int overflow_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint16_t overflow_port = 0u;
  char known_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  char offer_ufrag[64];
  char local_ufrag[64];
  char local_pwd[64];
  char username[160];
  rtc_platform_net_addr_t dst;
  uint8_t req_tid[12];
  int exhausted = 0;
  int have_local_creds = 0;
  int injected = 0;
  uint32_t now_ms = 0;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  memset(req_tid, 0x11, sizeof(req_tid));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);
  peer_cfg.max_retries = 30u;
  peer_cfg.retry_interval_ms = 10u;
  ASSERT_TRUE(rtc_test_extract_sdp_attr(g_test_chrome_offer_h264_g711, "a=ice-ufrag:",
                                        offer_ufrag, (uint16_t)sizeof(offer_ufrag)));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&known_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(known_fd, 0u, &known_port));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&overflow_fd));
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_bind(overflow_fd, 0u, &overflow_port));
  ASSERT_TRUE(rtc_test_build_host_candidate(known_candidate,
                                            (uint16_t)sizeof(known_candidate),
                                            known_port, 2130707000u));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, g_test_chrome_offer_h264_g711, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, known_candidate));

  for (i = 0; i < 64; ++i) {
    char candidate[RTC_CFG_MAX_CANDIDATE_LEN];
    uint16_t port = (uint16_t)(41000u + (uint16_t)i);
    rtc_result_t r;
    if (port == overflow_port || port == known_port) {
      continue;
    }
    ASSERT_TRUE(rtc_test_build_host_candidate(candidate, (uint16_t)sizeof(candidate), port,
                                              (uint32_t)(2130706000u + (uint32_t)i)));
    r = rtc_peer_add_remote_candidate(peer, candidate);
    if (r == RTC_ERR_RESOURCE_EXHAUSTED) {
      exhausted = 1;
      break;
    }
    ASSERT_EQ_INT(RTC_OK, r);
  }
  ASSERT_TRUE(exhausted == 1);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 80; ++i) {
    now_ms += 10u;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500u));
    if (!have_local_creds && cap.local_description_count > 0u) {
      ASSERT_TRUE(test_extract_local_ice_creds(cap.last_local_sdp, local_ufrag,
                                               (uint16_t)sizeof(local_ufrag),
                                               local_pwd,
                                               (uint16_t)sizeof(local_pwd)));
      have_local_creds = 1;
    }
    if (!injected && have_local_creds &&
        test_capture_binding_request_source(known_fd, &dst)) {
      ASSERT_TRUE(snprintf(username, sizeof(username), "%s:%s", local_ufrag,
                           offer_ufrag) > 0);
      ASSERT_TRUE(test_send_binding_request_from_remote(
          overflow_fd, &dst, username, local_pwd, 2122260223u, req_tid));
      injected = 1;
    }
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_FAILED) {
      break;
    }
  }

  ASSERT_TRUE(injected == 1);
  ASSERT_EQ_INT(RTC_PEER_STATE_FAILED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_EQ_INT(RTC_ERR_RESOURCE_EXHAUSTED, stats.ice_last_error);
  ASSERT_TRUE(logs.seen_ice_resource_failed >= 1u);

  rtc_platform_udp_close(&known_fd);
  rtc_platform_udp_close(&overflow_fd);
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

static int test_media_send_path_and_stats(void) {
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
  ASSERT_EQ_INT(0, poll_until_connected_with_offer(engine, peer, &cap,
                                                   g_test_chrome_offer_h264_g711_dynamic_pt));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=rtpmap:118 PCMA/8000"));
  ASSERT_TRUE(test_str_contains(cap.last_local_sdp, "a=rtpmap:102 H264/90000"));

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

  ASSERT_EQ_INT(0, cap.video_frames);
  ASSERT_EQ_INT(0, cap.audio_frames);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_TRUE(stats.tx_video_frames >= 1);
  ASSERT_TRUE(stats.tx_audio_frames >= 1);
  ASSERT_EQ_INT(0, stats.rx_video_frames);
  ASSERT_EQ_INT(0, stats.rx_audio_frames);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_invalid_srtp_srtcp_packet_observability(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_stats_t before_stats;
  rtc_peer_stats_t after_stats;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  test_remote_dtls_peer_t remote_peer;
  uint8_t invalid_rtp[12];
  uint8_t invalid_rtcp[8];
  uint16_t sent_len = 0u;
  uint32_t warns_before = 0u;
  uint32_t now_ms = 0u;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&before_stats, 0, sizeof(before_stats));
  memset(&after_stats, 0, sizeof(after_stats));
  memset(&remote_peer, 0, sizeof(remote_peer));
  memset(invalid_rtp, 0, sizeof(invalid_rtp));
  memset(invalid_rtcp, 0, sizeof(invalid_rtcp));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  invalid_rtp[0] = 0x80u;
  invalid_rtp[1] = 96u;
  invalid_rtp[2] = 0x00u;
  invalid_rtp[3] = 0x01u;
  invalid_rtp[4] = 0x11u;
  invalid_rtp[5] = 0x22u;
  invalid_rtp[6] = 0x33u;
  invalid_rtp[7] = 0x44u;
  invalid_rtp[8] = 0x55u;
  invalid_rtp[9] = 0x66u;
  invalid_rtp[10] = 0x77u;
  invalid_rtp[11] = 0x88u;

  invalid_rtcp[0] = 0x80u;
  invalid_rtcp[1] = 200u;
  invalid_rtcp[2] = 0x00u;
  invalid_rtcp[3] = 0x01u;
  invalid_rtcp[4] = 0x99u;
  invalid_rtcp[5] = 0x88u;
  invalid_rtcp[6] = 0x77u;
  invalid_rtcp[7] = 0x66u;

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(0, poll_until_connected_with_remote(engine, peer, &cap, &remote_peer,
                                                    &now_ms, g_test_chrome_offer_h264_g711));
  ASSERT_TRUE(remote_peer.peer_addr_valid == 1u);
  ASSERT_TRUE(remote_peer.remote_fd >= 0);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &before_stats));
  warns_before = logs.warns;

  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_sendto(remote_peer.remote_fd, &remote_peer.peer_addr,
                                                invalid_rtp,
                                                (uint16_t)sizeof(invalid_rtp),
                                                &sent_len));
  ASSERT_EQ_INT((int)sizeof(invalid_rtp), sent_len);
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_sendto(remote_peer.remote_fd, &remote_peer.peer_addr,
                                                invalid_rtcp,
                                                (uint16_t)sizeof(invalid_rtcp),
                                                &sent_len));
  ASSERT_EQ_INT((int)sizeof(invalid_rtcp), sent_len);

  for (i = 0; i < 16; ++i) {
    now_ms += 10u;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500u));
    ASSERT_TRUE(test_remote_dtls_peer_step(&remote_peer, cap.last_local_sdp));
  }

  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &after_stats));
  ASSERT_TRUE(after_stats.srtp_unprotect_fail >= before_stats.srtp_unprotect_fail + 2u);
  ASSERT_EQ_INT((int)before_stats.rtcp_rx_pkts, after_stats.rtcp_rx_pkts);
  ASSERT_TRUE(logs.warns >= warns_before + 1u);

  test_remote_dtls_peer_deinit(&remote_peer);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  return 0;
}

static int test_stop_start_reconnect_rebuilds_srtp(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  rtc_peer_stats_t stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));

  ASSERT_EQ_INT(0, poll_until_connected(engine, peer, &cap));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_TRUE(stats.srtp_active == 1u);

  ASSERT_EQ_INT(RTC_OK, rtc_peer_stop(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
  ASSERT_EQ_INT(RTC_PEER_STATE_STOPPED, state);

  ASSERT_EQ_INT(0, poll_until_connected(engine, peer, &cap));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
  ASSERT_EQ_INT(RTC_PEER_STATE_CONNECTED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_TRUE(stats.srtp_active == 1u);
  ASSERT_EQ_INT(RTC_DTLS_STATE_CONNECTED, stats.dtls_state);

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

static int test_dtls_fingerprint_mismatch_observability(void) {
  rtc_engine_t *engine = NULL;
  rtc_peer_t *peer = NULL;
  rtc_engine_config_t engine_cfg;
  rtc_peer_config_t peer_cfg;
  rtc_peer_state_t state = RTC_PEER_STATE_NEW;
  rtc_peer_stats_t stats;
  test_log_capture_t logs;
  test_peer_capture_t cap;
  test_remote_dtls_peer_t remote_peer;
  char offer_sdp[RTC_CFG_MAX_SDP_LEN];
  char remote_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  uint32_t now_ms = 0u;
  int i;

  memset(&logs, 0, sizeof(logs));
  memset(&cap, 0, sizeof(cap));
  memset(&stats, 0, sizeof(stats));
  fill_engine_cfg(&engine_cfg, &logs);
  fill_peer_cfg(&peer_cfg, &cap);
  ASSERT_TRUE(test_replace_offer_fingerprint(
      g_test_chrome_offer_h264_g711,
      "AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA:AA",
      offer_sdp, (uint16_t)sizeof(offer_sdp)));
  ASSERT_TRUE(test_remote_dtls_peer_init(&remote_peer, offer_sdp));
  ASSERT_TRUE(rtc_test_build_host_candidate(remote_candidate,
                                            (uint16_t)sizeof(remote_candidate),
                                            remote_peer.remote_port, 2130706431u));

  ASSERT_EQ_INT(RTC_OK, rtc_engine_create(&engine_cfg, &engine));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_create(engine, &peer_cfg, &peer));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_set_remote_description(peer, offer_sdp, "offer"));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_add_remote_candidate(peer, remote_candidate));
  ASSERT_EQ_INT(RTC_OK, rtc_peer_start(peer));

  for (i = 0; i < 80; ++i) {
    now_ms += 10u;
    ASSERT_EQ_INT(RTC_OK, rtc_engine_poll(engine, now_ms, 500u));
    ASSERT_TRUE(test_remote_dtls_peer_step(&remote_peer, cap.last_local_sdp));
    ASSERT_EQ_INT(RTC_OK, rtc_peer_get_state(peer, &state));
    if (state == RTC_PEER_STATE_FAILED || state == RTC_PEER_STATE_CONNECTED) {
      break;
    }
  }

  ASSERT_EQ_INT(RTC_PEER_STATE_FAILED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_get_stats(peer, &stats));
  ASSERT_EQ_INT(RTC_ERR_DTLS_HANDSHAKE_FAILED, stats.dtls_last_error);
  ASSERT_EQ_INT(RTC_DTLS_STATE_FAILED, stats.dtls_state);
  ASSERT_TRUE(logs.seen_dtls_failed >= 1u);

  test_remote_dtls_peer_deinit(&remote_peer);
  ASSERT_EQ_INT(RTC_OK, rtc_peer_destroy(peer));
  ASSERT_EQ_INT(RTC_OK, rtc_engine_destroy(engine));
  ASSERT_EQ_INT(0, assert_log_baseline_minimum(&logs, TEST_LOG_PROFILE_DTLS_FAIL));
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
  failures += test_missing_required_ssrc_fails();
  failures += test_offer_without_candidate_then_add_candidate();
  failures += test_candidate_before_offer_is_preserved();
  failures += test_h264_packetization_mode_requires_exact_one();
  failures += test_start_before_offer_waits_and_then_emits_answer();
  failures += test_ice_timeout_error_observability();
  failures += test_ice_protocol_error_observability();
  failures += test_ice_resource_exhausted_error_observability();
  failures += test_resource_exhaustion();
  failures += test_media_send_path_and_stats();
  failures += test_invalid_srtp_srtcp_packet_observability();
  failures += test_stop_start_reconnect_rebuilds_srtp();
  failures += test_queue_overflow_and_datachannel_stub();
  failures += test_dtls_fingerprint_mismatch_observability();
  failures += test_dtls_timeout_and_error_observability();

  if (failures != 0) {
    printf("test failures: %d\n", failures);
    return 1;
  }

  printf("all tests passed\n");
  return 0;
}
