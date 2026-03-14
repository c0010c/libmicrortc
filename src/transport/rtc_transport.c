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

void rtc_transport_init(rtc_transport_ctx_t *ctx) {
  rtc_result_t r;

  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->udp_socket_fd = RTC_PLATFORM_INVALID_SOCKET;
  ctx->mirror_loopback = 1u;
  rtc_transport_reset_queue(ctx->tx_queue, RTC_CFG_RTP_RX_QUEUE, &ctx->tx_head,
                            &ctx->tx_tail, &ctx->tx_size, &ctx->tx_high_watermark);
  rtc_transport_reset_queue(ctx->rx_queue, RTC_CFG_RTP_RX_QUEUE, &ctx->rx_head,
                            &ctx->rx_tail, &ctx->rx_size, &ctx->rx_high_watermark);

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
      rtc_result_t r;
      uint16_t read_len = 0u;

      memset(&packet, 0, sizeof(packet));
      r = rtc_platform_udp_recvfrom(ctx->udp_socket_fd, NULL, packet.wire,
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
      if (read_len < RTC_CFG_RTP_HEADER_LEN) {
        had_protocol_error = 1u;
        ctx->io_rx_error_count++;
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

uint32_t rtc_transport_rx_drop_count(const rtc_transport_ctx_t *ctx) {
  return ctx ? ctx->io_rx_drop_packets : 0u;
}
