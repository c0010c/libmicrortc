#ifndef RTC_CONFIG_H
#define RTC_CONFIG_H

#include <stddef.h>

#include "rtc/limits.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtc_arena_t {
    void *data;
    size_t size;
} rtc_arena_t;

typedef enum rtc_capacity_resource_t {
    RTC_CAPACITY_RESOURCE_ARENA = 0,
    RTC_CAPACITY_RESOURCE_SDP_BUFFER,
    RTC_CAPACITY_RESOURCE_ICE_CANDIDATES,
    RTC_CAPACITY_RESOURCE_TIMER_SLOTS,
    RTC_CAPACITY_RESOURCE_PACKET_CACHE,
    RTC_CAPACITY_RESOURCE_TRACE_BUFFER
} rtc_capacity_resource_t;

typedef struct rtc_capacity_diagnostics_t {
    rtc_capacity_resource_t resource;
    size_t required;
    size_t used;
} rtc_capacity_diagnostics_t;

typedef struct rtc_peer_connection_config_t {
    rtc_arena_t arena;
    rtc_peer_connection_limits_t limits;
    void *platform;
    void *executors;
    void *observer;
    void *security_backend;
} rtc_peer_connection_config_t;

#ifdef __cplusplus
}
#endif

#endif
