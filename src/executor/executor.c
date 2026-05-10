#include "executor/executor.h"

static rtc_executor_kind_t g_current_executor = RTC_EXECUTOR_SIGNALING;

void rtc_executor_set_current_for_test(rtc_executor_kind_t kind)
{
    g_current_executor = kind;
}

rtc_executor_kind_t rtc_executor_current_kind(void)
{
    return g_current_executor;
}

rtc_status_t rtc_executor_require(rtc_executor_kind_t expected)
{
    if (g_current_executor != expected) {
        return RTC_STATUS_AFFINITY_VIOLATION;
    }

    return RTC_STATUS_OK;
}
