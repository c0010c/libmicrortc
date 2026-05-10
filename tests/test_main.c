#include "test_runner.h"

int rtc_test_build(void);
int rtc_test_memory(void);
int rtc_test_executor_affinity(void);
int rtc_test_peer_connection(void);
int rtc_test_observability(void);
int rtc_test_sdp_writer(void);
int rtc_test_sdp_parser(void);
int rtc_test_jsep(void);
int rtc_test_stun(void);
int rtc_test_ice(void);
int rtc_test_datagram(void);
int rtc_test_security(void);
int rtc_test_media_api(void);
int rtc_test_rtp(void);
int rtc_test_rtcp(void);

int main(void)
{
    rtc_test_result_t result = {0, 0};

    RTC_RUN_TEST(rtc_test_build, result);
    RTC_RUN_TEST(rtc_test_memory, result);
    RTC_RUN_TEST(rtc_test_executor_affinity, result);
    RTC_RUN_TEST(rtc_test_peer_connection, result);
    RTC_RUN_TEST(rtc_test_observability, result);
    RTC_RUN_TEST(rtc_test_sdp_writer, result);
    RTC_RUN_TEST(rtc_test_sdp_parser, result);
    RTC_RUN_TEST(rtc_test_jsep, result);
    RTC_RUN_TEST(rtc_test_stun, result);
    RTC_RUN_TEST(rtc_test_ice, result);
    RTC_RUN_TEST(rtc_test_datagram, result);
    RTC_RUN_TEST(rtc_test_security, result);
    RTC_RUN_TEST(rtc_test_media_api, result);
    RTC_RUN_TEST(rtc_test_rtp, result);
    RTC_RUN_TEST(rtc_test_rtcp, result);

    if (result.failed != 0) {
        return 1;
    }

    return 0;
}
