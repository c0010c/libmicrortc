#include "webrtc/webrtc_config.h"
#include "webrtc/webrtc_status.h"

int main(void)
{
    webrtc_config_t cfg;
    webrtc_status_t status = WEBRTC_STATUS_OK;

    status = webrtc_config_init_default(&cfg);
    if (status != WEBRTC_STATUS_OK) {
        return 1;
    }

    if (cfg.ice_local_candidate_gather_timeout_ms != WEBRTC_CONFIG_DEFAULT_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS) {
        return 2;
    }

    if (cfg.ice_connection_check_timeout_ms != WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_TIMEOUT_MS) {
        return 3;
    }

    if (cfg.ice_candidate_nomination_timeout_ms != WEBRTC_CONFIG_DEFAULT_ICE_CANDIDATE_NOMINATION_TIMEOUT_MS) {
        return 4;
    }

    if (cfg.ice_connection_check_polling_interval_ms != WEBRTC_CONFIG_DEFAULT_ICE_CONNECTION_CHECK_POLLING_INTERVAL_MS) {
        return 5;
    }

    if (cfg.max_remote_candidate_count != WEBRTC_CONFIG_DEFAULT_MAX_REMOTE_CANDIDATE_COUNT) {
        return 6;
    }

    if (cfg.max_stun_transaction_count != WEBRTC_CONFIG_DEFAULT_MAX_STUN_TRANSACTION_COUNT) {
        return 7;
    }

    if (cfg.event_queue_capacity != WEBRTC_CONFIG_DEFAULT_EVENT_QUEUE_CAPACITY) {
        return 8;
    }

    if (cfg.data_channel_queue_capacity != WEBRTC_CONFIG_DEFAULT_DATA_CHANNEL_QUEUE_CAPACITY) {
        return 9;
    }

    if (cfg.maximum_transmission_unit != WEBRTC_CONFIG_DEFAULT_MAXIMUM_TRANSMISSION_UNIT) {
        return 10;
    }

    return 0;
}
