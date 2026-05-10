#include "media/media.h"

#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"
#include "rtcp/rtcp.h"
#include "rtp/rtp.h"
#include "srtp/srtp.h"

#include <string.h>

static void rtc_media_trace_packet(rtc_peer_connection_t *pc,
                                   rtc_media_kind_t kind, uint16_t sequence,
                                   uint32_t timestamp, rtc_status_t status,
                                   const char *reason)
{
    rtc_trace_field_t fields[7];
    size_t field_count = 6;

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "rtp";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = "send";
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_MEDIA_KIND;
    fields[2].value = kind == RTC_MEDIA_KIND_AUDIO_OPUS ? "opus" : "h264";
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_SEQUENCE;
    fields[3].value = 0;
    fields[3].number = sequence;
    fields[4].key = RTC_TRACE_FIELD_TIMESTAMP;
    fields[4].value = 0;
    fields[4].number = timestamp;
    fields[5].key = RTC_TRACE_FIELD_STATUS;
    fields[5].value = 0;
    fields[5].number = (uint64_t)status;
    if (reason != 0) {
        fields[6].key = RTC_TRACE_FIELD_REASON;
        fields[6].value = reason;
        fields[6].number = 0;
        field_count = 7;
    }

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_RTP_PACKET, fields, field_count);
}

static void rtc_media_trace_feedback(rtc_peer_connection_t *pc,
                                     const char *operation,
                                     rtc_media_kind_t kind,
                                     uint32_t ssrc,
                                     rtc_status_t status,
                                     const char *reason)
{
    rtc_trace_field_t fields[6];
    size_t field_count = 5;

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "rtcp";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_MEDIA_KIND;
    fields[2].value = kind == RTC_MEDIA_KIND_VIDEO_H264 ? "h264" : "opus";
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_SSRC;
    fields[3].value = 0;
    fields[3].number = ssrc;
    fields[4].key = RTC_TRACE_FIELD_STATUS;
    fields[4].value = 0;
    fields[4].number = (uint64_t)status;
    if (reason != 0) {
        fields[5].key = RTC_TRACE_FIELD_REASON;
        fields[5].value = reason;
        fields[5].number = 0;
        field_count = 6;
    }

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_MEDIA_FEEDBACK, fields,
                   field_count);
}

void rtc_media_emit_pli_feedback(rtc_peer_connection_t *pc,
                                 uint32_t media_ssrc)
{
    rtc_media_feedback_t feedback;

    if (pc == 0) {
        return;
    }

    memset(&feedback, 0, sizeof(feedback));
    feedback.type = RTC_MEDIA_FEEDBACK_PLI;
    feedback.kind = RTC_MEDIA_KIND_VIDEO_H264;
    feedback.ssrc = media_ssrc;
    feedback.retransmit_performed = 0;
    if (pc->observer.on_media_feedback != 0) {
        pc->observer.on_media_feedback(pc->observer.user_data, &feedback);
    }
    pc->counters.rtcp.pli_received++;
    rtc_media_trace_feedback(pc, "pli_received", RTC_MEDIA_KIND_VIDEO_H264,
                             media_ssrc, RTC_STATUS_OK, 0);
}

void rtc_media_emit_nack_feedback(rtc_peer_connection_t *pc,
                                  uint32_t media_ssrc,
                                  const rtc_media_feedback_t *feedback)
{
    rtc_media_feedback_t reported;
    rtc_media_kind_t kind;

    if (pc == 0 || feedback == 0) {
        return;
    }

    reported = *feedback;
    kind = media_ssrc == pc->rtcp_audio.ssrc ? RTC_MEDIA_KIND_AUDIO_OPUS
                                             : RTC_MEDIA_KIND_VIDEO_H264;
    reported.type = RTC_MEDIA_FEEDBACK_NACK;
    reported.kind = kind;
    reported.ssrc = media_ssrc;
    reported.retransmit_performed = 0;
    if (pc->observer.on_media_feedback != 0) {
        pc->observer.on_media_feedback(pc->observer.user_data, &reported);
    }
    pc->counters.rtcp.nack_received++;
    pc->counters.rtcp.nack_no_retransmit++;
    rtc_media_trace_feedback(pc, "nack_received", kind, media_ssrc,
                             RTC_STATUS_OK, "nack_no_retransmit");
}

static rtc_media_queue_slot_t *rtc_media_acquire_slot(rtc_peer_connection_t *pc)
{
    size_t i;

    for (i = 0; i < pc->media_queue_slot_count; ++i) {
        if (!pc->media_queue_slots[i].in_use) {
            pc->media_queue_slots[i].in_use = 1;
            pc->media_queue_slots[i].payload_len = 0;
            pc->media_queue_slots[i].dispatch_status = RTC_STATUS_OK;
            return &pc->media_queue_slots[i];
        }
    }
    pc->counters.media.media_queue_full++;
    return 0;
}

static void rtc_media_release_slot(rtc_media_queue_slot_t *slot)
{
    slot->payload_len = 0;
    slot->payload_type = 0;
    slot->sequence = 0;
    slot->timestamp = 0;
    slot->marker = 0;
    slot->in_use = 0;
}

static void rtc_media_network_send_task(void *user_data)
{
    rtc_media_queue_slot_t *slot = (rtc_media_queue_slot_t *)user_data;
    rtc_peer_connection_t *pc = slot->pc;
    size_t packet_len = slot->payload_len;
    rtc_status_t status;

    status = rtc_srtp_protect_rtp(pc, slot->payload, &packet_len,
                                  slot->payload_capacity);
    if (status == RTC_STATUS_OK) {
        slot->payload_len = packet_len;
        if (pc->observer.on_datagram != 0) {
            pc->observer.on_datagram(pc->observer.user_data, slot->payload,
                                     slot->payload_len);
        }
        pc->counters.rtp.packets_sent++;
        if (slot->kind == RTC_MEDIA_KIND_AUDIO_OPUS) {
            pc->rtcp_audio.packets_sent++;
            if (slot->payload_len >= RTC_RTP_HEADER_BYTES) {
                pc->rtcp_audio.octets_sent +=
                    slot->payload_len - RTC_RTP_HEADER_BYTES;
            }
        } else if (slot->kind == RTC_MEDIA_KIND_VIDEO_H264) {
            pc->rtcp_video.packets_sent++;
            if (slot->payload_len >= RTC_RTP_HEADER_BYTES) {
                pc->rtcp_video.octets_sent +=
                    slot->payload_len - RTC_RTP_HEADER_BYTES;
            }
        }
    } else {
        pc->counters.rtp.packets_dropped++;
    }
    slot->dispatch_status = status;
    rtc_media_release_slot(slot);
}

static rtc_status_t rtc_media_dispatch_slot(rtc_peer_connection_t *pc,
                                            rtc_media_queue_slot_t *slot,
                                            rtc_media_kind_t kind,
                                            uint16_t sequence,
                                            uint32_t timestamp)
{
    rtc_status_t status;

    status = pc->executors.network.post(pc->executors.network.user_data,
                                        rtc_media_network_send_task, slot);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        rtc_media_trace_packet(pc, kind, sequence, timestamp, status,
                               "network_post_failed");
        return status;
    }
    status = slot->dispatch_status;
    rtc_media_trace_packet(pc, kind, sequence, timestamp, status,
                           status == RTC_STATUS_OK ? 0 : "protect_failed");
    return status;
}

static void rtc_media_emit_typed_frame(rtc_peer_connection_t *pc,
                                       rtc_media_queue_slot_t *slot)
{
    rtc_media_frame_t frame;

    memset(&frame, 0, sizeof(frame));
    frame.kind = slot->kind;
    frame.data = slot->payload;
    frame.data_len = slot->payload_len;
    frame.timestamp = slot->timestamp;
    if (pc->observer.on_media_frame_typed != 0) {
        pc->observer.on_media_frame_typed(pc->observer.user_data, &frame);
    }
    if (pc->observer.on_media_frame != 0) {
        pc->observer.on_media_frame(pc->observer.user_data, slot->payload,
                                    slot->payload_len);
    }
    pc->counters.media.frames_received++;
}

static void rtc_media_emit_typed_frame_data(rtc_peer_connection_t *pc,
                                            rtc_media_kind_t kind,
                                            const uint8_t *data,
                                            size_t data_len,
                                            uint32_t timestamp)
{
    rtc_media_frame_t frame;

    memset(&frame, 0, sizeof(frame));
    frame.kind = kind;
    frame.data = data;
    frame.data_len = data_len;
    frame.timestamp = timestamp;
    if (pc->observer.on_media_frame_typed != 0) {
        pc->observer.on_media_frame_typed(pc->observer.user_data, &frame);
    }
    if (pc->observer.on_media_frame != 0) {
        pc->observer.on_media_frame(pc->observer.user_data, data, data_len);
    }
    pc->counters.media.frames_received++;
}

static void rtc_media_trace_h264_drop(rtc_peer_connection_t *pc,
                                      const char *reason,
                                      rtc_status_t status)
{
    rtc_trace_field_t fields[5];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "rtp";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = "h264_reassembly";
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_MEDIA_KIND;
    fields[2].value = "h264";
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_REASON;
    fields[3].value = reason;
    fields[3].number = 0;
    fields[4].key = RTC_TRACE_FIELD_STATUS;
    fields[4].value = 0;
    fields[4].number = (uint64_t)status;
    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_RTP_PACKET, fields, 5);
}

static void rtc_media_receive_task(void *user_data)
{
    rtc_media_queue_slot_t *slot = (rtc_media_queue_slot_t *)user_data;
    rtc_peer_connection_t *pc = slot->pc;
    int frame_ready = 0;
    const char *drop_reason = 0;
    rtc_status_t status;

    if (slot->payload_type == RTC_RTP_PAYLOAD_TYPE_OPUS) {
        slot->kind = RTC_MEDIA_KIND_AUDIO_OPUS;
        rtc_media_emit_typed_frame(pc, slot);
    } else if (slot->payload_type == RTC_RTP_PAYLOAD_TYPE_H264) {
        slot->kind = RTC_MEDIA_KIND_VIDEO_H264;
        status = rtc_rtp_depacketize_h264(
            slot->payload, slot->payload_len, slot->sequence, slot->timestamp,
            slot->marker, pc->h264_reassembly_buffer,
            pc->media_max_reassembly_bytes, &pc->h264_reassembly_len,
            &pc->h264_reassembly_expected_sequence,
            &pc->h264_reassembly_active, &pc->h264_reassembly_timestamp,
            &frame_ready, &drop_reason);
        if (drop_reason != 0) {
            pc->counters.media.h264_reassembly_drops++;
            rtc_media_trace_h264_drop(pc, drop_reason, status);
        }
        if (status != RTC_STATUS_OK) {
            pc->counters.rtp.packets_dropped++;
            slot->dispatch_status = status;
        } else if (frame_ready) {
            rtc_media_emit_typed_frame_data(
                pc, RTC_MEDIA_KIND_VIDEO_H264, pc->h264_reassembly_buffer,
                pc->h264_reassembly_len, pc->h264_reassembly_timestamp);
            pc->h264_reassembly_len = 0;
            pc->h264_reassembly_active = 0;
            slot->dispatch_status = RTC_STATUS_OK;
        } else {
            slot->dispatch_status = RTC_STATUS_OK;
        }
    } else {
        pc->counters.rtp.packets_dropped++;
        slot->dispatch_status = RTC_STATUS_UNSUPPORTED;
    }
    rtc_media_release_slot(slot);
}

static rtc_status_t rtc_media_dispatch_received_slot(
    rtc_peer_connection_t *pc, rtc_media_queue_slot_t *slot)
{
    rtc_status_t status;

    status = pc->executors.media.post(pc->executors.media.user_data,
                                      rtc_media_receive_task, slot);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        return status;
    }
    return slot->dispatch_status;
}

static void rtc_media_rtcp_receive_task(void *user_data)
{
    rtc_media_queue_slot_t *slot = (rtc_media_queue_slot_t *)user_data;
    rtc_peer_connection_t *pc = slot->pc;

    slot->dispatch_status =
        rtc_rtcp_parse_compound(pc, slot->payload, slot->payload_len);
    rtc_media_release_slot(slot);
}

static rtc_status_t rtc_media_dispatch_rtcp_slot(rtc_peer_connection_t *pc,
                                                 rtc_media_queue_slot_t *slot)
{
    rtc_status_t status;

    status = pc->executors.media.post(pc->executors.media.user_data,
                                      rtc_media_rtcp_receive_task, slot);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        return status;
    }
    return slot->dispatch_status;
}

rtc_status_t rtc_media_handle_rtp_datagram(rtc_peer_connection_t *pc,
                                           const uint8_t *data,
                                           size_t data_len)
{
    rtc_media_queue_slot_t *slot;
    rtc_rtp_header_t header;
    rtc_status_t status;
    size_t packet_len;

    if (pc == 0 || data == 0 || data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (data_len > pc->media_packet_capacity) {
        pc->counters.rtp.packets_dropped++;
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    slot = rtc_media_acquire_slot(pc);
    if (slot == 0) {
        return RTC_STATUS_CAPACITY;
    }
    slot->kind = RTC_MEDIA_KIND_AUDIO_OPUS;
    memcpy(slot->payload, data, data_len);
    slot->payload_len = data_len;
    packet_len = slot->payload_len;

    status = rtc_srtp_unprotect_rtp(pc, slot->payload, &packet_len);
    if (status != RTC_STATUS_OK) {
        pc->counters.rtp.packets_dropped++;
        rtc_media_release_slot(slot);
        return status;
    }

    status = rtc_rtp_parse_header(slot->payload, packet_len, &header);
    if (status != RTC_STATUS_OK) {
        pc->counters.rtp.packets_dropped++;
        rtc_media_release_slot(slot);
        return status;
    }

    memmove(slot->payload, slot->payload + header.header_len,
            packet_len - header.header_len);
    slot->payload_len = packet_len - header.header_len;
    slot->payload_type = header.payload_type;
    slot->sequence = header.sequence;
    slot->timestamp = header.timestamp;
    slot->marker = header.marker;
    slot->dispatch_status = RTC_STATUS_OK;
    pc->counters.rtp.packets_received++;
    if (header.payload_type == RTC_RTP_PAYLOAD_TYPE_OPUS) {
        pc->rtcp_audio.ssrc = header.ssrc;
        pc->rtcp_audio.packets_received++;
        pc->rtcp_audio.last_sequence = header.sequence;
    } else if (header.payload_type == RTC_RTP_PAYLOAD_TYPE_H264) {
        pc->rtcp_video.ssrc = header.ssrc;
        pc->rtcp_video.packets_received++;
        pc->rtcp_video.last_sequence = header.sequence;
    }

    return rtc_media_dispatch_received_slot(pc, slot);
}

rtc_status_t rtc_media_handle_rtcp_datagram(rtc_peer_connection_t *pc,
                                            const uint8_t *data,
                                            size_t data_len)
{
    rtc_media_queue_slot_t *slot;
    rtc_status_t status;
    size_t packet_len;

    if (pc == 0 || data == 0 || data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (data_len > pc->media_packet_capacity) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    slot = rtc_media_acquire_slot(pc);
    if (slot == 0) {
        return RTC_STATUS_CAPACITY;
    }
    memcpy(slot->payload, data, data_len);
    slot->payload_len = data_len;
    packet_len = slot->payload_len;

    status = rtc_srtp_unprotect_rtcp(pc, slot->payload, &packet_len);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        return status;
    }

    slot->payload_len = packet_len;
    return rtc_media_dispatch_rtcp_slot(pc, slot);
}

rtc_status_t rtc_media_send_rtcp_reports(rtc_peer_connection_t *pc)
{
    rtc_media_queue_slot_t *slot;
    size_t offset = 0;
    size_t written = 0;
    size_t packet_len;
    rtc_status_t status;

    if (pc == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    slot = rtc_media_acquire_slot(pc);
    if (slot == 0) {
        return RTC_STATUS_CAPACITY;
    }

    status = rtc_rtcp_write_sender_report(&pc->rtcp_audio, slot->payload,
                                          slot->payload_capacity, &written);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        return status;
    }
    offset += written;

    status = rtc_rtcp_write_sdes(
        pc->rtcp_audio.ssrc, "rtc-cname", 9,
        pc->limits.rtcp.max_sdes_cname_bytes, slot->payload + offset,
        slot->payload_capacity - offset, &written);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        return status;
    }
    offset += written;
    packet_len = offset;

    status = rtc_srtp_protect_rtcp(pc, slot->payload, &packet_len,
                                   slot->payload_capacity);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        return status;
    }

    slot->payload_len = packet_len;
    if (pc->observer.on_datagram != 0) {
        pc->observer.on_datagram(pc->observer.user_data, slot->payload,
                                 slot->payload_len);
    }
    pc->counters.rtcp.rtcp_sr_sent++;
    pc->counters.rtcp.rtcp_sdes_sent++;
    rtc_media_release_slot(slot);
    return RTC_STATUS_OK;
}

static void rtc_media_network_send_pli_task(void *user_data)
{
    rtc_media_queue_slot_t *slot = (rtc_media_queue_slot_t *)user_data;
    rtc_peer_connection_t *pc = slot->pc;
    size_t packet_len = slot->payload_len;
    rtc_status_t status;

    status = rtc_srtp_protect_rtcp(pc, slot->payload, &packet_len,
                                   slot->payload_capacity);
    if (status == RTC_STATUS_OK) {
        slot->payload_len = packet_len;
        if (pc->observer.on_datagram != 0) {
            pc->observer.on_datagram(pc->observer.user_data, slot->payload,
                                     slot->payload_len);
        }
        pc->counters.rtcp.pli_sent++;
    }
    slot->dispatch_status = status;
    rtc_media_release_slot(slot);
}

rtc_status_t rtc_media_request_keyframe(rtc_peer_connection_t *pc,
                                        rtc_media_kind_t kind)
{
    rtc_media_queue_slot_t *slot;
    rtc_status_t status;

    if (pc == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (kind != RTC_MEDIA_KIND_VIDEO_H264) {
        return RTC_STATUS_UNSUPPORTED;
    }

    slot = rtc_media_acquire_slot(pc);
    if (slot == 0) {
        return RTC_STATUS_CAPACITY;
    }
    slot->kind = RTC_MEDIA_KIND_VIDEO_H264;
    status = rtc_rtcp_write_pli(pc->rtcp_audio.ssrc, pc->rtcp_video.ssrc,
                                slot->payload, slot->payload_capacity,
                                &slot->payload_len);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        rtc_media_trace_feedback(pc, "pli_send", RTC_MEDIA_KIND_VIDEO_H264,
                                 pc->rtcp_video.ssrc, status,
                                 "pli_write_failed");
        return status;
    }

    status = pc->executors.network.post(pc->executors.network.user_data,
                                        rtc_media_network_send_pli_task,
                                        slot);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        rtc_media_trace_feedback(pc, "pli_send", RTC_MEDIA_KIND_VIDEO_H264,
                                 pc->rtcp_video.ssrc, status,
                                 "network_post_failed");
        return status;
    }

    status = slot->dispatch_status;
    rtc_media_trace_feedback(pc, "pli_send", RTC_MEDIA_KIND_VIDEO_H264,
                             pc->rtcp_video.ssrc, status,
                             status == RTC_STATUS_OK ? 0 : "protect_failed");
    return status;
}

static rtc_status_t rtc_media_send_opus(rtc_peer_connection_t *pc,
                                        const rtc_media_frame_t *frame)
{
    rtc_media_queue_slot_t *slot;
    rtc_rtp_packetizer_state_t state;
    uint16_t sequence;
    uint32_t timestamp;
    rtc_status_t status;

    if (frame->data_len > pc->media_max_payload_bytes) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    slot = rtc_media_acquire_slot(pc);
    if (slot == 0) {
        return RTC_STATUS_CAPACITY;
    }
    slot->kind = frame->kind;
    state.sequence = pc->audio_rtp_sequence;
    state.timestamp = pc->audio_rtp_timestamp;
    state.ssrc = pc->audio_rtp_ssrc;
    sequence = state.sequence;
    timestamp = frame->timestamp != 0 ? frame->timestamp : state.timestamp;

    status = rtc_rtp_packetize_opus(frame, &state, pc->media_max_payload_bytes,
                                    slot->payload, slot->payload_capacity,
                                    &slot->payload_len);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        rtc_media_trace_packet(pc, frame->kind, sequence, timestamp, status,
                               "opus_packetize_failed");
        return status;
    }
    pc->audio_rtp_sequence = state.sequence;
    pc->audio_rtp_timestamp = state.timestamp;

    status = rtc_media_dispatch_slot(pc, slot, frame->kind, sequence,
                                     timestamp);
    if (status == RTC_STATUS_OK) {
        pc->counters.media.frames_sent++;
    }
    return status;
}

static void rtc_media_release_slots(rtc_media_queue_slot_t **slots,
                                    size_t slot_count)
{
    size_t i;

    for (i = 0; i < slot_count; ++i) {
        if (slots[i] != 0 && slots[i]->in_use) {
            rtc_media_release_slot(slots[i]);
        }
    }
}

static rtc_status_t rtc_media_send_h264(rtc_peer_connection_t *pc,
                                        const rtc_media_frame_t *frame)
{
    rtc_media_queue_slot_t *slots[32];
    rtc_rtp_packet_buffer_t packets[32];
    rtc_rtp_packetizer_state_t state;
    size_t slot_count;
    size_t packet_count;
    size_t i;
    rtc_status_t status;

    if (pc->media_max_packets_per_frame > 32u) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }
    slot_count = pc->media_max_packets_per_frame;
    if (slot_count > pc->media_queue_slot_count) {
        slot_count = pc->media_queue_slot_count;
    }
    if (slot_count == 0) {
        return RTC_STATUS_CAPACITY_PACKET_CACHE;
    }

    for (i = 0; i < slot_count; ++i) {
        slots[i] = rtc_media_acquire_slot(pc);
        if (slots[i] == 0) {
            rtc_media_release_slots(slots, i);
            return RTC_STATUS_CAPACITY;
        }
        slots[i]->kind = frame->kind;
        packets[i].data = slots[i]->payload;
        packets[i].capacity = slots[i]->payload_capacity;
        packets[i].len = 0;
        packets[i].sequence = 0;
        packets[i].timestamp = 0;
        packets[i].marker = 0;
    }

    packet_count = slot_count;
    state.sequence = pc->video_rtp_sequence;
    state.timestamp = pc->video_rtp_timestamp;
    state.ssrc = pc->video_rtp_ssrc;
    status = rtc_rtp_packetize_h264(frame, &state, pc->media_max_payload_bytes,
                                    pc->media_max_packets_per_frame, packets,
                                    &packet_count);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slots(slots, slot_count);
        rtc_media_trace_packet(pc, frame->kind, pc->video_rtp_sequence,
                               frame->timestamp, status,
                               "h264_packetize_failed");
        return status;
    }

    pc->video_rtp_sequence = state.sequence;
    pc->video_rtp_timestamp = state.timestamp;
    rtc_media_release_slots(slots + packet_count, slot_count - packet_count);
    for (i = 0; i < packet_count; ++i) {
        slots[i]->payload_len = packets[i].len;
        status = rtc_media_dispatch_slot(pc, slots[i], frame->kind,
                                         packets[i].sequence,
                                         packets[i].timestamp);
        if (status != RTC_STATUS_OK) {
            rtc_media_release_slots(slots + i + 1, packet_count - i - 1);
            return status;
        }
    }
    pc->counters.media.frames_sent++;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_media_send_frame(rtc_peer_connection_t *pc,
                                  const rtc_media_frame_t *frame)
{
    if (pc == 0 || frame == 0 || frame->data == 0 || frame->data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (frame->kind == RTC_MEDIA_KIND_AUDIO_OPUS) {
        return rtc_media_send_opus(pc, frame);
    }
    if (frame->kind == RTC_MEDIA_KIND_VIDEO_H264) {
        return rtc_media_send_h264(pc, frame);
    }
    return RTC_STATUS_UNSUPPORTED;
}
