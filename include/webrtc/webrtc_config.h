#ifndef WEBRTC_WEBRTC_CONFIG_H
#define WEBRTC_WEBRTC_CONFIG_H

#include <stdint.h>

#include "webrtc/webrtc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WEBRTC_CONFIG_DEFAULT_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS ((uint32_t) 10000)
#define WEBRTC_CONFIG_MIN_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS ((uint32_t) 100)
#define WEBRTC_CONFIG_MAX_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS ((uint32_t) 120000)

#define WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_TIMEOUT_MS ((uint32_t) 12000)
#define WEBRTC_CONFIG_MIN_ICE_CONNECTION_CHECK_TIMEOUT_MS ((uint32_t) 100)
#define WEBRTC_CONFIG_MAX_ICE_CONNECTION_CHECK_TIMEOUT_MS ((uint32_t) 120000)

#define WEBRTC_CONFIG_DEFAULT_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS ((uint32_t) 12000)
#define WEBRTC_CONFIG_MIN_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS ((uint32_t) 100)
#define WEBRTC_CONFIG_MAX_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS ((uint32_t) 120000)

#define WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS ((uint32_t) 50)
#define WEBRTC_CONFIG_MIN_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS ((uint32_t) 10)
#define WEBRTC_CONFIG_MAX_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS ((uint32_t) 10000)

#define WEBRTC_CONFIG_DEFAULT_MAX_REMOTE_CANDIDATE_COUNT ((uint32_t) 100)
#define WEBRTC_CONFIG_MIN_MAX_REMOTE_CANDIDATE_COUNT ((uint32_t) 1)
#define WEBRTC_CONFIG_MAX_MAX_REMOTE_CANDIDATE_COUNT ((uint32_t) 1024)

#define WEBRTC_CONFIG_DEFAULT_MAX_STUN_TRANSACTION_COUNT ((uint32_t) 20)
#define WEBRTC_CONFIG_MIN_MAX_STUN_TRANSACTION_COUNT ((uint32_t) 1)
#define WEBRTC_CONFIG_MAX_MAX_STUN_TRANSACTION_COUNT ((uint32_t) 1024)

#define WEBRTC_CONFIG_DEFAULT_EVENT_QUEUE_CAPACITY ((uint32_t) 64)
#define WEBRTC_CONFIG_MIN_EVENT_QUEUE_CAPACITY ((uint32_t) 1)
#define WEBRTC_CONFIG_MAX_EVENT_QUEUE_CAPACITY ((uint32_t) 1024)

#define WEBRTC_CONFIG_DEFAULT_DATA_CHANNEL_QUEUE_CAPACITY ((uint32_t) 64)
#define WEBRTC_CONFIG_MIN_DATA_CHANNEL_QUEUE_CAPACITY ((uint32_t) 1)
#define WEBRTC_CONFIG_MAX_DATA_CHANNEL_QUEUE_CAPACITY ((uint32_t) 1024)

#define WEBRTC_CONFIG_DEFAULT_MAXIMUM_TRANSMISSION_UNIT ((uint32_t) 1200)
#define WEBRTC_CONFIG_MIN_MAXIMUM_TRANSMISSION_UNIT ((uint32_t) 576)
#define WEBRTC_CONFIG_MAX_MAXIMUM_TRANSMISSION_UNIT ((uint32_t) 1500)

typedef struct webrtc_config {
    /* Unit: milliseconds. 0 means use default. */
    uint32_t ice_local_candidate_gather_timeout_ms;

    /* Unit: milliseconds. 0 means use default. */
    uint32_t ice_connection_check_timeout_ms;

    /* Unit: milliseconds. 0 means use default. */
    uint32_t ice_candidate_nomination_timeout_ms;

    /* Unit: milliseconds. 0 means use default. */
    uint32_t ice_connection_check_polling_interval_ms;

    /* Unit: count. 0 means use default. */
    uint32_t max_remote_candidate_count;

    /* Unit: count. 0 means use default. */
    uint32_t max_stun_transaction_count;

    /* Unit: count. 0 means use default. */
    uint32_t event_queue_capacity;

    /* Unit: count. 0 means use default. */
    uint32_t data_channel_queue_capacity;

    /* Unit: bytes. 0 means use default. */
    uint32_t maximum_transmission_unit;
} webrtc_config_t;

webrtc_status_t webrtc_config_init_default(webrtc_config_t* out_config);
webrtc_status_t webrtc_config_validate(webrtc_config_t* inout_config);

#ifdef __cplusplus
}
#endif

#endif /* WEBRTC_WEBRTC_CONFIG_H */
