#include "srtp/rtc_srtp.h"

#include <string.h>

void rtc_srtp_init(rtc_srtp_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->state = RTC_SRTP_STATE_INACTIVE;
}

rtc_result_t rtc_srtp_activate(rtc_srtp_ctx_t *ctx) {
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state == RTC_SRTP_STATE_ACTIVE) {
    return RTC_OK;
  }
  if (ctx->state == RTC_SRTP_STATE_FAILED) {
    return RTC_ERR_INVALID_STATE;
  }
  ctx->state = RTC_SRTP_STATE_ACTIVE;
  return RTC_OK;
}

rtc_result_t rtc_srtp_protect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet) {
  (void)packet;
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state != RTC_SRTP_STATE_ACTIVE) {
    return RTC_ERR_INVALID_STATE;
  }
  return RTC_OK;
}

rtc_result_t rtc_srtp_unprotect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet) {
  (void)packet;
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state != RTC_SRTP_STATE_ACTIVE) {
    return RTC_ERR_INVALID_STATE;
  }
  return RTC_OK;
}
