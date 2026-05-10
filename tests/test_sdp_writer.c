#include "rtc/rtc.h"
#include "sdp/sdp.h"
#include "test_runner.h"

#include <stdio.h>
#include <string.h>

static rtc_sdp_parameters_t test_sdp_params(const char *setup)
{
    rtc_sdp_parameters_t params;
    params.ice_ufrag = "testufrag";
    params.ice_ufrag_len = strlen(params.ice_ufrag);
    params.ice_pwd = "testpassword1234567890";
    params.ice_pwd_len = strlen(params.ice_pwd);
    params.dtls_fingerprint =
        "sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    params.dtls_fingerprint_len = strlen(params.dtls_fingerprint);
    params.dtls_setup = setup;
    params.dtls_setup_len = strlen(setup);
    params.session_id = 1000;
    params.session_version = 2;
    return params;
}

static int read_fixture(const char *path, char *buffer, size_t capacity,
                        size_t *out_len)
{
    FILE *file;
    char raw[4096];
    size_t n;
    size_t i;
    size_t out;

    file = fopen(path, "rb");
    if (file == 0) {
        return 1;
    }
    n = fread(raw, 1, sizeof(raw), file);
    if (ferror(file)) {
        fclose(file);
        return 1;
    }
    fclose(file);
    out = 0;
    for (i = 0; i < n && out + 2u < capacity; ++i) {
        if (raw[i] == '\n' && (i == 0 || raw[i - 1u] != '\r')) {
            buffer[out++] = '\r';
            buffer[out++] = '\n';
        } else {
            buffer[out++] = raw[i];
        }
    }
    buffer[out] = '\0';
    *out_len = out;
    return 0;
}

static int contains_text(const char *data, const char *needle)
{
    return strstr(data, needle) != 0;
}

int rtc_test_sdp_writer(void)
{
    rtc_sdp_parameters_t params;
    char sdp[2048];
    char fixture[2048];
    size_t sdp_len;
    size_t fixture_len;

    params = test_sdp_params("actpass");
    sdp_len = sizeof(sdp);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_sdp_write_offer(&params, RTC_SDP_DIRECTION_SENDRECV,
                                        RTC_SDP_DIRECTION_SENDRECV, sdp,
                                        &sdp_len));
    RTC_TEST_ASSERT(contains_text(sdp, "a=group:BUNDLE 0 1"));
    RTC_TEST_ASSERT(contains_text(sdp, "a=rtcp-mux"));
    RTC_TEST_ASSERT(contains_text(sdp, "a=ice-options:trickle"));
    RTC_TEST_ASSERT(contains_text(sdp, "opus/48000/2"));
    RTC_TEST_ASSERT(contains_text(sdp, "H264/90000"));
    RTC_TEST_ASSERT(contains_text(sdp, "profile-level-id=42001f"));
    RTC_TEST_ASSERT(!contains_text(sdp, "a=candidate:"));
    RTC_TEST_EQ_INT(0, read_fixture("tests/fixtures/expected-local-offer.sdp",
                                    fixture, sizeof(fixture), &fixture_len));
    RTC_TEST_EQ_INT((int)fixture_len, (int)sdp_len);
    RTC_TEST_ASSERT(memcmp(sdp, fixture, sdp_len) == 0);

    sdp_len = sizeof(sdp);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_sdp_write_offer(&params, RTC_SDP_DIRECTION_RECVONLY,
                                        RTC_SDP_DIRECTION_RECVONLY, sdp,
                                        &sdp_len));
    RTC_TEST_ASSERT(contains_text(sdp, "a=recvonly"));
    RTC_TEST_ASSERT(!contains_text(sdp, "a=sendonly"));

    params = test_sdp_params("active");
    sdp_len = sizeof(sdp);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_sdp_write_answer(&params, RTC_SDP_DIRECTION_SENDRECV,
                                         RTC_SDP_DIRECTION_SENDRECV, sdp,
                                         &sdp_len));
    RTC_TEST_ASSERT(!contains_text(sdp, "a=candidate:"));
    RTC_TEST_EQ_INT(0, read_fixture("tests/fixtures/expected-local-answer.sdp",
                                    fixture, sizeof(fixture), &fixture_len));
    RTC_TEST_EQ_INT((int)fixture_len, (int)sdp_len);
    RTC_TEST_ASSERT(memcmp(sdp, fixture, sdp_len) == 0);

    sdp_len = 8;
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_SDP_BUFFER,
                    rtc_sdp_write_offer(&params, RTC_SDP_DIRECTION_SENDRECV,
                                        RTC_SDP_DIRECTION_SENDRECV, sdp,
                                        &sdp_len));
    RTC_TEST_ASSERT(sdp_len > 8);

    return 0;
}
