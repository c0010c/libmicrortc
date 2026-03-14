#include "test_common.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rtc/rtc.h"
#include "stun.h"

typedef struct {
  char src_ip[64];
  uint16_t src_port;
  uint8_t data[256];
  size_t len;
} rx_packet_t;

typedef struct {
  char dst_ip[64];
  uint16_t dst_port;
  uint8_t data[256];
  size_t len;
} tx_packet_t;

typedef struct {
  uint64_t now;
  rx_packet_t rx[32];
  size_t rx_head;
  size_t rx_tail;
  tx_packet_t tx[128];
  size_t tx_count;
  char logs[128][192];
  size_t log_count;
  rtc_ice_state_t ice_states[16];
  size_t ice_count;
} fake_net_t;

static void wr16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)v;
}

static void wr32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

static size_t build_binding_request(uint8_t *out,
                                    size_t cap,
                                    const uint8_t txn[12],
                                    int with_use_candidate,
                                    const char *username) {
  size_t off = 0;
  size_t uname_len = (username && username[0]) ? strlen(username) : 0;
  size_t uname_padded = (uname_len + 3u) & ~3u;
  uint16_t body_len = (uint16_t)((with_use_candidate ? 4u : 0u) + (uname_len > 0 ? (4u + uname_padded) : 0u));
  if (!out || !txn || cap < 20u + (size_t)body_len) {
    return 0;
  }
  wr16(out + off, 0x0001);
  off += 2;
  wr16(out + off, body_len);
  off += 2;
  wr32(out + off, RTC_STUN_MAGIC_COOKIE);
  off += 4;
  memcpy(out + off, txn, 12);
  off += 12;
  if (uname_len > 0) {
    wr16(out + off, 0x0006);
    off += 2;
    wr16(out + off, (uint16_t)uname_len);
    off += 2;
    memcpy(out + off, username, uname_len);
    off += uname_len;
    while ((off % 4u) != 0u) {
      out[off++] = 0;
    }
  }
  if (with_use_candidate) {
    wr16(out + off, 0x0025);
    off += 2;
    wr16(out + off, 0);
    off += 2;
  }
  return off;
}

static void net_reset(fake_net_t *n) {
  if (!n) {
    return;
  }
  memset(n, 0, sizeof(*n));
}

static int net_push_rx(fake_net_t *n, const char *src_ip, uint16_t src_port, const uint8_t *data, size_t len) {
  size_t slot;
  if (!n || !src_ip || !data || len == 0 || len > sizeof(n->rx[0].data)) {
    return -1;
  }
  slot = n->rx_tail % (sizeof(n->rx) / sizeof(n->rx[0]));
  snprintf(n->rx[slot].src_ip, sizeof(n->rx[slot].src_ip), "%s", src_ip);
  n->rx[slot].src_port = src_port;
  memcpy(n->rx[slot].data, data, len);
  n->rx[slot].len = len;
  n->rx_tail++;
  return 0;
}

static int log_contains(const fake_net_t *n, const char *needle) {
  size_t i;
  if (!n || !needle) {
    return 0;
  }
  for (i = 0; i < n->log_count; ++i) {
    if (strstr(n->logs[i], needle)) {
      return 1;
    }
  }
  return 0;
}

static size_t log_count_contains(const fake_net_t *n, const char *needle) {
  size_t i;
  size_t count = 0;
  if (!n || !needle) {
    return 0;
  }
  for (i = 0; i < n->log_count; ++i) {
    if (strstr(n->logs[i], needle)) {
      count++;
    }
  }
  return count;
}

static int ice_seen(const fake_net_t *n, rtc_ice_state_t st) {
  size_t i;
  if (!n) {
    return 0;
  }
  for (i = 0; i < n->ice_count; ++i) {
    if (n->ice_states[i] == st) {
      return 1;
    }
  }
  return 0;
}

static void on_ice(void *u, rtc_ice_state_t st) {
  fake_net_t *n = (fake_net_t *)u;
  if (!n) {
    return;
  }
  if (n->ice_count < (sizeof(n->ice_states) / sizeof(n->ice_states[0]))) {
    n->ice_states[n->ice_count++] = st;
  }
}

static int f_udp_open(void *u, const char *ip, uint16_t p) {
  (void)u;
  (void)ip;
  (void)p;
  return 1;
}

static int f_udp_send(void *u,
                      int fd,
                      const char *dst_ip,
                      uint16_t dst_port,
                      const uint8_t *data,
                      size_t len) {
  fake_net_t *n = (fake_net_t *)u;
  size_t slot;
  (void)fd;
  if (!n || !dst_ip || !data || len == 0 || len > sizeof(n->tx[0].data)) {
    return -1;
  }
  slot = n->tx_count % (sizeof(n->tx) / sizeof(n->tx[0]));
  snprintf(n->tx[slot].dst_ip, sizeof(n->tx[slot].dst_ip), "%s", dst_ip);
  n->tx[slot].dst_port = dst_port;
  memcpy(n->tx[slot].data, data, len);
  n->tx[slot].len = len;
  n->tx_count++;
  return (int)len;
}

static int f_udp_recv(void *u,
                      int fd,
                      char *src_ip,
                      size_t src_ip_len,
                      uint16_t *src_port,
                      uint8_t *buf,
                      size_t cap,
                      size_t *out_len) {
  fake_net_t *n = (fake_net_t *)u;
  size_t slot;
  rx_packet_t *pkt;
  (void)fd;
  if (!n || !buf || !out_len) {
    return -1;
  }
  if (n->rx_head == n->rx_tail) {
    *out_len = 0;
    return 0;
  }
  slot = n->rx_head % (sizeof(n->rx) / sizeof(n->rx[0]));
  pkt = &n->rx[slot];
  if (pkt->len > cap) {
    *out_len = 0;
    return -1;
  }
  if (src_ip && src_ip_len > 0) {
    snprintf(src_ip, src_ip_len, "%s", pkt->src_ip);
  }
  if (src_port) {
    *src_port = pkt->src_port;
  }
  memcpy(buf, pkt->data, pkt->len);
  *out_len = pkt->len;
  pkt->len = 0;
  n->rx_head++;
  return 1;
}

static void f_udp_close(void *u, int fd) {
  (void)u;
  (void)fd;
}

static uint64_t f_now_ms(void *u) {
  fake_net_t *n = (fake_net_t *)u;
  return n ? n->now : 0;
}

static int f_timer_arm(void *u, uint32_t id, uint64_t d) {
  (void)u;
  (void)id;
  (void)d;
  return 0;
}

static int f_timer_cancel(void *u, uint32_t id) {
  (void)u;
  (void)id;
  return 0;
}

static int f_rand(void *u, uint8_t *out, size_t len) {
  size_t i;
  (void)u;
  for (i = 0; i < len; ++i) {
    out[i] = (uint8_t)i;
  }
  return 0;
}

static void f_log(void *u, rtc_log_level_t l, const char *m) {
  fake_net_t *n = (fake_net_t *)u;
  size_t slot;
  (void)l;
  if (!n || !m) {
    return;
  }
  if (n->log_count >= (sizeof(n->logs) / sizeof(n->logs[0]))) {
    return;
  }
  slot = n->log_count++;
  snprintf(n->logs[slot], sizeof(n->logs[slot]), "%s", m);
}

static void fill_ops(rtc_platform_ops_t *ops) {
  memset(ops, 0, sizeof(*ops));
  ops->udp_open = f_udp_open;
  ops->udp_send = f_udp_send;
  ops->udp_recv = f_udp_recv;
  ops->udp_close = f_udp_close;
  ops->now_ms = f_now_ms;
  ops->timer_arm = f_timer_arm;
  ops->timer_cancel = f_timer_cancel;
  ops->rand_bytes = f_rand;
  ops->log = f_log;
}

static void test_basic_session_flow_no_stun(void) {
  fake_net_t net;
  rtc_platform_ops_t ops;
  rtc_config_t cfg;
  rtc_callbacks_t cbs;
  rtc_session_t *s;
  char answer[512];
  size_t answer_len = 0;
  uint16_t cid = 0;

  net_reset(&net);
  fill_ops(&ops);
  memset(&cfg, 0, sizeof(cfg));
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_ice_state = on_ice;
  cbs.user = &net;

  cfg.stun_server_ip = 0;
  cfg.stun_server_port = 0;
  cfg.max_channels = 2;
  cfg.max_message_size = 100;

  s = rtc_session_create(&cfg, &ops, &net, &cbs);
  ASSERT_TRUE(s != 0);
  ASSERT_EQ_INT(RTC_OK,
                rtc_session_set_remote_offer(s,
                                             "v=0\r\n"
                                             "a=ice-ufrag:remoteufrag\r\n"
                                             "a=ice-pwd:remotepwdxxxxxxxxxxxxxxxxxx\r\n"
                                             "a=candidate:1 1 UDP 2130706431 1.1.1.1 10000 typ host\r\n"));
  ASSERT_EQ_INT(RTC_OK, rtc_session_create_answer(s, answer, sizeof(answer), &answer_len));
  ASSERT_TRUE(answer_len > 0);
  ASSERT_EQ_INT(RTC_OK, rtc_session_start(s));
  ASSERT_TRUE(ice_seen(&net, RTC_ICE_CONNECTED));

  net.now += 200;
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));
  net.now += 200;
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));

  ASSERT_EQ_INT(RTC_OK, rtc_channel_open(s, "chat", &cid));
  ASSERT_EQ_INT(RTC_OK, rtc_channel_send(s, cid, (const uint8_t *)"ok", 2));
  ASSERT_EQ_INT(RTC_OK, rtc_channel_close(s, cid));

  ASSERT_EQ_INT(RTC_OK, rtc_session_close(s));
  rtc_session_destroy(s);
}

static void test_tuple_lock_by_use_candidate(void) {
  fake_net_t net;
  rtc_platform_ops_t ops;
  rtc_config_t cfg;
  rtc_callbacks_t cbs;
  rtc_session_t *s;
  uint8_t req[64];
  uint8_t txn_a[12] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  uint8_t txn_b[12] = {11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  uint8_t txn_c[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  const char *good_username = "0001020304050607:remoteufrag";
  size_t req_len;

  net_reset(&net);
  fill_ops(&ops);
  memset(&cfg, 0, sizeof(cfg));
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_ice_state = on_ice;
  cbs.user = &net;

  cfg.stun_server_ip = "9.9.9.9";
  cfg.stun_server_port = 3478;
  cfg.max_channels = 2;
  cfg.max_message_size = 100;
  cfg.log_level = RTC_LOG_DEBUG;

  s = rtc_session_create(&cfg, &ops, &net, &cbs);
  ASSERT_TRUE(s != 0);
  ASSERT_EQ_INT(RTC_OK,
                rtc_session_set_remote_offer(s,
                                             "v=0\r\n"
                                             "a=ice-ufrag:remoteufrag\r\n"
                                             "a=ice-pwd:remotepwdxxxxxxxxxxxxxxxxxx\r\n"
                                             "a=candidate:1 1 UDP 2130706431 203.0.113.9 50000 typ host\r\n"));
  ASSERT_EQ_INT(RTC_OK, rtc_session_start(s));
  ASSERT_TRUE(ice_seen(&net, RTC_ICE_CHECKING));

  req_len = build_binding_request(req, sizeof(req), txn_a, 0, good_username);
  ASSERT_TRUE(req_len > 0);
  ASSERT_EQ_INT(0, net_push_rx(&net, "2.2.2.2", 50000, req, req_len));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));
  ASSERT_TRUE(!ice_seen(&net, RTC_ICE_CONNECTED));

  req_len = build_binding_request(req, sizeof(req), txn_b, 1, good_username);
  ASSERT_TRUE(req_len > 0);
  ASSERT_EQ_INT(0, net_push_rx(&net, "2.2.2.2", 50001, req, req_len));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));
  ASSERT_TRUE(ice_seen(&net, RTC_ICE_CONNECTED));

  req_len = build_binding_request(req, sizeof(req), txn_c, 1, good_username);
  ASSERT_TRUE(req_len > 0);
  ASSERT_EQ_INT(0, net_push_rx(&net, "2.2.2.2", 50002, req, req_len));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));

  ASSERT_TRUE(log_contains(&net, "use-candidate=1"));
  ASSERT_TRUE(log_contains(&net, "remote tuple locked"));
  ASSERT_TRUE(log_contains(&net, "remote tuple switch ignored"));

  ASSERT_EQ_INT(RTC_OK, rtc_session_close(s));
  rtc_session_destroy(s);
}

static void test_repeated_locked_ice_check_logs_are_quiet(void) {
  fake_net_t net;
  rtc_platform_ops_t ops;
  rtc_config_t cfg;
  rtc_callbacks_t cbs;
  rtc_session_t *s;
  uint8_t req[64];
  uint8_t txn_a[12] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2};
  uint8_t txn_b[12] = {2, 1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  const char *good_username = "0001020304050607:remoteufrag";
  size_t req_len;

  net_reset(&net);
  fill_ops(&ops);
  memset(&cfg, 0, sizeof(cfg));
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_ice_state = on_ice;
  cbs.user = &net;

  cfg.stun_server_ip = "9.9.9.9";
  cfg.stun_server_port = 3478;
  cfg.max_channels = 2;
  cfg.max_message_size = 100;
  cfg.log_level = RTC_LOG_DEBUG;

  s = rtc_session_create(&cfg, &ops, &net, &cbs);
  ASSERT_TRUE(s != 0);
  ASSERT_EQ_INT(RTC_OK,
                rtc_session_set_remote_offer(s,
                                             "v=0\r\n"
                                             "a=ice-ufrag:remoteufrag\r\n"
                                             "a=ice-pwd:remotepwdxxxxxxxxxxxxxxxxxx\r\n"
                                             "a=candidate:1 1 UDP 2130706431 203.0.113.21 53000 typ host\r\n"));
  ASSERT_EQ_INT(RTC_OK, rtc_session_start(s));

  req_len = build_binding_request(req, sizeof(req), txn_a, 1, good_username);
  ASSERT_TRUE(req_len > 0);
  ASSERT_EQ_INT(0, net_push_rx(&net, "5.5.5.5", 53000, req, req_len));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));

  req_len = build_binding_request(req, sizeof(req), txn_b, 1, good_username);
  ASSERT_TRUE(req_len > 0);
  ASSERT_EQ_INT(0, net_push_rx(&net, "5.5.5.5", 53000, req, req_len));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));

  ASSERT_EQ_INT(1,
                (int)log_count_contains(&net,
                                        "ICE check request handled src=5.5.5.5:53000 use-candidate=1 auth=ok"));

  ASSERT_EQ_INT(RTC_OK, rtc_session_close(s));
  rtc_session_destroy(s);
}

static void test_use_candidate_auth_fail(void) {
  fake_net_t net;
  rtc_platform_ops_t ops;
  rtc_config_t cfg;
  rtc_callbacks_t cbs;
  rtc_session_t *s;
  uint8_t req[64];
  uint8_t txn_a[12] = {4, 3, 2, 1, 4, 3, 2, 1, 4, 3, 2, 1};
  size_t req_len;

  net_reset(&net);
  fill_ops(&ops);
  memset(&cfg, 0, sizeof(cfg));
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_ice_state = on_ice;
  cbs.user = &net;

  cfg.stun_server_ip = "9.9.9.9";
  cfg.stun_server_port = 3478;
  cfg.max_channels = 2;
  cfg.max_message_size = 100;
  cfg.log_level = RTC_LOG_DEBUG;

  s = rtc_session_create(&cfg, &ops, &net, &cbs);
  ASSERT_TRUE(s != 0);
  ASSERT_EQ_INT(RTC_OK,
                rtc_session_set_remote_offer(s,
                                             "v=0\r\n"
                                             "a=ice-ufrag:remoteufrag\r\n"
                                             "a=ice-pwd:remotepwdxxxxxxxxxxxxxxxxxx\r\n"
                                             "a=candidate:1 1 UDP 2130706431 203.0.113.11 52000 typ host\r\n"));
  ASSERT_EQ_INT(RTC_OK, rtc_session_start(s));
  ASSERT_TRUE(ice_seen(&net, RTC_ICE_CHECKING));

  req_len = build_binding_request(req, sizeof(req), txn_a, 1, "bad-username");
  ASSERT_TRUE(req_len > 0);
  ASSERT_EQ_INT(0, net_push_rx(&net, "4.4.4.4", 52000, req, req_len));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));
  ASSERT_TRUE(!ice_seen(&net, RTC_ICE_CONNECTED));
  ASSERT_TRUE(log_contains(&net, "auth=fail"));
  ASSERT_TRUE(log_contains(&net, "nomination ignored"));

  ASSERT_EQ_INT(RTC_OK, rtc_session_close(s));
  rtc_session_destroy(s);
}

static void test_tuple_lock_fallback_non_stun(void) {
  fake_net_t net;
  rtc_platform_ops_t ops;
  rtc_config_t cfg;
  rtc_callbacks_t cbs;
  rtc_session_t *s;
  uint8_t dtls_like[16] = {0x16, 0xFE, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x00};

  net_reset(&net);
  fill_ops(&ops);
  memset(&cfg, 0, sizeof(cfg));
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_ice_state = on_ice;
  cbs.user = &net;

  cfg.stun_server_ip = "9.9.9.9";
  cfg.stun_server_port = 3478;
  cfg.max_channels = 2;
  cfg.max_message_size = 100;
  cfg.log_level = RTC_LOG_DEBUG;

  s = rtc_session_create(&cfg, &ops, &net, &cbs);
  ASSERT_TRUE(s != 0);
  ASSERT_EQ_INT(RTC_OK,
                rtc_session_set_remote_offer(s,
                                             "v=0\r\n"
                                             "a=ice-ufrag:remoteufrag\r\n"
                                             "a=ice-pwd:remotepwdxxxxxxxxxxxxxxxxxx\r\n"
                                             "a=candidate:1 1 UDP 2130706431 203.0.113.10 51000 typ host\r\n"));
  ASSERT_EQ_INT(RTC_OK, rtc_session_start(s));
  ASSERT_TRUE(ice_seen(&net, RTC_ICE_CHECKING));

  ASSERT_EQ_INT(0, net_push_rx(&net, "3.3.3.3", 51001, dtls_like, sizeof(dtls_like)));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));
  ASSERT_TRUE(ice_seen(&net, RTC_ICE_CONNECTED));

  ASSERT_EQ_INT(0, net_push_rx(&net, "3.3.3.3", 51002, dtls_like, sizeof(dtls_like)));
  ASSERT_EQ_INT(RTC_OK, rtc_session_poll(s));

  ASSERT_TRUE(log_contains(&net, "first non-STUN packet"));
  ASSERT_TRUE(log_contains(&net, "non-STUN packet from unselected tuple"));

  ASSERT_EQ_INT(RTC_OK, rtc_session_close(s));
  rtc_session_destroy(s);
}

int main(void) {
  test_basic_session_flow_no_stun();
  test_tuple_lock_by_use_candidate();
  test_repeated_locked_ice_check_logs_are_quiet();
  test_use_candidate_auth_fail();
  test_tuple_lock_fallback_non_stun();
  return 0;
}
