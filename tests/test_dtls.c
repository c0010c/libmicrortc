#include "test_common.h"

#include <stdint.h>
#include <string.h>

#include "dtls.h"

#if RTC_WITH_MBEDTLS
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>
#endif

static int fake_send(void *user, const uint8_t *data, size_t len) {
  (void)user;
  (void)data;
  return (int)len;
}

#if RTC_WITH_MBEDTLS
#define TEST_DTLS_MAX_PACKET 2048
#define TEST_DTLS_MAX_QUEUE 16

typedef struct {
  uint8_t pkts[TEST_DTLS_MAX_QUEUE][TEST_DTLS_MAX_PACKET];
  size_t lens[TEST_DTLS_MAX_QUEUE];
  size_t head;
  size_t count;
} pkt_queue_t;

typedef struct {
  pkt_queue_t c2s;
  pkt_queue_t s2c;
  int server_error_count;
  char last_server_error[256];
} dtls_loop_t;

static int pkt_queue_push(pkt_queue_t *q, const uint8_t *data, size_t len) {
  size_t idx;
  if (!q || !data || len == 0 || len > TEST_DTLS_MAX_PACKET || q->count >= TEST_DTLS_MAX_QUEUE) {
    return -1;
  }
  idx = (q->head + q->count) % TEST_DTLS_MAX_QUEUE;
  memcpy(q->pkts[idx], data, len);
  q->lens[idx] = len;
  q->count += 1;
  return 0;
}

static int pkt_queue_pop(pkt_queue_t *q, uint8_t *out, size_t cap, size_t *out_len) {
  if (!q || !out || !out_len || q->count == 0) {
    return -1;
  }
  if (q->lens[q->head] > cap) {
    return -1;
  }
  memcpy(out, q->pkts[q->head], q->lens[q->head]);
  *out_len = q->lens[q->head];
  q->head = (q->head + 1) % TEST_DTLS_MAX_QUEUE;
  q->count -= 1;
  return 0;
}

static int server_send_capture(void *user, const uint8_t *data, size_t len) {
  dtls_loop_t *loop = (dtls_loop_t *)user;
  if (!loop || pkt_queue_push(&loop->s2c, data, len) != 0) {
    return -1;
  }
  return (int)len;
}

static void server_error_capture(void *user, const char *msg) {
  dtls_loop_t *loop = (dtls_loop_t *)user;
  if (!loop) {
    return;
  }
  loop->server_error_count += 1;
  if (msg) {
    size_t n = strlen(msg);
    if (n >= sizeof(loop->last_server_error)) {
      n = sizeof(loop->last_server_error) - 1;
    }
    memcpy(loop->last_server_error, msg, n);
    loop->last_server_error[n] = '\0';
  } else {
    loop->last_server_error[0] = '\0';
  }
}

static int client_send_capture(void *ctx, const unsigned char *buf, size_t len) {
  dtls_loop_t *loop = (dtls_loop_t *)ctx;
  if (!loop || pkt_queue_push(&loop->c2s, buf, len) != 0) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  return (int)len;
}

static int client_recv_capture(void *ctx, unsigned char *buf, size_t len) {
  dtls_loop_t *loop = (dtls_loop_t *)ctx;
  size_t pkt_len = 0;
  if (!loop || !buf || len == 0) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  if (loop->s2c.count == 0) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  if (pkt_queue_pop(&loop->s2c, buf, len, &pkt_len) != 0) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  return (int)pkt_len;
}

static void init_client_ssl(mbedtls_ssl_context *ssl,
                            mbedtls_ssl_config *conf,
                            mbedtls_entropy_context *entropy,
                            mbedtls_ctr_drbg_context *ctr_drbg,
                            mbedtls_timing_delay_context *timer,
                            dtls_loop_t *loop) {
  static const char kPers[] = "rtc-s1-test-dtls-client";
  int rc;

  mbedtls_ssl_init(ssl);
  mbedtls_ssl_config_init(conf);
  mbedtls_entropy_init(entropy);
  mbedtls_ctr_drbg_init(ctr_drbg);

  rc = mbedtls_ctr_drbg_seed(ctr_drbg,
                             mbedtls_entropy_func,
                             entropy,
                             (const unsigned char *)kPers,
                             sizeof(kPers) - 1);
  ASSERT_EQ_INT(0, rc);

  rc = mbedtls_ssl_config_defaults(conf,
                                   MBEDTLS_SSL_IS_CLIENT,
                                   MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                   MBEDTLS_SSL_PRESET_DEFAULT);
  ASSERT_EQ_INT(0, rc);

  mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, ctr_drbg);

  rc = mbedtls_ssl_setup(ssl, conf);
  ASSERT_EQ_INT(0, rc);

  mbedtls_ssl_set_bio(ssl, loop, client_send_capture, client_recv_capture, 0);
  mbedtls_ssl_set_timer_cb(ssl, timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);
}

static void free_client_ssl(mbedtls_ssl_context *ssl,
                            mbedtls_ssl_config *conf,
                            mbedtls_entropy_context *entropy,
                            mbedtls_ctr_drbg_context *ctr_drbg) {
  mbedtls_ssl_free(ssl);
  mbedtls_ssl_config_free(conf);
  mbedtls_ctr_drbg_free(ctr_drbg);
  mbedtls_entropy_free(entropy);
}

static void test_mbedtls_server_handles_hello_verify_retry(void) {
  dtls_loop_t loop;
  rtc_dtls_t server;
  mbedtls_ssl_context client_ssl;
  mbedtls_ssl_config client_conf;
  mbedtls_entropy_context client_entropy;
  mbedtls_ctr_drbg_context client_ctr_drbg;
  mbedtls_timing_delay_context client_timer;
  uint8_t pkt[TEST_DTLS_MAX_PACKET];
  size_t pkt_len = 0;
  int rc;
  int step;
  int client_done = 0;
  uint64_t now_ms = 1000;
  const uint8_t peer_id[] = "127.0.0.1:9999";

  memset(&loop, 0, sizeof(loop));
  memset(&client_timer, 0, sizeof(client_timer));

  rtc_dtls_init(&server, 0, 0);
  ASSERT_TRUE(server.using_mbedtls);
  rtc_dtls_set_io(&server, server_send_capture, &loop);
  rtc_dtls_set_callbacks(&server, 0, server_error_capture, &loop);
  ASSERT_EQ_INT(0, rtc_dtls_set_peer_transport_id(&server, peer_id, sizeof(peer_id) - 1));
  rtc_dtls_start(&server, now_ms++);
  ASSERT_EQ_INT(RTC_DTLS_CONNECTING, server.state);

  init_client_ssl(&client_ssl, &client_conf, &client_entropy, &client_ctr_drbg, &client_timer, &loop);

  rc = mbedtls_ssl_handshake(&client_ssl);
  ASSERT_TRUE(rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE);
  ASSERT_TRUE(loop.c2s.count > 0);

  ASSERT_EQ_INT(0, pkt_queue_pop(&loop.c2s, pkt, sizeof(pkt), &pkt_len));
  rtc_dtls_handle_incoming(&server, pkt, pkt_len, now_ms++);

  ASSERT_EQ_INT(RTC_DTLS_CONNECTING, server.state);
  ASSERT_EQ_INT(0, loop.server_error_count);
  ASSERT_TRUE(loop.s2c.count > 0);

  for (step = 0; step < 256 && (!client_done || server.state != RTC_DTLS_CONNECTED); ++step) {
    rc = mbedtls_ssl_handshake(&client_ssl);
    if (rc == 0) {
      client_done = 1;
    } else {
      ASSERT_TRUE(rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE);
    }

    while (loop.c2s.count > 0) {
      ASSERT_EQ_INT(0, pkt_queue_pop(&loop.c2s, pkt, sizeof(pkt), &pkt_len));
      rtc_dtls_handle_incoming(&server, pkt, pkt_len, now_ms++);
      ASSERT_TRUE(server.state != RTC_DTLS_FAILED);
    }

    rtc_dtls_poll(&server, now_ms++);
  }

  ASSERT_EQ_INT(RTC_DTLS_CONNECTED, server.state);
  ASSERT_TRUE(client_done);
  ASSERT_EQ_INT(0, loop.server_error_count);

  free_client_ssl(&client_ssl, &client_conf, &client_entropy, &client_ctr_drbg);
  rtc_dtls_deinit(&server);
}
#endif

int main(void) {
  rtc_dtls_t dtls;

  rtc_dtls_init(&dtls, 0, 0);
  rtc_dtls_set_io(&dtls, fake_send, 0);
  rtc_dtls_start(&dtls, 1000);
  ASSERT_EQ_INT(RTC_DTLS_CONNECTING, dtls.state);

  rtc_dtls_poll(&dtls, 2000);

#if RTC_WITH_MBEDTLS
  if (dtls.using_mbedtls) {
    ASSERT_EQ_INT(RTC_DTLS_CONNECTING, dtls.state);
  } else {
    ASSERT_EQ_INT(RTC_DTLS_CONNECTED, dtls.state);
  }
#else
  ASSERT_EQ_INT(RTC_DTLS_CONNECTED, dtls.state);
#endif

  rtc_dtls_deinit(&dtls);

#if RTC_WITH_MBEDTLS
  test_mbedtls_server_handles_hello_verify_retry();
#endif

  return 0;
}
