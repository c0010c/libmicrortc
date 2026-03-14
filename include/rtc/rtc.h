#ifndef RTC_RTC_H_
#define RTC_RTC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define RTC_API_VERSION 1u
#define RTC_MAX_CONTEXTS 4u

typedef enum rtc_log_level {
  RTC_LOG_ERROR = 0,
  RTC_LOG_WARN = 1,
  RTC_LOG_INFO = 2,
  RTC_LOG_DEBUG = 3
} rtc_log_level_t;

typedef enum rtc_result {
  RTC_OK = 0,
  RTC_ERR_INVALID_ARG = -1,
  RTC_ERR_INVALID_STATE = -2,
  RTC_ERR_RESOURCE_EXHAUSTED = -3,
  RTC_ERR_PROTOCOL = -4,
  RTC_ERR_TIMEOUT = -5,
  RTC_ERR_NOT_INIT = -6
} rtc_result_t;

typedef enum rtc_state {
  RTC_STATE_NEW = 0,
  RTC_STATE_CHECKING = 1,
  RTC_STATE_CONNECTED = 2,
  RTC_STATE_STOPPED = 3,
  RTC_STATE_FAILED = 4
} rtc_state_t;

typedef struct rtc_ctx rtc_ctx_t;

typedef void (*rtc_log_callback_t)(rtc_log_level_t level, const char *module, uint32_t ctx_id,
                                   rtc_result_t code, const char *message, void *user_data);

typedef void (*rtc_state_change_cb_t)(rtc_ctx_t *ctx, rtc_state_t prev_state, rtc_state_t new_state,
                                      void *user_data);

typedef struct rtc_global_config {
  uint16_t version;
  uint16_t size;
  rtc_log_level_t min_log_level;
  rtc_log_callback_t log_cb;
  void *log_user_data;
} rtc_global_config_t;

typedef struct rtc_ctx_config {
  uint16_t version;
  uint16_t size;
  uint16_t max_retries;
  uint16_t retry_interval_ms;
  rtc_state_change_cb_t on_state_change;
  void *user_data;
} rtc_ctx_config_t;

typedef struct rtc_stats {
  uint32_t dropped_packet_count;
  uint32_t retransmit_count;
  uint32_t timeout_count;
  uint32_t protocol_error_count;
  uint16_t retry_count;
  uint16_t queue_depth;
  uint16_t queue_high_watermark;
  uint16_t reserved;
} rtc_stats_t;

rtc_result_t rtc_global_init(const rtc_global_config_t *config);
rtc_result_t rtc_global_deinit(void);

rtc_result_t rtc_ctx_create(const rtc_ctx_config_t *config, rtc_ctx_t **out_ctx);
rtc_result_t rtc_ctx_destroy(rtc_ctx_t *ctx);

rtc_result_t rtc_ctx_start(rtc_ctx_t *ctx, uint32_t now_ms);
rtc_result_t rtc_ctx_stop(rtc_ctx_t *ctx);
rtc_result_t rtc_ctx_tick(rtc_ctx_t *ctx, uint32_t now_ms);
rtc_result_t rtc_ctx_mark_connected(rtc_ctx_t *ctx, uint32_t now_ms);

rtc_result_t rtc_ctx_get_state(const rtc_ctx_t *ctx, rtc_state_t *out_state);
rtc_result_t rtc_ctx_get_stats(const rtc_ctx_t *ctx, rtc_stats_t *out_stats);

rtc_result_t rtc_ctx_signal_packet_drop(rtc_ctx_t *ctx, uint32_t count);
rtc_result_t rtc_ctx_signal_retransmit(rtc_ctx_t *ctx, uint32_t count);
rtc_result_t rtc_ctx_signal_protocol_error(rtc_ctx_t *ctx, rtc_result_t code);

const char *rtc_threading_model(void);
int rtc_is_non_blocking(void);

#ifdef __cplusplus
}
#endif

#endif  // RTC_RTC_H_
