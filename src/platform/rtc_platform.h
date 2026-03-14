#ifndef RTC_PLATFORM_H_
#define RTC_PLATFORM_H_

#include <stddef.h>
#include <stdint.h>

#include "rtc/rtc.h"

typedef struct rtc_log_sink {
  rtc_log_level_t min_level;
  rtc_log_callback_t cb;
  void *user_data;
} rtc_log_sink_t;

void rtc_platform_log(const rtc_log_sink_t *sink, rtc_log_level_t level, const char *module,
                      uint32_t peer_id, rtc_result_t code, const char *message);

int rtc_platform_copy_string(char *dst, uint16_t dst_size, const char *src);
void rtc_platform_zero(void *ptr, size_t size);

#endif  // RTC_PLATFORM_H_
