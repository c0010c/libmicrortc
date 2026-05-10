#include "rtc/rtc.h"
#include "sdp/sdp.h"
#include "test_runner.h"

#include <stdio.h>
#include <string.h>

static int read_fixture(const char *path, char *buffer, size_t capacity,
                        size_t *out_len)
{
    FILE *file;
    size_t n;

    file = fopen(path, "rb");
    if (file == 0) {
        return 1;
    }
    n = fread(buffer, 1, capacity - 1u, file);
    if (ferror(file)) {
        fclose(file);
        return 1;
    }
    fclose(file);
    buffer[n] = '\0';
    *out_len = n;
    return 0;
}

int rtc_test_sdp_parser(void)
{
    char sdp[4096];
    size_t sdp_len;
    rtc_sdp_description_t description;

    RTC_TEST_EQ_INT(0, read_fixture("tests/fixtures/chrome-offer-audio-video.sdp",
                                    sdp, sizeof(sdp), &sdp_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_sdp_parse(sdp, sdp_len, &description));
    RTC_TEST_EQ_INT(RTC_SDP_TYPE_OFFER, description.type);
    RTC_TEST_EQ_INT(RTC_SDP_DIRECTION_SENDRECV,
                    description.audio.direction);
    RTC_TEST_EQ_INT(RTC_SDP_DIRECTION_SENDRECV,
                    description.video.direction);
    RTC_TEST_ASSERT(description.has_bundle);
    RTC_TEST_ASSERT(description.audio.has_rtcp_mux);
    RTC_TEST_ASSERT(description.video.has_rtcp_mux);
    RTC_TEST_ASSERT(description.ice_ufrag[0] != '\0');
    RTC_TEST_ASSERT(description.dtls_fingerprint[0] != '\0');
    RTC_TEST_EQ_INT(111, description.audio.payload_type);
    RTC_TEST_EQ_INT(103, description.video.payload_type);

    RTC_TEST_EQ_INT(0, read_fixture("tests/fixtures/chrome-answer-audio-video.sdp",
                                    sdp, sizeof(sdp), &sdp_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_sdp_parse(sdp, sdp_len, &description));
    RTC_TEST_EQ_INT(RTC_SDP_TYPE_ANSWER, description.type);

    RTC_TEST_EQ_INT(0, read_fixture("tests/fixtures/invalid-missing-bundle.sdp",
                                    sdp, sizeof(sdp), &sdp_len));
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_sdp_parse(sdp, sdp_len, &description));

    RTC_TEST_EQ_INT(0, read_fixture("tests/fixtures/invalid-sendonly.sdp",
                                    sdp, sizeof(sdp), &sdp_len));
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
                    rtc_sdp_parse(sdp, sdp_len, &description));

    return 0;
}
