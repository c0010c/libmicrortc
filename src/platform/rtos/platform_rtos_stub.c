#include "platform_rtos_stub.h"

#include <string.h>

static int unsupported_open(void *user, const char *bind_ip, uint16_t port) {
  (void)user;
  (void)bind_ip;
  (void)port;
  return -1;
}

static int unsupported_send(void *user,
                            int fd,
                            const char *dst_ip,
                            uint16_t dst_port,
                            const uint8_t *data,
                            size_t len) {
  (void)user;
  (void)fd;
  (void)dst_ip;
  (void)dst_port;
  (void)data;
  (void)len;
  return -1;
}

static int unsupported_recv(void *user,
                            int fd,
                            char *src_ip,
                            size_t src_ip_len,
                            uint16_t *src_port,
                            uint8_t *buf,
                            size_t cap,
                            size_t *out_len) {
  (void)user;
  (void)fd;
  (void)src_ip;
  (void)src_ip_len;
  (void)src_port;
  (void)buf;
  (void)cap;
  if (out_len) {
    *out_len = 0;
  }
  return 0;
}

static void unsupported_close(void *user, int fd) {
  (void)user;
  (void)fd;
}

static uint64_t stub_now(void *user) {
  (void)user;
  return 0;
}

static int stub_timer_arm(void *user, uint32_t timer_id, uint64_t deadline_ms) {
  (void)user;
  (void)timer_id;
  (void)deadline_ms;
  return 0;
}

static int stub_timer_cancel(void *user, uint32_t timer_id) {
  (void)user;
  (void)timer_id;
  return 0;
}

static int stub_rand(void *user, uint8_t *out, size_t len) {
  size_t i;
  (void)user;
  for (i = 0; i < len; ++i) {
    out[i] = (uint8_t)i;
  }
  return 0;
}

static void stub_log(void *user, rtc_log_level_t level, const char *msg) {
  (void)user;
  (void)level;
  (void)msg;
}

int rtc_rtos_stub_ops(rtc_platform_ops_t *out_ops) {
  if (!out_ops) {
    return -1;
  }
  memset(out_ops, 0, sizeof(*out_ops));
  out_ops->udp_open = unsupported_open;
  out_ops->udp_send = unsupported_send;
  out_ops->udp_recv = unsupported_recv;
  out_ops->udp_close = unsupported_close;
  out_ops->now_ms = stub_now;
  out_ops->timer_arm = stub_timer_arm;
  out_ops->timer_cancel = stub_timer_cancel;
  out_ops->rand_bytes = stub_rand;
  out_ops->log = stub_log;
  return 0;
}
