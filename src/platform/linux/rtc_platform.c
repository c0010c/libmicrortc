#include "platform/rtc_platform.h"

#include <stdio.h>
#include <string.h>

void rtc_platform_log(const rtc_log_sink_t *sink, rtc_log_level_t level, const char *module,
                      uint32_t peer_id, rtc_result_t code, const char *message) {
  const char *tag = "DEBUG";
  if (!sink) {
    return;
  }
  if (level > sink->min_level) {
    return;
  }

  if (sink->cb) {
    sink->cb(level, module, peer_id, code, message, sink->user_data);
    return;
  }

  if (level == RTC_LOG_ERROR) {
    tag = "ERROR";
  } else if (level == RTC_LOG_WARN) {
    tag = "WARN";
  } else if (level == RTC_LOG_INFO) {
    tag = "INFO";
  }

  fprintf(stderr, "%s\t%s\tpeer=%u\tcode=%d\t%s\n", tag, module ? module : "core",
          peer_id, code, message ? message : "");
}

int rtc_platform_copy_string(char *dst, uint16_t dst_size, const char *src) {
  size_t src_len;
  if (!dst || !src || dst_size == 0u) {
    return 0;
  }
  src_len = strlen(src);
  if (src_len + 1u > dst_size) {
    return 0;
  }
  memcpy(dst, src, src_len + 1u);
  return 1;
}

void rtc_platform_zero(void *ptr, size_t size) {
  if (!ptr || size == 0u) {
    return;
  }
  memset(ptr, 0, size);
}
