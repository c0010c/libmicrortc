#include "memory/allocator.h"

static rtc_core_allocator_stats_t g_rtc_allocator_stats;

void *rtc_core_alloc(rtc_arena_view_t *arena, size_t size, size_t alignment,
                     rtc_capacity_resource_t resource,
                     rtc_capacity_diagnostics_t *diag)
{
    void *ptr;

    ptr = rtc_arena_alloc(arena, size, alignment, resource, diag);
    if (ptr != 0) {
        g_rtc_allocator_stats.allocation_count++;
        g_rtc_allocator_stats.bytes_requested += size;
    } else {
        g_rtc_allocator_stats.failed_count++;
    }

    return ptr;
}

void rtc_core_reset_allocator_stats(void)
{
    g_rtc_allocator_stats.allocation_count = 0;
    g_rtc_allocator_stats.bytes_requested = 0;
    g_rtc_allocator_stats.failed_count = 0;
}

rtc_core_allocator_stats_t rtc_core_allocator_stats(void)
{
    return g_rtc_allocator_stats;
}
