#include "observability/counters.h"

void rtc_counters_init(rtc_peer_connection_counters_t *counters)
{
    if (counters == 0) {
        return;
    }

    counters->memory.capacity_errors = 0;
    counters->executor.affinity_errors = 0;
    counters->api.create_calls = 0;
    counters->api.destroy_calls = 0;
    counters->api.unsupported_api_calls = 0;
    counters->ice.local_candidates = 0;
    counters->ice.remote_candidates = 0;
    counters->ice.candidate_pairs = 0;
    counters->ice.selected_pairs = 0;
    counters->ice.gathering_failures = 0;
    counters->ice.checks_failed = 0;
    counters->stun.transactions_sent = 0;
    counters->stun.transactions_received = 0;
    counters->stun.transactions_timed_out = 0;
    counters->net.demux_stun = 0;
    counters->net.demux_dtls = 0;
    counters->net.demux_rtp = 0;
    counters->net.demux_rtcp = 0;
    counters->net.demux_unknown = 0;
    counters->dtls.handshake_started = 0;
    counters->dtls.handshake_completed = 0;
    counters->dtls.handshake_failed = 0;
    counters->dtls.early_datagrams_rejected = 0;
    counters->dtls.outgoing_datagrams = 0;
    counters->dtls.fingerprint_mismatch = 0;
    counters->dtls.key_export_failed = 0;
    counters->dtls.srtp_init_failed = 0;
    counters->srtp.protect_failed = 0;
    counters->srtp.unprotect_failed = 0;
    counters->srtp.replay_failed = 0;
    counters->trace.trace_events = 0;
}

void rtc_counters_note_trace(rtc_peer_connection_counters_t *counters)
{
    if (counters != 0) {
        counters->trace.trace_events++;
    }
}

void rtc_counters_note_capacity_error(rtc_peer_connection_counters_t *counters)
{
    if (counters != 0) {
        counters->memory.capacity_errors++;
    }
}

void rtc_counters_note_affinity_error(rtc_peer_connection_counters_t *counters)
{
    if (counters != 0) {
        counters->executor.affinity_errors++;
    }
}

void rtc_counters_note_unsupported_api(rtc_peer_connection_counters_t *counters)
{
    if (counters != 0) {
        counters->api.unsupported_api_calls++;
    }
}
