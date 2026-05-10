#ifndef RTC_TEST_RUNNER_H
#define RTC_TEST_RUNNER_H

#include <stdio.h>

typedef struct rtc_test_result_t {
    int passed;
    int failed;
} rtc_test_result_t;

#define RTC_TEST_ASSERT(expr)                                                   \
    do {                                                                        \
        if (!(expr)) {                                                          \
            fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__,         \
                    __LINE__, #expr);                                           \
            return 1;                                                           \
        }                                                                       \
    } while (0)

#define RTC_TEST_EQ_INT(expected, actual)                                       \
    do {                                                                        \
        int rtc_expected__ = (int)(expected);                                   \
        int rtc_actual__ = (int)(actual);                                       \
        if (rtc_expected__ != rtc_actual__) {                                   \
            fprintf(stderr, "%s:%d: expected %d, got %d\n", __FILE__,          \
                    __LINE__, rtc_expected__, rtc_actual__);                    \
            return 1;                                                           \
        }                                                                       \
    } while (0)

#define RTC_RUN_TEST(fn, result)                                                \
    do {                                                                        \
        int rtc_test_status__ = fn();                                           \
        if (rtc_test_status__ == 0) {                                           \
            (result).passed++;                                                  \
        } else {                                                                \
            (result).failed++;                                                  \
        }                                                                       \
    } while (0)

#endif
