#ifndef RTC_SECURITY_INTERNAL_H
#define RTC_SECURITY_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/security.h"
#include "rtc/status.h"

typedef struct rtc_peer_connection_t rtc_peer_connection_t;

rtc_status_t rtc_security_init(
    rtc_peer_connection_t *pc,
    const rtc_security_backend_config_t *backend);
void rtc_security_shutdown(rtc_peer_connection_t *pc);
rtc_status_t rtc_security_on_ice_connected(rtc_peer_connection_t *pc);
rtc_status_t rtc_security_prepare_local_fingerprint(rtc_peer_connection_t *pc);
rtc_status_t rtc_security_handle_dtls_datagram(rtc_peer_connection_t *pc,
                                               const uint8_t *data,
                                               size_t data_len);

#endif
