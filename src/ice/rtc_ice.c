#include "ice/rtc_ice.h"

#include <stdio.h>
#include <string.h>

#include "platform/rtc_platform.h"

void rtc_ice_init(rtc_ice_ctx_t *ctx) {
  if (!ctx) {
    return;
  }
  memset(ctx, 0, sizeof(*ctx));
  ctx->state = RTC_ICE_STATE_NEW;
}

rtc_result_t rtc_ice_start(rtc_ice_ctx_t *ctx, uint32_t peer_id, uint32_t now_ms) {
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }

  /* Keep remote description/candidates that may have been set before start. */
  ctx->local_description_emitted = 0u;
  ctx->local_candidate_emitted = 0u;
  ctx->local_candidate_count = 1u;
  ctx->connect_ticks = 0u;
  ctx->retry_count = 0u;
  ctx->state = RTC_ICE_STATE_GATHERING;
  ctx->last_tick_ms = now_ms;

  (void)snprintf(ctx->local_sdp, sizeof(ctx->local_sdp),
                 "v=0\r\na=group:BUNDLE 0\r\na=mid:0\r\na=ice-ufrag:u%u\r\n", peer_id);
  (void)snprintf(ctx->local_description_type, sizeof(ctx->local_description_type), "%s",
                 "offer");
  (void)snprintf(ctx->local_candidate, sizeof(ctx->local_candidate),
                 "candidate:%u 1 udp 2130706431 127.0.0.1 5000 typ host", peer_id);

  return RTC_OK;
}

rtc_result_t rtc_ice_set_remote_description(rtc_ice_ctx_t *ctx, const char *sdp,
                                            const char *type) {
  if (!ctx || !sdp || !type) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!rtc_platform_copy_string(ctx->remote_sdp, sizeof(ctx->remote_sdp), sdp)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  if (!rtc_platform_copy_string(ctx->remote_description_type,
                                sizeof(ctx->remote_description_type), type)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  ctx->remote_description_set = 1u;
  return RTC_OK;
}

rtc_result_t rtc_ice_add_remote_candidate(rtc_ice_ctx_t *ctx, const char *candidate) {
  if (!ctx || !candidate) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->remote_candidate_count >= RTC_CFG_MAX_REMOTE_CANDIDATES) {
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }
  if (!rtc_platform_copy_string(
          ctx->remote_candidates[ctx->remote_candidate_count], RTC_CFG_MAX_CANDIDATE_LEN,
          candidate)) {
    return RTC_ERR_BUFFER_TOO_SMALL;
  }
  ctx->remote_candidate_count++;
  return RTC_OK;
}

void rtc_ice_tick(rtc_ice_ctx_t *ctx, uint32_t now_ms, uint16_t retry_interval_ms,
                  uint16_t max_retries, rtc_ice_event_t *out_event) {
  uint32_t elapsed = 0;

  if (!ctx || !out_event) {
    return;
  }
  memset(out_event, 0, sizeof(*out_event));

  if (ctx->state == RTC_ICE_STATE_GATHERING) {
    if (!ctx->local_description_emitted) {
      out_event->emit_local_description = 1u;
      ctx->local_description_emitted = 1u;
    }
    if (!ctx->local_candidate_emitted) {
      out_event->emit_local_candidate = 1u;
      ctx->local_candidate_emitted = 1u;
    }
    ctx->state = RTC_ICE_STATE_CHECKING;
    ctx->last_tick_ms = now_ms;
    return;
  }

  if (ctx->state != RTC_ICE_STATE_CHECKING) {
    return;
  }

  if (ctx->remote_description_set && ctx->remote_candidate_count > 0u) {
    if (ctx->connect_ticks < RTC_CFG_ICE_CONNECT_TICKS) {
      ctx->connect_ticks++;
    }
    if (ctx->connect_ticks >= RTC_CFG_ICE_CONNECT_TICKS) {
      ctx->state = RTC_ICE_STATE_CONNECTED;
      out_event->connected = 1u;
    }
    return;
  }

  elapsed = now_ms - ctx->last_tick_ms;
  if (elapsed < retry_interval_ms) {
    return;
  }

  ctx->last_tick_ms = now_ms;
  if (ctx->retry_count < max_retries) {
    ctx->retry_count++;
    out_event->retry_performed = 1u;
    return;
  }

  ctx->state = RTC_ICE_STATE_FAILED;
  out_event->failed = 1u;
  out_event->error = RTC_ERR_TIMEOUT;
}
