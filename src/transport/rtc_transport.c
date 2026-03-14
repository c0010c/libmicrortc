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

void rtc_transport_init(rtc_transport_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  rtc_transport_reset_queue(ctx->tx_queue, RTC_CFG_RTP_RX_QUEUE, &ctx->tx_head,
                            &ctx->tx_tail, &ctx->tx_size, &ctx->tx_high_watermark);
  rtc_transport_reset_queue(ctx->rx_queue, RTC_CFG_RTP_RX_QUEUE, &ctx->rx_head,
                            &ctx->rx_tail, &ctx->rx_size, &ctx->rx_high_watermark);
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

rtc_result_t rtc_transport_pump_loopback(rtc_transport_ctx_t *ctx, uint16_t max_packets,
                                         uint16_t *out_pumped_count,
                                         uint32_t *out_dropped_count) {
  uint16_t pumped = 0;
  uint32_t dropped = 0;

  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }

  while (pumped < max_packets && ctx->tx_size > 0u) {
    if (ctx->rx_size >= RTC_CFG_RTP_RX_QUEUE) {
      dropped++;
      ctx->tx_head = (uint16_t)((ctx->tx_head + 1u) % RTC_CFG_RTP_RX_QUEUE);
      ctx->tx_size--;
      continue;
    }

    ctx->rx_queue[ctx->rx_tail] = ctx->tx_queue[ctx->tx_head];
    ctx->rx_tail = (uint16_t)((ctx->rx_tail + 1u) % RTC_CFG_RTP_RX_QUEUE);
    ctx->rx_size++;
    if (ctx->rx_size > ctx->rx_high_watermark) {
      ctx->rx_high_watermark = ctx->rx_size;
    }

    ctx->tx_head = (uint16_t)((ctx->tx_head + 1u) % RTC_CFG_RTP_RX_QUEUE);
    ctx->tx_size--;
    pumped++;
  }

  if (out_pumped_count) {
    *out_pumped_count = pumped;
  }
  if (out_dropped_count) {
    *out_dropped_count = dropped;
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
