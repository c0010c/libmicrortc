#ifndef RTC_RTCP_INTERNAL_H
#define RTC_RTCP_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/media.h"
#include "rtc/status.h"

typedef struct rtc_peer_connection_t rtc_peer_connection_t;

#define RTC_RTCP_PT_SR 200u
#define RTC_RTCP_PT_RR 201u
#define RTC_RTCP_PT_SDES 202u
#define RTC_RTCP_PT_RTPFB 205u
#define RTC_RTCP_PT_PSFB 206u
#define RTC_RTCP_FMT_NACK 1u
#define RTC_RTCP_FMT_PLI 1u

typedef struct rtc_rtcp_media_stats_t {
    uint32_t ssrc;
    uint64_t packets_sent;
    uint64_t octets_sent;
    uint64_t packets_received;
    uint16_t last_sequence;
    uint32_t jitter;
    uint64_t last_sr_ntp;
    uint32_t last_sr_rtp;
} rtc_rtcp_media_stats_t;

rtc_status_t rtc_rtcp_write_sender_report(
    const rtc_rtcp_media_stats_t *stats, uint8_t *out, size_t capacity,
    size_t *out_len);
rtc_status_t rtc_rtcp_write_receiver_report(
    const rtc_rtcp_media_stats_t *stats, uint8_t *out, size_t capacity,
    size_t *out_len);
rtc_status_t rtc_rtcp_write_sdes(uint32_t ssrc, const char *cname,
                                 size_t cname_len,
                                 size_t max_sdes_cname_bytes, uint8_t *out,
                                 size_t capacity, size_t *out_len);
rtc_status_t rtc_rtcp_write_pli(uint32_t sender_ssrc, uint32_t media_ssrc,
                                uint8_t *out, size_t capacity,
                                size_t *out_len);
rtc_status_t rtc_rtcp_parse_nack(const uint8_t *fci, size_t fci_len,
                                 rtc_media_feedback_t *out_feedback);
rtc_status_t rtc_rtcp_parse_compound(rtc_peer_connection_t *pc,
                                     const uint8_t *packet,
                                     size_t packet_len);

#endif
