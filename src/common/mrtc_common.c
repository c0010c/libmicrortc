#define _POSIX_C_SOURCE 200809L

#include "mrtc_common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

MRTC_STATUS mrtc_internal_status(MRTC_INTERNAL_STATUS status)
{
    switch (status) {
        case MRTC_INTERNAL_STATUS_OK:
            return MRTC_STATUS_OK;
        case MRTC_INTERNAL_STATUS_PARSE_ERROR:
            return MRTC_STATUS_PARSE_ERROR;
        case MRTC_INTERNAL_STATUS_INVALID_STATE:
            return MRTC_STATUS_INVALID_STATE;
        case MRTC_INTERNAL_STATUS_INVALID_ARG:
        case MRTC_INTERNAL_STATUS_NO_MEMORY:
        case MRTC_INTERNAL_STATUS_IO_ERROR:
        default:
            return MRTC_STATUS_INVALID_ARG;
    }
}

void *mrtc_calloc(size_t count, size_t size)
{
    return calloc(count, size);
}

char *mrtc_strdup(const char *value)
{
    if (value == 0) {
        return 0;
    }

    return mrtc_strndup(value, strlen(value));
}

char *mrtc_strndup(const char *value, size_t len)
{
    char *copy;

    if (value == 0) {
        return 0;
    }

    copy = (char *) malloc(len + 1);
    if (copy == 0) {
        return 0;
    }

    memcpy(copy, value, len);
    copy[len] = '\0';
    return copy;
}

void mrtc_free(void *value)
{
    free(value);
}

uint64_t mrtc_now_ns(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0;
    }

    return ((uint64_t) now.tv_sec * 1000000000ULL) + (uint64_t) now.tv_nsec;
}

MRTC_STATUS mrtc_random_bytes(uint8_t *buffer, size_t len)
{
    size_t offset = 0;
    int fd;

    if (buffer == 0 && len > 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    while (offset < len) {
        ssize_t read_len = read(fd, buffer + offset, len - offset);
        if (read_len < 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return MRTC_STATUS_INVALID_STATE;
        }
        if (read_len == 0) {
            close(fd);
            return MRTC_STATUS_INVALID_STATE;
        }
        offset += (size_t) read_len;
    }

    close(fd);
    return MRTC_STATUS_OK;
}

int mrtc_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t len)
{
    uint8_t diff = 0;
    size_t i;

    if (left == 0 || right == 0) {
        return 0;
    }

    for (i = 0; i < len; ++i) {
        diff = (uint8_t) (diff | (uint8_t) (left[i] ^ right[i]));
    }

    return diff == 0;
}
