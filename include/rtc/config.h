#ifndef RTC_CONFIG_H
#define RTC_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/executor.h"
#include "rtc/limits.h"
#include "rtc/observer.h"

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

typedef struct rtc_sdp_parameters_t {
    const char *ice_ufrag;
    size_t ice_ufrag_len;
    const char *ice_pwd;
    size_t ice_pwd_len;
    const char *dtls_fingerprint;
    size_t dtls_fingerprint_len;
    const char *dtls_setup;
    size_t dtls_setup_len;
    uint64_t session_id;
    uint64_t session_version;
} rtc_sdp_parameters_t;

typedef struct rtc_peer_connection_config_t {
    rtc_arena_t arena;
    rtc_peer_connection_limits_t limits;
    rtc_sdp_parameters_t sdp;
    void *platform;
    rtc_executors_t executors;
    rtc_observer_vtable_t observer;
    void *security_backend;
} rtc_peer_connection_config_t;

#ifdef __cplusplus
}
#endif

#endif
