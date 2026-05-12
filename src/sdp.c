#include "sdp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MRTC_SDP {
    char *normalized;
    char *first_media_line;
    size_t media_count;
    size_t attribute_count;
};

static int mrtc_has_prefix(const char *line, size_t line_len, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    return line_len >= prefix_len && memcmp(line, prefix, prefix_len) == 0;
}

static MRTC_STATUS mrtc_copy_string(const char *source, size_t len, char **target)
{
    char *copy = (char *) malloc(len + 1);

    if (copy == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (len > 0) {
        memcpy(copy, source, len);
    }
    copy[len] = '\0';
    *target = copy;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_append_line(char **buffer, size_t *len, size_t *capacity, const char *line, size_t line_len)
{
    size_t needed = *len + line_len + 2 + 1;
    char *grown;
    size_t new_capacity = *capacity;

    if (needed > new_capacity) {
        while (needed > new_capacity) {
            new_capacity = new_capacity == 0 ? 256 : new_capacity * 2;
        }

        grown = (char *) realloc(*buffer, new_capacity);
        if (grown == 0) {
            return MRTC_STATUS_INVALID_ARG;
        }

        *buffer = grown;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *len, line, line_len);
    *len += line_len;
    (*buffer)[(*len)++] = '\r';
    (*buffer)[(*len)++] = '\n';
    (*buffer)[*len] = '\0';
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_write_buffer(const char *value, char *buffer, size_t buffer_len, size_t *required_len)
{
    size_t needed;

    if (value == 0 || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    needed = strlen(value) + 1;
    *required_len = needed;

    if (buffer == 0 || buffer_len < needed) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memcpy(buffer, value, needed);
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sdp_parse(const char *sdp, MRTC_SDP **parsed_sdp)
{
    const char *cursor;
    int saw_version = 0;
    MRTC_STATUS status;
    MRTC_SDP *result;
    char *normalized = 0;
    size_t normalized_len = 0;
    size_t normalized_capacity = 0;

    if (sdp == 0 || sdp[0] == '\0' || parsed_sdp == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    result = (MRTC_SDP *) calloc(1, sizeof(*result));
    if (result == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    cursor = sdp;
    while (*cursor != '\0') {
        const char *line_start = cursor;
        size_t line_len;

        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }

        line_len = (size_t) (cursor - line_start);
        if (line_len > 0 && line_start[line_len - 1] == '\r') {
            line_len--;
        }

        if (line_len < 2 || line_start[1] != '=') {
            mrtc_sdp_free(result);
            free(normalized);
            return MRTC_STATUS_PARSE_ERROR;
        }

        if (mrtc_has_prefix(line_start, line_len, "v=0")) {
            saw_version = 1;
        } else if (mrtc_has_prefix(line_start, line_len, "m=")) {
            result->media_count++;
            if (result->first_media_line == 0) {
                status = mrtc_copy_string(line_start, line_len, &result->first_media_line);
                if (status != MRTC_STATUS_OK) {
                    mrtc_sdp_free(result);
                    free(normalized);
                    return status;
                }
            }
        } else if (mrtc_has_prefix(line_start, line_len, "a=")) {
            result->attribute_count++;
        }

        status = mrtc_append_line(&normalized, &normalized_len, &normalized_capacity, line_start, line_len);
        if (status != MRTC_STATUS_OK) {
            mrtc_sdp_free(result);
            free(normalized);
            return status;
        }

        if (*cursor == '\n') {
            cursor++;
        }
    }

    if (!saw_version || result->media_count == 0) {
        mrtc_sdp_free(result);
        free(normalized);
        return MRTC_STATUS_PARSE_ERROR;
    }

    result->normalized = normalized;
    *parsed_sdp = result;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sdp_serialize(const MRTC_SDP *parsed_sdp, char *buffer, size_t buffer_len, size_t *required_len)
{
    if (parsed_sdp == 0 || parsed_sdp->normalized == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    return mrtc_write_buffer(parsed_sdp->normalized, buffer, buffer_len, required_len);
}

MRTC_STATUS mrtc_sdp_create_answer(const char *remote_offer, char *buffer, size_t buffer_len, size_t *required_len)
{
    MRTC_STATUS status;
    MRTC_SDP *offer = 0;
    char answer[1024];

    status = mrtc_sdp_parse(remote_offer, &offer);
    if (status != MRTC_STATUS_OK) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    (void) snprintf(answer,
                   sizeof(answer),
                   "v=0\r\n"
                   "o=- 0 0 IN IP4 127.0.0.1\r\n"
                   "s=libmicrortc\r\n"
                   "t=0 0\r\n"
                   "%s\r\n"
                   "c=IN IP4 0.0.0.0\r\n"
                   "a=recvonly\r\n",
                   offer->first_media_line);

    mrtc_sdp_free(offer);
    return mrtc_write_buffer(answer, buffer, buffer_len, required_len);
}

void mrtc_sdp_free(MRTC_SDP *parsed_sdp)
{
    if (parsed_sdp == 0) {
        return;
    }

    free(parsed_sdp->normalized);
    free(parsed_sdp->first_media_line);
    free(parsed_sdp);
}
