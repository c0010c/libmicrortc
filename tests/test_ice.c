#include "test_common.h"

#include <stdint.h>
#include <string.h>

#include "ice.h"
#include "stun.h"

static void wr16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)v;
}
static void wr32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

int main(void) {
  rtc_ice_agent_t ice;
  uint8_t txn[12] = {0};
  uint8_t req[32];
  size_t req_len = 0;
  uint8_t resp[64] = {0};
  char ip[64] = {0};
  uint16_t port = 0;

  rtc_ice_init(&ice);
  ASSERT_EQ_INT(RTC_ICE_NEW, ice.state);

  ASSERT_EQ_INT(0, rtc_ice_build_stun_request(&ice, txn, req, sizeof(req), &req_len));
  ASSERT_EQ_INT(20, (int)req_len);
  ASSERT_EQ_INT(1, ice.stun_inflight);

  wr16(resp + 0, 0x0101);
  wr16(resp + 2, 12);
  wr32(resp + 4, RTC_STUN_MAGIC_COOKIE);
  memcpy(resp + 8, txn, 12);
  wr16(resp + 20, 0x0020);
  wr16(resp + 22, 8);
  resp[24] = 0;
  resp[25] = 1;
  wr16(resp + 26, (uint16_t)(4567 ^ (RTC_STUN_MAGIC_COOKIE >> 16)));
  wr32(resp + 28, (0x0A000001u ^ RTC_STUN_MAGIC_COOKIE));

  ASSERT_EQ_INT(0, rtc_ice_handle_stun_response(&ice, resp, 32, ip, sizeof(ip), &port));
  ASSERT_TRUE(strcmp(ip, "10.0.0.1") == 0);
  ASSERT_EQ_INT(4567, port);
  ASSERT_EQ_INT(1, ice.has_srflx);
  return 0;
}
