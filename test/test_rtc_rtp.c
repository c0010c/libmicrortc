#include "rtp/rtc_rtp.h"

#include <stdio.h>
#include <string.h>

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

typedef struct test_packet_collector {
  rtc_rtp_packet_t packets[128];
  uint16_t count;
} test_packet_collector_t;

static rtc_result_t collect_packet_cb(rtc_rtp_packet_t *packet, void *user_data) {
  test_packet_collector_t *collector = (test_packet_collector_t *)user_data;
  if (!collector || !packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (collector->count >= (uint16_t)(sizeof(collector->packets) / sizeof(collector->packets[0]))) {
    return RTC_ERR_OVERFLOW;
  }
  collector->packets[collector->count++] = *packet;
  return RTC_OK;
}

static uint16_t expected_fu_packet_count(uint16_t nalu_len) {
  uint16_t chunk = (uint16_t)(RTC_CFG_MAX_MEDIA_PAYLOAD - 2u);
  uint16_t payload_without_header = (uint16_t)(nalu_len - 1u);
  return (uint16_t)((payload_without_header + chunk - 1u) / chunk);
}

static int test_annexb_fragment_count(void) {
  uint8_t frame[RTC_CFG_MTU * 2u];
  uint16_t big_nalu_len = (uint16_t)(RTC_CFG_MAX_MEDIA_PAYLOAD + 17u);
  uint16_t off = 0u;
  uint16_t packet_count = 0u;
  uint16_t expected = 0u;

  memset(frame, 0, sizeof(frame));
  frame[off++] = 0x00u;
  frame[off++] = 0x00u;
  frame[off++] = 0x00u;
  frame[off++] = 0x01u;
  frame[off++] = 0x67u;
  frame[off++] = 0x11u;
  frame[off++] = 0x22u;
  frame[off++] = 0x33u;
  frame[off++] = 0x44u;
  frame[off++] = 0x00u;
  frame[off++] = 0x00u;
  frame[off++] = 0x01u;
  frame[off++] = 0x65u;
  while (off < (uint16_t)(13u + big_nalu_len)) {
    frame[off] = (uint8_t)(off & 0xFFu);
    off++;
  }

  ASSERT_EQ_INT(RTC_OK, rtc_rtp_count_video_h264_packets(frame, off, &packet_count));
  expected = (uint16_t)(1u + expected_fu_packet_count(big_nalu_len));
  ASSERT_EQ_INT(expected, packet_count);
  return 0;
}

static int test_fua_header_and_marker(void) {
  rtc_rtp_ctx_t ctx;
  test_packet_collector_t collector;
  uint8_t nalu[RTC_CFG_MTU + 64u];
  uint16_t packet_count = 0u;
  uint16_t i;

  memset(&ctx, 0, sizeof(ctx));
  memset(&collector, 0, sizeof(collector));
  memset(nalu, 0xAB, sizeof(nalu));
  nalu[0] = 0x65u;
  rtc_rtp_init(&ctx, 11u);

  ASSERT_EQ_INT(RTC_OK, rtc_rtp_packetize_video_h264(&ctx, nalu, (uint16_t)sizeof(nalu),
                                                     90000u, 1u, collect_packet_cb,
                                                     &collector, &packet_count));
  ASSERT_EQ_INT(packet_count, collector.count);
  ASSERT_TRUE(collector.count > 1u);

  ASSERT_EQ_INT(28, collector.packets[0].payload[0] & 0x1Fu);
  ASSERT_TRUE((collector.packets[0].payload[1] & 0x80u) != 0u);
  ASSERT_TRUE((collector.packets[0].payload[1] & 0x40u) == 0u);
  ASSERT_TRUE((collector.packets[0].wire[1] & 0x80u) == 0u);

  ASSERT_EQ_INT(28, collector.packets[collector.count - 1u].payload[0] & 0x1Fu);
  ASSERT_TRUE((collector.packets[collector.count - 1u].payload[1] & 0x80u) == 0u);
  ASSERT_TRUE((collector.packets[collector.count - 1u].payload[1] & 0x40u) != 0u);
  ASSERT_TRUE((collector.packets[collector.count - 1u].wire[1] & 0x80u) != 0u);

  for (i = 1u; i + 1u < collector.count; ++i) {
    ASSERT_TRUE((collector.packets[i].payload[1] & 0x80u) == 0u);
    ASSERT_TRUE((collector.packets[i].payload[1] & 0x40u) == 0u);
  }

  return 0;
}

static int test_fua_reassembly_success(void) {
  rtc_rtp_ctx_t tx;
  rtc_rtp_ctx_t rx;
  test_packet_collector_t collector;
  rtc_rtp_frame_t out_frame;
  uint8_t nalu[RTC_CFG_MTU + 96u];
  uint16_t packet_count = 0u;
  uint16_t i;

  memset(&tx, 0, sizeof(tx));
  memset(&rx, 0, sizeof(rx));
  memset(&collector, 0, sizeof(collector));
  memset(&out_frame, 0, sizeof(out_frame));
  memset(nalu, 0x5Au, sizeof(nalu));
  nalu[0] = 0x61u;
  rtc_rtp_init(&tx, 21u);
  rtc_rtp_init(&rx, 22u);

  ASSERT_EQ_INT(RTC_OK, rtc_rtp_packetize_video_h264(&tx, nalu, (uint16_t)sizeof(nalu),
                                                     90123u, 1u, collect_packet_cb,
                                                     &collector, &packet_count));
  ASSERT_TRUE(packet_count > 1u);

  for (i = 0u; i < packet_count - 1u; ++i) {
    ASSERT_EQ_INT(RTC_ERR_TIMEOUT,
                  rtc_rtp_decode(&rx, &collector.packets[i], &out_frame));
  }

  ASSERT_EQ_INT(RTC_OK,
                rtc_rtp_decode(&rx, &collector.packets[packet_count - 1u], &out_frame));
  ASSERT_EQ_INT(RTC_PACKET_KIND_VIDEO, out_frame.kind);
  ASSERT_EQ_INT((uint16_t)sizeof(nalu), out_frame.payload_len);
  ASSERT_TRUE(out_frame.payload_ptr != NULL);
  ASSERT_EQ_INT(0, memcmp(out_frame.payload_ptr, nalu, sizeof(nalu)));
  return 0;
}

static int test_fua_loss_drops_chain(void) {
  rtc_rtp_ctx_t tx;
  rtc_rtp_ctx_t rx;
  test_packet_collector_t collector;
  rtc_rtp_frame_t out_frame;
  uint8_t nalu[RTC_CFG_MTU * 2u];
  uint16_t packet_count = 0u;
  uint16_t i;
  uint8_t got_complete = 0u;

  memset(&tx, 0, sizeof(tx));
  memset(&rx, 0, sizeof(rx));
  memset(&collector, 0, sizeof(collector));
  memset(&out_frame, 0, sizeof(out_frame));
  memset(nalu, 0x17, sizeof(nalu));
  nalu[0] = 0x65u;
  rtc_rtp_init(&tx, 31u);
  rtc_rtp_init(&rx, 32u);

  ASSERT_EQ_INT(RTC_OK, rtc_rtp_packetize_video_h264(&tx, nalu, (uint16_t)sizeof(nalu),
                                                     90234u, 1u, collect_packet_cb,
                                                     &collector, &packet_count));
  ASSERT_TRUE(packet_count > 2u);

  ASSERT_EQ_INT(RTC_ERR_TIMEOUT, rtc_rtp_decode(&rx, &collector.packets[0], &out_frame));
  for (i = 2u; i < packet_count; ++i) {
    rtc_result_t r = rtc_rtp_decode(&rx, &collector.packets[i], &out_frame);
    if (r == RTC_OK) {
      got_complete = 1u;
      break;
    }
    ASSERT_EQ_INT(RTC_ERR_TIMEOUT, r);
  }
  ASSERT_TRUE(got_complete == 0u);
  return 0;
}

static int test_fua_reassembly_overflow(void) {
  rtc_rtp_ctx_t tx;
  rtc_rtp_ctx_t rx;
  test_packet_collector_t collector;
  rtc_rtp_frame_t out_frame;
  uint8_t nalu[RTC_CFG_H264_REASSEMBLY_MAX + 128u];
  uint16_t packet_count = 0u;
  uint16_t i;
  uint8_t overflow_seen = 0u;

  memset(&tx, 0, sizeof(tx));
  memset(&rx, 0, sizeof(rx));
  memset(&collector, 0, sizeof(collector));
  memset(&out_frame, 0, sizeof(out_frame));
  memset(nalu, 0x3Cu, sizeof(nalu));
  nalu[0] = 0x61u;
  rtc_rtp_init(&tx, 41u);
  rtc_rtp_init(&rx, 42u);

  ASSERT_EQ_INT(RTC_OK, rtc_rtp_packetize_video_h264(&tx, nalu, (uint16_t)sizeof(nalu),
                                                     90345u, 1u, collect_packet_cb,
                                                     &collector, &packet_count));
  ASSERT_TRUE(packet_count > 1u);

  for (i = 0u; i < packet_count; ++i) {
    rtc_result_t r = rtc_rtp_decode(&rx, &collector.packets[i], &out_frame);
    if (r == RTC_ERR_OVERFLOW) {
      overflow_seen = 1u;
      break;
    }
    ASSERT_TRUE(r == RTC_ERR_TIMEOUT || r == RTC_OK);
  }
  ASSERT_TRUE(overflow_seen == 1u);
  return 0;
}

static int test_dynamic_pt_mapping(void) {
  rtc_rtp_ctx_t tx;
  rtc_rtp_ctx_t rx;
  test_packet_collector_t collector;
  rtc_rtp_frame_t out_frame;
  uint8_t nalu[] = {0x61u, 0xAAu, 0xBBu, 0xCCu, 0xDDu};
  uint16_t packet_count = 0u;

  memset(&tx, 0, sizeof(tx));
  memset(&rx, 0, sizeof(rx));
  memset(&collector, 0, sizeof(collector));
  memset(&out_frame, 0, sizeof(out_frame));
  rtc_rtp_init(&tx, 51u);
  rtc_rtp_init(&rx, 52u);
  rtc_rtp_set_payload_types(&tx, 102, 118, 119);
  rtc_rtp_set_payload_types(&rx, 102, 118, 119);

  ASSERT_EQ_INT(RTC_OK, rtc_rtp_packetize_video_h264(&tx, nalu, (uint16_t)sizeof(nalu),
                                                     90456u, 1u, collect_packet_cb,
                                                     &collector, &packet_count));
  ASSERT_EQ_INT(1, packet_count);
  ASSERT_EQ_INT(102, collector.packets[0].wire[1] & 0x7Fu);
  ASSERT_EQ_INT(RTC_OK, rtc_rtp_decode(&rx, &collector.packets[0], &out_frame));

  collector.packets[0].wire[1] =
      (uint8_t)((collector.packets[0].wire[1] & 0x80u) | 96u);
  ASSERT_EQ_INT(RTC_ERR_PROTOCOL, rtc_rtp_decode(&rx, &collector.packets[0], &out_frame));
  return 0;
}

static int test_strict_ssrc_matching(void) {
  rtc_rtp_ctx_t tx;
  rtc_rtp_ctx_t rx;
  test_packet_collector_t collector;
  rtc_rtp_frame_t out_frame;
  uint8_t nalu[] = {0x65u, 0x10u, 0x20u, 0x30u};
  uint16_t packet_count = 0u;

  memset(&tx, 0, sizeof(tx));
  memset(&rx, 0, sizeof(rx));
  memset(&collector, 0, sizeof(collector));
  memset(&out_frame, 0, sizeof(out_frame));
  rtc_rtp_init(&tx, 61u);
  rtc_rtp_init(&rx, 62u);

  ASSERT_EQ_INT(RTC_OK, rtc_rtp_packetize_video_h264(&tx, nalu, (uint16_t)sizeof(nalu),
                                                     90567u, 1u, collect_packet_cb,
                                                     &collector, &packet_count));
  ASSERT_EQ_INT(1, packet_count);

  rtc_rtp_set_expected_remote_ssrc(&rx, 1u, 0x55667788u, 0u, 0u);
  ASSERT_EQ_INT(RTC_ERR_PROTOCOL, rtc_rtp_decode(&rx, &collector.packets[0], &out_frame));

  rtc_rtp_set_expected_remote_ssrc(&rx, 1u, tx.video_ssrc, 0u, 0u);
  ASSERT_EQ_INT(RTC_OK, rtc_rtp_decode(&rx, &collector.packets[0], &out_frame));
  return 0;
}

int main(void) {
  int failures = 0;

  failures += test_annexb_fragment_count();
  failures += test_fua_header_and_marker();
  failures += test_fua_reassembly_success();
  failures += test_fua_loss_drops_chain();
  failures += test_fua_reassembly_overflow();
  failures += test_dynamic_pt_mapping();
  failures += test_strict_ssrc_matching();

  if (failures != 0) {
    printf("test failures: %d\n", failures);
    return 1;
  }

  printf("all tests passed\n");
  return 0;
}
