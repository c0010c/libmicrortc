#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "platform/rtc_platform.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static rtc_result_t rtc_platform_map_socket_errno(int err) {
  if (err == EAGAIN || err == EWOULDBLOCK || err == EINTR) {
    return RTC_ERR_TIMEOUT;
  }
  if (err == EMFILE || err == ENFILE || err == ENOBUFS || err == ENOMEM) {
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }
  return RTC_ERR_PROTOCOL;
}

void rtc_platform_log(const rtc_log_sink_t *sink, rtc_log_level_t level, const char *module,
                      uint32_t peer_id, rtc_result_t code, const char *message) {
  const char *tag = "DEBUG";
  if (!sink) {
    return;
  }
  if (level > sink->min_level) {
    return;
  }

  if (sink->cb) {
    sink->cb(level, module, peer_id, code, message, sink->user_data);
    return;
  }

  if (level == RTC_LOG_ERROR) {
    tag = "ERROR";
  } else if (level == RTC_LOG_WARN) {
    tag = "WARN";
  } else if (level == RTC_LOG_INFO) {
    tag = "INFO";
  }

  fprintf(stderr, "%s\t%s\tpeer=%u\tcode=%d\t%s\n", tag, module ? module : "core",
          peer_id, code, message ? message : "");
}

int rtc_platform_copy_string(char *dst, uint16_t dst_size, const char *src) {
  size_t src_len;
  if (!dst || !src || dst_size == 0u) {
    return 0;
  }
  src_len = strlen(src);
  if (src_len + 1u > dst_size) {
    return 0;
  }
  memcpy(dst, src, src_len + 1u);
  return 1;
}

void rtc_platform_zero(void *ptr, size_t size) {
  if (!ptr || size == 0u) {
    return;
  }
  memset(ptr, 0, size);
}

rtc_result_t rtc_platform_udp_create_nonblock(int *out_socket_fd) {
  int fd;
  int flags;

  if (!out_socket_fd) {
    return RTC_ERR_INVALID_ARG;
  }
  *out_socket_fd = RTC_PLATFORM_INVALID_SOCKET;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return rtc_platform_map_socket_errno(errno);
  }

  flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    (void)close(fd);
    return rtc_platform_map_socket_errno(errno);
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    (void)close(fd);
    return rtc_platform_map_socket_errno(errno);
  }

  *out_socket_fd = fd;
  return RTC_OK;
}

rtc_result_t rtc_platform_udp_bind(int socket_fd, uint16_t bind_port,
                                   uint16_t *out_bound_port) {
  struct sockaddr_in local_addr;
  socklen_t addr_len = (socklen_t)sizeof(local_addr);

  if (socket_fd < 0 || !out_bound_port) {
    return RTC_ERR_INVALID_ARG;
  }
  *out_bound_port = 0u;

  memset(&local_addr, 0, sizeof(local_addr));
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(bind_port);
  local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(socket_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
    return rtc_platform_map_socket_errno(errno);
  }

  if (getsockname(socket_fd, (struct sockaddr *)&local_addr, &addr_len) < 0) {
    return rtc_platform_map_socket_errno(errno);
  }

  *out_bound_port = ntohs(local_addr.sin_port);
  return RTC_OK;
}

rtc_result_t rtc_platform_udp_sendto(int socket_fd, const rtc_platform_net_addr_t *remote,
                                     const uint8_t *buf, uint16_t len,
                                     uint16_t *out_sent_len) {
  struct sockaddr_in remote_addr;
  ssize_t sent_len;

  if (socket_fd < 0 || !remote || !buf || len == 0u || !out_sent_len) {
    return RTC_ERR_INVALID_ARG;
  }
  if (remote->family != RTC_PLATFORM_IP_FAMILY_IPV4) {
    return RTC_ERR_NOT_SUPPORTED;
  }

  memset(&remote_addr, 0, sizeof(remote_addr));
  remote_addr.sin_family = AF_INET;
  remote_addr.sin_port = htons(remote->port);
  memcpy(&remote_addr.sin_addr, remote->addr, 4u);

  sent_len = sendto(socket_fd, buf, len, 0, (const struct sockaddr *)&remote_addr,
                    sizeof(remote_addr));
  if (sent_len < 0) {
    return rtc_platform_map_socket_errno(errno);
  }
  if (sent_len > UINT16_MAX) {
    return RTC_ERR_OVERFLOW;
  }

  *out_sent_len = (uint16_t)sent_len;
  return RTC_OK;
}

rtc_result_t rtc_platform_udp_recvfrom(int socket_fd, rtc_platform_net_addr_t *src,
                                       uint8_t *buf, uint16_t buf_cap,
                                       uint16_t *out_recv_len) {
  struct sockaddr_in src_addr;
  socklen_t src_len = (socklen_t)sizeof(src_addr);
  ssize_t read_len;

  if (socket_fd < 0 || !buf || buf_cap == 0u || !out_recv_len) {
    return RTC_ERR_INVALID_ARG;
  }
  *out_recv_len = 0u;

  memset(&src_addr, 0, sizeof(src_addr));
  read_len =
      recvfrom(socket_fd, buf, buf_cap, 0, (struct sockaddr *)&src_addr, &src_len);
  if (read_len < 0) {
    return rtc_platform_map_socket_errno(errno);
  }
  if (read_len > UINT16_MAX) {
    return RTC_ERR_OVERFLOW;
  }

  if (src) {
    memset(src, 0, sizeof(*src));
    src->family = RTC_PLATFORM_IP_FAMILY_IPV4;
    src->port = ntohs(src_addr.sin_port);
    memcpy(src->addr, &src_addr.sin_addr, 4u);
  }

  *out_recv_len = (uint16_t)read_len;
  return RTC_OK;
}

void rtc_platform_udp_close(int *io_socket_fd) {
  if (!io_socket_fd || *io_socket_fd < 0) {
    return;
  }
  (void)close(*io_socket_fd);
  *io_socket_fd = RTC_PLATFORM_INVALID_SOCKET;
}

int rtc_platform_parse_ipv4(const char *ip, uint8_t out_addr[4]) {
  struct in_addr in_addr4;

  if (!ip || !out_addr) {
    return 0;
  }
  if (inet_pton(AF_INET, ip, &in_addr4) != 1) {
    return 0;
  }
  memcpy(out_addr, &in_addr4, 4u);
  return 1;
}

rtc_result_t rtc_platform_get_default_ipv4(uint8_t out_addr[4]) {
  struct ifconf ifc;
  struct ifreq ifreqs[16];
  int fd;
  int i;
  int count;

  if (!out_addr) {
    return RTC_ERR_INVALID_ARG;
  }
  memset(out_addr, 0, 4u);

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return rtc_platform_map_socket_errno(errno);
  }

  memset(&ifc, 0, sizeof(ifc));
  memset(ifreqs, 0, sizeof(ifreqs));
  ifc.ifc_len = (int)sizeof(ifreqs);
  ifc.ifc_req = ifreqs;
  if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
    (void)close(fd);
    return rtc_platform_map_socket_errno(errno);
  }

  count = ifc.ifc_len / (int)sizeof(struct ifreq);
  for (i = 0; i < count; ++i) {
    struct ifreq flags_req;
    struct sockaddr_in *addr_in;

    if (ifreqs[i].ifr_addr.sa_family != AF_INET) {
      continue;
    }

    memset(&flags_req, 0, sizeof(flags_req));
    (void)snprintf(flags_req.ifr_name, sizeof(flags_req.ifr_name), "%s",
                   ifreqs[i].ifr_name);
    if (ioctl(fd, SIOCGIFFLAGS, &flags_req) < 0) {
      continue;
    }
    if ((flags_req.ifr_flags & IFF_UP) == 0) {
      continue;
    }
    if ((flags_req.ifr_flags & IFF_LOOPBACK) != 0) {
      continue;
    }

    addr_in = (struct sockaddr_in *)&ifreqs[i].ifr_addr;
    if (addr_in->sin_addr.s_addr == htonl(INADDR_ANY)) {
      continue;
    }

    memcpy(out_addr, &addr_in->sin_addr, 4u);
    (void)close(fd);
    return RTC_OK;
  }

  (void)close(fd);
  return RTC_ERR_NOT_SUPPORTED;
}
