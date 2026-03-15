#include "webrtc/webrtc_config.h"
#include "webrtc/webrtc_status.h"

static int test_default_backfill(void)
{
    webrtc_config_t cfg;
    webrtc_status_t status = WEBRTC_STATUS_OK;

    status = webrtc_config_init_default(&cfg);
    if (status != WEBRTC_STATUS_OK) {
        return 1;
    }

    cfg.ice_local_candidate_gather_timeout_ms = 0;
    cfg.max_stun_transaction_count = 0;

    status = webrtc_config_validate(&cfg);
    if (status != WEBRTC_STATUS_OK) {
        return 2;
    }

    if (cfg.ice_local_candidate_gather_timeout_ms != WEBRTC_CONFIG_DEFAULT_ICE_LOCAL_CANDIDATE_GATHER_TIMEOUT_MS) {
        return 3;
    }

    if (cfg.max_stun_transaction_count != WEBRTC_CONFIG_DEFAULT_MAX_STUN_TRANSACTION_COUNT) {
        return 4;
    }

    return 0;
}

static int test_out_of_range(void)
{
    webrtc_config_t cfg;
    webrtc_status_t status = WEBRTC_STATUS_OK;

    status = webrtc_config_init_default(&cfg);
    if (status != WEBRTC_STATUS_OK) {
        return 11;
    }

    cfg.maximum_transmission_unit = WEBRTC_CONFIG_MAX_MAXIMUM_TRANSMISSION_UNIT + 1;
    status = webrtc_config_validate(&cfg);
    if (status != WEBRTC_STATUS_CONFIG_OUT_OF_RANGE) {
        return 12;
    }

    cfg.maximum_transmission_unit = WEBRTC_CONFIG_DEFAULT_MAXIMUM_TRANSMISSION_UNIT;
    cfg.event_queue_capacity = WEBRTC_CONFIG_MAX_EVENT_QUEUE_CAPACITY + 1;
    status = webrtc_config_validate(&cfg);
    if (status != WEBRTC_STATUS_CONFIG_OUT_OF_RANGE) {
        return 13;
    }

    return 0;
}

int main(void)
{
    int rc = test_default_backfill();
    if (rc != 0) {
        return rc;
    }

    rc = test_out_of_range();
    if (rc != 0) {
        return rc;
    }

    return 0;
}
