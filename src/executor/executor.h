#ifndef RTC_EXECUTOR_INTERNAL_H
#define RTC_EXECUTOR_INTERNAL_H

#include "rtc/executor.h"

void rtc_executor_set_current_for_test(rtc_executor_kind_t kind);
rtc_executor_kind_t rtc_executor_current_kind(void);
rtc_status_t rtc_executor_require(rtc_executor_kind_t expected);

#endif
