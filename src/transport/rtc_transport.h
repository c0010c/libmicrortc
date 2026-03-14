#ifndef RTC_TRANSPORT_H_
#define RTC_TRANSPORT_H_

#include <stdint.h>

#include "rtc/rtc.h"
#include "rtp/rtc_rtp.h"

typedef struct rtc_transport_ctx {
  rtc_rtp_packet_t tx_queue[RTC_CFG_RTP_RX_QUEUE];
  rtc_rtp_packet_t rx_queue[RTC_CFG_RTP_RX_QUEUE];
  uint16_t tx_head;
  uint16_t tx_tail;
  uint16_t tx_size;
  uint16_t tx_high_watermark;
  uint16_t rx_head;
  uint16_t rx_tail;
  uint16_t rx_size;
  uint16_t rx_high_watermark;
} rtc_transport_ctx_t;

void rtc_transport_init(rtc_transport_ctx_t *ctx);

rtc_result_t rtc_transport_enqueue_tx(rtc_transport_ctx_t *ctx,
                                      const rtc_rtp_packet_t *packet);

rtc_result_t rtc_transport_pump_loopback(rtc_transport_ctx_t *ctx, uint16_t max_packets,
                                         uint16_t *out_pumped_count,
                                         uint32_t *out_dropped_count);

rtc_result_t rtc_transport_dequeue_rx(rtc_transport_ctx_t *ctx,
                                      rtc_rtp_packet_t *out_packet);

uint16_t rtc_transport_tx_depth(const rtc_transport_ctx_t *ctx);
uint16_t rtc_transport_tx_high_watermark(const rtc_transport_ctx_t *ctx);
uint16_t rtc_transport_rx_depth(const rtc_transport_ctx_t *ctx);
uint16_t rtc_transport_rx_high_watermark(const rtc_transport_ctx_t *ctx);

#endif  // RTC_TRANSPORT_H_
