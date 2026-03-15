#include "rtc/rtc.h"

#include <stdio.h>
#include <string.h>

#include "platform/rtc_platform.h"
#include "rtp/rtc_rtp.h"
#include "transport/rtc_transport.h"

#define ASSERT_EQ_INT(expected, actual)                                                   \
  do {                                                                                    \
    int exp_val__ = (expected);                                                           \
    int act_val__ = (actual);                                                             \
    if (exp_val__ != act_val__) {                                                         \
      printf("ASSERT_EQ_INT failed at %s:%d expected=%d actual=%d\n", __FILE__, __LINE__, \
             exp_val__, act_val__);                                                       \
      return 1;                                                                           \
    }                                                                                     \
  } while (0)

#define ASSERT_TRUE(expr)                                                         \
  do {                                                                            \
    if (!(expr)) {                                                                \
      printf("ASSERT_TRUE failed at %s:%d expr=%s\n", __FILE__, __LINE__, #expr); \
      return 1;                                                                   \
    }                                                                             \
  } while (0)

static void build_test_packet(rtc_rtp_packet_t *pkt, uint16_t seq, uint8_t marker_byte) {
  if (!pkt) {
    return;
  }
  memset(pkt, 0, sizeof(*pkt));
  pkt->kind = RTC_PACKET_KIND_VIDEO;
  pkt->seq = seq;
  pkt->timestamp = 90000u + seq;
  pkt->ssrc = 0x11223344u;
  pkt->wire_len = 16u;
  pkt->wire[0] = 0x80u;
  pkt->wire[1] = marker_byte;
  pkt->wire[2] = (uint8_t)(seq >> 8);
  pkt->wire[3] = (uint8_t)(seq & 0xFFu);
  pkt->wire[4] = 0x12u;
  pkt->wire[5] = 0x34u;
  pkt->wire[6] = 0x56u;
  pkt->wire[7] = marker_byte;
  pkt->wire[8] = 0x11u;
  pkt->wire[9] = 0x22u;
  pkt->wire[10] = 0x33u;
  pkt->wire[11] = 0x44u;
  pkt->wire[12] = marker_byte;
  pkt->wire[13] = (uint8_t)(marker_byte + 1u);
  pkt->wire[14] = (uint8_t)(marker_byte + 2u);
  pkt->wire[15] = (uint8_t)(marker_byte + 3u);
}

static void build_test_dtls_packet(uint8_t *buf, uint16_t *out_len, uint8_t marker) {
  if (!buf || !out_len) {
    return;
  }
  memset(buf, 0, 20u);
  buf[0] = 22u;
  buf[1] = 0xFEu;
  buf[2] = 0xFDu;
  buf[3] = 0u;
  buf[4] = 0u;
  buf[5] = 0u;
  buf[6] = 0u;
  buf[7] = 0u;
  buf[8] = 0u;
  buf[9] = marker;
  buf[10] = 0u;
  buf[11] = 0u;
  buf[12] = 0u;
  buf[13] = 8u;
  buf[14] = 1u;
  buf[15] = marker;
  buf[16] = marker;
  buf[17] = marker;
  buf[18] = marker;
  buf[19] = marker;
  *out_len = 20u;
}

static void build_test_stun_packet(uint8_t *buf, uint16_t *out_len, uint8_t marker) {
  if (!buf || !out_len) {
    return;
  }
  memset(buf, 0, 20u);
  buf[0] = 0x00u;
  buf[1] = 0x01u;
  buf[2] = 0x00u;
  buf[3] = 0x00u;
  buf[4] = 0x21u;
  buf[5] = 0x12u;
  buf[6] = 0xA4u;
  buf[7] = 0x42u;
  buf[8] = marker;
  buf[9] = marker;
  buf[10] = marker;
  buf[11] = marker;
  buf[12] = marker;
  buf[13] = marker;
  buf[14] = marker;
  buf[15] = marker;
  buf[16] = marker;
  buf[17] = marker;
  buf[18] = marker;
  buf[19] = marker;
  *out_len = 20u;
}

static int test_udp_loopback_io(void) {
  rtc_transport_ctx_t ctx;
  rtc_rtp_packet_t packet;
  rtc_rtp_packet_t out_packet;
  uint16_t local_port;
  int rounds;

  memset(&ctx, 0, sizeof(ctx));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);

  local_port = rtc_transport_local_port(&ctx);
  ASSERT_TRUE(local_port > 0u);
  ASSERT_EQ_INT(RTC_OK, rtc_transport_set_remote_ipv4(&ctx, 127u, 0u, 0u, 1u, local_port));

  build_test_packet(&packet, 1u, 0x60u);
  ASSERT_EQ_INT(RTC_OK, rtc_transport_enqueue_tx(&ctx, &packet));

  for (rounds = 0; rounds < 8; ++rounds) {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    ASSERT_EQ_INT(RTC_OK, rtc_transport_pump_io(&ctx, 4u, &sent, &recv, &dropped));
    if (recv > 0u) {
      break;
    }
  }

  ASSERT_EQ_INT(RTC_OK, rtc_transport_dequeue_rx(&ctx, &out_packet));
  ASSERT_EQ_INT(packet.wire_len, out_packet.wire_len);
  ASSERT_EQ_INT(0, memcmp(packet.wire, out_packet.wire, packet.wire_len));

  rtc_transport_deinit(&ctx);
  return 0;
}

static int test_rx_overflow_drop_counter(void) {
  rtc_transport_ctx_t ctx;
  rtc_rtp_packet_t packet;
  uint32_t drops_before;
  uint16_t i;

  memset(&ctx, 0, sizeof(ctx));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);
  ASSERT_EQ_INT(
      RTC_OK,
      rtc_transport_set_remote_ipv4(&ctx, 127u, 0u, 0u, 1u, rtc_transport_local_port(&ctx)));

  for (i = 0u; i < RTC_CFG_RTP_RX_QUEUE; ++i) {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    build_test_packet(&packet, (uint16_t)(100u + i), (uint8_t)(0x10u + i));
    ASSERT_EQ_INT(RTC_OK, rtc_transport_enqueue_tx(&ctx, &packet));
    ASSERT_EQ_INT(RTC_OK, rtc_transport_pump_io(&ctx, 2u, &sent, &recv, &dropped));
    ASSERT_TRUE(recv > 0u);
  }

  ASSERT_EQ_INT(RTC_CFG_RTP_RX_QUEUE, rtc_transport_rx_depth(&ctx));
  drops_before = rtc_transport_rx_drop_count(&ctx);

  build_test_packet(&packet, 300u, 0x4Fu);
  ASSERT_EQ_INT(RTC_OK, rtc_transport_enqueue_tx(&ctx, &packet));
  {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    ASSERT_EQ_INT(RTC_OK, rtc_transport_pump_io(&ctx, 2u, &sent, &recv, &dropped));
    ASSERT_TRUE(dropped > 0u);
  }
  ASSERT_TRUE(rtc_transport_rx_drop_count(&ctx) > drops_before);

  rtc_transport_deinit(&ctx);
  return 0;
}

static int test_pump_bounds_and_watermarks(void) {
  rtc_transport_ctx_t ctx;
  rtc_rtp_packet_t packet;
  uint16_t seq = 0u;
  int i;

  memset(&ctx, 0, sizeof(ctx));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);
  ASSERT_EQ_INT(
      RTC_OK,
      rtc_transport_set_remote_ipv4(&ctx, 127u, 0u, 0u, 1u, rtc_transport_local_port(&ctx)));

  for (i = 0; i < 1000; ++i) {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;

    if (rtc_transport_tx_depth(&ctx) < RTC_CFG_RTP_RX_QUEUE) {
      build_test_packet(&packet, seq++, (uint8_t)(i & 0x7Fu));
      ASSERT_EQ_INT(RTC_OK, rtc_transport_enqueue_tx(&ctx, &packet));
    }

    ASSERT_EQ_INT(RTC_OK, rtc_transport_pump_io(&ctx, 2u, &sent, &recv, &dropped));

    if ((i & 1) == 0) {
      rtc_rtp_packet_t out_packet;
      (void)rtc_transport_dequeue_rx(&ctx, &out_packet);
    }

    ASSERT_TRUE(rtc_transport_tx_depth(&ctx) <= RTC_CFG_RTP_RX_QUEUE);
    ASSERT_TRUE(rtc_transport_rx_depth(&ctx) <= RTC_CFG_RTP_RX_QUEUE);
    ASSERT_TRUE(rtc_transport_tx_high_watermark(&ctx) <= RTC_CFG_RTP_RX_QUEUE);
    ASSERT_TRUE(rtc_transport_rx_high_watermark(&ctx) <= RTC_CFG_RTP_RX_QUEUE);
  }

  rtc_transport_deinit(&ctx);
  return 0;
}

static int test_recv_budget_consumes_invalid_packets(void) {
  rtc_transport_ctx_t ctx;
  rtc_platform_net_addr_t local;
  rtc_rtp_packet_t packet;
  uint8_t bad = 0x01u;
  int sender_fd = RTC_PLATFORM_INVALID_SOCKET;
  int i;

  memset(&ctx, 0, sizeof(ctx));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);

  ASSERT_TRUE(rtc_transport_local_port(&ctx) > 0u);
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&sender_fd));

  memset(&local, 0, sizeof(local));
  local.family = RTC_PLATFORM_IP_FAMILY_IPV4;
  local.port = rtc_transport_local_port(&ctx);
  local.addr[0] = 127u;
  local.addr[1] = 0u;
  local.addr[2] = 0u;
  local.addr[3] = 1u;

  for (i = 0; i < 64; ++i) {
    uint16_t sent_len = 0u;
    ASSERT_EQ_INT(RTC_OK,
                  rtc_platform_udp_sendto(sender_fd, &local, &bad, 1u, &sent_len));
    ASSERT_EQ_INT(1, sent_len);
  }

  {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    ASSERT_EQ_INT(RTC_ERR_PROTOCOL,
                  rtc_transport_pump_io(&ctx, 2u, &sent, &recv, &dropped));
  }

  build_test_packet(&packet, 777u, 0x66u);
  {
    uint16_t sent_len = 0u;
    ASSERT_EQ_INT(RTC_OK,
                  rtc_platform_udp_sendto(sender_fd, &local, packet.wire,
                                          packet.wire_len, &sent_len));
    ASSERT_EQ_INT(packet.wire_len, sent_len);
  }

  {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    ASSERT_EQ_INT(RTC_ERR_PROTOCOL,
                  rtc_transport_pump_io(&ctx, 2u, &sent, &recv, &dropped));
  }

  {
    rtc_rtp_packet_t out;
    ASSERT_EQ_INT(RTC_ERR_TIMEOUT, rtc_transport_dequeue_rx(&ctx, &out));
  }

  rtc_platform_udp_close(&sender_fd);
  rtc_transport_deinit(&ctx);
  return 0;
}

static int test_dtls_demux_to_dtls_queue(void) {
  rtc_transport_ctx_t ctx;
  rtc_platform_net_addr_t dst;
  int sender_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint8_t dtls[RTC_CFG_MTU];
  uint16_t dtls_len = 0u;
  rtc_rtp_packet_t out_rtp;
  rtc_transport_dtls_packet_t out_dtls;

  memset(&ctx, 0, sizeof(ctx));
  memset(&dst, 0, sizeof(dst));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);

  ASSERT_TRUE(rtc_transport_local_port(&ctx) > 0u);
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&sender_fd));

  dst.family = RTC_PLATFORM_IP_FAMILY_IPV4;
  dst.port = rtc_transport_local_port(&ctx);
  dst.addr[0] = 127u;
  dst.addr[1] = 0u;
  dst.addr[2] = 0u;
  dst.addr[3] = 1u;

  build_test_dtls_packet(dtls, &dtls_len, 0x33u);
  {
    uint16_t sent_len = 0u;
    ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_sendto(sender_fd, &dst, dtls, dtls_len, &sent_len));
    ASSERT_EQ_INT(dtls_len, sent_len);
  }

  {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    ASSERT_EQ_INT(RTC_OK, rtc_transport_pump_io(&ctx, 4u, &sent, &recv, &dropped));
  }

  memset(&out_rtp, 0, sizeof(out_rtp));
  ASSERT_EQ_INT(RTC_ERR_TIMEOUT, rtc_transport_dequeue_rx(&ctx, &out_rtp));
  ASSERT_EQ_INT(RTC_OK, rtc_transport_dequeue_dtls(&ctx, &out_dtls));
  ASSERT_EQ_INT(dtls_len, out_dtls.len);
  ASSERT_EQ_INT(0, memcmp(dtls, out_dtls.data, dtls_len));

  rtc_platform_udp_close(&sender_fd);
  rtc_transport_deinit(&ctx);
  return 0;
}

static int test_dtls_queue_overflow_is_bounded(void) {
  rtc_transport_ctx_t ctx;
  rtc_platform_net_addr_t dst;
  int sender_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint8_t dtls[RTC_CFG_MTU];
  uint16_t dtls_len = 0u;
  uint32_t drop_before;
  uint16_t i;

  memset(&ctx, 0, sizeof(ctx));
  memset(&dst, 0, sizeof(dst));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);

  ASSERT_TRUE(rtc_transport_local_port(&ctx) > 0u);
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&sender_fd));

  dst.family = RTC_PLATFORM_IP_FAMILY_IPV4;
  dst.port = rtc_transport_local_port(&ctx);
  dst.addr[0] = 127u;
  dst.addr[1] = 0u;
  dst.addr[2] = 0u;
  dst.addr[3] = 1u;
  drop_before = rtc_transport_dtls_drop_count(&ctx);

  for (i = 0u; i < (uint16_t)(RTC_CFG_DTLS_MAILBOX_CAP + 6u); ++i) {
    uint16_t sent_len = 0u;
    build_test_dtls_packet(dtls, &dtls_len, (uint8_t)i);
    ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_sendto(sender_fd, &dst, dtls, dtls_len, &sent_len));
    ASSERT_EQ_INT(dtls_len, sent_len);
  }

  {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    ASSERT_EQ_INT(RTC_OK, rtc_transport_pump_io(&ctx, (uint16_t)(RTC_CFG_DTLS_MAILBOX_CAP + 8u),
                                                &sent, &recv, &dropped));
    ASSERT_TRUE(dropped > 0u);
  }
  ASSERT_TRUE(rtc_transport_dtls_drop_count(&ctx) > drop_before);
  ASSERT_TRUE(rtc_transport_dtls_depth(&ctx) <= RTC_CFG_DTLS_MAILBOX_CAP);
  ASSERT_TRUE(rtc_transport_dtls_high_watermark(&ctx) <= RTC_CFG_DTLS_MAILBOX_CAP);

  rtc_platform_udp_close(&sender_fd);
  rtc_transport_deinit(&ctx);
  return 0;
}

static int test_stun_queue_overflow_is_bounded(void) {
  rtc_transport_ctx_t ctx;
  rtc_platform_net_addr_t dst;
  int sender_fd = RTC_PLATFORM_INVALID_SOCKET;
  uint8_t stun[RTC_CFG_MTU];
  uint16_t stun_len = 0u;
  rtc_transport_stun_packet_t out_stun;
  uint32_t drop_before;
  uint16_t i;

  memset(&ctx, 0, sizeof(ctx));
  memset(&dst, 0, sizeof(dst));
  memset(&out_stun, 0, sizeof(out_stun));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);

  ASSERT_TRUE(rtc_transport_local_port(&ctx) > 0u);
  ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_create_nonblock(&sender_fd));

  dst.family = RTC_PLATFORM_IP_FAMILY_IPV4;
  dst.port = rtc_transport_local_port(&ctx);
  dst.addr[0] = 127u;
  dst.addr[1] = 0u;
  dst.addr[2] = 0u;
  dst.addr[3] = 1u;
  drop_before = rtc_transport_stun_drop_count(&ctx);

  for (i = 0u; i < (uint16_t)(RTC_CFG_RTCP_FB_QUEUE + 6u); ++i) {
    uint16_t sent_len = 0u;
    build_test_stun_packet(stun, &stun_len, (uint8_t)i);
    ASSERT_EQ_INT(RTC_OK, rtc_platform_udp_sendto(sender_fd, &dst, stun, stun_len, &sent_len));
    ASSERT_EQ_INT(stun_len, sent_len);
  }

  {
    uint16_t sent = 0u;
    uint16_t recv = 0u;
    uint32_t dropped = 0u;
    ASSERT_EQ_INT(RTC_OK,
                  rtc_transport_pump_io(&ctx, (uint16_t)(RTC_CFG_RTCP_FB_QUEUE + 8u),
                                        &sent, &recv, &dropped));
    ASSERT_TRUE(dropped > 0u);
  }

  ASSERT_TRUE(rtc_transport_stun_drop_count(&ctx) > drop_before);
  ASSERT_TRUE(rtc_transport_stun_depth(&ctx) <= RTC_CFG_RTCP_FB_QUEUE);
  ASSERT_TRUE(rtc_transport_stun_high_watermark(&ctx) <= RTC_CFG_RTCP_FB_QUEUE);
  ASSERT_EQ_INT(RTC_OK, rtc_transport_dequeue_stun(&ctx, &out_stun));
  ASSERT_EQ_INT(stun_len, out_stun.len);

  rtc_platform_udp_close(&sender_fd);
  rtc_transport_deinit(&ctx);
  return 0;
}

static int test_transport_io_error_aggregate_counter(void) {
  rtc_transport_ctx_t ctx;
  rtc_rtp_packet_t packet;
  uint32_t io_errors_before;
  uint16_t sent = 0u;
  uint16_t recv = 0u;
  uint32_t dropped = 0u;

  memset(&ctx, 0, sizeof(ctx));
  memset(&packet, 0, sizeof(packet));
  rtc_transport_init(&ctx);
  rtc_transport_set_loopback_mirror(&ctx, 0u);

  build_test_packet(&packet, 999u, 0x7Fu);
  io_errors_before = rtc_transport_io_error_count(&ctx);
  ASSERT_EQ_INT(RTC_OK, rtc_transport_enqueue_tx(&ctx, &packet));
  ASSERT_EQ_INT(RTC_ERR_INVALID_STATE,
                rtc_transport_pump_io(&ctx, 1u, &sent, &recv, &dropped));
  ASSERT_TRUE(dropped >= 1u);
  ASSERT_TRUE(rtc_transport_io_error_count(&ctx) > io_errors_before);

  rtc_transport_deinit(&ctx);
  return 0;
}

int main(void) {
  int failures = 0;

  failures += test_udp_loopback_io();
  failures += test_rx_overflow_drop_counter();
  failures += test_pump_bounds_and_watermarks();
  failures += test_recv_budget_consumes_invalid_packets();
  failures += test_dtls_demux_to_dtls_queue();
  failures += test_dtls_queue_overflow_is_bounded();
  failures += test_stun_queue_overflow_is_bounded();
  failures += test_transport_io_error_aggregate_counter();

  if (failures != 0) {
    printf("transport test failures: %d\n", failures);
    return 1;
  }

  printf("transport tests passed\n");
  return 0;
}
