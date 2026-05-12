#ifndef MRTC_COMMON_H
#define MRTC_COMMON_H

#include <micrortc/micrortc.h>

#include <stddef.h>
#include <stdint.h>

typedef enum MRTC_INTERNAL_STATUS {
    MRTC_INTERNAL_STATUS_OK = 0,
    MRTC_INTERNAL_STATUS_INVALID_ARG = 1,
    MRTC_INTERNAL_STATUS_NO_MEMORY = 2,
    MRTC_INTERNAL_STATUS_PARSE_ERROR = 3,
    MRTC_INTERNAL_STATUS_IO_ERROR = 4,
    MRTC_INTERNAL_STATUS_INVALID_STATE = 5
} MRTC_INTERNAL_STATUS;

MRTC_STATUS mrtc_internal_status(MRTC_INTERNAL_STATUS status);
void *mrtc_calloc(size_t count, size_t size);
char *mrtc_strdup(const char *value);
char *mrtc_strndup(const char *value, size_t len);
void mrtc_free(void *value);
uint64_t mrtc_now_ns(void);
MRTC_STATUS mrtc_random_bytes(uint8_t *buffer, size_t len);
int mrtc_constant_time_equal(const uint8_t *left, const uint8_t *right, size_t len);

#endif /* MRTC_COMMON_H */
