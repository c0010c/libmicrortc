#include "media/media.h"

#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"
#include "rtp/rtp.h"
#include "srtp/srtp.h"

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
        pc->counters.media.frames_sent++;
    } else {
        pc->counters.rtp.packets_dropped++;
    }
    slot->dispatch_status = status;
    rtc_media_release_slot(slot);
}

rtc_status_t rtc_media_send_frame(rtc_peer_connection_t *pc,
                                  const rtc_media_frame_t *frame)
{
    rtc_media_queue_slot_t *slot;
    rtc_rtp_packetizer_state_t state;
    uint16_t sequence;
    uint32_t timestamp;
    rtc_status_t status;

    if (pc == 0 || frame == 0 || frame->data == 0 || frame->data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (frame->kind != RTC_MEDIA_KIND_AUDIO_OPUS) {
        return RTC_STATUS_UNSUPPORTED;
    }
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

    status = pc->executors.network.post(pc->executors.network.user_data,
                                        rtc_media_network_send_task, slot);
    if (status != RTC_STATUS_OK) {
        rtc_media_release_slot(slot);
        rtc_media_trace_packet(pc, frame->kind, sequence, timestamp, status,
                               "network_post_failed");
        return status;
    }
    status = slot->dispatch_status;
    rtc_media_trace_packet(pc, frame->kind, sequence, timestamp, status,
                           status == RTC_STATUS_OK ? 0 : "protect_failed");
    return status;
}
