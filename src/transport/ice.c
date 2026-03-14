#include "ice.h"

#include <string.h>

#include "stun.h"

void rtc_ice_init(rtc_ice_agent_t *a) {
  if (!a) {
    return;
  }
  memset(a, 0, sizeof(*a));
  a->state = RTC_ICE_NEW;
}

void rtc_ice_set_state(rtc_ice_agent_t *a, rtc_ice_state_t st) {
  if (!a) {
    return;
  }
  a->state = st;
}

int rtc_ice_build_stun_request(rtc_ice_agent_t *a,
                               const uint8_t txn[12],
                               uint8_t *out,
                               size_t cap,
                               size_t *written) {
  if (!a || !txn) {
    return -1;
  }
  memcpy(a->pending_stun_txn, txn, 12);
  a->stun_inflight = 1;
  return rtc_stun_build_binding_request(txn, out, cap, written);
}

int rtc_ice_handle_stun_response(rtc_ice_agent_t *a,
                                 const uint8_t *pkt,
                                 size_t len,
                                 char *out_ip,
                                 size_t out_ip_len,
                                 uint16_t *out_port) {
  if (!a || !a->stun_inflight) {
    return -1;
  }
  if (rtc_stun_parse_binding_response(pkt, len, a->pending_stun_txn, out_ip, out_ip_len, out_port) != 0) {
    return -1;
  }
  a->has_srflx = 1;
  a->stun_inflight = 0;
  return 0;
}
