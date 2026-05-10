#ifndef RTC_ICE_ICE_H
#define RTC_ICE_ICE_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/status.h"

typedef struct rtc_peer_connection_t rtc_peer_connection_t;

typedef enum rtc_ice_state_t {
    RTC_ICE_NEW = 0,
    RTC_ICE_GATHERING,
    RTC_ICE_GATHERING_COMPLETE,
    RTC_ICE_CHECKING,
    RTC_ICE_CONNECTED,
    RTC_ICE_FAILED
} rtc_ice_state_t;

rtc_status_t rtc_ice_gather_candidates(rtc_peer_connection_t *pc);
rtc_status_t rtc_ice_handle_stun_response(rtc_peer_connection_t *pc,
                                          const uint8_t *data,
                                          size_t data_len);
void rtc_ice_stun_transaction_timeout(void *user_data);

#endif
