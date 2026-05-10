#include "rtc/rtc.h"
#include "test_runner.h"

int rtc_test_build(void)
{
    RTC_TEST_EQ_INT(0, RTC_STATUS_OK);
    return 0;
}
