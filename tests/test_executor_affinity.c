#include "executor/executor.h"
#include "rtc/rtc.h"
#include "test_runner.h"

int rtc_test_executor_affinity(void)
{
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_executor_require(RTC_EXECUTOR_SIGNALING));
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_executor_require(RTC_EXECUTOR_MEDIA));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_executor_require(RTC_EXECUTOR_MEDIA));
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_executor_require(RTC_EXECUTOR_NETWORK));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_executor_require(RTC_EXECUTOR_NETWORK));
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_executor_require(RTC_EXECUTOR_SIGNALING));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    return 0;
}
