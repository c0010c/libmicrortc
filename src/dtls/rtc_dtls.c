#include "dtls/rtc_dtls.h"

#include <string.h>

void rtc_dtls_init(rtc_dtls_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->state = RTC_DTLS_STATE_NEW;
}

rtc_result_t rtc_dtls_start(rtc_dtls_ctx_t *ctx) {
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state == RTC_DTLS_STATE_CONNECTED ||
      ctx->state == RTC_DTLS_STATE_HANDSHAKE) {
    return RTC_OK;
  }
  if (ctx->state == RTC_DTLS_STATE_FAILED) {
    return RTC_ERR_INVALID_STATE;
  }
  ctx->state = RTC_DTLS_STATE_HANDSHAKE;
  ctx->handshake_ticks = 0u;
  return RTC_OK;
}

void rtc_dtls_tick(rtc_dtls_ctx_t *ctx, rtc_dtls_event_t *out_event) {
  if (!ctx || !out_event) {
    return;
  }
  memset(out_event, 0, sizeof(*out_event));
  if (ctx->state != RTC_DTLS_STATE_HANDSHAKE) {
    return;
  }
  if (ctx->handshake_ticks < RTC_CFG_DTLS_CONNECT_TICKS) {
    ctx->handshake_ticks++;
  }
  if (ctx->handshake_ticks >= RTC_CFG_DTLS_CONNECT_TICKS) {
    ctx->state = RTC_DTLS_STATE_CONNECTED;
    out_event->connected = 1u;
  }
}
