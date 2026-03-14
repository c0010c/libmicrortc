#ifndef RTC_ICE_H
#define RTC_ICE_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/rtc_types.h"

typedef struct {
  char ip[64];
  uint16_t port;
  int is_srflx;
} rtc_ice_candidate_t;

typedef struct {
  rtc_ice_state_t state;
  rtc_ice_candidate_t local_host;
  rtc_ice_candidate_t local_srflx;
  int has_srflx;
  uint8_t pending_stun_txn[12];
  int stun_inflight;
} rtc_ice_agent_t;

void rtc_ice_init(rtc_ice_agent_t *a);
void rtc_ice_set_state(rtc_ice_agent_t *a, rtc_ice_state_t st);
int rtc_ice_build_stun_request(rtc_ice_agent_t *a,
                               const uint8_t txn[12],
                               uint8_t *out,
                               size_t cap,
                               size_t *written);
int rtc_ice_handle_stun_response(rtc_ice_agent_t *a,
                                 const uint8_t *pkt,
                                 size_t len,
                                 char *out_ip,
                                 size_t out_ip_len,
                                 uint16_t *out_port);

#endif
