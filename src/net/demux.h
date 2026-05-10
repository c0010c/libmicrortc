#ifndef RTC_NET_DEMUX_H
#define RTC_NET_DEMUX_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/status.h"

typedef enum rtc_net_protocol_t {
    RTC_NET_PROTOCOL_STUN = 0,
    RTC_NET_PROTOCOL_DTLS,
    RTC_NET_PROTOCOL_RTP,
    RTC_NET_PROTOCOL_RTCP,
    RTC_NET_PROTOCOL_UNKNOWN
} rtc_net_protocol_t;

rtc_status_t rtc_net_demux_datagram(const uint8_t *data, size_t data_len,
                                    rtc_net_protocol_t *out_protocol);
const char *rtc_net_protocol_name(rtc_net_protocol_t protocol);

#endif
