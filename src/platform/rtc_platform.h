#ifndef RTC_PLATFORM_H_
#define RTC_PLATFORM_H_

#include <stddef.h>
#include <stdint.h>

#include "rtc/rtc.h"

#define RTC_PLATFORM_INVALID_SOCKET (-1)

typedef enum rtc_platform_ip_family {
  RTC_PLATFORM_IP_FAMILY_UNSPEC = 0,
  RTC_PLATFORM_IP_FAMILY_IPV4 = 4
} rtc_platform_ip_family_t;

typedef struct rtc_platform_net_addr {
  rtc_platform_ip_family_t family;
  uint16_t port;
  uint8_t addr[16];
} rtc_platform_net_addr_t;

typedef struct rtc_log_sink {
  rtc_log_level_t min_level;
  rtc_log_callback_t cb;
  void *user_data;
} rtc_log_sink_t;

void rtc_platform_log(const rtc_log_sink_t *sink, rtc_log_level_t level, const char *module,
                      uint32_t peer_id, rtc_result_t code, const char *message);

int rtc_platform_copy_string(char *dst, uint16_t dst_size, const char *src);
void rtc_platform_zero(void *ptr, size_t size);

rtc_result_t rtc_platform_udp_create_nonblock(int *out_socket_fd);
rtc_result_t rtc_platform_udp_bind(int socket_fd, uint16_t bind_port, uint16_t *out_bound_port);
rtc_result_t rtc_platform_udp_sendto(int socket_fd, const rtc_platform_net_addr_t *remote,
                                     const uint8_t *buf, uint16_t len,
                                     uint16_t *out_sent_len);
rtc_result_t rtc_platform_udp_recvfrom(int socket_fd, rtc_platform_net_addr_t *src,
                                       uint8_t *buf, uint16_t buf_cap,
                                       uint16_t *out_recv_len);
void rtc_platform_udp_close(int *io_socket_fd);
int rtc_platform_parse_ipv4(const char *ip, uint8_t out_addr[4]);

#endif  // RTC_PLATFORM_H_
