#ifndef RTC_TRANSPORT_H_
#define RTC_TRANSPORT_H_

#include <stdint.h>

#include "platform/rtc_platform.h"
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

  int udp_socket_fd;
  uint16_t local_port;
  uint8_t remote_addr_valid;
  uint8_t mirror_loopback;
  rtc_platform_net_addr_t remote_addr;

  uint32_t io_tx_sent_packets;
  uint32_t io_rx_recv_packets;
  uint32_t io_rx_drop_packets;
  uint32_t io_tx_would_block_count;
  uint32_t io_tx_error_count;
  uint32_t io_rx_would_block_count;
  uint32_t io_rx_error_count;
} rtc_transport_ctx_t;

void rtc_transport_init(rtc_transport_ctx_t *ctx);
void rtc_transport_deinit(rtc_transport_ctx_t *ctx);

rtc_result_t rtc_transport_set_remote_ipv4(rtc_transport_ctx_t *ctx, uint8_t a,
                                           uint8_t b, uint8_t c, uint8_t d,
                                           uint16_t port);
void rtc_transport_set_loopback_mirror(rtc_transport_ctx_t *ctx, uint8_t enabled);

rtc_result_t rtc_transport_enqueue_tx(rtc_transport_ctx_t *ctx,
                                      const rtc_rtp_packet_t *packet);

rtc_result_t rtc_transport_pump_io(rtc_transport_ctx_t *ctx, uint16_t max_packets,
                                   uint16_t *out_sent_count,
                                   uint16_t *out_recv_count,
                                   uint32_t *out_dropped_count);

rtc_result_t rtc_transport_dequeue_rx(rtc_transport_ctx_t *ctx,
                                      rtc_rtp_packet_t *out_packet);

uint16_t rtc_transport_local_port(const rtc_transport_ctx_t *ctx);
uint16_t rtc_transport_tx_depth(const rtc_transport_ctx_t *ctx);
uint16_t rtc_transport_tx_high_watermark(const rtc_transport_ctx_t *ctx);
uint16_t rtc_transport_rx_depth(const rtc_transport_ctx_t *ctx);
uint16_t rtc_transport_rx_high_watermark(const rtc_transport_ctx_t *ctx);
uint32_t rtc_transport_rx_drop_count(const rtc_transport_ctx_t *ctx);

#endif  // RTC_TRANSPORT_H_
