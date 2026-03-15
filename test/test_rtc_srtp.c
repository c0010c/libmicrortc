#include "rtc/rtc.h"

#include <stdio.h>
#include <string.h>

#include "rtp/rtc_rtp.h"
#include "srtp/rtc_srtp.h"

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

static void build_test_rtp_packet(rtc_rtp_packet_t *packet) {
  if (!packet) {
    return;
  }
  memset(packet, 0, sizeof(*packet));
  packet->wire[0] = 0x80u;
  packet->wire[1] = 96u;
  packet->wire[2] = 0x12u;
  packet->wire[3] = 0x34u;
  packet->wire[4] = 0x00u;
  packet->wire[5] = 0x00u;
  packet->wire[6] = 0x00u;
  packet->wire[7] = 0x11u;
  packet->wire[8] = 0x11u;
  packet->wire[9] = 0x22u;
  packet->wire[10] = 0x33u;
  packet->wire[11] = 0x44u;
  packet->wire[12] = 0xAAu;
  packet->wire[13] = 0xBBu;
  packet->wire[14] = 0xCCu;
  packet->wire[15] = 0xDDu;
  packet->wire_len = 16u;
}

static void build_test_rtcp_packet(rtc_rtp_packet_t *packet) {
  if (!packet) {
    return;
  }
  memset(packet, 0, sizeof(*packet));
  packet->wire[0] = 0x80u;
  packet->wire[1] = 200u;
  packet->wire[2] = 0x00u;
  packet->wire[3] = 0x01u;
  packet->wire[4] = 0x55u;
  packet->wire[5] = 0x66u;
  packet->wire[6] = 0x77u;
  packet->wire[7] = 0x88u;
  packet->wire_len = 8u;
}

static void fill_role_keys(rtc_srtp_key_material_t *server_keys,
                           rtc_srtp_key_material_t *client_keys) {
  uint8_t client_write[30];
  uint8_t server_write[30];
  uint16_t i;

  if (!server_keys || !client_keys) {
    return;
  }

  for (i = 0u; i < 30u; ++i) {
    client_write[i] = (uint8_t)(0x11u + i);
    server_write[i] = (uint8_t)(0xA1u + i);
  }

  memset(server_keys, 0, sizeof(*server_keys));
  memset(client_keys, 0, sizeof(*client_keys));
  server_keys->profile = 1u;
  server_keys->key_len = 30u;
  client_keys->profile = 1u;
  client_keys->key_len = 30u;

  /* DTLS server role: outbound=server_write, inbound=client_write */
  memcpy(server_keys->outbound_key, server_write, 30u);
  memcpy(server_keys->inbound_key, client_write, 30u);

  /* DTLS client role: outbound=client_write, inbound=server_write */
  memcpy(client_keys->outbound_key, client_write, 30u);
  memcpy(client_keys->inbound_key, server_write, 30u);
}

static int test_rtp_role_separation_and_peer_success(void) {
  rtc_srtp_ctx_t local_ctx;
  rtc_srtp_ctx_t remote_ctx;
  rtc_srtp_key_material_t server_keys;
  rtc_srtp_key_material_t client_keys;
  rtc_rtp_packet_t plain;
  rtc_rtp_packet_t cipher;
  rtc_rtp_packet_t peer_decoded;
  rtc_rtp_packet_t self_decoded;

  fill_role_keys(&server_keys, &client_keys);
  rtc_srtp_init(&local_ctx);
  rtc_srtp_init(&remote_ctx);

  ASSERT_EQ_INT(RTC_OK, rtc_srtp_activate(&local_ctx, &server_keys));
  ASSERT_EQ_INT(RTC_OK, rtc_srtp_activate(&remote_ctx, &client_keys));

  build_test_rtp_packet(&plain);
  cipher = plain;
  ASSERT_EQ_INT(RTC_OK, rtc_srtp_protect(&local_ctx, &cipher));

  self_decoded = cipher;
  ASSERT_EQ_INT(RTC_ERR_AUTH_FAILED, rtc_srtp_unprotect(&local_ctx, &self_decoded));

  peer_decoded = cipher;
  ASSERT_EQ_INT(RTC_OK, rtc_srtp_unprotect(&remote_ctx, &peer_decoded));
  ASSERT_EQ_INT(plain.wire_len, peer_decoded.wire_len);
  ASSERT_EQ_INT(0, memcmp(plain.wire, peer_decoded.wire, plain.wire_len));

  rtc_srtp_deinit(&remote_ctx);
  rtc_srtp_deinit(&local_ctx);
  return 0;
}

static int test_rtcp_paths_and_auth_failure_code(void) {
  rtc_srtp_ctx_t local_ctx;
  rtc_srtp_ctx_t remote_ctx;
  rtc_srtp_key_material_t server_keys;
  rtc_srtp_key_material_t client_keys;
  rtc_rtp_packet_t plain;
  rtc_rtp_packet_t cipher;
  rtc_rtp_packet_t peer_decoded;
  rtc_rtp_packet_t self_decoded;

  fill_role_keys(&server_keys, &client_keys);
  rtc_srtp_init(&local_ctx);
  rtc_srtp_init(&remote_ctx);

  ASSERT_EQ_INT(RTC_OK, rtc_srtp_activate(&local_ctx, &server_keys));
  ASSERT_EQ_INT(RTC_OK, rtc_srtp_activate(&remote_ctx, &client_keys));

  build_test_rtcp_packet(&plain);
  cipher = plain;
  ASSERT_EQ_INT(RTC_OK, rtc_srtp_protect_rtcp(&local_ctx, &cipher));

  self_decoded = cipher;
  ASSERT_EQ_INT(RTC_ERR_AUTH_FAILED,
                rtc_srtp_unprotect_rtcp(&local_ctx, &self_decoded));

  peer_decoded = cipher;
  ASSERT_EQ_INT(RTC_OK, rtc_srtp_unprotect_rtcp(&remote_ctx, &peer_decoded));
  ASSERT_EQ_INT(plain.wire_len, peer_decoded.wire_len);
  ASSERT_EQ_INT(0, memcmp(plain.wire, peer_decoded.wire, plain.wire_len));

  rtc_srtp_deinit(&remote_ctx);
  rtc_srtp_deinit(&local_ctx);
  return 0;
}

int main(void) {
  int failures = 0;

  failures += test_rtp_role_separation_and_peer_success();
  failures += test_rtcp_paths_and_auth_failure_code();

  if (failures != 0) {
    printf("srtp tests failures: %d\n", failures);
    return 1;
  }

  printf("srtp tests passed\n");
  return 0;
}
