#include "api/peer_connection.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "executor/executor.h"
#include "ice/ice.h"
#include "memory/allocator.h"
#include "net/demux.h"
#include "observability/counters.h"
#include "observability/observer.h"
#include "observability/trace.h"
#include "media/media.h"
#include "rtp/rtp.h"
#include "security/security.h"

static void rtc_pc_trace(rtc_peer_connection_t *pc, const char *event,
                         const char *operation, rtc_status_t status)
{
    rtc_trace_field_t fields[3];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "peer_connection";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_STATUS;
    fields[2].value = 0;
    fields[2].number = (uint64_t)status;

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, event, fields, 3);
}

static void rtc_pc_trace_jsep(rtc_peer_connection_t *pc, const char *event,
                              const char *operation, rtc_status_t status,
                              rtc_jsep_state_t target_state,
                              rtc_sdp_type_t type)
{
    rtc_trace_field_t fields[5];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "jsep";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = operation;
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_STATUS;
    fields[2].value = 0;
    fields[2].number = (uint64_t)status;
    fields[3].key = RTC_TRACE_FIELD_TARGET_STATE;
    fields[3].value = rtc_jsep_state_name(target_state);
    fields[3].number = 0;
    fields[4].key = RTC_TRACE_FIELD_DESCRIPTION_TYPE;
    fields[4].value = type == RTC_SDP_TYPE_OFFER ? "offer" : "answer";
    fields[4].number = 0;

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, event, fields, 5);
}

static const char *rtc_ice_candidate_type_name(rtc_ice_candidate_type_t type)
{
    return type == RTC_ICE_CANDIDATE_TYPE_SRFLX ? "srflx" : "host";
}

static void rtc_pc_trace_remote_candidate(
    rtc_peer_connection_t *pc, const rtc_ice_candidate_summary_t *candidate,
    size_t candidate_id)
{
    rtc_trace_field_t fields[7];

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "ice";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = "add_ice_candidate";
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_CANDIDATE_TYPE;
    fields[2].value = rtc_ice_candidate_type_name(candidate->type);
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_REMOTE_ADDRESS;
    fields[3].value = candidate->address;
    fields[3].number = 0;
    fields[4].key = RTC_TRACE_FIELD_PORT;
    fields[4].value = 0;
    fields[4].number = candidate->port;
    fields[5].key = RTC_TRACE_FIELD_CANDIDATE_ID;
    fields[5].value = 0;
    fields[5].number = candidate_id;
    fields[6].key = RTC_TRACE_FIELD_STATUS;
    fields[6].value = 0;
    fields[6].number = RTC_STATUS_OK;

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_ICE_CANDIDATE_REMOTE, fields, 7);
}

static rtc_status_t rtc_pc_affinity_violation(rtc_peer_connection_t *pc,
                                              const char *operation)
{
    rtc_counters_note_affinity_error(&pc->counters);
    rtc_observer_emit_error(&pc->observer, RTC_STATUS_AFFINITY_VIOLATION,
                            "executor", operation, 0);
    rtc_pc_trace(pc, RTC_TRACE_AFFINITY_VIOLATION, operation,
                 RTC_STATUS_AFFINITY_VIOLATION);
    return RTC_STATUS_AFFINITY_VIOLATION;
}

static rtc_status_t rtc_pc_unsupported(rtc_peer_connection_t *pc,
                                       const char *operation)
{
    rtc_counters_note_unsupported_api(&pc->counters);
    rtc_observer_emit_error(&pc->observer, RTC_STATUS_UNSUPPORTED, "api",
                            operation, 0);
    rtc_pc_trace(pc, RTC_TRACE_UNSUPPORTED_API, operation,
                 RTC_STATUS_UNSUPPORTED);
    return RTC_STATUS_UNSUPPORTED;
}

static int rtc_executor_vtable_valid(const rtc_executor_vtable_t *executor)
{
    return executor != 0 && executor->post != 0 &&
           executor->schedule_timer != 0 && executor->cancel_timer != 0;
}

static int rtc_stun_ip_char_valid(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F') ||
           (ch >= 'a' && ch <= 'f') || ch == ':' || ch == '.';
}

static rtc_status_t rtc_validate_stun_config(
    const rtc_peer_connection_config_t *config)
{
    size_t i;

    if (config->stun_server_count > 1u) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (config->stun_server_count == 0u) {
        return RTC_STATUS_OK;
    }
    if (config->stun_server.ip == 0 || config->stun_server.ip_len == 0 ||
        config->stun_server.port == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < config->stun_server.ip_len; ++i) {
        if (!rtc_stun_ip_char_valid(config->stun_server.ip[i])) {
            return RTC_STATUS_INVALID_ARGUMENT;
        }
    }

    return RTC_STATUS_OK;
}

static rtc_status_t rtc_validate_local_host_config(
    const rtc_peer_connection_config_t *config)
{
    size_t i;

    if (config->local_host_ip == 0 || config->local_host_ip_len == 0 ||
        config->local_host_port == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0; i < config->local_host_ip_len; ++i) {
        if (!rtc_stun_ip_char_valid(config->local_host_ip[i])) {
            return RTC_STATUS_INVALID_ARGUMENT;
        }
    }

    return RTC_STATUS_OK;
}

static rtc_status_t rtc_validate_config(const rtc_peer_connection_config_t *config)
{
    rtc_status_t status;

    if (config == 0 || config->arena.data == 0 || config->arena.size == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    if (!rtc_executor_vtable_valid(&config->executors.signaling) ||
        !rtc_executor_vtable_valid(&config->executors.media) ||
        !rtc_executor_vtable_valid(&config->executors.network)) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    if (config->limits.sdp.max_description_bytes == 0 ||
        config->limits.ice.max_candidates == 0 ||
        config->limits.ice.max_candidate_pairs == 0 ||
        config->limits.ice.max_transactions == 0 ||
        config->limits.rtp.max_packet_cache == 0 ||
        config->limits.rtp.max_payload_bytes == 0 ||
        config->limits.rtp.max_packets_per_frame == 0 ||
        config->limits.rtp.max_reassembly_bytes == 0 ||
        config->limits.rtp.max_media_queue_slots == 0 ||
        config->limits.rtcp.max_reports == 0 ||
        config->limits.rtcp.max_feedback_packets == 0 ||
        config->limits.rtcp.max_sdes_cname_bytes == 0 ||
        config->sdp.ice_ufrag == 0 || config->sdp.ice_ufrag_len == 0 ||
        config->sdp.ice_pwd == 0 || config->sdp.ice_pwd_len == 0 ||
        config->sdp.dtls_fingerprint == 0 ||
        config->sdp.dtls_fingerprint_len == 0 ||
        config->sdp.dtls_setup == 0 || config->sdp.dtls_setup_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    status = rtc_validate_local_host_config(config);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    return rtc_validate_stun_config(config);
}

static rtc_status_t rtc_pc_alloc_sdp_buffer(rtc_arena_view_t *arena,
                                            size_t size, char **out_buffer,
                                            rtc_capacity_diagnostics_t *diag)
{
    *out_buffer = (char *)rtc_core_alloc(arena, size, sizeof(char),
                                         RTC_CAPACITY_RESOURCE_SDP_BUFFER,
                                         diag);
    return *out_buffer != 0 ? RTC_STATUS_OK : RTC_STATUS_CAPACITY_SDP_BUFFER;
}

static rtc_status_t rtc_pc_alloc_candidates(rtc_arena_view_t *arena,
                                            size_t max_candidates,
                                            char **out_candidates,
                                            rtc_capacity_diagnostics_t *diag)
{
    size_t bytes;

    bytes = max_candidates * RTC_PC_REMOTE_CANDIDATE_SLOT_BYTES;
    *out_candidates = (char *)rtc_core_alloc(
        arena, bytes, sizeof(char), RTC_CAPACITY_RESOURCE_ICE_CANDIDATES, diag);
    return *out_candidates != 0 ? RTC_STATUS_OK
                                : RTC_STATUS_CAPACITY_ICE_CANDIDATES;
}

static rtc_status_t rtc_pc_alloc_candidate_summaries(
    rtc_arena_view_t *arena, size_t max_candidates,
    rtc_ice_candidate_summary_t **out_candidates,
    rtc_capacity_diagnostics_t *diag)
{
    size_t bytes;

    bytes = max_candidates * sizeof(**out_candidates);
    *out_candidates = (rtc_ice_candidate_summary_t *)rtc_core_alloc(
        arena, bytes, sizeof(void *), RTC_CAPACITY_RESOURCE_ICE_CANDIDATES,
        diag);
    return *out_candidates != 0 ? RTC_STATUS_OK
                                : RTC_STATUS_CAPACITY_ICE_CANDIDATES;
}

static rtc_status_t rtc_pc_alloc_candidate_pairs(
    rtc_arena_view_t *arena, size_t max_pairs,
    rtc_ice_candidate_pair_t **out_pairs, rtc_capacity_diagnostics_t *diag)
{
    size_t bytes;

    bytes = max_pairs * sizeof(**out_pairs);
    *out_pairs = (rtc_ice_candidate_pair_t *)rtc_core_alloc(
        arena, bytes, sizeof(void *), RTC_CAPACITY_RESOURCE_ICE_PAIRS, diag);
    return *out_pairs != 0 ? RTC_STATUS_OK : RTC_STATUS_CAPACITY_ICE_PAIRS;
}

static rtc_status_t rtc_pc_alloc_stun_transactions(
    rtc_arena_view_t *arena, size_t max_transactions,
    rtc_stun_transaction_t **out_transactions,
    rtc_capacity_diagnostics_t *diag)
{
    size_t bytes;

    bytes = max_transactions * sizeof(**out_transactions);
    *out_transactions = (rtc_stun_transaction_t *)rtc_core_alloc(
        arena, bytes, sizeof(void *),
        RTC_CAPACITY_RESOURCE_STUN_TRANSACTIONS, diag);
    return *out_transactions != 0 ? RTC_STATUS_OK
                                  : RTC_STATUS_CAPACITY_STUN_TRANSACTIONS;
}

static rtc_status_t rtc_pc_alloc_media_queue_slots(
    rtc_arena_view_t *arena, size_t max_slots,
    rtc_media_queue_slot_t **out_slots, rtc_capacity_diagnostics_t *diag)
{
    size_t bytes;

    bytes = max_slots * sizeof(**out_slots);
    *out_slots = (rtc_media_queue_slot_t *)rtc_core_alloc(
        arena, bytes, sizeof(void *), RTC_CAPACITY_RESOURCE_MEDIA_FRAME, diag);
    return *out_slots != 0 ? RTC_STATUS_OK : RTC_STATUS_CAPACITY;
}

static rtc_status_t rtc_pc_alloc_media_packet_cache(
    rtc_arena_view_t *arena, size_t max_slots, size_t max_payload_bytes,
    uint8_t **out_cache, rtc_capacity_diagnostics_t *diag)
{
    size_t bytes;

    bytes = max_slots * max_payload_bytes;
    *out_cache = (uint8_t *)rtc_core_alloc(
        arena, bytes, sizeof(void *), RTC_CAPACITY_RESOURCE_PACKET_CACHE, diag);
    return *out_cache != 0 ? RTC_STATUS_OK : RTC_STATUS_CAPACITY_PACKET_CACHE;
}

static rtc_status_t rtc_pc_alloc_h264_reassembly_buffer(
    rtc_arena_view_t *arena, size_t max_reassembly_bytes,
    uint8_t **out_buffer, rtc_capacity_diagnostics_t *diag)
{
    *out_buffer = (uint8_t *)rtc_core_alloc(
        arena, max_reassembly_bytes, sizeof(void *),
        RTC_CAPACITY_RESOURCE_RTP_REASSEMBLY, diag);
    return *out_buffer != 0 ? RTC_STATUS_OK : RTC_STATUS_CAPACITY;
}

static rtc_status_t rtc_pc_alloc_security_session_storage(
    rtc_peer_connection_t *pc, const rtc_security_backend_config_t *backend,
    rtc_capacity_diagnostics_t *diag)
{
    size_t storage_bytes;

    pc->security_session_storage = 0;
    pc->security_session_storage_bytes = 0;
    if (backend == 0) {
        return RTC_STATUS_OK;
    }

    storage_bytes = pc->limits.dtls.max_session_storage_bytes;
    if (backend->session_storage_bytes > storage_bytes || storage_bytes == 0) {
        if (diag != 0) {
            diag->resource = RTC_CAPACITY_RESOURCE_DTLS_SESSION;
            diag->required = backend->session_storage_bytes;
            diag->used = storage_bytes;
        }
        return RTC_STATUS_CAPACITY;
    }

    pc->security_session_storage = rtc_core_alloc(
        &pc->arena, storage_bytes, sizeof(void *),
        RTC_CAPACITY_RESOURCE_DTLS_SESSION, diag);
    if (pc->security_session_storage == 0) {
        return RTC_STATUS_CAPACITY;
    }
    pc->security_session_storage_bytes = storage_bytes;
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_require_pc(rtc_peer_connection_t *pc)
{
    if (pc == 0 || pc->is_closed) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    return RTC_STATUS_OK;
}

static rtc_status_t rtc_unsupported_signaling(rtc_peer_connection_t *pc,
                                              const char *operation)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, operation);
    }

    return rtc_pc_unsupported(pc, operation);
}

static void rtc_pc_trace_demux(rtc_peer_connection_t *pc,
                               rtc_net_protocol_t protocol,
                               rtc_status_t status, const char *reason)
{
    rtc_trace_field_t fields[5];
    size_t field_count = 4;

    fields[0].key = RTC_TRACE_FIELD_SUBSYSTEM;
    fields[0].value = "net";
    fields[0].number = 0;
    fields[1].key = RTC_TRACE_FIELD_OPERATION;
    fields[1].value = "receive_datagram";
    fields[1].number = 0;
    fields[2].key = RTC_TRACE_FIELD_PROTOCOL;
    fields[2].value = rtc_net_protocol_name(protocol);
    fields[2].number = 0;
    fields[3].key = RTC_TRACE_FIELD_STATUS;
    fields[3].value = 0;
    fields[3].number = (uint64_t)status;
    if (reason != 0) {
        fields[4].key = RTC_TRACE_FIELD_REASON;
        fields[4].value = reason;
        fields[4].number = 0;
        field_count = 5;
    }

    rtc_counters_note_trace(&pc->counters);
    rtc_trace_emit(&pc->observer, RTC_TRACE_NET_DEMUX, fields, field_count);
}

static void rtc_pc_note_demux_counter(rtc_peer_connection_t *pc,
                                      rtc_net_protocol_t protocol)
{
    switch (protocol) {
    case RTC_NET_PROTOCOL_STUN:
        pc->counters.net.demux_stun++;
        break;
    case RTC_NET_PROTOCOL_DTLS:
        pc->counters.net.demux_dtls++;
        break;
    case RTC_NET_PROTOCOL_RTP:
        pc->counters.net.demux_rtp++;
        break;
    case RTC_NET_PROTOCOL_RTCP:
        pc->counters.net.demux_rtcp++;
        break;
    case RTC_NET_PROTOCOL_UNKNOWN:
    default:
        pc->counters.net.demux_unknown++;
        break;
    }
}

static int rtc_pc_datagram_looks_like_stun(const uint8_t *data,
                                           size_t data_len)
{
    return data != 0 && data_len >= 20u && (data[0] & 0xC0u) == 0;
}

static int rtc_media_kind_supported(rtc_media_kind_t kind)
{
    return kind == RTC_MEDIA_KIND_AUDIO_OPUS ||
           kind == RTC_MEDIA_KIND_VIDEO_H264;
}

rtc_status_t rtc_peer_connection_create(const rtc_peer_connection_config_t *config,
                                        rtc_capacity_diagnostics_t *diag,
                                        rtc_peer_connection_t **out_pc)
{
    rtc_arena_view_t arena;
    rtc_peer_connection_t *pc;
    rtc_status_t status;

    if (out_pc == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    *out_pc = 0;

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_validate_config(config);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    rtc_arena_init(&arena, config->arena);
    pc = (rtc_peer_connection_t *)rtc_core_alloc(
        &arena, sizeof(*pc), sizeof(void *), RTC_CAPACITY_RESOURCE_ARENA, diag);
    if (pc == 0) {
        return RTC_STATUS_CAPACITY_ARENA;
    }

    pc->arena = arena;
    pc->limits = config->limits;
    pc->sdp = config->sdp;
    pc->local_host_ip = config->local_host_ip;
    pc->local_host_ip_len = config->local_host_ip_len;
    pc->local_host_port = config->local_host_port;
    pc->stun_server = config->stun_server;
    pc->stun_server_count = config->stun_server_count;
    pc->executors = config->executors;
    pc->observer = config->observer;
    rtc_counters_init(&pc->counters);
    pc->counters.api.create_calls = 1;
    pc->signaling_state = RTC_JSEP_STABLE;
    pc->local_description_len = 0;
    pc->remote_description_len = 0;
    pc->remote_candidate_slot_bytes = RTC_PC_REMOTE_CANDIDATE_SLOT_BYTES;
    pc->local_candidate_count = 0;
    pc->remote_candidate_count = 0;
    pc->candidate_pair_count = 0;
    pc->stun_transaction_count = 0;
    pc->ice_state = RTC_ICE_NEW;
    pc->ice_role = RTC_ICE_ROLE_UNKNOWN;
    pc->stun_transaction_nonce = 1;
    pc->srflx_candidate_gathered = 0;
    pc->connectivity_checks_started = 0;
    pc->remote_candidate_pending_pairs = 0;
    pc->media_queue_slots = 0;
    pc->media_packet_cache = 0;
    pc->h264_reassembly_buffer = 0;
    pc->media_queue_slot_count = config->limits.rtp.max_media_queue_slots;
    pc->media_max_payload_bytes = config->limits.rtp.max_payload_bytes;
    pc->media_packet_capacity = RTC_RTP_HEADER_BYTES +
                                config->limits.rtp.max_payload_bytes +
                                RTC_RTP_SRTP_MAX_TRAILER_BYTES;
    pc->media_max_packets_per_frame = config->limits.rtp.max_packets_per_frame;
    pc->media_max_reassembly_bytes = config->limits.rtp.max_reassembly_bytes;
    pc->audio_rtp_sequence = 1;
    pc->audio_rtp_timestamp = 0;
    pc->audio_rtp_ssrc = 0x11111111u;
    pc->video_rtp_sequence = 1;
    pc->video_rtp_timestamp = 0;
    pc->video_rtp_ssrc = 0x22222222u;
    memset(&pc->rtcp_audio, 0, sizeof(pc->rtcp_audio));
    memset(&pc->rtcp_video, 0, sizeof(pc->rtcp_video));
    pc->rtcp_audio.ssrc = pc->audio_rtp_ssrc;
    pc->rtcp_video.ssrc = pc->video_rtp_ssrc;
    pc->h264_reassembly_len = 0;
    pc->h264_reassembly_expected_sequence = 0;
    pc->h264_reassembly_timestamp = 0;
    pc->h264_reassembly_active = 0;
    pc->security_backend = 0;
    pc->security_session = 0;
    pc->security_session_storage = 0;
    pc->security_session_storage_bytes = 0;
    pc->local_dtls_fingerprint[0] = '\0';
    pc->dtls_role = RTC_SECURITY_DTLS_ROLE_CLIENT;
    pc->dtls_state = RTC_SECURITY_DTLS_NEW;
    pc->srtp_ready = 0;
    pc->is_closed = 0;

    status = rtc_pc_alloc_sdp_buffer(&pc->arena,
                                     config->limits.sdp.max_description_bytes,
                                     &pc->local_description, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_pc_alloc_sdp_buffer(&pc->arena,
                                     config->limits.sdp.max_description_bytes,
                                     &pc->remote_description, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_pc_alloc_candidates(&pc->arena,
                                     config->limits.ice.max_candidates,
                                     &pc->remote_candidates, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_pc_alloc_candidate_summaries(
        &pc->arena, config->limits.ice.max_candidates,
        &pc->local_candidate_summaries, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    memset(pc->local_candidate_summaries, 0,
           config->limits.ice.max_candidates *
               sizeof(*pc->local_candidate_summaries));
    status = rtc_pc_alloc_candidate_summaries(
        &pc->arena, config->limits.ice.max_candidates,
        &pc->remote_candidate_summaries, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    memset(pc->remote_candidate_summaries, 0,
           config->limits.ice.max_candidates *
               sizeof(*pc->remote_candidate_summaries));
    status = rtc_pc_alloc_candidate_pairs(
        &pc->arena, config->limits.ice.max_candidate_pairs,
        &pc->candidate_pairs, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    memset(pc->candidate_pairs, 0,
           config->limits.ice.max_candidate_pairs * sizeof(*pc->candidate_pairs));
    status = rtc_pc_alloc_stun_transactions(
        &pc->arena, config->limits.ice.max_transactions,
        &pc->stun_transactions, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    memset(pc->stun_transactions, 0,
           config->limits.ice.max_transactions *
               sizeof(*pc->stun_transactions));
    status = rtc_pc_alloc_media_queue_slots(
        &pc->arena, config->limits.rtp.max_media_queue_slots,
        &pc->media_queue_slots, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    memset(pc->media_queue_slots, 0,
           config->limits.rtp.max_media_queue_slots *
               sizeof(*pc->media_queue_slots));
    status = rtc_pc_alloc_media_packet_cache(
        &pc->arena, config->limits.rtp.max_media_queue_slots,
        pc->media_packet_capacity, &pc->media_packet_cache, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    {
        size_t i;
        for (i = 0; i < config->limits.rtp.max_media_queue_slots; ++i) {
            pc->media_queue_slots[i].payload =
                pc->media_packet_cache +
                (i * pc->media_packet_capacity);
            pc->media_queue_slots[i].payload_capacity =
                pc->media_packet_capacity;
            pc->media_queue_slots[i].pc = pc;
            pc->media_queue_slots[i].dispatch_status = RTC_STATUS_OK;
            pc->media_queue_slots[i].in_use = 0;
        }
    }
    status = rtc_pc_alloc_h264_reassembly_buffer(
        &pc->arena, config->limits.rtp.max_reassembly_bytes,
        &pc->h264_reassembly_buffer, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_pc_alloc_security_session_storage(
        pc, config->security_backend, diag);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_security_init(pc, config->security_backend);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    *out_pc = pc;
    rtc_pc_trace(pc, RTC_TRACE_PC_CREATE, "create", RTC_STATUS_OK);

    return RTC_STATUS_OK;
}

static rtc_status_t rtc_unsupported_network(rtc_peer_connection_t *pc,
                                            const char *operation)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_NETWORK);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, operation);
    }

    return rtc_pc_unsupported(pc, operation);
}

rtc_status_t rtc_peer_connection_gather_candidates(rtc_peer_connection_t *pc)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_NETWORK);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "gather_candidates");
    }

    return rtc_ice_gather_candidates(pc);
}

rtc_status_t rtc_peer_connection_start_connectivity_checks(
    rtc_peer_connection_t *pc)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_NETWORK);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "start_connectivity_checks");
    }

    return rtc_ice_start_connectivity_checks(pc);
}

rtc_status_t rtc_peer_connection_destroy(rtc_peer_connection_t *pc)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "destroy");
    }

    pc->counters.api.destroy_calls++;
    rtc_security_shutdown(pc);
    rtc_pc_trace(pc, RTC_TRACE_PC_DESTROY, "destroy", RTC_STATUS_OK);
    pc->is_closed = 1;
    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_create_offer(rtc_peer_connection_t *pc,
                                              char *out_sdp,
                                              size_t *inout_sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "create_offer");
    }
    status = rtc_jsep_can_create_offer(pc->signaling_state);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "jsep", "create_offer",
                                0);
        rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_REJECT, "create_offer", status,
                          pc->signaling_state, RTC_SDP_TYPE_OFFER);
        return status;
    }
    status = rtc_security_prepare_local_fingerprint(pc);
    if (status != RTC_STATUS_OK) {
        rtc_pc_trace(pc, RTC_TRACE_SDP_WRITE, "create_offer", status);
        return status;
    }
    status = rtc_sdp_write_offer(&pc->sdp, RTC_SDP_DIRECTION_SENDRECV,
                                 RTC_SDP_DIRECTION_SENDRECV, out_sdp,
                                 inout_sdp_len);
    rtc_pc_trace(pc, RTC_TRACE_SDP_WRITE, "create_offer", status);
    return status;
}

rtc_status_t rtc_peer_connection_create_answer(rtc_peer_connection_t *pc,
                                               char *out_sdp,
                                               size_t *inout_sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "create_answer");
    }
    status = rtc_jsep_can_create_answer(pc->signaling_state);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "jsep", "create_answer",
                                0);
        rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_REJECT, "create_answer", status,
                          pc->signaling_state, RTC_SDP_TYPE_ANSWER);
        return status;
    }
    status = rtc_security_prepare_local_fingerprint(pc);
    if (status != RTC_STATUS_OK) {
        rtc_pc_trace(pc, RTC_TRACE_SDP_WRITE, "create_answer", status);
        return status;
    }
    status = rtc_sdp_write_answer(&pc->sdp, RTC_SDP_DIRECTION_SENDRECV,
                                  RTC_SDP_DIRECTION_SENDRECV, out_sdp,
                                  inout_sdp_len);
    rtc_pc_trace(pc, RTC_TRACE_SDP_WRITE, "create_answer", status);
    return status;
}

static rtc_status_t rtc_pc_store_description(rtc_peer_connection_t *pc,
                                             const char *operation,
                                             const char *sdp, size_t sdp_len,
                                             int is_local)
{
    rtc_sdp_description_t parsed;
    rtc_jsep_state_t next_state;
    rtc_status_t status;

    if (sdp == 0 || sdp_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (sdp_len >= pc->limits.sdp.max_description_bytes) {
        return RTC_STATUS_CAPACITY_SDP_BUFFER;
    }

    status = rtc_sdp_parse(sdp, sdp_len, &parsed);
    rtc_pc_trace(pc, RTC_TRACE_SDP_PARSE, operation, status);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "sdp", operation, 0);
        return status;
    }

    status = is_local ? rtc_jsep_apply_local(pc->signaling_state, parsed.type,
                                             &next_state)
                      : rtc_jsep_apply_remote(pc->signaling_state, parsed.type,
                                              &next_state);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "jsep", operation, 0);
        rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_REJECT, operation, status,
                          pc->signaling_state, parsed.type);
        return status;
    }

    if (is_local) {
        memcpy(pc->local_description, sdp, sdp_len);
        pc->local_description[sdp_len] = '\0';
        pc->local_description_len = sdp_len;
        pc->local_summary = parsed;
        if (parsed.type == RTC_SDP_TYPE_OFFER) {
            pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
        }
    } else {
        memcpy(pc->remote_description, sdp, sdp_len);
        pc->remote_description[sdp_len] = '\0';
        pc->remote_description_len = sdp_len;
        pc->remote_summary = parsed;
        if (parsed.type == RTC_SDP_TYPE_OFFER) {
            pc->ice_role = RTC_ICE_ROLE_CONTROLLED;
        }
    }
    pc->signaling_state = next_state;
    rtc_pc_trace_jsep(pc, RTC_TRACE_JSEP_TRANSITION, operation, RTC_STATUS_OK,
                      next_state, parsed.type);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_set_local_description(rtc_peer_connection_t *pc,
                                                       const char *sdp,
                                                       size_t sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "set_local_description");
    }
    return rtc_pc_store_description(pc, "set_local_description", sdp, sdp_len,
                                    1);
}

rtc_status_t rtc_peer_connection_set_remote_description(rtc_peer_connection_t *pc,
                                                        const char *sdp,
                                                        size_t sdp_len)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "set_remote_description");
    }
    return rtc_pc_store_description(pc, "set_remote_description", sdp, sdp_len,
                                    0);
}

typedef struct rtc_candidate_token_t {
    const char *ptr;
    size_t len;
} rtc_candidate_token_t;

static int rtc_token_equals(rtc_candidate_token_t token, const char *literal)
{
    return token.len == strlen(literal) &&
           memcmp(token.ptr, literal, token.len) == 0;
}

static int rtc_token_equals_ci(rtc_candidate_token_t token, const char *literal)
{
    size_t i;

    if (token.len != strlen(literal)) {
        return 0;
    }

    for (i = 0; i < token.len; ++i) {
        char left = token.ptr[i];
        char right = literal[i];
        if (left >= 'A' && left <= 'Z') {
            left = (char)(left - 'A' + 'a');
        }
        if (right >= 'A' && right <= 'Z') {
            right = (char)(right - 'A' + 'a');
        }
        if (left != right) {
            return 0;
        }
    }

    return 1;
}

static int rtc_token_copy(char *dest, size_t dest_size,
                          rtc_candidate_token_t token)
{
    if (dest == 0 || dest_size == 0 || token.len >= dest_size) {
        return 0;
    }

    memcpy(dest, token.ptr, token.len);
    dest[token.len] = '\0';
    return 1;
}

static int rtc_token_parse_u64(rtc_candidate_token_t token, uint64_t max_value,
                               uint64_t *out_value)
{
    size_t i;
    uint64_t value;

    if (token.len == 0 || out_value == 0) {
        return 0;
    }

    value = 0;
    for (i = 0; i < token.len; ++i) {
        uint64_t digit;

        if (token.ptr[i] < '0' || token.ptr[i] > '9') {
            return 0;
        }
        digit = (uint64_t)(token.ptr[i] - '0');
        if (value > (max_value - digit) / 10u) {
            return 0;
        }
        value = value * 10u + digit;
    }

    *out_value = value;
    return 1;
}

static int rtc_next_candidate_token(const char *text, size_t len,
                                    size_t *offset,
                                    rtc_candidate_token_t *out_token)
{
    size_t start;

    while (*offset < len && text[*offset] == ' ') {
        (*offset)++;
    }
    if (*offset >= len) {
        return 0;
    }

    start = *offset;
    while (*offset < len && text[*offset] != ' ') {
        (*offset)++;
    }

    out_token->ptr = text + start;
    out_token->len = *offset - start;
    return 1;
}

static rtc_status_t rtc_parse_remote_candidate(
    const char *candidate, size_t candidate_len,
    rtc_ice_candidate_summary_t *out_candidate)
{
    rtc_candidate_token_t foundation;
    rtc_candidate_token_t component;
    rtc_candidate_token_t transport;
    rtc_candidate_token_t priority;
    rtc_candidate_token_t address;
    rtc_candidate_token_t port;
    rtc_candidate_token_t typ;
    rtc_candidate_token_t type;
    size_t offset;
    uint64_t value;

    if (candidate_len >= 12u && memcmp(candidate, "a=candidate:", 12u) == 0) {
        offset = 12u;
    } else if (candidate_len >= 10u &&
               memcmp(candidate, "candidate:", 10u) == 0) {
        offset = 10u;
    } else {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    if (!rtc_next_candidate_token(candidate, candidate_len, &offset,
                                  &foundation) ||
        !rtc_next_candidate_token(candidate, candidate_len, &offset,
                                  &component) ||
        !rtc_next_candidate_token(candidate, candidate_len, &offset,
                                  &transport) ||
        !rtc_next_candidate_token(candidate, candidate_len, &offset,
                                  &priority) ||
        !rtc_next_candidate_token(candidate, candidate_len, &offset,
                                  &address) ||
        !rtc_next_candidate_token(candidate, candidate_len, &offset, &port) ||
        !rtc_next_candidate_token(candidate, candidate_len, &offset, &typ) ||
        !rtc_next_candidate_token(candidate, candidate_len, &offset, &type)) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    if (!rtc_token_copy(out_candidate->foundation,
                        sizeof(out_candidate->foundation), foundation) ||
        !rtc_token_copy(out_candidate->transport,
                        sizeof(out_candidate->transport), transport) ||
        !rtc_token_copy(out_candidate->address, sizeof(out_candidate->address),
                        address)) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    if (!rtc_token_parse_u64(component, 1u, &value) || value != 1u) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    out_candidate->component = (uint16_t)value;

    if (!rtc_token_equals_ci(transport, "udp")) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    if (!rtc_token_parse_u64(priority, UINT32_MAX, &value)) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    out_candidate->priority = (uint32_t)value;

    if (!rtc_token_parse_u64(port, 65535u, &value) || value == 0u) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    out_candidate->port = (uint16_t)value;

    if (!rtc_token_equals(typ, "typ")) {
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    if (rtc_token_equals(type, "host")) {
        out_candidate->type = RTC_ICE_CANDIDATE_TYPE_HOST;
    } else if (rtc_token_equals(type, "srflx")) {
        out_candidate->type = RTC_ICE_CANDIDATE_TYPE_SRFLX;
    } else {
        return RTC_STATUS_UNSUPPORTED;
    }

    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_add_ice_candidate(rtc_peer_connection_t *pc,
                                                   const char *candidate,
                                                   size_t candidate_len)
{
    rtc_status_t status;
    rtc_ice_candidate_summary_t parsed;
    char *slot;
    size_t candidate_id;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }
    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "add_ice_candidate");
    }
    if (candidate == 0 || candidate_len == 0 ||
        candidate_len >= pc->remote_candidate_slot_bytes) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    status = rtc_parse_remote_candidate(candidate, candidate_len, &parsed);
    if (status == RTC_STATUS_PROTOCOL_ERROR) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR, "ice",
                                "add_ice_candidate", 0);
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "ice",
                                "add_ice_candidate", 0);
        return status;
    }
    if (pc->remote_candidate_count >= pc->limits.ice.max_candidates) {
        return RTC_STATUS_CAPACITY_ICE_CANDIDATES;
    }

    candidate_id = pc->remote_candidate_count;
    slot = pc->remote_candidates +
           candidate_id * pc->remote_candidate_slot_bytes;
    memcpy(slot, candidate, candidate_len);
    slot[candidate_len] = '\0';
    pc->remote_candidate_summaries[candidate_id] = parsed;
    pc->remote_candidate_count++;
    pc->counters.ice.remote_candidates++;
    if (pc->connectivity_checks_started) {
        status = rtc_ice_add_remote_candidate_pairs(pc, candidate_id);
        if (status != RTC_STATUS_OK) {
            pc->remote_candidate_count--;
            return status;
        }
        pc->remote_candidate_pending_pairs = 0;
    }
    rtc_pc_trace_remote_candidate(pc, &parsed, candidate_id);
    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_receive_datagram(rtc_peer_connection_t *pc,
                                                  const uint8_t *data,
                                                  size_t data_len)
{
    rtc_status_t status;
    rtc_net_protocol_t protocol;
    const char *reason = 0;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_NETWORK);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "receive_datagram");
    }

    status = rtc_net_demux_datagram(data, data_len, &protocol);
    if (status != RTC_STATUS_OK) {
        rtc_observer_emit_error(&pc->observer, status, "net",
                                "receive_datagram", 0);
        rtc_pc_trace_demux(pc, RTC_NET_PROTOCOL_UNKNOWN, status,
                           "invalid_datagram");
        return status;
    }

    if (protocol == RTC_NET_PROTOCOL_UNKNOWN &&
        rtc_pc_datagram_looks_like_stun(data, data_len)) {
        protocol = RTC_NET_PROTOCOL_STUN;
        reason = "malformed_stun";
    }

    rtc_pc_note_demux_counter(pc, protocol);
    rtc_pc_trace_demux(pc, protocol, RTC_STATUS_OK, reason);

    if (protocol == RTC_NET_PROTOCOL_STUN) {
        status = rtc_ice_handle_stun_response(pc, data, data_len);
        if (status != RTC_STATUS_OK) {
            if (reason == 0) {
                reason = "malformed_stun";
            }
            rtc_pc_trace_demux(pc, protocol, status, reason);
            return RTC_STATUS_PROTOCOL_ERROR;
        }
        return RTC_STATUS_OK;
    }

    if (protocol == RTC_NET_PROTOCOL_DTLS) {
        return rtc_security_handle_dtls_datagram(pc, data, data_len);
    }

    if (protocol == RTC_NET_PROTOCOL_RTP) {
        return rtc_media_handle_rtp_datagram(pc, data, data_len);
    }

    if (protocol == RTC_NET_PROTOCOL_RTCP) {
        return rtc_media_handle_rtcp_datagram(pc, data, data_len);
    }

    if (protocol == RTC_NET_PROTOCOL_UNKNOWN) {
        rtc_observer_emit_error(&pc->observer, RTC_STATUS_PROTOCOL_ERROR, "net",
                                "receive_datagram", 0);
        rtc_pc_trace_demux(pc, protocol, RTC_STATUS_PROTOCOL_ERROR,
                           "unknown_datagram");
        return RTC_STATUS_PROTOCOL_ERROR;
    }

    return RTC_STATUS_OK;
}

rtc_status_t rtc_peer_connection_send_media_frame(
    rtc_peer_connection_t *pc,
    const rtc_media_frame_t *frame)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_MEDIA);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "send_media_frame");
    }

    if (frame == 0 || frame->data == 0 || frame->data_len == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }
    if (!rtc_media_kind_supported(frame->kind)) {
        return RTC_STATUS_UNSUPPORTED;
    }

    return rtc_media_send_frame(pc, frame);
}

rtc_status_t rtc_peer_connection_request_keyframe(
    rtc_peer_connection_t *pc,
    rtc_media_kind_t kind)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK) {
        return status;
    }

    status = rtc_executor_require(RTC_EXECUTOR_MEDIA);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "request_keyframe");
    }

    if (!rtc_media_kind_supported(kind)) {
        return RTC_STATUS_UNSUPPORTED;
    }

    return rtc_pc_unsupported(pc, "request_keyframe");
}

rtc_status_t rtc_peer_connection_get_counters(
    rtc_peer_connection_t *pc,
    rtc_peer_connection_counters_t *out_counters)
{
    rtc_status_t status;

    status = rtc_require_pc(pc);
    if (status != RTC_STATUS_OK || out_counters == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    status = rtc_executor_require(RTC_EXECUTOR_SIGNALING);
    if (status != RTC_STATUS_OK) {
        return rtc_pc_affinity_violation(pc, "get_counters");
    }

    *out_counters = pc->counters;
    return RTC_STATUS_OK;
}
