#include "srtp/rtc_srtp.h"

#include <string.h>

#define RTC_SRTP_RTCP_MIN_LEN 8u

static uint8_t g_rtc_srtp_initialized = 0u;

static rtc_result_t rtc_srtp_map_error(srtp_err_status_t err) {
  if (err == srtp_err_status_ok) {
    return RTC_OK;
  }
  if (err == srtp_err_status_auth_fail) {
    return RTC_ERR_AUTH_FAILED;
  }
  if (err == srtp_err_status_buffer_small) {
    return RTC_ERR_OVERFLOW;
  }
  if (err == srtp_err_status_replay_fail || err == srtp_err_status_replay_old) {
    return RTC_ERR_PROTOCOL;
  }
  return RTC_ERR_PROTOCOL;
}

void rtc_srtp_init(rtc_srtp_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->state = RTC_SRTP_STATE_INACTIVE;
}

void rtc_srtp_deinit(rtc_srtp_ctx_t *ctx) {
  if (!ctx) {
    return;
  }

  if (ctx->inbound_session != NULL) {
    (void)srtp_dealloc(ctx->inbound_session);
    ctx->inbound_session = NULL;
  }
  if (ctx->outbound_session != NULL) {
    (void)srtp_dealloc(ctx->outbound_session);
    ctx->outbound_session = NULL;
  }

  memset(&ctx->inbound_policy, 0, sizeof(ctx->inbound_policy));
  memset(&ctx->outbound_policy, 0, sizeof(ctx->outbound_policy));
  memset(ctx->inbound_policy_key, 0, sizeof(ctx->inbound_policy_key));
  memset(ctx->outbound_policy_key, 0, sizeof(ctx->outbound_policy_key));
  ctx->state = RTC_SRTP_STATE_INACTIVE;
}

rtc_result_t rtc_srtp_activate(rtc_srtp_ctx_t *ctx,
                               const rtc_srtp_key_material_t *keys) {
  srtp_err_status_t r;

  if (!ctx || !keys) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state == RTC_SRTP_STATE_ACTIVE) {
    return RTC_OK;
  }
  if (keys->key_len != 30u || keys->profile != 1u) {
    ctx->state = RTC_SRTP_STATE_FAILED;
    return RTC_ERR_SRTP_ACTIVATE_FAILED;
  }

  rtc_srtp_deinit(ctx);

  if (!g_rtc_srtp_initialized) {
    r = srtp_init();
    if (r != srtp_err_status_ok) {
      ctx->state = RTC_SRTP_STATE_FAILED;
      return RTC_ERR_SRTP_ACTIVATE_FAILED;
    }
    g_rtc_srtp_initialized = 1u;
  }

  memset(&ctx->inbound_policy, 0, sizeof(ctx->inbound_policy));
  memset(&ctx->outbound_policy, 0, sizeof(ctx->outbound_policy));

  srtp_crypto_policy_set_rtp_default(&ctx->inbound_policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&ctx->inbound_policy.rtcp);
  srtp_crypto_policy_set_rtp_default(&ctx->outbound_policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&ctx->outbound_policy.rtcp);

  memcpy(ctx->inbound_policy_key, keys->inbound_key, 30u);
  memcpy(ctx->outbound_policy_key, keys->outbound_key, 30u);

  ctx->inbound_policy.ssrc.type = ssrc_any_inbound;
  ctx->inbound_policy.key = ctx->inbound_policy_key;
  ctx->inbound_policy.next = NULL;

  ctx->outbound_policy.ssrc.type = ssrc_any_outbound;
  ctx->outbound_policy.key = ctx->outbound_policy_key;
  ctx->outbound_policy.next = NULL;

  r = srtp_create(&ctx->inbound_session, &ctx->inbound_policy);
  if (r != srtp_err_status_ok) {
    rtc_srtp_deinit(ctx);
    ctx->state = RTC_SRTP_STATE_FAILED;
    return RTC_ERR_SRTP_ACTIVATE_FAILED;
  }

  r = srtp_create(&ctx->outbound_session, &ctx->outbound_policy);
  if (r != srtp_err_status_ok) {
    rtc_srtp_deinit(ctx);
    ctx->state = RTC_SRTP_STATE_FAILED;
    return RTC_ERR_SRTP_ACTIVATE_FAILED;
  }

  ctx->state = RTC_SRTP_STATE_ACTIVE;
  return RTC_OK;
}

rtc_result_t rtc_srtp_protect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet) {
  srtp_err_status_t r;
  size_t out_len;

  if (!ctx || !packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state != RTC_SRTP_STATE_ACTIVE || ctx->outbound_session == NULL) {
    return RTC_ERR_INVALID_STATE;
  }
  if (packet->wire_len < RTC_CFG_RTP_HEADER_LEN) {
    return RTC_ERR_PROTOCOL;
  }

  out_len = sizeof(packet->wire);
  r = srtp_protect(ctx->outbound_session,
                   packet->wire,
                   packet->wire_len,
                   packet->wire,
                   &out_len,
                   0u);
  if (r != srtp_err_status_ok) {
    return rtc_srtp_map_error(r);
  }
  if (out_len > UINT16_MAX) {
    return RTC_ERR_OVERFLOW;
  }

  packet->wire_len = (uint16_t)out_len;
  return RTC_OK;
}

rtc_result_t rtc_srtp_unprotect(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet) {
  srtp_err_status_t r;
  size_t out_len;

  if (!ctx || !packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state != RTC_SRTP_STATE_ACTIVE || ctx->inbound_session == NULL) {
    return RTC_ERR_INVALID_STATE;
  }
  if (packet->wire_len < RTC_CFG_RTP_HEADER_LEN) {
    return RTC_ERR_PROTOCOL;
  }

  out_len = sizeof(packet->wire);
  r = srtp_unprotect(ctx->inbound_session,
                     packet->wire,
                     packet->wire_len,
                     packet->wire,
                     &out_len);
  if (r != srtp_err_status_ok) {
    return rtc_srtp_map_error(r);
  }
  if (out_len > UINT16_MAX || out_len < RTC_CFG_RTP_HEADER_LEN) {
    return RTC_ERR_PROTOCOL;
  }

  packet->wire_len = (uint16_t)out_len;
  return RTC_OK;
}

rtc_result_t rtc_srtp_protect_rtcp(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet) {
  srtp_err_status_t r;
  size_t out_len;

  if (!ctx || !packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state != RTC_SRTP_STATE_ACTIVE || ctx->outbound_session == NULL) {
    return RTC_ERR_INVALID_STATE;
  }
  if (packet->wire_len < RTC_SRTP_RTCP_MIN_LEN) {
    return RTC_ERR_PROTOCOL;
  }

  out_len = sizeof(packet->wire);
  r = srtp_protect_rtcp(ctx->outbound_session,
                        packet->wire,
                        packet->wire_len,
                        packet->wire,
                        &out_len,
                        0u);
  if (r != srtp_err_status_ok) {
    return rtc_srtp_map_error(r);
  }
  if (out_len > UINT16_MAX) {
    return RTC_ERR_OVERFLOW;
  }

  packet->wire_len = (uint16_t)out_len;
  return RTC_OK;
}

rtc_result_t rtc_srtp_unprotect_rtcp(rtc_srtp_ctx_t *ctx, rtc_rtp_packet_t *packet) {
  srtp_err_status_t r;
  size_t out_len;

  if (!ctx || !packet) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state != RTC_SRTP_STATE_ACTIVE || ctx->inbound_session == NULL) {
    return RTC_ERR_INVALID_STATE;
  }
  if (packet->wire_len < RTC_SRTP_RTCP_MIN_LEN) {
    return RTC_ERR_PROTOCOL;
  }

  out_len = sizeof(packet->wire);
  r = srtp_unprotect_rtcp(ctx->inbound_session,
                          packet->wire,
                          packet->wire_len,
                          packet->wire,
                          &out_len);
  if (r != srtp_err_status_ok) {
    return rtc_srtp_map_error(r);
  }
  if (out_len > UINT16_MAX || out_len < RTC_SRTP_RTCP_MIN_LEN) {
    return RTC_ERR_PROTOCOL;
  }

  packet->wire_len = (uint16_t)out_len;
  return RTC_OK;
}
