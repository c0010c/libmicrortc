#include "rtcp/rtcp.h"

#include "api/peer_connection.h"
#include "media/media.h"

#include <string.h>

static void rtc_write_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value >> 8);
    out[1] = (uint8_t)(value & 0xffu);
}

static void rtc_write_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)((value >> 16) & 0xffu);
    out[2] = (uint8_t)((value >> 8) & 0xffu);
    out[3] = (uint8_t)(value & 0xffu);
}

static uint16_t rtc_read_u16(const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8) | in[1]);
}

static uint32_t rtc_read_u32(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8) | in[3];
}

static void rtc_rtcp_write_header(uint8_t *out, uint8_t count,
                                  uint8_t packet_type, uint16_t length_words)
{
    out[0] = (uint8_t)(0x80u | (count & 0x1fu));
    out[1] = packet_type;
    rtc_write_u16(out + 2, length_words);
}

static rtc_rtcp_media_stats_t *rtc_rtcp_find_stats(rtc_peer_connection_t *pc,
                                                   uint32_t ssrc)
{
    if (pc == 0) {
        return 0;
    }
    if (pc->rtcp_audio.ssrc == ssrc) {
        return &pc->rtcp_audio;
    }
    if (pc->rtcp_video.ssrc == ssrc) {
        return &pc->rtcp_video;
    }
    return &pc->rtcp_audio;
}

rtc_status_t rtc_rtcp_write_sender_report(
    const rtc_rtcp_media_stats_t *stats, uint8_t *out, size_t capacity,
    size_t *out_len)
{
    uint32_t ntp_msw;
    uint32_t ntp_lsw;

    if (stats == 0 || out == 0 || out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (capacity < 28u) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    ntp_msw = (uint32_t)(stats->last_sr_ntp >> 32);
    ntp_lsw = (uint32_t)(stats->last_sr_ntp & 0xffffffffu);
    rtc_rtcp_write_header(out, 0, RTC_RTCP_PT_SR, 6);
    rtc_write_u32(out + 4, stats->ssrc);
    rtc_write_u32(out + 8, ntp_msw);
    rtc_write_u32(out + 12, ntp_lsw);
    rtc_write_u32(out + 16, stats->last_sr_rtp);
    rtc_write_u32(out + 20, (uint32_t)stats->packets_sent);
    rtc_write_u32(out + 24, (uint32_t)stats->octets_sent);
    *out_len = 28;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtcp_write_receiver_report(
    const rtc_rtcp_media_stats_t *stats, uint8_t *out, size_t capacity,
    size_t *out_len)
{
    if (stats == 0 || out == 0 || out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (capacity < 8u) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    rtc_rtcp_write_header(out, 0, RTC_RTCP_PT_RR, 1);
    rtc_write_u32(out + 4, stats->ssrc);
    *out_len = 8;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtcp_write_sdes(uint32_t ssrc, const char *cname,
                                 size_t cname_len,
                                 size_t max_sdes_cname_bytes, uint8_t *out,
                                 size_t capacity, size_t *out_len)
{
    size_t body_len;
    size_t padded_len;
    size_t total_len;

    if (cname == 0 || out == 0 || out_len == 0 || cname_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (cname_len > max_sdes_cname_bytes || cname_len > 255u) {
        return RTC_STATUS_CAPACITY;
    }

    body_len = 4u + 2u + cname_len + 1u;
    padded_len = (body_len + 3u) & ~((size_t)3u);
    total_len = 4u + padded_len;
    if (capacity < total_len) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    rtc_rtcp_write_header(out, 1, RTC_RTCP_PT_SDES,
                          (uint16_t)(total_len / 4u - 1u));
    rtc_write_u32(out + 4, ssrc);
    out[8] = 1;
    out[9] = (uint8_t)cname_len;
    memcpy(out + 10, cname, cname_len);
    memset(out + 10 + cname_len, 0, padded_len - 6u - cname_len);
    *out_len = total_len;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtcp_write_pli(uint32_t sender_ssrc, uint32_t media_ssrc,
                                uint8_t *out, size_t capacity,
                                size_t *out_len)
{
    if (out == 0 || out_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (capacity < 12u) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    rtc_rtcp_write_header(out, RTC_RTCP_FMT_PLI, RTC_RTCP_PT_PSFB, 2);
    rtc_write_u32(out + 4, sender_ssrc);
    rtc_write_u32(out + 8, media_ssrc);
    *out_len = 12;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtcp_parse_nack(const uint8_t *fci, size_t fci_len,
                                 rtc_media_feedback_t *out_feedback)
{
    size_t offset;
    size_t count;

    if (fci == 0 || out_feedback == 0 || fci_len < 4u ||
        (fci_len & 3u) != 0u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    memset(out_feedback, 0, sizeof(*out_feedback));
    out_feedback->type = RTC_MEDIA_FEEDBACK_NACK;
    out_feedback->retransmit_performed = 0;
    count = 0;
    for (offset = 0; offset < fci_len && count < 17u; offset += 4u) {
        uint16_t pid;
        uint16_t blp;
        uint8_t bit;

        pid = rtc_read_u16(fci + offset);
        blp = rtc_read_u16(fci + offset + 2u);
        if (offset == 0) {
            out_feedback->pid = pid;
            out_feedback->blp = blp;
        }
        out_feedback->lost_sequence_numbers[count++] = pid;
        for (bit = 0; bit < 16u && count < 17u; ++bit) {
            if ((blp & (uint16_t)(1u << bit)) != 0u) {
                out_feedback->lost_sequence_numbers[count++] =
                    (uint16_t)(pid + bit + 1u);
            }
        }
    }
    out_feedback->lost_sequence_number_count = count;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_rtcp_parse_sr(rtc_peer_connection_t *pc,
                                      const uint8_t *packet,
                                      size_t packet_len)
{
    rtc_rtcp_media_stats_t *stats;
    uint32_t ssrc;

    if (packet_len < 28u) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    ssrc = rtc_read_u32(packet + 4);
    stats = rtc_rtcp_find_stats(pc, ssrc);
    if (stats != 0) {
        stats->ssrc = ssrc;
        stats->last_sr_ntp = ((uint64_t)rtc_read_u32(packet + 8) << 32) |
                             rtc_read_u32(packet + 12);
        stats->last_sr_rtp = rtc_read_u32(packet + 16);
        if (pc != 0) {
            pc->counters.rtcp.rtcp_sr_received++;
        }
    }
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_rtcp_parse_rr(rtc_peer_connection_t *pc,
                                      const uint8_t *packet,
                                      size_t packet_len)
{
    rtc_rtcp_media_stats_t *stats;
    uint32_t ssrc;

    if (packet_len < 8u) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    ssrc = rtc_read_u32(packet + 4);
    stats = rtc_rtcp_find_stats(pc, ssrc);
    if (stats != 0) {
        stats->ssrc = ssrc;
        if (pc != 0) {
            pc->counters.rtcp.rtcp_rr_received++;
        }
    }
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_rtcp_parse_sdes(rtc_peer_connection_t *pc,
                                        const uint8_t *packet,
                                        size_t packet_len)
{
    size_t offset = 4;
    uint8_t chunks;
    uint8_t i;

    if (packet_len < 8u) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    chunks = (uint8_t)(packet[0] & 0x1fu);
    for (i = 0; i < chunks; ++i) {
        if (offset + 4u > packet_len) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        offset += 4u;
        while (offset < packet_len && packet[offset] != 0u) {
            uint8_t item_len;
            if (offset + 2u > packet_len) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            item_len = packet[offset + 1u];
            offset += 2u;
            if (offset + item_len > packet_len) {
                return RTC_STATUS_PROTOCOL_ERROR;
            }
            offset += item_len;
        }
        if (offset < packet_len) {
            offset++;
        }
        while ((offset & 3u) != 0u && offset < packet_len) {
            offset++;
        }
    }
    if (pc != 0) {
        pc->counters.rtcp.rtcp_sdes_received++;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_rtcp_parse_psfb(rtc_peer_connection_t *pc,
                                        const uint8_t *packet,
                                        size_t packet_len)
{
    uint8_t fmt;
    uint32_t media_ssrc;

    if (packet_len < 12u) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    fmt = (uint8_t)(packet[0] & 0x1fu);
    if (fmt != RTC_RTCP_FMT_PLI) {
        return RTC_STATUS_UNSUPPORTED;
    }

    media_ssrc = rtc_read_u32(packet + 8);
    rtc_media_emit_pli_feedback(pc, media_ssrc);
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_rtcp_parse_rtpfb(rtc_peer_connection_t *pc,
                                         const uint8_t *packet,
                                         size_t packet_len)
{
    rtc_media_feedback_t feedback;
    uint8_t fmt;
    uint32_t media_ssrc;
    rtc_status_t status;

    if (packet_len < 16u) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    fmt = (uint8_t)(packet[0] & 0x1fu);
    if (fmt != RTC_RTCP_FMT_NACK) {
        return RTC_STATUS_UNSUPPORTED;
    }

    media_ssrc = rtc_read_u32(packet + 8);
    status = rtc_rtcp_parse_nack(packet + 12, packet_len - 12u, &feedback);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    rtc_media_emit_nack_feedback(pc, media_ssrc, &feedback);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_rtcp_parse_compound(rtc_peer_connection_t *pc,
                                     const uint8_t *packet,
                                     size_t packet_len)
{
    size_t offset = 0;

    if (packet == 0 || packet_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    while (offset < packet_len) {
        uint8_t packet_type;
        uint16_t length_words;
        size_t rtcp_len;
        rtc_status_t status;

        if (packet_len - offset < 4u) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        if ((packet[offset] >> 6) != 2u) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        packet_type = packet[offset + 1u];
        length_words = rtc_read_u16(packet + offset + 2u);
        rtcp_len = ((size_t)length_words + 1u) * 4u;
        if (rtcp_len < 4u || offset + rtcp_len > packet_len) {
            return RTC_STATUS_PROTOCOL_ERROR;
        }

        if (packet_type == RTC_RTCP_PT_SR) {
            status = rtc_rtcp_parse_sr(pc, packet + offset, rtcp_len);
        } else if (packet_type == RTC_RTCP_PT_RR) {
            status = rtc_rtcp_parse_rr(pc, packet + offset, rtcp_len);
        } else if (packet_type == RTC_RTCP_PT_SDES) {
            status = rtc_rtcp_parse_sdes(pc, packet + offset, rtcp_len);
        } else if (packet_type == RTC_RTCP_PT_RTPFB) {
            status = rtc_rtcp_parse_rtpfb(pc, packet + offset, rtcp_len);
        } else if (packet_type == RTC_RTCP_PT_PSFB) {
            status = rtc_rtcp_parse_psfb(pc, packet + offset, rtcp_len);
        } else {
            return RTC_STATUS_UNSUPPORTED;
        }
        if (status != RTC_STATUS_OK) {
            return status;
        }
        offset += rtcp_len;
    }

    return RTC_STATUS_OK;
}
