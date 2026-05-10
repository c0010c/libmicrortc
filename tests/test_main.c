#include "test_runner.h"

int rtc_test_build(void);

int main(void)
{
    rtc_test_result_t result = {0, 0};

    RTC_RUN_TEST(rtc_test_build, result);

    if (result.failed != 0) {
        return 1;
    }

    return 0;
}
