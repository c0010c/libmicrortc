#include "../../src/sdp.h"

#include <stdio.h>
#include <string.h>

static int contains(const char *text, const char *needle)
{
    return strstr(text, needle) != 0;
}

static int read_fixture(char *buffer, size_t buffer_len)
{
    FILE *file = fopen("tests/fixtures/minimal_offer.sdp", "rb");
    size_t read_len;

    if (file == 0) {
        return 0;
    }

    read_len = fread(buffer, 1, buffer_len - 1, file);
    fclose(file);

    buffer[read_len] = '\0';
    return read_len > 0;
}

static int answer_for_setup(const char *setup, const char *expected_answer_setup)
{
    char offer[2048];
    char answer[4096];
    size_t required_len = 0;

    snprintf(offer,
             sizeof(offer),
             "v=0\r\n"
             "o=- 1 1 IN IP4 127.0.0.1\r\n"
             "s=-\r\n"
             "t=0 0\r\n"
             "m=video 9 UDP/TLS/RTP/SAVPF 96\r\n"
             "c=IN IP4 0.0.0.0\r\n"
             "a=mid:0\r\n"
             "a=fingerprint:sha-256 11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00\r\n"
             "a=setup:%s\r\n",
             setup);

    if (mrtc_sdp_create_answer_ex(offer,
                                  1,
                                  "localUfrag",
                                  "localPassword000000000000",
                                  "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99",
                                  answer,
                                  sizeof(answer),
                                  &required_len) != MRTC_STATUS_OK) {
        return 0;
    }

    return contains(answer, expected_answer_setup) &&
           contains(answer, "a=fingerprint:sha-256 AA:BB") &&
           contains(answer, "a=ice-ufrag:localUfrag") &&
           contains(answer, "a=ice-pwd:localPassword") &&
           contains(answer, "m=application 9 UDP/DTLS/SCTP webrtc-datachannel") &&
           contains(answer, "a=sctp-port:5000");
}

int main(void)
{
    char fixture[2048];
    char serialized[2048];
    char answer[2048];
    size_t required_len = 0;
    MRTC_SDP *parsed = 0;

    if (!read_fixture(fixture, sizeof(fixture))) {
        return 1;
    }

    if (mrtc_sdp_parse(fixture, &parsed) != MRTC_STATUS_OK || parsed == 0) {
        return 1;
    }

    if (mrtc_sdp_serialize(parsed, serialized, sizeof(serialized), &required_len) != MRTC_STATUS_OK) {
        mrtc_sdp_free(parsed);
        return 1;
    }

    if (!contains(serialized, "v=0") || !contains(serialized, "m=") || !contains(serialized, "a=rtpmap")) {
        mrtc_sdp_free(parsed);
        return 1;
    }

    mrtc_sdp_free(parsed);

    if (mrtc_sdp_create_answer("not-sdp", answer, sizeof(answer), &required_len) != MRTC_STATUS_PARSE_ERROR) {
        return 1;
    }

    if (mrtc_sdp_create_answer(fixture, answer, sizeof(answer), &required_len) != MRTC_STATUS_OK) {
        return 1;
    }

    if (required_len == 0 || !contains(answer, "v=0") || !contains(answer, "m=") ||
        !contains(answer, "a=fingerprint:sha-256") || !contains(answer, "a=ice-ufrag:") ||
        !contains(answer, "a=setup:active")) {
        return 1;
    }

    if (!answer_for_setup("actpass", "a=setup:active") ||
        !answer_for_setup("active", "a=setup:passive") ||
        !answer_for_setup("passive", "a=setup:active")) {
        return 1;
    }

    if (mrtc_sdp_parse("v=0\r\nm=video 9 UDP/TLS/RTP/SAVPF 96\r\na=fingerprint:sha-1 00:11\r\n", &parsed) != MRTC_STATUS_PARSE_ERROR) {
        return 1;
    }

    return 0;
}
