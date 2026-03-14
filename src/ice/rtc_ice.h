#ifndef RTC_ICE_H_
#define RTC_ICE_H_

#include <stdint.h>

#include "rtc/rtc.h"

typedef enum rtc_ice_state {
  RTC_ICE_STATE_NEW = 0,
  RTC_ICE_STATE_GATHERING = 1,
  RTC_ICE_STATE_CHECKING = 2,
  RTC_ICE_STATE_CONNECTED = 3,
  RTC_ICE_STATE_FAILED = 4
} rtc_ice_state_t;

typedef struct rtc_ice_event {
  uint8_t emit_local_description;
  uint8_t emit_local_candidate;
  uint8_t retry_performed;
  uint8_t connected;
  uint8_t failed;
  rtc_result_t error;
} rtc_ice_event_t;

typedef struct rtc_ice_ctx {
  rtc_ice_state_t state;
  uint8_t local_description_emitted;
  uint8_t local_candidate_emitted;
  uint8_t remote_description_set;
  uint16_t local_candidate_count;
  uint16_t remote_candidate_count;
  uint16_t connect_ticks;
  uint16_t retry_count;
  uint32_t last_tick_ms;
  char local_sdp[RTC_CFG_MAX_SDP_LEN];
  char local_description_type[16];
  char local_candidate[RTC_CFG_MAX_CANDIDATE_LEN];
  char remote_sdp[RTC_CFG_MAX_SDP_LEN];
  char remote_description_type[16];
  char remote_candidates[RTC_CFG_MAX_REMOTE_CANDIDATES][RTC_CFG_MAX_CANDIDATE_LEN];
} rtc_ice_ctx_t;

void rtc_ice_init(rtc_ice_ctx_t *ctx);
rtc_result_t rtc_ice_start(rtc_ice_ctx_t *ctx, uint32_t peer_id, uint32_t now_ms);
rtc_result_t rtc_ice_set_remote_description(rtc_ice_ctx_t *ctx, const char *sdp,
                                            const char *type);
rtc_result_t rtc_ice_add_remote_candidate(rtc_ice_ctx_t *ctx, const char *candidate);
void rtc_ice_tick(rtc_ice_ctx_t *ctx, uint32_t now_ms, uint16_t retry_interval_ms,
                  uint16_t max_retries, rtc_ice_event_t *out_event);

#endif  // RTC_ICE_H_
