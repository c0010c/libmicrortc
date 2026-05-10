#ifndef RTC_COUNTERS_H
#define RTC_COUNTERS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_memory_counters_t {
    uint64_t capacity_errors;
} rtc_memory_counters_t;

typedef struct rtc_executor_counters_t {
    uint64_t affinity_errors;
} rtc_executor_counters_t;

typedef struct rtc_api_counters_t {
    uint64_t create_calls;
    uint64_t destroy_calls;
    uint64_t unsupported_api_calls;
} rtc_api_counters_t;

typedef struct rtc_trace_counters_t {
    uint64_t trace_events;
} rtc_trace_counters_t;

typedef struct rtc_peer_connection_counters_t {
    rtc_memory_counters_t memory;
    rtc_executor_counters_t executor;
    rtc_api_counters_t api;
    rtc_trace_counters_t trace;
} rtc_peer_connection_counters_t;

#ifdef __cplusplus
}
#endif

#endif
