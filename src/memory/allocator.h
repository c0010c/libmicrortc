#ifndef RTC_MEMORY_ALLOCATOR_H
#define RTC_MEMORY_ALLOCATOR_H

#include <stddef.h>

#include "memory/arena.h"

typedef struct rtc_core_allocator_stats_t {
    size_t allocation_count;
    size_t bytes_requested;
    size_t failed_count;
} rtc_core_allocator_stats_t;

void *rtc_core_alloc(rtc_arena_view_t *arena, size_t size, size_t alignment,
                     rtc_capacity_resource_t resource,
                     rtc_capacity_diagnostics_t *diag);
void rtc_core_reset_allocator_stats(void);
rtc_core_allocator_stats_t rtc_core_allocator_stats(void);

#endif
