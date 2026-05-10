#include "memory/allocator.h"
#include "memory/arena.h"
#include "rtc/rtc.h"
#include "test_runner.h"

#include <stdint.h>

int rtc_test_memory(void)
{
    unsigned char storage[32];
    rtc_arena_view_t arena;
    rtc_capacity_diagnostics_t diag;
    rtc_core_allocator_stats_t stats;
    void *first;
    void *second;

    rtc_arena_init(&arena, (rtc_arena_t){storage, sizeof(storage)});

    first = rtc_arena_alloc(&arena, 1, 8, RTC_CAPACITY_RESOURCE_ARENA, &diag);
    RTC_TEST_ASSERT(first != 0);
    RTC_TEST_EQ_INT(0, ((uintptr_t)first) % 8u);

    second = rtc_arena_alloc(&arena, sizeof(storage), 8,
                             RTC_CAPACITY_RESOURCE_TRACE_BUFFER, &diag);
    RTC_TEST_ASSERT(second == 0);
    RTC_TEST_EQ_INT(RTC_CAPACITY_RESOURCE_TRACE_BUFFER, diag.resource);
    RTC_TEST_EQ_INT((int)sizeof(storage), (int)diag.required);

    rtc_core_reset_allocator_stats();
    stats = rtc_core_allocator_stats();
    RTC_TEST_EQ_INT(0, (int)stats.allocation_count);

    second = rtc_core_alloc(&arena, 4, 4, RTC_CAPACITY_RESOURCE_ARENA, &diag);
    RTC_TEST_ASSERT(second != 0);
    stats = rtc_core_allocator_stats();
    RTC_TEST_EQ_INT(1, (int)stats.allocation_count);
    RTC_TEST_EQ_INT(4, (int)stats.bytes_requested);

    return 0;
}
