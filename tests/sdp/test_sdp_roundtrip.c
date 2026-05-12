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

    if (required_len == 0 || !contains(answer, "v=0") || !contains(answer, "m=")) {
        return 1;
    }

    return 0;
}
