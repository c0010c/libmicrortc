#ifndef RTC_MEMORY_ARENA_H
#define RTC_MEMORY_ARENA_H

#include <stddef.h>

#include "rtc/config.h"

typedef struct rtc_arena_view_t {
    unsigned char *data;
    size_t size;
    size_t offset;
} rtc_arena_view_t;

void rtc_arena_init(rtc_arena_view_t *arena, rtc_arena_t backing);
void *rtc_arena_alloc(rtc_arena_view_t *arena, size_t size, size_t alignment,
                      rtc_capacity_resource_t resource,
                      rtc_capacity_diagnostics_t *diag);
size_t rtc_arena_used(const rtc_arena_view_t *arena);
size_t rtc_arena_remaining(const rtc_arena_view_t *arena);

#endif
