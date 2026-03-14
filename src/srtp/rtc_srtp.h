#ifndef RTC_SRTP_H_
#define RTC_SRTP_H_

#include "rtc/rtc.h"
#include "rtp/rtc_rtp.h"

typedef enum rtc_srtp_state {
  RTC_SRTP_STATE_INACTIVE = 0,
  RTC_SRTP_STATE_ACTIVE = 1,
  RTC_SRTP_STATE_FAILED = 2
} rtc_srtp_state_t;

typedef struct rtc_srtp_ctx {
  rtc_srtp_state_t state;
} rtc_srtp_ctx_t;

void rtc_srtp_init(rtc_srtp_ctx_t *ctx);
rtc_result_t rtc_srtp_activate(rtc_srtp_ctx_t *ctx);
rtc_result_t rtc_srtp_protect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet);
rtc_result_t rtc_srtp_unprotect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet);

#endif  // RTC_SRTP_H_
