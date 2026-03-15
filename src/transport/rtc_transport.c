#include "transport/rtc_transport.h"

#include <string.h>

static void rtc_transport_reset_queue(rtc_rtp_packet_t *queue, uint16_t capacity,
                                      uint16_t *head, uint16_t *tail, uint16_t *size,
                                      uint16_t *high_watermark) {
  if (!queue || !head || !tail || !size || !high_watermark) {
    return;
  }
  memset(queue, 0, sizeof(rtc_rtp_packet_t) * capacity);
  *head = 0u;
  *tail = 0u;
  *size = 0u;
  *high_watermark = 0u;
}

static void rtc_transport_reset_stun_queue(rtc_transport_stun_packet_t *queue,
                                           uint16_t capacity, uint16_t *head,
                                           uint16_t *tail, uint16_t *size,
                                           uint16_t *high_watermark) {
  if (!queue || !head || !tail || !size || !high_watermark) {
    return;
  }
  memset(queue, 0, sizeof(rtc_transport_stun_packet_t) * capacity);
  *head = 0u;
  *tail = 0u;
  *size = 0u;
  *high_watermark = 0u;
}

static void rtc_transport_reset_dtls_queue(rtc_transport_dtls_packet_t *queue,
                                           uint16_t capacity, uint16_t *head,
                                           uint16_t *tail, uint16_t *size,
                                           uint16_t *high_watermark) {
  if (!queue || !head || !tail || !size || !high_watermark) {
    return;
  }
  memset(queue, 0, sizeof(rtc_transport_dtls_packet_t) * capacity);
  *head = 0u;
  *tail = 0u;
  *size = 0u;
  *high_watermark = 0u;
}

static int rtc_transport_push_rx(rtc_transport_ctx_t *ctx,
                                 const rtc_rtp_packet_t *packet) {
  if (!ctx || !packet) {
    return 0;
  }
  if (ctx->rx_size >= RTC_CFG_RTP_RX_QUEUE) {
    return 0;
  }
  ctx->rx_queue[ctx->rx_tail] = *packet;
  ctx->rx_tail = (uint16_t)((ctx->rx_tail + 1u) % RTC_CFG_RTP_RX_QUEUE);
  ctx->rx_size++;
  if (ctx->rx_size > ctx->rx_high_watermark) {
    ctx->rx_high_watermark = ctx->rx_size;
  }
  return 1;
}

static int rtc_transport_push_stun(
    rtc_transport_ctx_t *ctx, const rtc_platform_net_addr_t *src,
    const uint8_t *buf, uint16_t len) {
  rtc_transport_stun_packet_t *slot;

  if (!ctx || !src || !buf || len == 0u || len > RTC_CFG_MTU) {
    return 0;
  }
  if (ctx->stun_rx_size >= RTC_CFG_RTCP_FB_QUEUE) {
    return 0;
  }

  slot = &ctx->stun_rx_queue[ctx->stun_rx_tail];
  memset(slot, 0, sizeof(*slot));
  slot->src_addr = *src;
  slot->len = len;
  memcpy(slot->data, buf, len);

  ctx->stun_rx_tail = (uint16_t)((ctx->stun_rx_tail + 1u) % RTC_CFG_RTCP_FB_QUEUE);
  ctx->stun_rx_size++;
  if (ctx->stun_rx_size > ctx->stun_rx_high_watermark) {
    ctx->stun_rx_high_watermark = ctx->stun_rx_size;
  }
  return 1;
}

static int rtc_transport_push_dtls(rtc_transport_ctx_t *ctx,
                                   const rtc_platform_net_addr_t *src,
                                   const uint8_t *buf, uint16_t len) {
  rtc_transport_dtls_packet_t *slot;

  if (!ctx || !src || !buf || len == 0u || len > RTC_CFG_DTLS_MAX_DATAGRAM) {
    return 0;
  }
  if (ctx->dtls_rx_size >= RTC_CFG_DTLS_MAILBOX_CAP) {
    return 0;
  }

  slot = &ctx->dtls_rx_queue[ctx->dtls_rx_tail];
  memset(slot, 0, sizeof(*slot));
  slot->src_addr = *src;
  slot->len = len;
  memcpy(slot->data, buf, len);

  ctx->dtls_rx_tail = (uint16_t)((ctx->dtls_rx_tail + 1u) % RTC_CFG_DTLS_MAILBOX_CAP);
  ctx->dtls_rx_size++;
  if (ctx->dtls_rx_size > ctx->dtls_rx_high_watermark) {
    ctx->dtls_rx_high_watermark = ctx->dtls_rx_size;
  }
  return 1;
}

static int rtc_transport_is_stun_packet(const uint8_t *buf, uint16_t len) {
  uint16_t msg_len;

  if (!buf || len < 20u) {
    return 0;
  }
  if ((buf[0] & 0xC0u) != 0u) {
    return 0;
  }
  if (buf[4] != 0x21u || buf[5] != 0x12u || buf[6] != 0xA4u || buf[7] != 0x42u) {
    return 0;
  }

  msg_len = (uint16_t)(((uint16_t)buf[2] << 8) | (uint16_t)buf[3]);
  if ((msg_len & 0x0003u) != 0u) {
    return 0;
  }
  if ((uint32_t)msg_len + 20u > (uint32_t)len) {
    return 0;
  }

  return 1;
}

static int rtc_transport_is_dtls_packet(const uint8_t *buf, uint16_t len) {
  if (!buf || len == 0u) {
    return 0;
  }
  return buf[0] >= 20u && buf[0] <= 63u;
}

static int rtc_transport_is_rtcp_packet(const uint8_t *buf, uint16_t len) {
  if (!buf || len < 8u) {
    return 0;
  }
  if ((buf[0] & 0xC0u) != 0x80u) {
    return 0;
  }
  return buf[1] >= 192u && buf[1] <= 223u;
}

void rtc_transport_init(rtc_transport_ctx_t *ctx) {
  rtc_result_t r;

  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->udp_socket_fd = RTC_PLATFORM_INVALID_SOCKET;
  ctx->mirror_loopback = 0u;
  rtc_transport_reset_queue(ctx->tx_queue, RTC_CFG_RTP_RX_QUEUE, &ctx->tx_head,
                            &ctx->tx_tail, &ctx->tx_size, &ctx->tx_high_watermark);
  rtc_transport_reset_queue(ctx->rx_queue, RTC_CFG_RTP_RX_QUEUE, &ctx->rx_head,
                            &ctx->rx_tail, &ctx->rx_size, &ctx->rx_high_watermark);
  rtc_transport_reset_stun_queue(ctx->stun_rx_queue, RTC_CFG_RTCP_FB_QUEUE,
                                 &ctx->stun_rx_head, &ctx->stun_rx_tail,
                                 &ctx->stun_rx_size, &ctx->stun_rx_high_watermark);
  rtc_transport_reset_dtls_queue(ctx->dtls_rx_queue, RTC_CFG_DTLS_MAILBOX_CAP,
                                 &ctx->dtls_rx_head, &ctx->dtls_rx_tail,
                                 &ctx->dtls_rx_size, &ctx->dtls_rx_high_watermark);

  r = rtc_platform_udp_create_nonblock(&ctx->udp_socket_fd);
  if (r != RTC_OK) {
    ctx->io_tx_error_count++;
    ctx->udp_socket_fd = RTC_PLATFORM_INVALID_SOCKET;
    return;
  }

  r = rtc_platform_udp_bind(ctx->udp_socket_fd, 0u, &ctx->local_port);
  if (r != RTC_OK) {
    ctx->io_tx_error_count++;
    rtc_platform_udp_close(&ctx->udp_socket_fd);
    ctx->local_port = 0u;
  }
}

void rtc_transport_deinit(rtc_transport_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  rtc_platform_udp_close(&ctx->udp_socket_fd);
  memset(ctx, 0, sizeof(*ctx));
  ctx->udp_socket_fd = RTC_PLATFORM_INVALID_SOCKET;
}

rtc_result_t rtc_transport_set_remote_ipv4(rtc_transport_ctx_t *ctx, uint8_t a,
                                           uint8_t b, uint8_t c, uint8_t d,
                                           uint16_t port) {
  if (!ctx || port == 0u) {
    return RTC_ERR_INVALID_ARG;
  }

  memset(&ctx->remote_addr, 0, sizeof(ctx->remote_addr));
  ctx->remote_addr.family = RTC_PLATFORM_IP_FAMILY_IPV4;
  ctx->remote_addr.port = port;
  ctx->remote_addr.addr[0] = a;
  ctx->remote_addr.addr[1] = b;
  ctx->remote_addr.addr[2] = c;
  ctx->remote_addr.addr[3] = d;
  ctx->remote_addr_valid = 1u;
  return RTC_OK;
}

void rtc_transport_set_loopback_mirror(rtc_transport_ctx_t *ctx, uint8_t enabled) {
  if (!ctx) {
    return;
  }
  ctx->mirror_loopback = enabled ? 1u : 0u;
}

rtc_result_t rtc_transport_enqueue_tx(rtc_transport_ctx_t *ctx,
                                      const rtc_rtp_packet_t *packet) {
  if (!ctx || !packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->tx_size >= RTC_CFG_RTP_RX_QUEUE) {
    return RTC_ERR_OVERFLOW;
  }

  ctx->tx_queue[ctx->tx_tail] = *packet;
  ctx->tx_tail = (uint16_t)((ctx->tx_tail + 1u) % RTC_CFG_RTP_RX_QUEUE);
  ctx->tx_size++;
  if (ctx->tx_size > ctx->tx_high_watermark) {
    ctx->tx_high_watermark = ctx->tx_size;
  }
  return RTC_OK;
}

rtc_result_t rtc_transport_send_stun(rtc_transport_ctx_t *ctx,
                                     const rtc_platform_net_addr_t *remote,
                                     const uint8_t *buf, uint16_t len,
                                     uint16_t *out_sent_len) {
  rtc_result_t r;
  uint16_t sent_len = 0u;

  if (!ctx || !remote || !buf || len == 0u || len > RTC_CFG_MTU || !out_sent_len) {
    return RTC_ERR_INVALID_ARG;
  }
  *out_sent_len = 0u;

  if (ctx->udp_socket_fd < 0) {
    ctx->io_stun_tx_error_count++;
    return RTC_ERR_INVALID_STATE;
  }

  r = rtc_platform_udp_sendto(ctx->udp_socket_fd, remote, buf, len, &sent_len);
  if (r == RTC_ERR_TIMEOUT) {
    ctx->io_stun_tx_would_block_count++;
    return r;
  }
  if (r != RTC_OK || sent_len != len) {
    ctx->io_stun_tx_error_count++;
    if (r != RTC_OK) {
      return r;
    }
    return RTC_ERR_PROTOCOL;
  }

  ctx->io_stun_tx_packets++;
  *out_sent_len = sent_len;
  return RTC_OK;
}

rtc_result_t rtc_transport_send_dtls(rtc_transport_ctx_t *ctx,
                                     const rtc_platform_net_addr_t *remote,
                                     const uint8_t *buf, uint16_t len,
                                     uint16_t *out_sent_len) {
  rtc_result_t r;
  uint16_t sent_len = 0u;

  if (!ctx || !remote || !buf || len == 0u || len > RTC_CFG_DTLS_MAX_DATAGRAM ||
      !out_sent_len) {
    return RTC_ERR_INVALID_ARG;
  }
  *out_sent_len = 0u;

  if (ctx->udp_socket_fd < 0) {
    ctx->io_dtls_tx_error_count++;
    return RTC_ERR_INVALID_STATE;
  }

  r = rtc_platform_udp_sendto(ctx->udp_socket_fd, remote, buf, len, &sent_len);
  if (r == RTC_ERR_TIMEOUT) {
    ctx->io_dtls_tx_would_block_count++;
    return r;
  }
  if (r != RTC_OK || sent_len != len) {
    ctx->io_dtls_tx_error_count++;
    if (r != RTC_OK) {
      return r;
    }
    return RTC_ERR_PROTOCOL;
  }

  ctx->io_dtls_tx_packets++;
  *out_sent_len = sent_len;
  return RTC_OK;
}

rtc_result_t rtc_transport_pump_io(rtc_transport_ctx_t *ctx, uint16_t max_packets,
                                   uint16_t *out_sent_count,
                                   uint16_t *out_recv_count,
                                   uint32_t *out_dropped_count) {
  uint16_t sent = 0;
  uint16_t received = 0;
  uint16_t recv_from_socket = 0;
  uint32_t dropped = 0;
  uint8_t had_invalid_state = 0u;
  uint8_t had_protocol_error = 0u;

  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }

  while (sent < max_packets && ctx->tx_size > 0u) {
    rtc_rtp_packet_t *packet = &ctx->tx_queue[ctx->tx_head];
    rtc_result_t r;
    uint16_t sent_len = 0u;

    if (ctx->udp_socket_fd < 0 || !ctx->remote_addr_valid) {
      had_invalid_state = 1u;
      ctx->io_tx_error_count++;
      dropped++;
      ctx->tx_head = (uint16_t)((ctx->tx_head + 1u) % RTC_CFG_RTP_RX_QUEUE);
      ctx->tx_size--;
      continue;
    }

    r = rtc_platform_udp_sendto(ctx->udp_socket_fd, &ctx->remote_addr, packet->wire,
                                packet->wire_len, &sent_len);
    if (r == RTC_ERR_TIMEOUT) {
      ctx->io_tx_would_block_count++;
      break;
    }
    if (r != RTC_OK || sent_len != packet->wire_len) {
      had_protocol_error = 1u;
      ctx->io_tx_error_count++;
      dropped++;
      ctx->tx_head = (uint16_t)((ctx->tx_head + 1u) % RTC_CFG_RTP_RX_QUEUE);
      ctx->tx_size--;
      continue;
    }

    ctx->io_tx_sent_packets++;
    ctx->tx_head = (uint16_t)((ctx->tx_head + 1u) % RTC_CFG_RTP_RX_QUEUE);
    ctx->tx_size--;
    sent++;

    if (ctx->mirror_loopback) {
      if (!rtc_transport_push_rx(ctx, packet)) {
        dropped++;
        ctx->io_rx_drop_packets++;
      } else {
        received++;
        ctx->io_rx_recv_packets++;
      }
    }
  }

  if (ctx->udp_socket_fd >= 0) {
    while (recv_from_socket < max_packets) {
      rtc_rtp_packet_t packet;
      rtc_platform_net_addr_t src_addr;
      rtc_result_t r;
      uint16_t read_len = 0u;

      memset(&packet, 0, sizeof(packet));
      memset(&src_addr, 0, sizeof(src_addr));
      r = rtc_platform_udp_recvfrom(ctx->udp_socket_fd, &src_addr, packet.wire,
                                    (uint16_t)sizeof(packet.wire), &read_len);
      if (r == RTC_ERR_TIMEOUT) {
        ctx->io_rx_would_block_count++;
        break;
      }
      if (r != RTC_OK) {
        had_protocol_error = 1u;
        ctx->io_rx_error_count++;
        break;
      }
      recv_from_socket++;
      if (rtc_transport_is_stun_packet(packet.wire, read_len)) {
        if (!rtc_transport_push_stun(ctx, &src_addr, packet.wire, read_len)) {
          dropped++;
          ctx->io_stun_rx_drop_packets++;
        } else {
          received++;
          ctx->io_stun_rx_packets++;
        }
        continue;
      }
      if (rtc_transport_is_dtls_packet(packet.wire, read_len)) {
        if (!rtc_transport_push_dtls(ctx, &src_addr, packet.wire, read_len)) {
          dropped++;
          ctx->io_dtls_rx_drop_packets++;
        } else {
          received++;
          ctx->io_dtls_rx_packets++;
        }
        continue;
      }
      if (read_len < RTC_CFG_RTP_HEADER_LEN &&
          !rtc_transport_is_rtcp_packet(packet.wire, read_len)) {
        had_protocol_error = 1u;
        ctx->io_rx_error_count++;
        ctx->io_dtls_rx_error_count++;
        continue;
      }
      packet.wire_len = read_len;

      if (!rtc_transport_push_rx(ctx, &packet)) {
        dropped++;
        ctx->io_rx_drop_packets++;
        continue;
      }

      received++;
      ctx->io_rx_recv_packets++;
    }
  }

  if (out_sent_count) {
    *out_sent_count = sent;
  }
  if (out_recv_count) {
    *out_recv_count = received;
  }
  if (out_dropped_count) {
    *out_dropped_count = dropped;
  }

  if (had_protocol_error) {
    return RTC_ERR_PROTOCOL;
  }
  if (had_invalid_state) {
    return RTC_ERR_INVALID_STATE;
  }
  return RTC_OK;
}

rtc_result_t rtc_transport_dequeue_rx(rtc_transport_ctx_t *ctx,
                                      rtc_rtp_packet_t *out_packet) {
  if (!ctx || !out_packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->rx_size == 0u) {
    return RTC_ERR_TIMEOUT;
  }

  *out_packet = ctx->rx_queue[ctx->rx_head];
  ctx->rx_head = (uint16_t)((ctx->rx_head + 1u) % RTC_CFG_RTP_RX_QUEUE);
  ctx->rx_size--;
  return RTC_OK;
}

rtc_result_t rtc_transport_dequeue_stun(rtc_transport_ctx_t *ctx,
                                        rtc_transport_stun_packet_t *out_packet) {
  if (!ctx || !out_packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->stun_rx_size == 0u) {
    return RTC_ERR_TIMEOUT;
  }

  *out_packet = ctx->stun_rx_queue[ctx->stun_rx_head];
  ctx->stun_rx_head = (uint16_t)((ctx->stun_rx_head + 1u) % RTC_CFG_RTCP_FB_QUEUE);
  ctx->stun_rx_size--;
  return RTC_OK;
}

rtc_result_t rtc_transport_dequeue_dtls(rtc_transport_ctx_t *ctx,
                                        rtc_transport_dtls_packet_t *out_packet) {
  if (!ctx || !out_packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->dtls_rx_size == 0u) {
    return RTC_ERR_TIMEOUT;
  }

  *out_packet = ctx->dtls_rx_queue[ctx->dtls_rx_head];
  ctx->dtls_rx_head = (uint16_t)((ctx->dtls_rx_head + 1u) % RTC_CFG_DTLS_MAILBOX_CAP);
  ctx->dtls_rx_size--;
  return RTC_OK;
}

uint16_t rtc_transport_local_port(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->local_port : 0u;
}

uint16_t rtc_transport_tx_depth(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->tx_size : 0u;
}

uint16_t rtc_transport_tx_high_watermark(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->tx_high_watermark : 0u;
}

uint16_t rtc_transport_rx_depth(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->rx_size : 0u;
}

uint16_t rtc_transport_rx_high_watermark(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->rx_high_watermark : 0u;
}

uint16_t rtc_transport_stun_depth(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->stun_rx_size : 0u;
}

uint16_t rtc_transport_stun_high_watermark(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->stun_rx_high_watermark : 0u;
}

uint16_t rtc_transport_dtls_depth(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->dtls_rx_size : 0u;
}

uint16_t rtc_transport_dtls_high_watermark(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->dtls_rx_high_watermark : 0u;
}

uint32_t rtc_transport_rx_drop_count(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->io_rx_drop_packets : 0u;
}

uint32_t rtc_transport_stun_drop_count(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->io_stun_rx_drop_packets : 0u;
}

uint32_t rtc_transport_dtls_drop_count(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->io_dtls_rx_drop_packets : 0u;
}

uint32_t rtc_transport_io_error_count(const rtc_transport_ctx_t *ctx) {
  uint32_t total;
  if (!ctx) {
    return 0u;
  }
  total = 0u;
  total += ctx->io_tx_error_count;
  total += ctx->io_rx_error_count;
  total += ctx->io_stun_rx_error_count;
  total += ctx->io_stun_tx_error_count;
  total += ctx->io_dtls_rx_error_count;
  total += ctx->io_dtls_tx_error_count;
  return total;
}

uint32_t rtc_transport_dtls_rx_count(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->io_dtls_rx_packets : 0u;
}

uint32_t rtc_transport_dtls_tx_count(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->io_dtls_tx_packets : 0u;
}
