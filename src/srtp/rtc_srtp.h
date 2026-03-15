#ifndef RTC_SRTP_H_
#define RTC_SRTP_H_

#include <srtp.h>

#include "rtc/rtc.h"
#include "rtp/rtc_rtp.h"

typedef enum rtc_srtp_state {
  RTC_SRTP_STATE_INACTIVE = 0,
  RTC_SRTP_STATE_ACTIVE = 1,
  RTC_SRTP_STATE_FAILED = 2
} rtc_srtp_state_t;

typedef struct rtc_srtp_key_material {
  uint8_t inbound_key[30];
  uint8_t outbound_key[30];
  uint8_t key_len;
  uint8_t profile;
} rtc_srtp_key_material_t;

typedef struct rtc_srtp_ctx {
  rtc_srtp_state_t state;
  srtp_t inbound_session;
  srtp_t outbound_session;
  srtp_policy_t inbound_policy;
  srtp_policy_t outbound_policy;
  uint8_t inbound_policy_key[30];
  uint8_t outbound_policy_key[30];
} rtc_srtp_ctx_t;

void rtc_srtp_init(rtc_srtp_ctx_t *ctx);
void rtc_srtp_deinit(rtc_srtp_ctx_t *ctx);
rtc_result_t rtc_srtp_activate(rtc_srtp_ctx_t *ctx,
                               const rtc_srtp_key_material_t *keys);
rtc_result_t rtc_srtp_protect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet);
rtc_result_t rtc_srtp_unprotect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet);
rtc_result_t rtc_srtp_protect_rtcp(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet);
rtc_result_t rtc_srtp_unprotect_rtcp(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet);

#endif  // RTC_SRTP_H_
