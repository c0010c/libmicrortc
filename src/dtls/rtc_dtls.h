#ifndef RTC_DTLS_H_
#define RTC_DTLS_H_

#include <stdint.h>

#include "rtc/rtc.h"

typedef enum rtc_dtls_state {
  RTC_DTLS_STATE_NEW = 0,
  RTC_DTLS_STATE_HANDSHAKE = 1,
  RTC_DTLS_STATE_CONNECTED = 2,
  RTC_DTLS_STATE_FAILED = 3
} rtc_dtls_state_t;

typedef struct rtc_dtls_event {
  uint8_t connected;
  uint8_t failed;
  rtc_result_t error;
} rtc_dtls_event_t;

typedef struct rtc_dtls_ctx {
  rtc_dtls_state_t state;
  uint16_t handshake_ticks;
} rtc_dtls_ctx_t;

void rtc_dtls_init(rtc_dtls_ctx_t *ctx);
rtc_result_t rtc_dtls_start(rtc_dtls_ctx_t *ctx);
void rtc_dtls_tick(rtc_dtls_ctx_t *ctx, rtc_dtls_event_t *out_event);

#endif  // RTC_DTLS_H_
