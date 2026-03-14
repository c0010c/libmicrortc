#include "rtc/rtc.h"

#include <stdio.h>
#include <string.h>

#define ASSERT_EQ_INT(expected, actual)                                                   \
  do {                                                                                    \
    int exp_val = (expected);                                                             \
    int act_val = (actual);                                                               \
    if (exp_val != act_val) {                                                             \
      printf("ASSERT_EQ_INT failed at %s:%d expected=%d actual=%d\n", __FILE__, __LINE__, \
             exp_val, act_val);                                                           \
      return 1;                                                                           \
    }                                                                                     \
  } while (0)

#define ASSERT_TRUE(expr)                                                        \
  do {                                                                           \
    if (!(expr)) {                                                               \
      printf("ASSERT_TRUE failed at %s:%d expr=%s\n", __FILE__, __LINE__, #expr); \
      return 1;                                                                  \
    }                                                                            \
  } while (0)

typedef struct test_log_capture {
  uint32_t error_count;
  uint32_t warn_count;
  uint32_t info_count;
  uint32_t debug_count;
  rtc_result_t last_code;
  uint32_t last_ctx_id;
  char last_message[64];
} test_log_capture_t;

typedef struct test_state_capture {
  uint32_t transition_count;
  rtc_state_t last_prev;
  rtc_state_t last_curr;
} test_state_capture_t;

static void test_log_callback(rtc_log_level_t level, const char *module, uint32_t ctx_id,
                              rtc_result_t code, const char *message, void *user_data) {
  test_log_capture_t *capture = (test_log_capture_t *)user_data;
  (void)module;

  if (!capture) {
    return;
  }

  if (level == RTC_LOG_ERROR) {
    capture->error_count++;
  } else if (level == RTC_LOG_WARN) {
    capture->warn_count++;
  } else if (level == RTC_LOG_INFO) {
    capture->info_count++;
  } else if (level == RTC_LOG_DEBUG) {
    capture->debug_count++;
  }

  capture->last_code = code;
  capture->last_ctx_id = ctx_id;
  if (message) {
    (void)snprintf(capture->last_message, sizeof(capture->last_message), "%s", message);
  } else {
    capture->last_message[0] = '\0';
  }
}

static void test_state_callback(rtc_ctx_t *ctx, rtc_state_t prev_state, rtc_state_t new_state,
                                void *user_data) {
  test_state_capture_t *capture = (test_state_capture_t *)user_data;
  (void)ctx;
  if (!capture) {
    return;
  }
  capture->transition_count++;
  capture->last_prev = prev_state;
  capture->last_curr = new_state;
}

static int test_lifecycle_and_idempotent(void) {
  test_log_capture_t logs;
  test_state_capture_t states;
  rtc_global_config_t global_cfg;
  rtc_ctx_config_t ctx_cfg;
  rtc_ctx_t *ctx;
  rtc_state_t state;

  memset(&logs, 0, sizeof(logs));
  memset(&states, 0, sizeof(states));
  memset(&global_cfg, 0, sizeof(global_cfg));
  memset(&ctx_cfg, 0, sizeof(ctx_cfg));

  global_cfg.version = RTC_API_VERSION;
  global_cfg.size = (uint16_t)sizeof(global_cfg);
  global_cfg.min_log_level = RTC_LOG_DEBUG;
  global_cfg.log_cb = test_log_callback;
  global_cfg.log_user_data = &logs;

  ASSERT_EQ_INT(RTC_OK, rtc_global_init(&global_cfg));
  ASSERT_EQ_INT(RTC_OK, rtc_global_init(&global_cfg));

  ctx_cfg.version = RTC_API_VERSION;
  ctx_cfg.size = (uint16_t)sizeof(ctx_cfg);
  ctx_cfg.max_retries = 2;
  ctx_cfg.retry_interval_ms = 10;
  ctx_cfg.on_state_change = test_state_callback;
  ctx_cfg.user_data = &states;

  ctx = NULL;
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_create(&ctx_cfg, &ctx));
  ASSERT_TRUE(ctx != NULL);

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_get_state(ctx, &state));
  ASSERT_EQ_INT(RTC_STATE_NEW, state);

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_start(ctx, 10));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_start(ctx, 20));

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_get_state(ctx, &state));
  ASSERT_EQ_INT(RTC_STATE_CHECKING, state);

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_stop(ctx));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_stop(ctx));

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_destroy(ctx));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_destroy(ctx));

  ASSERT_EQ_INT(RTC_OK, rtc_global_deinit());
  ASSERT_EQ_INT(RTC_OK, rtc_global_deinit());

  ASSERT_TRUE(states.transition_count >= 2);
  ASSERT_TRUE(logs.info_count >= 1);
  return 0;
}

static int test_invalid_argument_handling(void) {
  test_log_capture_t logs;
  rtc_global_config_t global_cfg;
  rtc_ctx_config_t ctx_cfg;
  rtc_ctx_t *ctx;
  rtc_state_t state;
  rtc_stats_t stats;

  memset(&logs, 0, sizeof(logs));
  memset(&global_cfg, 0, sizeof(global_cfg));
  memset(&ctx_cfg, 0, sizeof(ctx_cfg));

  global_cfg.version = RTC_API_VERSION;
  global_cfg.size = (uint16_t)sizeof(global_cfg);
  global_cfg.min_log_level = RTC_LOG_DEBUG;
  global_cfg.log_cb = test_log_callback;
  global_cfg.log_user_data = &logs;

  ASSERT_EQ_INT(RTC_OK, rtc_global_init(&global_cfg));

  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_create(NULL, NULL));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_get_state(NULL, &state));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_get_state((rtc_ctx_t *)0x1, NULL));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_get_stats(NULL, &stats));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_signal_packet_drop(NULL, 1));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_signal_protocol_error(NULL, RTC_ERR_PROTOCOL));

  ctx_cfg.version = RTC_API_VERSION;
  ctx_cfg.size = (uint16_t)sizeof(ctx_cfg);
  ctx_cfg.max_retries = 2;
  ctx_cfg.retry_interval_ms = 10;

  ctx = NULL;
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_create(&ctx_cfg, &ctx));
  ASSERT_TRUE(ctx != NULL);

  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_get_state(ctx, NULL));
  ASSERT_EQ_INT(RTC_ERR_INVALID_ARG, rtc_ctx_get_stats(ctx, NULL));

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_destroy(ctx));
  ASSERT_EQ_INT(RTC_OK, rtc_global_deinit());

  ASSERT_TRUE(logs.error_count >= 1);
  return 0;
}

static int test_resource_exhaustion(void) {
  rtc_ctx_t *ctxs[RTC_MAX_CONTEXTS + 1];
  rtc_global_config_t global_cfg;
  rtc_ctx_config_t ctx_cfg;
  uint32_t i;

  memset(&global_cfg, 0, sizeof(global_cfg));
  memset(&ctx_cfg, 0, sizeof(ctx_cfg));
  memset(ctxs, 0, sizeof(ctxs));

  global_cfg.version = RTC_API_VERSION;
  global_cfg.size = (uint16_t)sizeof(global_cfg);
  global_cfg.min_log_level = RTC_LOG_INFO;

  ASSERT_EQ_INT(RTC_OK, rtc_global_init(&global_cfg));

  ctx_cfg.version = RTC_API_VERSION;
  ctx_cfg.size = (uint16_t)sizeof(ctx_cfg);
  ctx_cfg.max_retries = 2;
  ctx_cfg.retry_interval_ms = 10;

  for (i = 0; i < RTC_MAX_CONTEXTS; ++i) {
    ASSERT_EQ_INT(RTC_OK, rtc_ctx_create(&ctx_cfg, &ctxs[i]));
    ASSERT_TRUE(ctxs[i] != NULL);
  }

  ASSERT_EQ_INT(RTC_ERR_RESOURCE_EXHAUSTED, rtc_ctx_create(&ctx_cfg, &ctxs[RTC_MAX_CONTEXTS]));

  for (i = 0; i < RTC_MAX_CONTEXTS; ++i) {
    ASSERT_EQ_INT(RTC_OK, rtc_ctx_destroy(ctxs[i]));
  }

  ASSERT_EQ_INT(RTC_OK, rtc_global_deinit());
  return 0;
}

static int test_timeout_retry_flow(void) {
  rtc_global_config_t global_cfg;
  rtc_ctx_config_t ctx_cfg;
  rtc_ctx_t *ctx;
  rtc_state_t state;
  rtc_stats_t stats;

  memset(&global_cfg, 0, sizeof(global_cfg));
  memset(&ctx_cfg, 0, sizeof(ctx_cfg));
  memset(&stats, 0, sizeof(stats));

  global_cfg.version = RTC_API_VERSION;
  global_cfg.size = (uint16_t)sizeof(global_cfg);
  global_cfg.min_log_level = RTC_LOG_INFO;
  ASSERT_EQ_INT(RTC_OK, rtc_global_init(&global_cfg));

  ctx_cfg.version = RTC_API_VERSION;
  ctx_cfg.size = (uint16_t)sizeof(ctx_cfg);
  ctx_cfg.max_retries = 2;
  ctx_cfg.retry_interval_ms = 10;

  ctx = NULL;
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_create(&ctx_cfg, &ctx));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_start(ctx, 100));

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_tick(ctx, 105));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_tick(ctx, 110));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_tick(ctx, 120));
  ASSERT_EQ_INT(RTC_ERR_TIMEOUT, rtc_ctx_tick(ctx, 130));

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_get_state(ctx, &state));
  ASSERT_EQ_INT(RTC_STATE_FAILED, state);
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_get_stats(ctx, &stats));
  ASSERT_EQ_INT(2, (int)stats.retry_count);
  ASSERT_EQ_INT(2, (int)stats.retransmit_count);
  ASSERT_EQ_INT(1, (int)stats.timeout_count);

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_destroy(ctx));
  ASSERT_EQ_INT(RTC_OK, rtc_global_deinit());
  return 0;
}

static int test_observability_counters_and_logs(void) {
  test_log_capture_t logs;
  rtc_global_config_t global_cfg;
  rtc_ctx_config_t ctx_cfg;
  rtc_ctx_t *ctx;
  rtc_stats_t stats;

  memset(&logs, 0, sizeof(logs));
  memset(&global_cfg, 0, sizeof(global_cfg));
  memset(&ctx_cfg, 0, sizeof(ctx_cfg));
  memset(&stats, 0, sizeof(stats));

  global_cfg.version = RTC_API_VERSION;
  global_cfg.size = (uint16_t)sizeof(global_cfg);
  global_cfg.min_log_level = RTC_LOG_DEBUG;
  global_cfg.log_cb = test_log_callback;
  global_cfg.log_user_data = &logs;
  ASSERT_EQ_INT(RTC_OK, rtc_global_init(&global_cfg));

  ctx_cfg.version = RTC_API_VERSION;
  ctx_cfg.size = (uint16_t)sizeof(ctx_cfg);
  ctx_cfg.max_retries = 2;
  ctx_cfg.retry_interval_ms = 10;

  ctx = NULL;
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_create(&ctx_cfg, &ctx));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_signal_packet_drop(ctx, 3));
  ASSERT_EQ_INT(RTC_OK, rtc_ctx_signal_retransmit(ctx, 2));
  ASSERT_EQ_INT(RTC_ERR_PROTOCOL, rtc_ctx_signal_protocol_error(ctx, RTC_ERR_PROTOCOL));

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_get_stats(ctx, &stats));
  ASSERT_EQ_INT(3, (int)stats.dropped_packet_count);
  ASSERT_EQ_INT(2, (int)stats.retransmit_count);
  ASSERT_EQ_INT(1, (int)stats.protocol_error_count);

  ASSERT_TRUE(logs.error_count >= 1);
  ASSERT_TRUE(logs.last_code == RTC_ERR_PROTOCOL);

  ASSERT_EQ_INT(RTC_OK, rtc_ctx_destroy(ctx));
  ASSERT_EQ_INT(RTC_OK, rtc_global_deinit());
  return 0;
}

int main(void) {
  int failures = 0;

  failures += test_lifecycle_and_idempotent();
  failures += test_invalid_argument_handling();
  failures += test_resource_exhaustion();
  failures += test_timeout_retry_flow();
  failures += test_observability_counters_and_logs();

  if (failures != 0) {
    printf("test failures: %d\n", failures);
    return 1;
  }

  printf("all tests passed\n");
  return 0;
}
