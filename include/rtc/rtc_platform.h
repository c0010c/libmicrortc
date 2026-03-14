#ifndef RTC_PLATFORM_H
#define RTC_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

#include "rtc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int (*udp_open)(void *user, const char *bind_ip, uint16_t port);
  int (*udp_send)(void *user,
                  int fd,
                  const char *dst_ip,
                  uint16_t dst_port,
                  const uint8_t *data,
                  size_t len);
  int (*udp_recv)(void *user,
                  int fd,
                  char *src_ip,
                  size_t src_ip_len,
                  uint16_t *src_port,
                  uint8_t *buf,
                  size_t cap,
                  size_t *out_len);
  void (*udp_close)(void *user, int fd);

  uint64_t (*now_ms)(void *user);
  int (*timer_arm)(void *user, uint32_t timer_id, uint64_t deadline_ms);
  int (*timer_cancel)(void *user, uint32_t timer_id);
  int (*rand_bytes)(void *user, uint8_t *out, size_t len);
  void (*log)(void *user, rtc_log_level_t level, const char *msg);
} rtc_platform_ops_t;

#ifdef __cplusplus
}
#endif

#endif
