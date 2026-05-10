#include "net/demux.h"

#include "stun/stun.h"

const char *rtc_net_protocol_name(rtc_net_protocol_t protocol)
{
    switch (protocol) {
    case RTC_NET_PROTOCOL_STUN:
        return "stun";
    case RTC_NET_PROTOCOL_DTLS:
        return "dtls";
    case RTC_NET_PROTOCOL_RTP:
        return "rtp";
    case RTC_NET_PROTOCOL_RTCP:
        return "rtcp";
    case RTC_NET_PROTOCOL_UNKNOWN:
    default:
        return "unknown";
    }
}

rtc_status_t rtc_net_demux_datagram(const uint8_t *data, size_t data_len,
                                    rtc_net_protocol_t *out_protocol)
{
    uint8_t first;

    if (data == 0 || data_len == 0 || out_protocol == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    if (rtc_stun_is_datagram(data, data_len)) {
        *out_protocol = RTC_NET_PROTOCOL_STUN;
        return RTC_STATUS_OK;
    }

    first = data[0];
    if (first >= 20u && first <= 63u) {
        *out_protocol = RTC_NET_PROTOCOL_DTLS;
        return RTC_STATUS_OK;
    }

    if (first >= 128u && first <= 191u && data_len >= 2u) {
        if (data[1] >= 192u && data[1] <= 223u) {
            *out_protocol = RTC_NET_PROTOCOL_RTCP;
        } else {
            *out_protocol = RTC_NET_PROTOCOL_RTP;
        }
        return RTC_STATUS_OK;
    }

    *out_protocol = RTC_NET_PROTOCOL_UNKNOWN;
    return RTC_STATUS_OK;
}
