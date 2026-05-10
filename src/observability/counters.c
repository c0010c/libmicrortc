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
