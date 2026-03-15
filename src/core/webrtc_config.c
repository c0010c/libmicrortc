#include "webrtc/webrtc_config.h"

#include <stddef.h>

static uint32_t clamp_or_default(uint32_t value, uint32_t default_value)
{
    if (value == 0U) {
        return default_value;
    }

    return value;
}

static webrtc_status_t validate_range_u32(uint32_t value, uint32_t min_value, uint32_t max_value)
{
    if (value < min_value || value > max_value) {
        return WEBRTC_STATUS_CONFIG_OUT_OF_RANGE;
    }

    return WEBRTC_STATUS_OK;
}

webrtc_status_t webrtc_config_init_default(webrtc_config_t* out_config)
{
    if (out_config == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    out_config->ice_local_candidate_gather_timeout_ms = WEBRTC_CONFIG_DEFAULT_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS;
    out_config->ice_connection_check_timeout_ms = WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_TIMEOUT_MS;
    out_config->ice_candidate_nomination_timeout_ms = WEBRTC_CONFIG_DEFAULT_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS;
    out_config->ice_connection_check_polling_interval_ms = WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS;
    out_config->max_remote_candidate_count = WEBRTC_CONFIG_DEFAULT_MAX_REMOTE_CANDIDATE_COUNT;
    out_config->max_stun_transaction_count = WEBRTC_CONFIG_DEFAULT_MAX_STUN_TRANSACTION_COUNT;
    out_config->event_queue_capacity = WEBRTC_CONFIG_DEFAULT_EVENT_QUEUE_CAPACITY;
    out_config->data_channel_queue_capacity = WEBRTC_CONFIG_DEFAULT_DATA_CHANNEL_QUEUE_CAPACITY;
    out_config->maximum_transmission_unit = WEBRTC_CONFIG_DEFAULT_MAXIMUM_TRANSMISSION_UNIT;

    return WEBRTC_STATUS_OK;
}

webrtc_status_t webrtc_config_validate(webrtc_config_t* inout_config)
{
    webrtc_status_t status = WEBRTC_STATUS_OK;

    if (inout_config == NULL) {
        return WEBRTC_STATUS_INVALID_ARG;
    }

    /* Match reference behavior: 0 means use default, then validate bounds. */
    inout_config->ice_local_candidate_gather_timeout_ms =
        clamp_or_default(inout_config->ice_local_candidate_gather_timeout_ms,
                         WEBRTC_CONFIG_DEFAULT_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS);
    inout_config->ice_connection_check_timeout_ms =
        clamp_or_default(inout_config->ice_connection_check_timeout_ms,
                         WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_TIMEOUT_MS);
    inout_config->ice_candidate_nomination_timeout_ms =
        clamp_or_default(inout_config->ice_candidate_nomination_timeout_ms,
                         WEBRTC_CONFIG_DEFAULT_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS);
    inout_config->ice_connection_check_polling_interval_ms =
        clamp_or_default(inout_config->ice_connection_check_polling_interval_ms,
                         WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS);
    inout_config->max_remote_candidate_count =
        clamp_or_default(inout_config->max_remote_candidate_count, WEBRTC_CONFIG_DEFAULT_MAX_REMOTE_CANDIDATE_COUNT);
    inout_config->max_stun_transaction_count =
        clamp_or_default(inout_config->max_stun_transaction_count, WEBRTC_CONFIG_DEFAULT_MAX_STUN_TRANSACTION_COUNT);
    inout_config->event_queue_capacity =
        clamp_or_default(inout_config->event_queue_capacity, WEBRTC_CONFIG_DEFAULT_EVENT_QUEUE_CAPACITY);
    inout_config->data_channel_queue_capacity =
        clamp_or_default(inout_config->data_channel_queue_capacity, WEBRTC_CONFIG_DEFAULT_DATA_CHANNEL_QUEUE_CAPACITY);
    inout_config->maximum_transmission_unit =
        clamp_or_default(inout_config->maximum_transmission_unit, WEBRTC_CONFIG_DEFAULT_MAXIMUM_TRANSMISSION_UNIT);

    status = validate_range_u32(inout_config->ice_local_candidate_gather_timeout_ms,
                                WEBRTC_CONFIG_MIN_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS,
                                WEBRTC_CONFIG_MAX_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->ice_connection_check_timeout_ms,
                                WEBRTC_CONFIG_MIN_ICE_CONNECTION_CHECK_TIMEOUT_MS,
                                WEBRTC_CONFIG_MAX_ICE_CONNECTION_CHECK_TIMEOUT_MS);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->ice_candidate_nomination_timeout_ms,
                                WEBRTC_CONFIG_MIN_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS,
                                WEBRTC_CONFIG_MAX_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->ice_connection_check_polling_interval_ms,
                                WEBRTC_CONFIG_MIN_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS,
                                WEBRTC_CONFIG_MAX_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->max_remote_candidate_count, WEBRTC_CONFIG_MIN_MAX_REMOTE_CANDIDATE_COUNT,
                                WEBRTC_CONFIG_MAX_MAX_REMOTE_CANDIDATE_COUNT);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->max_stun_transaction_count, WEBRTC_CONFIG_MIN_MAX_STUN_TRANSACTION_COUNT,
                                WEBRTC_CONFIG_MAX_MAX_STUN_TRANSACTION_COUNT);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->event_queue_capacity, WEBRTC_CONFIG_MIN_EVENT_QUEUE_CAPACITY,
                                WEBRTC_CONFIG_MAX_EVENT_QUEUE_CAPACITY);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->data_channel_queue_capacity, WEBRTC_CONFIG_MIN_DATA_CHANNEL_QUEUE_CAPACITY,
                                WEBRTC_CONFIG_MAX_DATA_CHANNEL_QUEUE_CAPACITY);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    status = validate_range_u32(inout_config->maximum_transmission_unit, WEBRTC_CONFIG_MIN_MAXIMUM_TRANSMISSION_UNIT,
                                WEBRTC_CONFIG_MAX_MAXIMUM_TRANSMISSION_UNIT);
    if (status != WEBRTC_STATUS_OK) {
        return status;
    }

    return WEBRTC_STATUS_OK;
}
