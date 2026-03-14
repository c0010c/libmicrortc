#include "platform_linux.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int l_udp_open(void *user, const char *bind_ip, uint16_t port) {
  int fd;
  struct sockaddr_in addr;
  (void)user;
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return -1;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = bind_ip ? inet_addr(bind_ip) : INADDR_ANY;
  if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  (void)fcntl(fd, F_SETFL, O_NONBLOCK);
  return fd;
}

static int l_udp_send(void *user,
                      int fd,
                      const char *dst_ip,
                      uint16_t dst_port,
                      const uint8_t *data,
                      size_t len) {
  struct sockaddr_in dst;
  (void)user;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_port = htons(dst_port);
  dst.sin_addr.s_addr = inet_addr(dst_ip);
  return (int)sendto(fd, data, len, 0, (const struct sockaddr *)&dst, sizeof(dst));
}

static int l_udp_recv(void *user,
                      int fd,
                      char *src_ip,
                      size_t src_ip_len,
                      uint16_t *src_port,
                      uint8_t *buf,
                      size_t cap,
                      size_t *out_len) {
  struct sockaddr_in src;
  socklen_t slen = sizeof(src);
  ssize_t n;
  (void)user;
  n = recvfrom(fd, buf, cap, 0, (struct sockaddr *)&src, &slen);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      if (out_len) {
        *out_len = 0;
      }
      return 0;
    }
    return -1;
  }
  if (src_ip && src_ip_len > 0) {
    snprintf(src_ip, src_ip_len, "%s", inet_ntoa(src.sin_addr));
  }
  if (src_port) {
    *src_port = ntohs(src.sin_port);
  }
  if (out_len) {
    *out_len = (size_t)n;
  }
  return 1;
}

static void l_udp_close(void *user, int fd) {
  (void)user;
  close(fd);
}

static uint64_t l_now_ms(void *user) {
  struct timeval tv;
  (void)user;
  gettimeofday(&tv, 0);
  return (uint64_t)tv.tv_sec * 1000u + (uint64_t)(tv.tv_usec / 1000u);
}

static int l_timer_arm(void *user, uint32_t timer_id, uint64_t deadline_ms) {
  (void)user;
  (void)timer_id;
  (void)deadline_ms;
  return 0;
}

static int l_timer_cancel(void *user, uint32_t timer_id) {
  (void)user;
  (void)timer_id;
  return 0;
}

static int l_rand_bytes(void *user, uint8_t *out, size_t len) {
  size_t i;
  (void)user;
  for (i = 0; i < len; ++i) {
    out[i] = (uint8_t)(rand() & 0xFF);
  }
  return 0;
}

static void l_log(void *user, rtc_log_level_t level, const char *msg) {
  const char *tag = "I";
  (void)user;
  if (level == RTC_LOG_ERROR) {
    tag = "E";
  } else if (level == RTC_LOG_WARN) {
    tag = "W";
  } else if (level == RTC_LOG_DEBUG) {
    tag = "D";
  }
  fprintf(stderr, "[rtc][%s] %s\n", tag, msg);
}

int rtc_linux_default_ops(rtc_platform_ops_t *out_ops) {
  if (!out_ops) {
    return -1;
  }
  memset(out_ops, 0, sizeof(*out_ops));
  out_ops->udp_open = l_udp_open;
  out_ops->udp_send = l_udp_send;
  out_ops->udp_recv = l_udp_recv;
  out_ops->udp_close = l_udp_close;
  out_ops->now_ms = l_now_ms;
  out_ops->timer_arm = l_timer_arm;
  out_ops->timer_cancel = l_timer_cancel;
  out_ops->rand_bytes = l_rand_bytes;
  out_ops->log = l_log;
  return 0;
}
