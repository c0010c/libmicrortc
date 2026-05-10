#ifndef RTC_SRTP_INTERNAL_H
#define RTC_SRTP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/status.h"

typedef struct rtc_peer_connection_t rtc_peer_connection_t;

rtc_status_t rtc_srtp_protect_rtp(rtc_peer_connection_t *pc,
                                  uint8_t *packet,
                                  size_t *inout_len,
                                  size_t capacity);
rtc_status_t rtc_srtp_unprotect_rtp(rtc_peer_connection_t *pc,
                                    uint8_t *packet,
                                    size_t *inout_len);
rtc_status_t rtc_srtp_protect_rtcp(rtc_peer_connection_t *pc,
                                   uint8_t *packet,
                                   size_t *inout_len,
                                   size_t capacity);
rtc_status_t rtc_srtp_unprotect_rtcp(rtc_peer_connection_t *pc,
                                     uint8_t *packet,
                                     size_t *inout_len);

#endif
