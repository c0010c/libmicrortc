#include "rtc/rtc.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define RTC_MODULE_NAME "session"

struct rtc_ctx {
  uint8_t in_use;
  uint8_t reserved0;
  uint16_t reserved1;
  uint32_t ctx_id;
  rtc_state_t state;
  rtc_ctx_config_t config;
  rtc_stats_t stats;
  uint32_t last_tick_ms;
};

typedef struct rtc_runtime {
  uint8_t initialized;
  uint8_t reserved0;
  uint16_t reserved1;
  uint32_t next_ctx_id;
  rtc_global_config_t global_cfg;
  struct rtc_ctx pool[RTC_MAX_CONTEXTS];
} rtc_runtime_t;

static rtc_runtime_t g_rtc;

static int rtc_global_cfg_compatible(const rtc_global_config_t *config) {
  if (!config) {
    return 1;
  }
  if (config->version != RTC_API_VERSION) {
    return 0;
  }
  if (config->size < sizeof(rtc_global_config_t)) {
    return 0;
  }
  return 1;
}

static int rtc_ctx_cfg_compatible(const rtc_ctx_config_t *config) {
  if (!config) {
    return 0;
  }
  if (config->version != RTC_API_VERSION) {
    return 0;
  }
  if (config->size < sizeof(rtc_ctx_config_t)) {
    return 0;
  }
  return 1;
}

static void rtc_default_log(rtc_log_level_t level, uint32_t ctx_id, rtc_result_t code,
                            const char *message) {
  const char *tag = "DEBUG";
  if (level == RTC_LOG_ERROR) {
    tag = "ERROR";
  } else if (level == RTC_LOG_WARN) {
    tag = "WARN";
  } else if (level == RTC_LOG_INFO) {
    tag = "INFO";
  }

  fprintf(stderr, "%s\t%s\tctx=%u\tcode=%d\t%s\n", tag, RTC_MODULE_NAME, ctx_id, code,
          message ? message : "");
}

static void rtc_log(rtc_log_level_t level, uint32_t ctx_id, rtc_result_t code, const char *message) {
  if (!g_rtc.initialized) {
    return;
  }
  if (level > g_rtc.global_cfg.min_log_level) {
    return;
  }
  if (g_rtc.global_cfg.log_cb) {
    g_rtc.global_cfg.log_cb(level, RTC_MODULE_NAME, ctx_id, code, message,
                            g_rtc.global_cfg.log_user_data);
    return;
  }
  rtc_default_log(level, ctx_id, code, message);
}

static int rtc_ctx_in_pool(const rtc_ctx_t *ctx, uint32_t *out_idx) {
  uint32_t i = 0;
  if (!ctx) {
    return 0;
  }
  for (i = 0; i < RTC_MAX_CONTEXTS; ++i) {
    if ((const rtc_ctx_t *)&g_rtc.pool[i] == ctx) {
      if (out_idx) {
        *out_idx = i;
      }
      return 1;
    }
  }
  return 0;
}

static rtc_result_t rtc_ctx_validate(rtc_ctx_t *ctx, uint32_t *out_idx) {
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.initialized) {
    return RTC_ERR_NOT_INIT;
  }
  if (!rtc_ctx_in_pool(ctx, out_idx)) {
    rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "ctx pointer out of range");
    return RTC_ERR_INVALID_ARG;
  }
  if (!ctx->in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  return RTC_OK;
}

static void rtc_state_transition(rtc_ctx_t *ctx, rtc_state_t next_state, rtc_result_t code,
                                 const char *reason, rtc_log_level_t level) {
  rtc_state_t prev;
  if (!ctx) {
    return;
  }
  prev = ctx->state;
  if (prev == next_state) {
    return;
  }
  ctx->state = next_state;
  rtc_log(level, ctx->ctx_id, code, reason);
  if (ctx->config.on_state_change) {
    ctx->config.on_state_change(ctx, prev, next_state, ctx->config.user_data);
  }
}

rtc_result_t rtc_global_init(const rtc_global_config_t *config) {
  if (g_rtc.initialized) {
    return RTC_OK;
  }
  if (!rtc_global_cfg_compatible(config)) {
    return RTC_ERR_INVALID_ARG;
  }

  memset(&g_rtc, 0, sizeof(g_rtc));
  g_rtc.initialized = 1;
  g_rtc.next_ctx_id = 1;
  g_rtc.global_cfg.version = RTC_API_VERSION;
  g_rtc.global_cfg.size = (uint16_t)sizeof(g_rtc.global_cfg);
  g_rtc.global_cfg.min_log_level = RTC_LOG_INFO;
  g_rtc.global_cfg.log_cb = NULL;
  g_rtc.global_cfg.log_user_data = NULL;
  if (config) {
    g_rtc.global_cfg = *config;
  }

  rtc_log(RTC_LOG_INFO, 0, RTC_OK, "global init");
  return RTC_OK;
}

rtc_result_t rtc_global_deinit(void) {
  if (!g_rtc.initialized) {
    return RTC_OK;
  }
  rtc_log(RTC_LOG_INFO, 0, RTC_OK, "global deinit");
  memset(&g_rtc, 0, sizeof(g_rtc));
  return RTC_OK;
}

rtc_result_t rtc_ctx_create(const rtc_ctx_config_t *config, rtc_ctx_t **out_ctx) {
  uint32_t i = 0;
  rtc_ctx_t *ctx = NULL;

  if (!config || !out_ctx) {
    rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "create bad argument");
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.initialized) {
    return RTC_ERR_NOT_INIT;
  }
  if (!rtc_ctx_cfg_compatible(config)) {
    rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "create bad config version/size");
    return RTC_ERR_INVALID_ARG;
  }

  for (i = 0; i < RTC_MAX_CONTEXTS; ++i) {
    if (!g_rtc.pool[i].in_use) {
      ctx = &g_rtc.pool[i];
      break;
    }
  }
  if (!ctx) {
    rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_RESOURCE_EXHAUSTED, "context pool exhausted");
    return RTC_ERR_RESOURCE_EXHAUSTED;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->in_use = 1;
  ctx->ctx_id = g_rtc.next_ctx_id++;
  ctx->state = RTC_STATE_NEW;
  ctx->config = *config;
  if (ctx->config.max_retries == 0u) {
    ctx->config.max_retries = 1u;
  }
  if (ctx->config.retry_interval_ms == 0u) {
    ctx->config.retry_interval_ms = 1u;
  }

  *out_ctx = ctx;
  rtc_log(RTC_LOG_INFO, ctx->ctx_id, RTC_OK, "context created");
  return RTC_OK;
}

rtc_result_t rtc_ctx_destroy(rtc_ctx_t *ctx) {
  uint32_t idx = 0;

  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.initialized) {
    return RTC_ERR_NOT_INIT;
  }
  if (!rtc_ctx_in_pool(ctx, &idx)) {
    rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "destroy bad ctx pointer");
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.pool[idx].in_use) {
    return RTC_OK;
  }

  rtc_log(RTC_LOG_INFO, g_rtc.pool[idx].ctx_id, RTC_OK, "context destroyed");
  memset(&g_rtc.pool[idx], 0, sizeof(g_rtc.pool[idx]));
  return RTC_OK;
}

rtc_result_t rtc_ctx_start(rtc_ctx_t *ctx, uint32_t now_ms) {
  rtc_result_t ret = rtc_ctx_validate(ctx, NULL);
  if (ret != RTC_OK) {
    return ret;
  }
  if (ctx->state == RTC_STATE_FAILED) {
    rtc_log(RTC_LOG_ERROR, ctx->ctx_id, RTC_ERR_INVALID_STATE, "start from failed state");
    return RTC_ERR_INVALID_STATE;
  }
  if (ctx->state == RTC_STATE_CHECKING || ctx->state == RTC_STATE_CONNECTED) {
    return RTC_OK;
  }

  ctx->last_tick_ms = now_ms;
  rtc_state_transition(ctx, RTC_STATE_CHECKING, RTC_OK, "state new->checking", RTC_LOG_INFO);
  return RTC_OK;
}

rtc_result_t rtc_ctx_stop(rtc_ctx_t *ctx) {
  rtc_result_t ret = rtc_ctx_validate(ctx, NULL);
  if (ret != RTC_OK) {
    return ret;
  }
  if (ctx->state == RTC_STATE_STOPPED) {
    return RTC_OK;
  }
  rtc_state_transition(ctx, RTC_STATE_STOPPED, RTC_OK, "state -> stopped", RTC_LOG_INFO);
  return RTC_OK;
}

rtc_result_t rtc_ctx_tick(rtc_ctx_t *ctx, uint32_t now_ms) {
  uint32_t elapsed_ms = 0;
  rtc_result_t ret = rtc_ctx_validate(ctx, NULL);
  if (ret != RTC_OK) {
    return ret;
  }
  if (ctx->state != RTC_STATE_CHECKING) {
    return RTC_ERR_INVALID_STATE;
  }

  elapsed_ms = now_ms - ctx->last_tick_ms;
  if (elapsed_ms < ctx->config.retry_interval_ms) {
    return RTC_OK;
  }
  ctx->last_tick_ms = now_ms;

  if (ctx->stats.retry_count < ctx->config.max_retries) {
    ctx->stats.retry_count++;
    ctx->stats.retransmit_count++;
    rtc_log(RTC_LOG_DEBUG, ctx->ctx_id, RTC_OK, "connectivity retry");
    return RTC_OK;
  }

  ctx->stats.timeout_count++;
  rtc_state_transition(ctx, RTC_STATE_FAILED, RTC_ERR_TIMEOUT, "retry exhausted, timeout",
                       RTC_LOG_WARN);
  return RTC_ERR_TIMEOUT;
}

rtc_result_t rtc_ctx_mark_connected(rtc_ctx_t *ctx, uint32_t now_ms) {
  rtc_result_t ret = rtc_ctx_validate(ctx, NULL);
  if (ret != RTC_OK) {
    return ret;
  }
  if (ctx->state == RTC_STATE_CONNECTED) {
    return RTC_OK;
  }
  if (ctx->state != RTC_STATE_CHECKING) {
    return RTC_ERR_INVALID_STATE;
  }
  ctx->last_tick_ms = now_ms;
  rtc_state_transition(ctx, RTC_STATE_CONNECTED, RTC_OK, "state checking->connected",
                       RTC_LOG_INFO);
  return RTC_OK;
}

rtc_result_t rtc_ctx_get_state(const rtc_ctx_t *ctx, rtc_state_t *out_state) {
  uint32_t idx = 0;
  if (!ctx || !out_state) {
    rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "get_state bad argument");
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.initialized) {
    return RTC_ERR_NOT_INIT;
  }
  if (!rtc_ctx_in_pool(ctx, &idx)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.pool[idx].in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  *out_state = g_rtc.pool[idx].state;
  return RTC_OK;
}

rtc_result_t rtc_ctx_get_stats(const rtc_ctx_t *ctx, rtc_stats_t *out_stats) {
  uint32_t idx = 0;
  if (!ctx || !out_stats) {
    rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "get_stats bad argument");
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.initialized) {
    return RTC_ERR_NOT_INIT;
  }
  if (!rtc_ctx_in_pool(ctx, &idx)) {
    return RTC_ERR_INVALID_ARG;
  }
  if (!g_rtc.pool[idx].in_use) {
    return RTC_ERR_INVALID_STATE;
  }
  *out_stats = g_rtc.pool[idx].stats;
  return RTC_OK;
}

rtc_result_t rtc_ctx_signal_packet_drop(rtc_ctx_t *ctx, uint32_t count) {
  rtc_result_t ret = rtc_ctx_validate(ctx, NULL);
  if (ret != RTC_OK) {
    if (ret == RTC_ERR_INVALID_ARG) {
      rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "drop counter bad argument");
    }
    return ret;
  }
  ctx->stats.dropped_packet_count += count;
  rtc_log(RTC_LOG_WARN, ctx->ctx_id, RTC_OK, "packet drop observed");
  return RTC_OK;
}

rtc_result_t rtc_ctx_signal_retransmit(rtc_ctx_t *ctx, uint32_t count) {
  rtc_result_t ret = rtc_ctx_validate(ctx, NULL);
  if (ret != RTC_OK) {
    if (ret == RTC_ERR_INVALID_ARG) {
      rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "retransmit counter bad argument");
    }
    return ret;
  }
  ctx->stats.retransmit_count += count;
  rtc_log(RTC_LOG_DEBUG, ctx->ctx_id, RTC_OK, "retransmit observed");
  return RTC_OK;
}

rtc_result_t rtc_ctx_signal_protocol_error(rtc_ctx_t *ctx, rtc_result_t code) {
  rtc_result_t ret = rtc_ctx_validate(ctx, NULL);
  rtc_result_t signal_code = code;

  if (ret != RTC_OK) {
    if (ret == RTC_ERR_INVALID_ARG) {
      rtc_log(RTC_LOG_ERROR, 0, RTC_ERR_INVALID_ARG, "protocol error bad argument");
    }
    return ret;
  }

  if (signal_code == RTC_OK) {
    signal_code = RTC_ERR_PROTOCOL;
  }
  ctx->stats.protocol_error_count++;
  rtc_state_transition(ctx, RTC_STATE_FAILED, signal_code, "protocol violation", RTC_LOG_ERROR);
  return signal_code;
}

const char *rtc_threading_model(void) {
  return "caller-serialized, no internal background threads";
}

int rtc_is_non_blocking(void) {
  return 1;
}
